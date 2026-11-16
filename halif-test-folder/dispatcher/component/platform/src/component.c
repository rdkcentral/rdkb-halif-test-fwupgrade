#include "vl_generic.h"
#ifdef VL_SERVER
#include "platformhal_apis.h"
static int command(char *s, char *r, size_t n) { return platformhal_dispatch_command(s,r,n); }
static int json(char *s, char *r, size_t n) { return platformhal_dispatch_json(s,r,n); }
#define COMMAND command
#define JSON json
#else
#define COMMAND NULL
#define JSON NULL
#endif
const vl_component vl_component_config = {
    "platform", "--platform-api", "PLATFORM_VL_SOCKET_PATH", "/tmp/platform_vl-run.sock",
    UINT32_C(0x50564c31), "status: -1\nerror: invalid HAL request payload\n",
    COMMAND, JSON
};
