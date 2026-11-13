#ifndef ETHSW_VL_COMMON_H
#define ETHSW_VL_COMMON_H

#include <stdint.h>

#define ETHSW_VL_MAGIC UINT32_C(0x45535731) /* "ESW1": EthSW VL, protocol version 1 */
#define ETHSW_VL_MAX_PAYLOAD 8192U
#define ETHSW_VL_TIMEOUT_MS 10000
/* Used when ETHSW_VL_SOCKET_PATH is unset/empty and --socket is omitted. */
#define ETHSW_VL_DEFAULT_SOCKET_PATH "/tmp/ethsw_vl-run.sock"
#define ETHSW_VL_SOCKET_ENV "ETHSW_VL_SOCKET_PATH"

/*
 * Wire header: three network-byte-order uint32_t words: magic, type, size.
 * ACK: the same header with type=ACK and size=0.
 * Request payload: HAL request byte (1), followed by a NUL-terminated
 * command string: "--ethsw-api=NAME [--FIELD=VALUE ...]" or "--list-apis".
 * HAL calls have no API-specific shortcuts. NAME is the ccsp_hal_ethsw.h
 * function name, e.g. CcspHalEthSwGetPortStatus.
 * Value-bearing options also accept "--key value". Tokens must not contain
 * whitespace; socket selection is CLI-local and never part of the payload.
 * JSON request payload: request byte (2), API name, newline, and a
 * NUL-terminated JSON object. JSON whitespace is preserved verbatim.
 * Supported JSON schemas (each requires exactly its listed fields):
 *   CcspHalEthSwSetPortCfg              {"PortId": int, "LinkRate": int, "DuplexMode": int}
 *   CcspHalEthSwSetPortAdminStatus      {"PortId": int, "AdminStatus": int}
 *   CcspHalEthSwSetAgingSpeed           {"PortId": int, "AgingSpeed": int}
 *   CcspHalEthSwLocatePortByMacAddress  {"mac": string|null}
 *   CcspHalExtSw_setEthWanEnable        {"Flag": int}
 *   CcspHalExtSw_setEthWanPort          {"Port": int}
 *   CcspHalExtSw_ethPortConfigure       {"ifname": string|null, "WanMode": int}
 *                                       (only when built with
 *                                       FEATURE_RDKB_WAN_MANAGER and
 *                                       FEATURE_RDKB_AUTO_PORT_SWITCH)
 * Strings carry no escapes or control characters. A JSON null becomes a real
 * local NULL pointer in the server; no pointer or struct ever crosses the
 * wire. Inline document, not a path.
 * API selection, argument validation, and HAL calls belong to the server.
 * Response payload: status byte, followed by NUL-terminated result text.
 * Exchange: request header, ACK, request payload, response header, ACK,
 * response payload. One request per connection. No pointers/structs on wire.
 */
#endif
