#include "vl_generic.h"
#ifdef VL_SERVER
#include "ethswhal_apis.h"
static int command(char *s, char *r, size_t n) { return ethswhal_dispatch_command(s,r,n); }
static int json(char *s, char *r, size_t n) { return ethswhal_dispatch_json(s,r,n); }
#define COMMAND command
#define JSON json
#else
#define COMMAND NULL
#define JSON NULL
#endif
const vl_component vl_component_config = {
    "ethsw", "--ethsw-api", "ETHSW_VL_SOCKET_PATH", "/tmp/ethsw_vl-run.sock",
    UINT32_C(0x45535731), "status: -1\nerror: invalid HAL request payload\n",
    COMMAND, JSON
};
