#ifndef ETHSWHAL_APIS_H
#define ETHSWHAL_APIS_H
#include <stddef.h>
#include "vl_common.h"
#include "vl_generic.h"

/* PUBLIC_INTERFACE */
/** Parse a mutable NUL-terminated command and call its typed EthSW HAL adapter
 * (ccsp_hal_ethsw.h). Caller owns buffers and serializes calls. Argument
 * pointers are valid only during the synchronous call. Capacity includes the
 * response terminator. Returns a VL status (VL_OK, VL_INVALID_REQUEST or
 * VL_HAL_ERROR from vl_generic.h); response text includes the HAL return code.
 */
int ethswhal_dispatch_command(char *command, char *response, size_t capacity);

/* Dispatch API-name/newline/JSON text; no paths or pointers cross IPC. */
int ethswhal_dispatch_json(char *request, char *response, size_t capacity);
#endif
