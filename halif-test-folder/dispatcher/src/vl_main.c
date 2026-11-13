#define _POSIX_C_SOURCE 200809L
#include "vl_generic.h"


#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stopped;

static void stop_server(int signal_number)
{
    (void)signal_number;
    stopped = 1;
}

/* PUBLIC_INTERFACE */
/**
 * Start the configured component VL server using optional --socket=PATH, then
 * its nonempty socket environment override, then its configured default path.
 * Return zero on graceful shutdown, nonzero on configuration/transport failure.
 */
int main(int argc, char **argv)
{
    /* Read the socket environment override configured by the component. */
    const char *path = getenv(vl_component_config.socket_env);
    struct sigaction action;
    int fd, rc, i;
    /* Resolve the fallback before parsing so explicit values retain priority. */
    if (!path || !*path)
        path = VL_DEFAULT_SOCKET_PATH;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--socket=PATH]\nSocket environment: %s; default: %s\n", argv[0], vl_component_config.socket_env, VL_DEFAULT_SOCKET_PATH);
            return 0;
        }
        if (strncmp(argv[i], "--socket=", 9) != 0) {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 2;
        }
        path = argv[i] + 9;
    }
    if (!path || !*path) {
        fprintf(stderr, "Provide a nonempty --socket=PATH or component socket environment value\n");
        return 2;
    }
    memset(&action, 0, sizeof(action));
    action.sa_handler = stop_server;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGTERM, &action, NULL) < 0) {
        perror("sigaction");
        return 1;
    }
    fd = vl_create_server(path);
    if (fd < 0) {
        perror("Create server socket (existing paths are not overwritten)");
        return 1;
    }
    fprintf(stderr, "%s VL MOCK server on %s (no hardware calls)\n", vl_component_config.name, path);
    rc = vl_server_loop(fd, &stopped);
    if (rc < 0)
        fprintf(stderr, "Server failed: %s\n", strerror(errno));
    close(fd);
    unlink(path);
    return rc < 0 ? 1 : 0;
}
