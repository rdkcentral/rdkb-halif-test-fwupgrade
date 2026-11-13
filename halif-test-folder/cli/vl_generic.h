#ifndef VL_GENERIC_H
#define VL_GENERIC_H
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    const char *name, *api_option, *socket_env, *default_socket;
    uint32_t magic;
    const char *invalid_payload;
    int (*dispatch_command)(char *, char *, size_t);
    int (*dispatch_json)(char *, char *, size_t);
} vl_component;
extern const vl_component vl_component_config;
#define VL_MAX_PAYLOAD 8192U
#define VL_TIMEOUT_MS 10000
#define VL_DEFAULT_SOCKET_PATH (vl_component_config.default_socket)
#define VL_MSG_ACK 2U
#define VL_MSG_REQUEST 3U
#define VL_MSG_RESPONSE 4U
#define VL_REQ_HAL 1U
#define VL_REQ_HAL_JSON 2U
#define VL_OK 0
#define VL_INVALID_REQUEST 1
#define VL_HAL_ERROR 2
int vl_connect(const char *);
int vl_create_server(const char *);
int vl_send_frame(int, uint32_t, uint32_t, const void *, uint32_t);
int vl_recv_frame(int, uint32_t, uint32_t, void *, uint32_t, uint32_t *);
int vl_send_frame_cancel(int, uint32_t, uint32_t, const void *, uint32_t, const volatile sig_atomic_t *);
int vl_recv_frame_cancel(int, uint32_t, uint32_t, void *, uint32_t, uint32_t *, const volatile sig_atomic_t *);
int vl_cancelled(const volatile sig_atomic_t *);
int vl_server_loop(int, const volatile sig_atomic_t *);
#endif

