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

