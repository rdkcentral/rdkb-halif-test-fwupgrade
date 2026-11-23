/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#ifndef FWUPGRADEHAL_APIS_H
#define FWUPGRADEHAL_APIS_H
#include <stddef.h>
#include "vl_common.h"

/* PUBLIC_INTERFACE */
/** Parse a mutable NUL-terminated command and call its typed HAL adapter.
 * Caller owns buffers and serializes calls. Argument pointers are valid only
 * during the synchronous call. Capacity includes the response terminator.
 * Returns VL status; response text includes the HAL return code.
 */
fwupgrade_vl_status_t fwupgradehal_dispatch_command(char *command, char *response,
                                                    size_t capacity);

/* PUBLIC_INTERFACE */
/** Dispatch API-name/newline/JSON text; no paths or pointers cross IPC. */
fwupgrade_vl_status_t fwupgradehal_dispatch_json(char *request, char *response,
                                                 size_t capacity);
#endif
