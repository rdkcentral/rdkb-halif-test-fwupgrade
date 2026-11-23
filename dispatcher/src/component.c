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
#include "vl_generic.h"
#ifdef VL_SERVER
#include "fwupgradehal_apis.h"
static int command(char *s, char *r, size_t n) { return fwupgradehal_dispatch_command(s,r,n); }
static int json(char *s, char *r, size_t n) { return fwupgradehal_dispatch_json(s,r,n); }
#define COMMAND command
#define JSON json
#else
#define COMMAND NULL
#define JSON NULL
#endif
const vl_component vl_component_config = {
    "fwupgrade", "--fwupgrade-api", "FWUPGRADE_VL_SOCKET_PATH", "/tmp/fwupgrade_vl-run.sock",
    UINT32_C(0x464c5631), "status: -1\nerror: invalid HAL request payload\n",
    COMMAND, JSON
};