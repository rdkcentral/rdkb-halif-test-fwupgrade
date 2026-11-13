#define _POSIX_C_SOURCE 200809L
#include "ethswhal_apis.h"
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Included after the system headers: it defines short names such as UP, DOWN,
 * INT and CHAR as macros.
 */
#include "ccsp_hal_ethsw.h"

/* Typed HAL objects stay here; only text crosses IPC. */
static int invalid_request(char *response, size_t capacity,
                                            const char *message)
{
    snprintf(response, capacity, "status: -1\nerror: %s\n", message);
    return VL_INVALID_REQUEST;
}

/* Command adapters own every output object; only bounded text crosses IPC. */
#define BAD_ARGS (-3)
#define OUT_NONE (-1)
#define ARG_COUNT 16
#define COUNT(array) (sizeof(array) / sizeof((array)[0]))
/* A NULL value is a JSON null: adapters pass it to the HAL as a NULL pointer. */
struct api_args { const char *key[ARG_COUNT]; char *value[ARG_COUNT]; size_t count; };

static int has_key(const struct api_args *a, const char *key)
{
    size_t i;
    for (i = 0; i < a->count; ++i)
        if (!strcmp(a->key[i], key)) return 1;
    return 0;
}

static char *arg(const struct api_args *a, const char *key)
{
    size_t i;
    for (i = 0; i < a->count; ++i)
        if (!strcmp(a->key[i], key)) return a->value[i];
    return NULL;
}

static int number(const struct api_args *a, const char *key, long min, long max, long *out)
{
    const char *p = arg(a, key), *digit = p;
    char *end;
    long value;
    if (!p || !*p) return 0;
    if (*digit == '-' && min < 0) ++digit;
    if (!*digit) return 0;
    for (; *digit; ++digit) if (*digit < '0' || *digit > '9') return 0;
    errno = 0;
    value = strtol(p, &end, 10);
    if (errno || *end || value < min || value > max) return 0;
    *out = value;
    return 1;
}
#define NUM(key, min, max, var) long var; if (!number(a, key, min, max, &var)) return BAD_ARGS

/* Incremental formatter rejects overflow rather than returning truncated data. */
static int append_text(char *out, size_t cap, size_t *used, const char *format, ...)
{
    int n;
    va_list ap;
    if (*used >= cap) return 0;
    va_start(ap, format);
    n = vsnprintf(out + *used, cap - *used, format, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap - *used) return 0;
    *used += (size_t)n;
    return 1;
}

/*
 * Optional --output-pointer test token. It selects which real local pointer
 * is passed to the HAL as NULL; no address ever crosses IPC.
 *   BUFFER (default) / DEFAULT  every output is a valid object.
 *   NULL                        the API's first listed output is NULL.
 *   NULL_<NAME>                 the named output is NULL (names per adapter).
 * Returns OUT_NONE, the index of the output to pass as NULL, or BAD_ARGS.
 */
static int select_null_output(const struct api_args *a,
                              const char *const *names, size_t count)
{
    const char *mode = arg(a, "output-pointer");
    size_t i;
    if (mode == NULL || !strcmp(mode, "BUFFER") || !strcmp(mode, "DEFAULT"))
        return OUT_NONE;
    if (!strcmp(mode, "NULL"))
        return 0;
    for (i = 0; i < count; ++i)
        if (!strcmp(mode, names[i])) return (int)i;
    return BAD_ARGS;
}

/* ------------------------------------------------------------------ */
/* Text helpers                                                        */
/* ------------------------------------------------------------------ */

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Parse "aa:bb:cc:dd:ee:ff" into 6 octets; reject anything else. */
static int parse_mac(const char *text, unsigned char mac[6])
{
    size_t i;
    if (!text || strlen(text) != 17) return 0;
    for (i = 0; i < 6; ++i) {
        int high = hex_value(text[i * 3]);
        int low = hex_value(text[i * 3 + 1]);
        if (high < 0 || low < 0) return 0;
        if (i < 5 && text[i * 3 + 2] != ':') return 0;
        mac[i] = (unsigned char)((high << 4) | low);
    }
    return 1;
}

static void format_mac(const UCHAR mac[6], char out[18])
{
    snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static int format_device(char *out, size_t cap, size_t *used,
                         const char *label, const eth_device_t *dev)
{
    char mac[18];
    format_mac(dev->eth_devMacAddress, mac);
    return append_text(out, cap, used,
                       "%s: mac=%s port=%d vlan=%d tx_rate=%d rx_rate=%d active=%d\n",
                       label, mac, (int)dev->eth_port, (int)dev->eth_vlanid,
                       (int)dev->eth_devTxRate, (int)dev->eth_devRxRate,
                       (int)dev->eth_Active);
}

/* ------------------------------------------------------------------ */
/* Server-local callbacks                                              */
/* ------------------------------------------------------------------ */

/* Callbacks cannot cross IPC, so registration installs these recorders and
 * the harness APIs below report what they saw. State resets with the process.
 */
static struct {
    unsigned long associated;
    unsigned long disassociated;
    unsigned long wan_up;
    unsigned long wan_down;
    eth_device_t last_device;
    int has_last_device;
} events;

static INT record_association(eth_device_t *dev)
{
    if (dev == NULL) return RETURN_ERR;
    /* The HAL owns *dev only during the call, so copy what is needed. */
    events.last_device = *dev;
    events.has_last_device = 1;
    if (dev->eth_Active) ++events.associated;
    else ++events.disassociated;
    return RETURN_OK;
}

static void record_wan_up(void) { ++events.wan_up; }
static void record_wan_down(void) { ++events.wan_down; }

/* ------------------------------------------------------------------ */
/* Lifecycle, port status and configuration                            */
/* ------------------------------------------------------------------ */

/* COMMAND CcspHalEthSwInit| */
static INT call_CcspHalEthSwInit(const struct api_args *a, char *out, size_t cap)
{
    (void)a; (void)out; (void)cap;
    return CcspHalEthSwInit();
}

/* COMMAND CcspHalEthSwGetPortStatus|PortId ?output-pointer */
static INT call_CcspHalEthSwGetPortStatus(const struct api_args *a, char *out, size_t cap)
{
    static const char *const nulls[] = {"NULL_LINK_RATE", "NULL_DUPLEX_MODE", "NULL_STATUS"};
    CCSP_HAL_ETHSW_LINK_RATE rate = CCSP_HAL_ETHSW_LINK_NULL;
    CCSP_HAL_ETHSW_DUPLEX_MODE duplex = CCSP_HAL_ETHSW_DUPLEX_Auto;
    CCSP_HAL_ETHSW_LINK_STATUS status = CCSP_HAL_ETHSW_LINK_Up;
    int null_out = select_null_output(a, nulls, COUNT(nulls));
    INT rc;
    NUM("PortId", INT_MIN, INT_MAX, port);
    if (null_out == BAD_ARGS) return BAD_ARGS;
    rc = CcspHalEthSwGetPortStatus((CCSP_HAL_ETHSW_PORT)port,
                                   null_out == 0 ? NULL : &rate,
                                   null_out == 1 ? NULL : &duplex,
                                   null_out == 2 ? NULL : &status);
    if (rc == RETURN_OK)
        snprintf(out, cap, "link_rate: %d\nduplex_mode: %d\nlink_status: %d\n",
                 (int)rate, (int)duplex, (int)status);
    return rc;
}

/* COMMAND CcspHalEthSwGetPortCfg|PortId ?output-pointer */
static INT call_CcspHalEthSwGetPortCfg(const struct api_args *a, char *out, size_t cap)
{
    static const char *const nulls[] = {"NULL_LINK_RATE", "NULL_DUPLEX_MODE"};
    CCSP_HAL_ETHSW_LINK_RATE rate = CCSP_HAL_ETHSW_LINK_NULL;
    CCSP_HAL_ETHSW_DUPLEX_MODE duplex = CCSP_HAL_ETHSW_DUPLEX_Auto;
    int null_out = select_null_output(a, nulls, COUNT(nulls));
    INT rc;
    NUM("PortId", INT_MIN, INT_MAX, port);
    if (null_out == BAD_ARGS) return BAD_ARGS;
    rc = CcspHalEthSwGetPortCfg((CCSP_HAL_ETHSW_PORT)port,
                                null_out == 0 ? NULL : &rate,
                                null_out == 1 ? NULL : &duplex);
    if (rc == RETURN_OK)
        snprintf(out, cap, "link_rate: %d\nduplex_mode: %d\n", (int)rate, (int)duplex);
    return rc;
}

/* COMMAND CcspHalEthSwSetPortCfg|PortId LinkRate DuplexMode */
static INT call_CcspHalEthSwSetPortCfg(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    NUM("PortId", INT_MIN, INT_MAX, port);
    NUM("LinkRate", INT_MIN, INT_MAX, rate);
    NUM("DuplexMode", INT_MIN, INT_MAX, duplex);
    /* Out-of-range enum values are passed through for the HAL to reject. */
    return CcspHalEthSwSetPortCfg((CCSP_HAL_ETHSW_PORT)port,
                                  (CCSP_HAL_ETHSW_LINK_RATE)rate,
                                  (CCSP_HAL_ETHSW_DUPLEX_MODE)duplex);
}

/* COMMAND CcspHalEthSwGetPortAdminStatus|PortId ?output-pointer */
static INT call_CcspHalEthSwGetPortAdminStatus(const struct api_args *a, char *out, size_t cap)
{
    static const char *const nulls[] = {"NULL_ADMIN_STATUS"};
    CCSP_HAL_ETHSW_ADMIN_STATUS admin = CCSP_HAL_ETHSW_AdminUp;
    int null_out = select_null_output(a, nulls, COUNT(nulls));
    INT rc;
    NUM("PortId", INT_MIN, INT_MAX, port);
    if (null_out == BAD_ARGS) return BAD_ARGS;
    rc = CcspHalEthSwGetPortAdminStatus((CCSP_HAL_ETHSW_PORT)port,
                                        null_out == 0 ? NULL : &admin);
    if (rc == RETURN_OK)
        snprintf(out, cap, "admin_status: %d\n", (int)admin);
    return rc;
}

/* COMMAND CcspHalEthSwSetPortAdminStatus|PortId AdminStatus */
static INT call_CcspHalEthSwSetPortAdminStatus(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    NUM("PortId", INT_MIN, INT_MAX, port);
    NUM("AdminStatus", INT_MIN, INT_MAX, admin);
    return CcspHalEthSwSetPortAdminStatus((CCSP_HAL_ETHSW_PORT)port,
                                          (CCSP_HAL_ETHSW_ADMIN_STATUS)admin);
}

/* COMMAND CcspHalEthSwSetAgingSpeed|PortId AgingSpeed */
static INT call_CcspHalEthSwSetAgingSpeed(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    NUM("PortId", INT_MIN, INT_MAX, port);
    NUM("AgingSpeed", INT_MIN, INT_MAX, speed);
    return CcspHalEthSwSetAgingSpeed((CCSP_HAL_ETHSW_PORT)port, (INT)speed);
}

/* ------------------------------------------------------------------ */
/* MAC lookup and associated devices                                   */
/* ------------------------------------------------------------------ */

/* COMMAND CcspHalEthSwLocatePortByMacAddress|mac ?output-pointer
 * mac is "aa:bb:cc:dd:ee:ff"; a JSON null or NULL_MAC passes a NULL pointer.
 */
static INT call_CcspHalEthSwLocatePortByMacAddress(const struct api_args *a, char *out, size_t cap)
{
    static const char *const nulls[] = {"NULL_PORT", "NULL_MAC"};
    unsigned char mac[6] = {0};
    unsigned char *mac_in = mac;
    const char *text = arg(a, "mac");
    INT port = 0;
    int null_out = select_null_output(a, nulls, COUNT(nulls));
    INT rc;
    if (null_out == BAD_ARGS) return BAD_ARGS;
    if (null_out == 1 || text == NULL)
        mac_in = NULL;
    else if (!parse_mac(text, mac))
        return BAD_ARGS;
    rc = CcspHalEthSwLocatePortByMacAddress(mac_in, null_out == 0 ? NULL : &port);
    if (rc == RETURN_OK)
        snprintf(out, cap, "port: %d\n", (int)port);
    return rc;
}

/* COMMAND CcspHalExtSw_getAssociatedDevice|?output-pointer
 * The HAL allocates the array; this adapter releases it with free().
 */
static INT call_CcspHalExtSw_getAssociatedDevice(const struct api_args *a, char *out, size_t cap)
{
    static const char *const nulls[] = {"NULL_SIZE", "NULL_DEVICES"};
    ULONG count = 0;
    eth_device_t *devices = NULL;
    int null_out = select_null_output(a, nulls, COUNT(nulls));
    size_t used = 0, i;
    INT rc;
    if (null_out == BAD_ARGS) return BAD_ARGS;
    rc = CcspHalExtSw_getAssociatedDevice(null_out == 0 ? NULL : &count,
                                          null_out == 1 ? NULL : &devices);
    if (rc != RETURN_OK) return rc; /* no array was handed over */
    if (!append_text(out, cap, &used, "count: %lu\n", (unsigned long)count))
        rc = RETURN_ERR;
    for (i = 0; rc == RETURN_OK && devices != NULL && i < count; ++i) {
        char label[32];
        snprintf(label, sizeof(label), "device[%lu]", (unsigned long)i);
        if (!format_device(out, cap, &used, label, &devices[i]))
            rc = RETURN_ERR;
    }
    free(devices);
    return rc;
}

/* COMMAND CcspHalExtSw_ethAssociatedDevice_callback_register|?callback
 * callback is ENABLE (default, installs the recorder) or NULL (no callback).
 */
static INT call_CcspHalExtSw_ethAssociatedDevice_callback_register(const struct api_args *a,
                                                                   char *out, size_t cap)
{
    const char *mode = arg(a, "callback");
    if (mode == NULL || !strcmp(mode, "ENABLE"))
        CcspHalExtSw_ethAssociatedDevice_callback_register(record_association);
    else if (!strcmp(mode, "NULL"))
        CcspHalExtSw_ethAssociatedDevice_callback_register(NULL);
    else
        return BAD_ARGS;
    snprintf(out, cap, "callback: %s\n", mode ? mode : "ENABLE");
    return RETURN_OK;
}

/* ------------------------------------------------------------------ */
/* Ethernet WAN                                                        */
/* ------------------------------------------------------------------ */

#if defined(FEATURE_RDKB_WAN_MANAGER) && defined(FEATURE_RDKB_AUTO_PORT_SWITCH)
/* COMMAND CcspHalExtSw_ethPortConfigure|ifname WanMode
 * Only JSON can pass a NULL ifname ({"ifname": null}).
 */
static INT call_CcspHalExtSw_ethPortConfigure(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    NUM("WanMode", 0, UCHAR_MAX, mode);
    return CcspHalExtSw_ethPortConfigure(arg(a, "ifname"), (BOOLEAN)mode);
}
#endif

/* COMMAND CcspHalExtSw_getEthWanEnable|?output-pointer */
static INT call_CcspHalExtSw_getEthWanEnable(const struct api_args *a, char *out, size_t cap)
{
    static const char *const nulls[] = {"NULL_FLAG"};
    BOOLEAN flag = FALSE;
    int null_out = select_null_output(a, nulls, COUNT(nulls));
    INT rc;
    if (null_out == BAD_ARGS) return BAD_ARGS;
    rc = CcspHalExtSw_getEthWanEnable(null_out == 0 ? NULL : &flag);
    if (rc == RETURN_OK)
        snprintf(out, cap, "enable: %d\n", (int)flag);
    return rc;
}

/* COMMAND CcspHalExtSw_setEthWanEnable|Flag */
static INT call_CcspHalExtSw_setEthWanEnable(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    /* BOOLEAN is an unsigned char: values other than 0/1 reach the HAL. */
    NUM("Flag", 0, UCHAR_MAX, flag);
    return CcspHalExtSw_setEthWanEnable((BOOLEAN)flag);
}

#ifdef FEATURE_RDKB_AUTO_PORT_SWITCH
/* The HAL has no error channel: the result is TRUE for WAN, FALSE for LAN. */
/* COMMAND CcspHalExtSw_getCurrentWanHWConf| */
static INT call_CcspHalExtSw_getCurrentWanHWConf(const struct api_args *a, char *out, size_t cap)
{
    (void)a;
    snprintf(out, cap, "wan_hw_conf: %d\n", (int)CcspHalExtSw_getCurrentWanHWConf());
    return RETURN_OK;
}
#endif

/* COMMAND CcspHalExtSw_getEthWanPort|?output-pointer */
static INT call_CcspHalExtSw_getEthWanPort(const struct api_args *a, char *out, size_t cap)
{
    static const char *const nulls[] = {"NULL_PORT"};
    UINT port = 0;
    int null_out = select_null_output(a, nulls, COUNT(nulls));
    INT rc;
    if (null_out == BAD_ARGS) return BAD_ARGS;
    rc = CcspHalExtSw_getEthWanPort(null_out == 0 ? NULL : &port);
    if (rc == RETURN_OK)
        snprintf(out, cap, "port: %u\n", port);
    return rc;
}

/* COMMAND CcspHalExtSw_setEthWanPort|Port */
static INT call_CcspHalExtSw_setEthWanPort(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    NUM("Port", 0, INT_MAX, port);
    return CcspHalExtSw_setEthWanPort((UINT)port);
}

/* COMMAND GWP_RegisterEthWan_Callback|?callback
 * callback is ENABLE (default, both recorders), UP_ONLY, DOWN_ONLY, or NULL
 * (a NULL appCallBack pointer).
 */
static INT call_GWP_RegisterEthWan_Callback(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "callback");
    appCallBack callbacks = {record_wan_up, record_wan_down};
    if (mode == NULL || !strcmp(mode, "ENABLE")) {
        GWP_RegisterEthWan_Callback(&callbacks);
    } else if (!strcmp(mode, "UP_ONLY")) {
        callbacks.pGWP_act_EthWanLinkDown = NULL;
        GWP_RegisterEthWan_Callback(&callbacks);
    } else if (!strcmp(mode, "DOWN_ONLY")) {
        callbacks.pGWP_act_EthWanLinkUP = NULL;
        GWP_RegisterEthWan_Callback(&callbacks);
    } else if (!strcmp(mode, "NULL")) {
        GWP_RegisterEthWan_Callback(NULL);
    } else {
        return BAD_ARGS;
    }
    snprintf(out, cap, "callback: %s\n", mode ? mode : "ENABLE");
    return RETURN_OK;
}

/* The HAL returns 1 (up), 0 (down) or a negative value (unknown), not RETURN_*.
 * A negative value is reported verbatim and is not an adapter failure.
 */
/* COMMAND GWP_GetEthWanLinkStatus| */
static INT call_GWP_GetEthWanLinkStatus(const struct api_args *a, char *out, size_t cap)
{
    (void)a;
    snprintf(out, cap, "link_status: %d\n", (int)GWP_GetEthWanLinkStatus());
    return RETURN_OK;
}

/* Largest buffer the adapter will hand to the HAL; the header documents
 * maxSize values of 11 to 262, and larger ones are still passed for the HAL
 * to reject.
 */
#define INTERFACE_BUFFER_SIZE 1024

/* COMMAND GWP_GetEthWanInterfaceName|?maxSize ?output-pointer
 * maxSize defaults to 64 (the header's minimum buffer) and must fit the
 * adapter buffer.
 */
static INT call_GWP_GetEthWanInterfaceName(const struct api_args *a, char *out, size_t cap)
{
    static const char *const nulls[] = {"NULL_BUFFER"};
    unsigned char name[INTERFACE_BUFFER_SIZE] = {0};
    long max_size = 64;
    int null_out = select_null_output(a, nulls, COUNT(nulls));
    INT rc;
    if (null_out == BAD_ARGS) return BAD_ARGS;
    if (arg(a, "maxSize") != NULL &&
        !number(a, "maxSize", 0, INTERFACE_BUFFER_SIZE, &max_size))
        return BAD_ARGS;
    rc = GWP_GetEthWanInterfaceName(null_out == 0 ? NULL : name, (ULONG)max_size);
    name[sizeof(name) - 1] = '\0';
    if (rc == RETURN_OK)
        snprintf(out, cap, "interface: %s\n", (char *)name);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Statistics                                                          */
/* ------------------------------------------------------------------ */

/* COMMAND CcspHalEthSwGetEthPortStats|PortId ?output-pointer */
static INT call_CcspHalEthSwGetEthPortStats(const struct api_args *a, char *out, size_t cap)
{
    static const char *const nulls[] = {"NULL_STATS"};
    CCSP_HAL_ETH_STATS stats;
    int null_out = select_null_output(a, nulls, COUNT(nulls));
    INT rc;
    NUM("PortId", INT_MIN, INT_MAX, port);
    if (null_out == BAD_ARGS) return BAD_ARGS;
    memset(&stats, 0, sizeof(stats));
    rc = CcspHalEthSwGetEthPortStats((CCSP_HAL_ETHSW_PORT)port,
                                     null_out == 0 ? NULL : &stats);
    if (rc == RETURN_OK)
        snprintf(out, cap,
                 "BytesSent: %llu\nBytesReceived: %llu\n"
                 "PacketsSent: %lu\nPacketsReceived: %lu\n"
                 "ErrorsSent: %lu\nErrorsReceived: %lu\n"
                 "UnicastPacketsSent: %lu\nUnicastPacketsReceived: %lu\n"
                 "DiscardPacketsSent: %lu\nDiscardPacketsReceived: %lu\n"
                 "MulticastPacketsSent: %lu\nMulticastPacketsReceived: %lu\n"
                 "BroadcastPacketsSent: %lu\nBroadcastPacketsReceived: %lu\n"
                 "UnknownProtoPacketsReceived: %lu\n",
                 (unsigned long long)stats.BytesSent,
                 (unsigned long long)stats.BytesReceived,
                 (unsigned long)stats.PacketsSent,
                 (unsigned long)stats.PacketsReceived,
                 (unsigned long)stats.ErrorsSent,
                 (unsigned long)stats.ErrorsReceived,
                 (unsigned long)stats.UnicastPacketsSent,
                 (unsigned long)stats.UnicastPacketsReceived,
                 (unsigned long)stats.DiscardPacketsSent,
                 (unsigned long)stats.DiscardPacketsReceived,
                 (unsigned long)stats.MulticastPacketsSent,
                 (unsigned long)stats.MulticastPacketsReceived,
                 (unsigned long)stats.BroadcastPacketsSent,
                 (unsigned long)stats.BroadcastPacketsReceived,
                 (unsigned long)stats.UnknownProtoPacketsReceived);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Harness APIs: report events seen by the server-local callbacks      */
/* ------------------------------------------------------------------ */

/* COMMAND ethsw_vl_get_callback_events| */
static INT call_ethsw_vl_get_callback_events(const struct api_args *a, char *out, size_t cap)
{
    size_t used = 0;
    (void)a;
    if (!append_text(out, cap, &used,
                     "associated: %lu\ndisassociated: %lu\nwan_up: %lu\nwan_down: %lu\n",
                     events.associated, events.disassociated,
                     events.wan_up, events.wan_down))
        return RETURN_ERR;
    if (!events.has_last_device)
        return append_text(out, cap, &used, "last_device: none\n") ? RETURN_OK : RETURN_ERR;
    return format_device(out, cap, &used, "last_device", &events.last_device)
               ? RETURN_OK : RETURN_ERR;
}

/* COMMAND ethsw_vl_clear_callback_events| */
static INT call_ethsw_vl_clear_callback_events(const struct api_args *a, char *out, size_t cap)
{
    (void)a; (void)out; (void)cap;
    memset(&events, 0, sizeof(events));
    return RETURN_OK;
}

/* ------------------------------------------------------------------ */
/* API table                                                           */
/* ------------------------------------------------------------------ */

/* Register adapters with required keys and ?optional keys. */
struct api_entry {
    const char *name;
    const char *options;
    INT (*call)(const struct api_args *, char *, size_t);
};
static const struct api_entry api_entries[] = {
    {"CcspHalEthSwInit", "", call_CcspHalEthSwInit},
    {"CcspHalEthSwGetPortStatus", "PortId ?output-pointer", call_CcspHalEthSwGetPortStatus},
    {"CcspHalEthSwGetPortCfg", "PortId ?output-pointer", call_CcspHalEthSwGetPortCfg},
    {"CcspHalEthSwSetPortCfg", "PortId LinkRate DuplexMode", call_CcspHalEthSwSetPortCfg},
    {"CcspHalEthSwGetPortAdminStatus", "PortId ?output-pointer", call_CcspHalEthSwGetPortAdminStatus},
    {"CcspHalEthSwSetPortAdminStatus", "PortId AdminStatus", call_CcspHalEthSwSetPortAdminStatus},
    {"CcspHalEthSwSetAgingSpeed", "PortId AgingSpeed", call_CcspHalEthSwSetAgingSpeed},
    {"CcspHalEthSwLocatePortByMacAddress", "mac ?output-pointer", call_CcspHalEthSwLocatePortByMacAddress},
    {"CcspHalExtSw_getAssociatedDevice", "?output-pointer", call_CcspHalExtSw_getAssociatedDevice},
    {"CcspHalExtSw_ethAssociatedDevice_callback_register", "?callback",
     call_CcspHalExtSw_ethAssociatedDevice_callback_register},
#if defined(FEATURE_RDKB_WAN_MANAGER) && defined(FEATURE_RDKB_AUTO_PORT_SWITCH)
    {"CcspHalExtSw_ethPortConfigure", "ifname WanMode", call_CcspHalExtSw_ethPortConfigure},
#endif
    {"CcspHalExtSw_getEthWanEnable", "?output-pointer", call_CcspHalExtSw_getEthWanEnable},
    {"CcspHalExtSw_setEthWanEnable", "Flag", call_CcspHalExtSw_setEthWanEnable},
#ifdef FEATURE_RDKB_AUTO_PORT_SWITCH
    {"CcspHalExtSw_getCurrentWanHWConf", "", call_CcspHalExtSw_getCurrentWanHWConf},
#endif
    {"CcspHalExtSw_getEthWanPort", "?output-pointer", call_CcspHalExtSw_getEthWanPort},
    {"CcspHalExtSw_setEthWanPort", "Port", call_CcspHalExtSw_setEthWanPort},
    {"CcspHalEthSwGetEthPortStats", "PortId ?output-pointer", call_CcspHalEthSwGetEthPortStats},
    {"GWP_RegisterEthWan_Callback", "?callback", call_GWP_RegisterEthWan_Callback},
    {"GWP_GetEthWanLinkStatus", "", call_GWP_GetEthWanLinkStatus},
    {"GWP_GetEthWanInterfaceName", "?maxSize ?output-pointer", call_GWP_GetEthWanInterfaceName},
    {"ethsw_vl_get_callback_events", "", call_ethsw_vl_get_callback_events},
    {"ethsw_vl_clear_callback_events", "", call_ethsw_vl_clear_callback_events},
};

/* Validate required and optional fields independently of their argv order. */
static int valid_options(const struct api_entry *entry, const struct api_args *a)
{
    const char *cursor = entry->options;
    size_t matched = 0;
    while (*cursor) {
        const char *key;
        size_t length, i;
        int optional = *cursor == '?';
        int found = 0;
        if (optional) ++cursor;
        key = cursor;
        while (*cursor && *cursor != ' ') ++cursor;
        length = (size_t)(cursor - key);
        for (i = 0; i < a->count; ++i) {
            if (strlen(a->key[i]) == length && !strncmp(a->key[i], key, length)) {
                found = 1; ++matched; break;
            }
        }
        if (!optional && !found) return 0;
        while (*cursor == ' ') ++cursor;
    }
    return matched == a->count;
}

static int format_api_result(INT rc, const char *result,
                                              char *response, size_t cap)
{
    size_t used = 0;
    if (rc == BAD_ARGS) return invalid_request(response, cap, "invalid argument value");
    /* Non-zero HAL codes (such as -1) are reported verbatim in "status:". */
    if (!append_text(response, cap, &used, "status: %d\n", rc) ||
        (rc == RETURN_OK && !append_text(response, cap, &used, "%s", result)) ||
        (rc != RETURN_OK && !append_text(response, cap, &used, "error: HAL operation failed\n"))) {
        snprintf(response, cap, "status: -1\nerror: response too large\n");
        return VL_HAL_ERROR;
    }
    return rc == RETURN_OK ? VL_OK : VL_HAL_ERROR;
}

/* The frame owns this mutable command buffer for the duration of dispatch. */
int ethswhal_dispatch_command(char *command, char *response, size_t cap)
{
    struct api_args args = {0};
    const char *name = NULL;
    char *save = NULL, *token;
    const struct api_entry *entry = NULL;
    size_t i, used = 0;
    INT rc;
    char result[VL_MAX_PAYLOAD] = {0};
    if (!response || cap == 0) return VL_INVALID_REQUEST;
    if (!command) return invalid_request(response, cap, "missing command");
    token = strtok_r(command, " \t\r\n", &save);
    while (token) {
        char *key;
        char *value;
        char *equals;

        if (strcmp(token, "--list-apis") == 0) {
            if (name)
                return invalid_request(response, cap, "multiple commands");
            name = "list";
        } else {
            /* All HAL APIs use the same --ethsw-api selector. */
            if (strncmp(token, "--", 2) != 0 || !token[2])
                return invalid_request(response, cap,
                                       "expected --key=value or --key value");

            key = token + 2;
            equals = strchr(key, '=');
            if (equals) {
                *equals = 0;
                value = equals + 1;
            } else {
                value = strtok_r(NULL, " \t\r\n", &save);
            }

            if (!value || !*value || strncmp(value, "--", 2) == 0)
                return invalid_request(response, cap, "missing option value");

            if (strcmp(key, "ethsw-api") == 0) {
                if (name)
                    return invalid_request(response, cap, "multiple commands");
                name = value;
            } else {
                if (!*key || args.count == ARG_COUNT || has_key(&args, key))
                    return invalid_request(response, cap,
                                           "duplicate or excessive options");
                args.key[args.count] = key;
                args.value[args.count] = value;
                ++args.count;
            }
        }
        token = strtok_r(NULL, " \t\r\n", &save);
    }
    if (!name)
        return invalid_request(response, cap, "missing API selector");
    if (!strcmp(name, "list")) {
        if (args.count) return invalid_request(response, cap, "list takes no arguments");
        if (!append_text(response, cap, &used, "status: 0\n")) return VL_HAL_ERROR;
        for (i = 0; i < COUNT(api_entries); ++i)
            if (!append_text(response, cap, &used, "api: %s options: %s\n",
                             api_entries[i].name, api_entries[i].options)) {
                snprintf(response, cap, "status: -1\nerror: response too large\n");
                return VL_HAL_ERROR;
            }
        return VL_OK;
    }

    for (i = 0; i < COUNT(api_entries); ++i)
        if (!strcmp(name, api_entries[i].name)) { entry = &api_entries[i]; break; }
    if (!entry) return invalid_request(response, cap, "unknown or disabled API; use --list-apis");
    if (!valid_options(entry, &args))
        return invalid_request(response, cap,
            "missing required or unknown API options; use --list-apis");
    rc = entry->call(&args, result, sizeof(result));
    return format_api_result(rc, result, response, cap);
}

/*
 * Deliberately narrow JSON schema: API-specific literal keys with integer,
 * string, or null (string fields only) values. No nested objects, booleans,
 * arrays, or extra fields, and strings carry no escapes or control characters.
 * Parse the entire document before invoking any HAL operation.
 */
struct json_field { const char *key; char type; /* 'i' integer, 's' string|null */ };

static char *json_space(char *cursor)
{
    return cursor + strspn(cursor, " \t\r\n");
}

static int parse_json(char *json, struct api_args *args,
                      const struct json_field *fields, size_t count)
{
    char *cursor = json_space(json);
    size_t i;
    if (!count || count > ARG_COUNT || *cursor != '{') return 0;
    ++cursor;
    for (i = 0; i < count; ++i) {
        char *key, *end, *value = NULL, *terminate = NULL;
        size_t k;
        int separator;
        cursor = json_space(cursor);
        if (*cursor != '"') return 0;
        key = ++cursor;
        end = strchr(cursor, '"');
        if (!end) return 0;
        *end = 0;
        cursor = json_space(end + 1);
        for (k = 0; k < count; ++k)
            if (!strcmp(key, fields[k].key)) break;
        if (k == count || has_key(args, fields[k].key)) return 0;
        if (*cursor != ':') return 0;
        cursor = json_space(cursor + 1);
        if (fields[k].type == 's') {
            if (!strncmp(cursor, "null", 4)) {
                cursor += 4; /* value stays NULL: a real NULL pointer for the HAL */
            } else {
                char *p;
                if (*cursor != '"') return 0;
                value = ++cursor;
                end = strchr(cursor, '"');
                if (!end) return 0;
                for (p = value; p < end; ++p)
                    if (*p == '\\' || (unsigned char)*p < 0x20) return 0;
                *end = 0;
                cursor = end + 1;
            }
        } else {
            value = cursor;
            if (*cursor == '-') ++cursor;
            if (*cursor < '0' || *cursor > '9') return 0;
            /* JSON forbids leading zeroes; values such as 01 fail at delimiter. */
            if (*cursor == '0') ++cursor;
            else while (*cursor >= '0' && *cursor <= '9') ++cursor;
            terminate = cursor;
        }
        cursor = json_space(cursor);
        separator = *cursor;
        if (separator != (i + 1 == count ? '}' : ',')) return 0;
        if (terminate) {
            /* Normalize JSON negative zero before existing numeric validation. */
            if (value[0] == '-' && value[1] == '0' && terminate - value == 2) ++value;
            *terminate = 0;
        }
        args->key[args->count] = fields[k].key;
        args->value[args->count++] = value;
        ++cursor;
    }
    return *json_space(cursor) == 0;
}

struct json_api {
    const char *name;
    const struct json_field *fields;
    size_t count;
    INT (*call)(const struct api_args *, char *, size_t);
    const char *error;
};
static const struct json_field port_cfg_fields[] = {{"PortId", 'i'}, {"LinkRate", 'i'}, {"DuplexMode", 'i'}};
static const struct json_field port_admin_fields[] = {{"PortId", 'i'}, {"AdminStatus", 'i'}};
static const struct json_field aging_fields[] = {{"PortId", 'i'}, {"AgingSpeed", 'i'}};
static const struct json_field locate_fields[] = {{"mac", 's'}};
static const struct json_field wan_enable_fields[] = {{"Flag", 'i'}};
static const struct json_field wan_port_fields[] = {{"Port", 'i'}};
#if defined(FEATURE_RDKB_WAN_MANAGER) && defined(FEATURE_RDKB_AUTO_PORT_SWITCH)
static const struct json_field port_configure_fields[] = {{"ifname", 's'}, {"WanMode", 'i'}};
#endif
static const struct json_api json_apis[] = {
    {"CcspHalEthSwSetPortCfg", port_cfg_fields, COUNT(port_cfg_fields),
     call_CcspHalEthSwSetPortCfg,
     "expected JSON integer fields PortId, LinkRate, DuplexMode"},
    {"CcspHalEthSwSetPortAdminStatus", port_admin_fields, COUNT(port_admin_fields),
     call_CcspHalEthSwSetPortAdminStatus,
     "expected JSON integer fields PortId, AdminStatus"},
    {"CcspHalEthSwSetAgingSpeed", aging_fields, COUNT(aging_fields),
     call_CcspHalEthSwSetAgingSpeed,
     "expected JSON integer fields PortId, AgingSpeed"},
    {"CcspHalEthSwLocatePortByMacAddress", locate_fields, COUNT(locate_fields),
     call_CcspHalEthSwLocatePortByMacAddress,
     "expected JSON string-or-null field mac"},
    {"CcspHalExtSw_setEthWanEnable", wan_enable_fields, COUNT(wan_enable_fields),
     call_CcspHalExtSw_setEthWanEnable,
     "expected JSON integer field Flag"},
    {"CcspHalExtSw_setEthWanPort", wan_port_fields, COUNT(wan_port_fields),
     call_CcspHalExtSw_setEthWanPort,
     "expected JSON integer field Port"},
#if defined(FEATURE_RDKB_WAN_MANAGER) && defined(FEATURE_RDKB_AUTO_PORT_SWITCH)
    {"CcspHalExtSw_ethPortConfigure", port_configure_fields, COUNT(port_configure_fields),
     call_CcspHalExtSw_ethPortConfigure,
     "expected JSON string-or-null field ifname and integer field WanMode"},
#endif
};

/* PUBLIC_INTERFACE */
int ethswhal_dispatch_json(char *request, char *response, size_t cap)
{
    struct api_args args = {0};
    char result[VL_MAX_PAYLOAD] = {0};
    char *json;
    size_t i;
    INT rc;
    if (!response || cap == 0) return VL_INVALID_REQUEST;
    if (!request || !(json = strchr(request, '\n')))
        return invalid_request(response, cap, "missing JSON API envelope");
    *json++ = 0;
    for (i = 0; i < COUNT(json_apis); ++i) {
        const struct json_api *api = &json_apis[i];
        if (strcmp(request, api->name)) continue;
        if (!parse_json(json, &args, api->fields, api->count))
            return invalid_request(response, cap, api->error);
        rc = api->call(&args, result, sizeof(result));
        return format_api_result(rc, result, response, cap);
    }
    return invalid_request(response, cap,
        "JSON is supported only for APIs that take input fields; use --list-apis");
}
