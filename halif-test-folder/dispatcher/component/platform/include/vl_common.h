#ifndef PLATFORM_VL_COMMON_H
#define PLATFORM_VL_COMMON_H

#include <stdint.h>

#define PLATFORM_VL_MAGIC UINT32_C(0x50564c31) /* "PVL1", protocol version 1 */
#define PLATFORM_VL_MAX_PAYLOAD 8192U
#define PLATFORM_VL_TIMEOUT_MS 10000
/* Used when PLATFORM_VL_SOCKET_PATH is unset/empty and --socket is omitted. */
#define PLATFORM_VL_DEFAULT_SOCKET_PATH "/tmp/platform_vl-run.sock"
#define PLATFORM_VL_MSG_ACK 2U
#define PLATFORM_VL_MSG_REQUEST 3U
#define PLATFORM_VL_MSG_RESPONSE 4U
#define PLATFORM_VL_REQ_HAL 1U
#define PLATFORM_VL_REQ_HAL_JSON 2U

typedef enum {
    PLATFORM_VL_OK = 0,
    PLATFORM_VL_INVALID_REQUEST = 1,
    PLATFORM_VL_HAL_ERROR = 2
} platform_vl_status_t;

/*
 * Wire header: three network-byte-order uint32_t words: magic, type, size.
 * ACK: the same header with type=ACK and size=0.
 * Request payload: HAL request byte (1), followed by a NUL-terminated
 * command string: "--platform-api=NAME [--FIELD=VALUE ...]" or
 * "--list-apis". HAL calls have no API-specific shortcuts.
 * Value-bearing options also accept "--key value". Tokens must not contain
 * whitespace; socket selection is CLI-local and never part of the payload.
 * JSON request payload: request byte (2), API name, newline, and a
 * NUL-terminated JSON object. JSON whitespace is preserved verbatim.
 * Supports platform_hal_setLed with integer fields LedColor, State, Interval,
 * and platform_hal_SetTelnetEnable with integer field flag (0 or 1).
 * Each schema requires exactly its listed fields. Inline document, not a path.
 * API selection, argument validation, and HAL calls belong to the server.
 * Response payload: status byte, followed by NUL-terminated result text.
 * Exchange: request header, ACK, request payload, response header, ACK,
 * response payload. One request per connection. No pointers/structs on wire.
 */
#endif
