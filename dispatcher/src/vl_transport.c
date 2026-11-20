#define _POSIX_C_SOURCE 200809L

#include "vl_generic.h"


#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static int64_t now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        return -1;
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int vl_cancelled(const volatile sig_atomic_t *stop)
{
    if (stop && *stop) {
        errno = ECANCELED;
        return 1;
    }
    return 0;
}

static int wait_ready(int fd, short events, int64_t deadline,
                      const volatile sig_atomic_t *stop)
{
    struct pollfd pfd = {fd, events, 0};
    for (;;) {
        int64_t now = now_ms();
        int64_t remaining = deadline - now;
        int rc;
        if (vl_cancelled(stop))
            return -1;
        if (now < 0)
            return -1;
        if (remaining <= 0) {
            errno = ETIMEDOUT;
            return -1;
        }
        /* Bound the check-to-poll signal race as well as interrupted waits. */
        rc = poll(&pfd, 1, remaining > 200 ? 200 : (int)remaining);
        if (vl_cancelled(stop))
            return -1;
        if (rc < 0 && errno == EINTR)
            continue;
        if (rc == 0)
            continue;
        if (rc < 0)
            return -1;
        /* Read queued data even when POLLHUP accompanies POLLIN. */
        if (pfd.revents & events)
            return 0;
        errno = ECONNRESET;
        return -1;
    }
}

static int transfer(int fd, void *buffer, size_t size, int writing,
                    int64_t deadline, const volatile sig_atomic_t *stop)
{
    unsigned char *bytes = buffer;
    size_t offset = 0;
    while (offset < size) {
        ssize_t count;
        if (wait_ready(fd, writing ? POLLOUT : POLLIN, deadline, stop) < 0)
            return -1;
        if (writing)
            count = send(fd, bytes + offset, size - offset,
                         MSG_NOSIGNAL | MSG_DONTWAIT);
        else
            count = recv(fd, bytes + offset, size - offset, MSG_DONTWAIT);
        if (count < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            return -1;
        }
        if (count == 0) {
            errno = ECONNRESET;
            return -1;
        }
        offset += (size_t)count;
    }
    return 0;
}

static int address(const char *path, struct sockaddr_un *addr)
{
    if (!path || !*path || strlen(path) >= sizeof(addr->sun_path)) {
        errno = EINVAL;
        return -1;
    }
    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;
    memcpy(addr->sun_path, path, strlen(path) + 1);
    return 0;
}

static int new_socket(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd >= 0 && fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    return fd;
}

/* PUBLIC_INTERFACE */
/** Create an owner-only listening endpoint; existing paths cause bind failure. */
int vl_create_server(const char *path)
{
    struct sockaddr_un addr;
    mode_t previous;
    int fd, rc, saved;
    if (address(path, &addr) < 0)
        return -1;
    fd = new_socket();
    if (fd < 0)
        return -1;
    /* Avoid the bind-to-chmod window; this application is single-threaded. */
    previous = umask(0077);
    rc = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    saved = errno;
    umask(previous);
    if (rc < 0) {
        close(fd);
        errno = saved;
        return -1;
    }
    if (listen(fd, 8) < 0) {
        saved = errno;
        close(fd);
        unlink(path);
        errno = saved;
        return -1;
    }
    return fd;
}

int vl_send_frame_cancel(int fd, uint32_t magic, uint32_t type, const void *data, uint32_t size,
                      const volatile sig_atomic_t *stop)
{
    uint32_t header[3] = {htonl(magic), htonl(type), htonl(size)};
    uint32_t ack[3];
    int64_t deadline = now_ms() + VL_TIMEOUT_MS;
    if (!data || !size || size > VL_MAX_PAYLOAD ||
        (type != VL_MSG_REQUEST && type != VL_MSG_RESPONSE)) {
        errno = EINVAL;
        return -1;
    }
    if (transfer(fd, header, sizeof(header), 1, deadline, stop) < 0 ||
        transfer(fd, ack, sizeof(ack), 0, deadline, stop) < 0)
        return -1;
    if (ntohl(ack[0]) != magic ||
        ntohl(ack[1]) != VL_MSG_ACK || ntohl(ack[2]) != 0) {
        errno = EPROTO;
        return -1;
    }
    return transfer(fd, (void *)data, size, 1, deadline, stop);
}

/* PUBLIC_INTERFACE */
/** Send header, require a valid ACK, then send all payload bytes by deadline. */
int vl_send_frame(int fd, uint32_t magic, uint32_t type, const void *data, uint32_t size)
{
    return vl_send_frame_cancel(fd, magic, type, data, size, NULL);
}

int vl_recv_frame_cancel(int fd, uint32_t magic, uint32_t type, void *data,
                      uint32_t capacity, uint32_t *size,
                      const volatile sig_atomic_t *stop)
{
    uint32_t header[3];
    uint32_t ack[3] = {htonl(magic), htonl(VL_MSG_ACK), 0};
    uint32_t length;
    int64_t deadline = now_ms() + VL_TIMEOUT_MS;
    if (!data || !size) {
        errno = EINVAL;
        return -1;
    }
    if (transfer(fd, header, sizeof(header), 0, deadline, stop) < 0)
        return -1;
    length = ntohl(header[2]);
    if (ntohl(header[0]) != magic || ntohl(header[1]) != type ||
        !length || length > capacity || length > VL_MAX_PAYLOAD) {
        errno = EPROTO;
        return -1;
    }
    if (transfer(fd, ack, sizeof(ack), 1, deadline, stop) < 0 ||
        transfer(fd, data, length, 0, deadline, stop) < 0)
        return -1;
    *size = length;
    return 0;
}

/* PUBLIC_INTERFACE */
/** Validate the frame header before ACK; read exactly the declared payload. */
int vl_recv_frame(int fd, uint32_t magic, uint32_t type, void *data,
                          uint32_t capacity, uint32_t *size)
{
    return vl_recv_frame_cancel(fd, magic, type, data, capacity, size, NULL);
}

int vl_connect(const char *path)
{
    struct sockaddr_un addr;
    int fd, flags, rc, error = 0;
    int64_t deadline, now;
    socklen_t length = sizeof(error);
    if (address(path, &addr) < 0)
        return -1;
    now = now_ms();
    if (now < 0)
        return -1;
    deadline = now + VL_TIMEOUT_MS;
    fd = new_socket();
    if (fd < 0)
        return -1;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        goto fail;
    for (;;) {
        now = now_ms();
        if (now < 0)
            goto fail;
        if (now >= deadline) {
            errno = ETIMEDOUT;
            goto fail;
        }
        rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        if (rc == 0)
            return fd;
        if (errno == EAGAIN) {
            /* Queue saturation has not started a connection. Neither POLLOUT
             * nor SO_ERROR proves success; pace actual connection retries.
             */
            now = now_ms();
            if (now < 0)
                goto fail;
            if (now >= deadline) {
                errno = ETIMEDOUT;
                goto fail;
            }
            rc = poll(NULL, 0, deadline - now > 10 ? 10 :
                      (int)(deadline - now));
            if (rc < 0 && errno != EINTR)
                goto fail;
            continue;
        }
        if (errno != EINPROGRESS)
            goto fail;
        if (wait_ready(fd, POLLOUT, deadline, NULL) < 0 ||
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0)
            goto fail;
        if (error) {
            errno = error;
            goto fail;
        }
        return fd;
    }
fail:
    error = errno;
    close(fd);
    errno = error;
    return -1;
}

