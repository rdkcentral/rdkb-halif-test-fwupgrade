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

static void handle_client(int fd, const volatile sig_atomic_t *stop)
{
    unsigned char request[VL_MAX_PAYLOAD];
    unsigned char response[VL_MAX_PAYLOAD];
    uint32_t size;
    if (vl_recv_frame_cancel(fd, vl_component_config.magic, VL_MSG_REQUEST, request,
                   sizeof(request), &size, stop) < 0 || vl_cancelled(stop))
        return;
    if (size < 3 ||
        (request[0] != VL_REQ_HAL &&
         request[0] != VL_REQ_HAL_JSON) ||
        request[size - 1] != '\0' || memchr(request + 1, '\0', size - 2)) {
        response[0] = VL_INVALID_REQUEST;
        snprintf((char *)response + 1, sizeof(response) - 1,
                 "%s", vl_component_config.invalid_payload);
    } else {
        if (vl_cancelled(stop))
            return;
        if (request[0] == VL_REQ_HAL_JSON)
            response[0] = vl_component_config.dispatch_json((char *)request + 1,
                                          (char *)response + 1,
                                          sizeof(response) - 1);
        else
            response[0] = vl_component_config.dispatch_command((char *)request + 1,
                                          (char *)response + 1,
                                          sizeof(response) - 1);
    }
    size = (uint32_t)strlen((char *)response + 1) + 2;
    if (vl_send_frame_cancel(fd, vl_component_config.magic, VL_MSG_RESPONSE, response, size, stop) < 0 &&
        !vl_cancelled(stop))
        fprintf(stderr, "Client response failed: %s\n", strerror(errno));
}

/* PUBLIC_INTERFACE */
/** Poll for clients and serve one HAL request at a time until signaled to stop. */
int vl_server_loop(int fd, const volatile sig_atomic_t *stop)
{
    struct pollfd pfd = {fd, POLLIN, 0};
    while (!*stop) {
        int rc = poll(&pfd, 1, 200);
        int client;
        if (*stop)
            break;
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (rc == 0)
            continue;
        if (!(pfd.revents & POLLIN)) {
            errno = EIO;
            return -1;
        }
        client = accept(fd, NULL, NULL);
        if (client < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (!*stop && fcntl(client, F_SETFD, FD_CLOEXEC) == 0)
            handle_client(client, stop);
        close(client);
    }
    return 0;
}
