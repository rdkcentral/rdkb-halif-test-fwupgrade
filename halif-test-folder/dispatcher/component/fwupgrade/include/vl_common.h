#ifndef FWUPGRADE_VL_COMMON_H
#define FWUPGRADE_VL_COMMON_H

#include <stdint.h>

#define FWUPGRADE_VL_MAGIC UINT32_C(0x464c5631) /* "FLV1": FWUpgrade VL, protocol version 1 */
#define FWUPGRADE_VL_MAX_PAYLOAD 8192U
#define FWUPGRADE_VL_TIMEOUT_MS 10000
/* Used when FWUPGRADE_VL_SOCKET_PATH is unset/empty and --socket is omitted. */
#define FWUPGRADE_VL_DEFAULT_SOCKET_PATH "/tmp/fwupgrade_vl-run.sock"
#define FWUPGRADE_VL_MSG_ACK 2U
#define FWUPGRADE_VL_MSG_REQUEST 3U
#define FWUPGRADE_VL_MSG_RESPONSE 4U
#define FWUPGRADE_VL_REQ_HAL 1U
#define FWUPGRADE_VL_REQ_HAL_JSON 2U

typedef enum {
    FWUPGRADE_VL_OK = 0,
    FWUPGRADE_VL_INVALID_REQUEST = 1,
    FWUPGRADE_VL_HAL_ERROR = 2
} fwupgrade_vl_status_t;

/*
 * Wire header: three network-byte-order uint32_t words: magic, type, size.
 * ACK: the same header with type=ACK and size=0.
 * Request payload: HAL request byte (1), followed by a NUL-terminated
 * command string: "--fwupgrade-api=NAME [--FIELD=VALUE ...]" or
 * "--list-apis". HAL calls have no API-specific shortcuts.
 * Value-bearing options also accept "--key value". Tokens must not contain
 * whitespace; socket selection is CLI-local and never part of the payload.
 * JSON request payload: request byte (2), API name, newline, and a
 * NUL-terminated JSON object. JSON whitespace is preserved verbatim.
 * Supported JSON schemas (each requires exactly its listed fields):
 *   fwupgrade_hal_set_download_url       {"url": string|null, "filename": string|null}
 *   fwupgrade_hal_set_download_interface {"interface": integer}
 *   fwupgrade_hal_download_install       {"url": string|null}
 * Strings carry no escapes or control characters. A JSON null becomes a real
 * local NULL pointer in the server; no pointer or struct ever crosses the wire.
 * Inline document, not a path.
 * API selection, argument validation, and HAL calls belong to the server.
 * Response payload: status byte, followed by NUL-terminated result text.
 * Exchange: request header, ACK, request payload, response header, ACK,
 * response payload. One request per connection. No pointers/structs on wire.
 */
#endif
