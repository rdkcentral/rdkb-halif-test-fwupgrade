#ifndef PLATFORMHAL_APIS_H
#define PLATFORMHAL_APIS_H
#include <stddef.h>
#include "vl_common.h"

/* PUBLIC_INTERFACE */
/** Parse a mutable NUL-terminated command and call its typed HAL adapter.
 * Caller owns buffers and serializes calls. Argument pointers are valid only
 * during the synchronous call. Capacity includes the response terminator.
 * Returns VL status; response text includes the HAL return code.
 */
platform_vl_status_t platformhal_dispatch_command(char *command, char *response,
                                                 size_t capacity);

/* Dispatch API-name/newline/JSON text; no paths or pointers cross IPC. */
platform_vl_status_t platformhal_dispatch_json(char *request, char *response,
                                              size_t capacity);
#endif
