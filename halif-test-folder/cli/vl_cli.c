#define _POSIX_C_SOURCE 200809L
#include "vl_generic.h"
#include "../dispatcher/component/ethsw/include/vl_common.h"
#include "../dispatcher/component/fwupgrade/include/vl_common.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct component_route {
    const char *name;
    uint32_t magic;
    const char *selector;
};

static const struct component_route routes[] = {
    {"ethsw",       ETHSW_VL_MAGIC,       "--ethsw-api"},
    {"platform",    UINT32_C(0x50564c31), "--platform-api"},
    {"fwupgrade",   FWUPGRADE_VL_MAGIC,   "--fwupgrade-api"}
};

static void usage(const char *program)
{
    printf("Usage: %s --component=NAME --socket=PATH "
           "(--api=NAME [--FIELD=VALUE ...] [--json=JSON] | --list-apis)\n"
           "Components: bridge_util, ethsw, platform, fwupgrade.\n"
           "Magic and API selector are resolved internally.\n"
           "Exit: 0 success, 1 HAL/transport failure, 2 invalid request.\n",
           program);
}

static int token(const char *s)
{
    return *s && !strpbrk(s, " \t\r\n");
}

static int append(unsigned char *request, size_t *used, const char *s)
{
    size_t length = strlen(s);
    if (!token(s) || *used + length + 2 > VL_MAX_PAYLOAD)
        return -1;
    if (*used > 1)
        request[(*used)++] = ' ';
    memcpy(request + *used, s, length + 1);
    *used += length;
    return 0;
}



/* PUBLIC_INTERFACE */
/** Forward one request using explicit endpoint, protocol and server selector. */
int main(int argc, char **argv)
{
    const char *path = NULL, *component_name = NULL, *selector = NULL;
    const char *api_name = NULL, *json = NULL;
    const struct component_route *route = NULL;
    struct { const char *name; const char **value; } options[] = {
        {"--socket", &path},
        {"--component", &component_name},
        {"--api", &api_name},
        {"--json", &json}
    };
    unsigned char request[VL_MAX_PAYLOAD] = {VL_REQ_HAL, 0};
    unsigned char response[VL_MAX_PAYLOAD];
    size_t used = 1, j;
    uint32_t size, magic;

    int i, fd, listing = 0, fields = 0;
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help")) {
            usage(argv[0]);
            return 0;
        }
        for (j = 0; j < sizeof(options) / sizeof(options[0]); ++j) {
            size_t n = strlen(options[j].name);
            const char *value;
            if (strncmp(argv[i], options[j].name, n) ||
                (argv[i][n] && argv[i][n] != '='))
                continue;
            if (*options[j].value) {
                fprintf(stderr, "Duplicate routing/API/JSON option\n");
                return 2;
            }
            if (argv[i][n] == '=')
                value = argv[i] + n + 1;
            else if (++i < argc)
                value = argv[i];
            else {
                fprintf(stderr, "Option requires a value\n");
                return 2;
            }
            if (options[j].value != &json &&
                (!*value || !strncmp(value, "--", 2))) {
                fprintf(stderr, "Option requires a nonempty value\n");
                return 2;
            }
            *options[j].value = value;
            break;
        }
        if (j < sizeof(options) / sizeof(options[0]))
            continue;
        if (!strcmp(argv[i], "--magic") ||
            !strncmp(argv[i], "--magic=", sizeof("--magic=") - 1) ||
            !strcmp(argv[i], "--api-option") ||
            !strncmp(argv[i], "--api-option=", sizeof("--api-option=") - 1)) {
            fprintf(stderr,
                    "Use --component; --magic and --api-option are no longer supported\n");
            return 2;
        }
        if (!strcmp(argv[i], "--list-apis")) {
            if (listing++) {
                fprintf(stderr, "Duplicate --list-apis\n");
                return 2;
            }
            continue;
        }
        if (append(request, &used, argv[i]) < 0) {
            fprintf(stderr, "Command arguments must be nonempty tokens without whitespace and fit the request\n");
            return 2;
        }
        fields = 1;
    }
    if (!path || !component_name ||
        (listing ? (api_name || json || fields) : !api_name) ||
        (api_name && !token(api_name)) || (json && fields)) {
        usage(argv[0]);
        return 2;
    }

    for (j = 0; j < sizeof(routes) / sizeof(routes[0]); ++j) {
        if (!strcmp(component_name, routes[j].name)) {
            route = &routes[j];
            break;
        }
    }

    if (!route) {
        fprintf(stderr, "Unknown component: %s\n", component_name);
        return 2;
    }

    magic = route->magic;
    selector = route->selector;
    if (fields) {
        const char *cursor = (const char *)request + 1;
        size_t n = strlen(selector);
        while (*cursor) {
            if (!strncmp(cursor, selector, n) &&
                (!cursor[n] || cursor[n] == '=' || cursor[n] == ' ')) {
                fprintf(stderr, "Use --api, not the wire API selector\n");
                return 2;
            }
            cursor = strchr(cursor, ' ');
            if (!cursor)
                break;
            ++cursor;
        }
    }
    if (json) {
        int length = snprintf((char *)request + 1, sizeof(request) - 1,
                              "%s\n%s", api_name, json);
        if (length < 0 || (size_t)length >= sizeof(request) - 1) {
            fprintf(stderr, "JSON request too large\n");
            return 2;
        }
        request[0] = VL_REQ_HAL_JSON;
        used = (size_t)length + 1;
    } else if (listing) {
        if (append(request, &used, "--list-apis") < 0)
            return 2;
    } else if (append(request, &used, selector) < 0 ||
               append(request, &used, api_name) < 0) {
        fprintf(stderr, "Request too large\n");
        return 2;
    }
    fd = vl_connect(path);
    if (fd < 0) {
        perror("Connect to VL");
        return 1;
    }
    if (vl_send_frame(fd, magic, VL_MSG_REQUEST, request,
                              (uint32_t)used + 1) < 0 ||
        vl_recv_frame(fd, magic, VL_MSG_RESPONSE, response,
                              sizeof(response), &size) < 0) {
        perror("VL exchange");
        close(fd);
        return 1;
    }
    close(fd);
    if (size < 2 || response[size - 1] != '\0' ||
        memchr(response + 1, '\0', size - 2) ||
        response[0] > VL_HAL_ERROR) {
        fprintf(stderr, "Invalid server response\n");
        return 1;
    }
    /* Display response, excluding the wire status byte and terminating NUL. */
    printf("Response received (%u bytes)\n", (unsigned int)size);
    if (size > 2) {
        printf("\n=== RESPONSE ===\n");
        fwrite(response + 1, 1, size - 2, stdout);
        printf("\n=== END ===\n\n");
    }
    if (response[0] == VL_INVALID_REQUEST)
        return 2;
    return response[0] == VL_OK ? 0 : 1;
}
