#include "platform_hal.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>

/* Process-local mock state, initially enabled. Calls must be serialized.
 * This library does not query hardware or start/stop a Telnet service.
 */
static BOOLEAN telnet_enabled = TRUE;

/* PUBLIC_INTERFACE */
/**
 * @brief Read the current mock Telnet enable status.
 * @param[out] pFlag Receives TRUE if enabled, FALSE if disabled.
 * @return RETURN_OK on success, or RETURN_ERR when pFlag is NULL.
 */
INT platform_hal_GetTelnetEnable(BOOLEAN *pFlag)
{
    if (pFlag == NULL)
        return RETURN_ERR;

    *pFlag = telnet_enabled;
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/**
 * @brief Update the process-local mock Telnet enable status.
 * @param[in] Flag TRUE to enable, FALSE to disable.
 * @return RETURN_OK on success; RETURN_ERR for other values, without
 *         changing the current state.
 */
INT platform_hal_SetTelnetEnable(BOOLEAN Flag)
{
    if (Flag != TRUE && Flag != FALSE)
        return RETURN_ERR;

    telnet_enabled = Flag;
    return RETURN_OK;
}


/* Process-local mock timeout, in seconds. Reset on server restart. */
static ULONG webui_timeout = 840UL;

INT platform_hal_GetWebUITimeout(ULONG *pValue)
{
    if (pValue == NULL)
        return RETURN_ERR;

    *pValue = webui_timeout;
    return RETURN_OK;
}

INT platform_hal_SetWebUITimeout(ULONG value)
{
    if (value != 0UL && (value < 30UL || value > 86400UL))
        return RETURN_ERR;

    webui_timeout = value;
    return RETURN_OK;
}

/* Mock platform configuration.
 * State is process-local, reset on restart, and calls must be serialized.
 * Device identity and memory values below are fixtures, not measurements.
 * For size-less getters callers must allocate the buffer required by the header.
 */
static BOOLEAN ssh_enabled = TRUE;
static const CHAR *snmp_mode = "rgLanIp";

/* Reject insufficient capacity without partially modifying the output. */
static INT mock_copy(CHAR *output, ULONG capacity, const CHAR *text)
{
    size_t length = strlen(text);
    if (output == NULL || capacity <= length)
        return RETURN_ERR;
    memcpy(output, text, length + 1);
    return RETURN_OK;
}

INT platform_hal_GetDeviceConfigStatus(CHAR *pValue)
{
    return mock_copy(pValue, 128UL, "Complete");
}

/* Empty-list mocks only: no DHCP configuration access or allocation. */
static INT mock_get_dhcp_options(
    dhcp_opt_list **req_opt_list,
    dhcp_opt_list **send_opt_list)
{
    if (req_opt_list != NULL)
        *req_opt_list = NULL;

    if (send_opt_list != NULL)
        *send_opt_list = NULL;

    if (req_opt_list == NULL || send_opt_list == NULL)
        return RETURN_ERR;

    return RETURN_OK;
}

INT platform_hal_GetDhcpv6_Options(
    dhcp_opt_list **req_opt_list,
    dhcp_opt_list **send_opt_list)
{
    return mock_get_dhcp_options(req_opt_list, send_opt_list);
}

INT platform_hal_GetDhcpv4_Options(
    dhcp_opt_list **req_opt_list,
    dhcp_opt_list **send_opt_list)
{
    return mock_get_dhcp_options(req_opt_list, send_opt_list);
}

INT platform_hal_GetSSHEnable(BOOLEAN *pFlag)
{
    if (pFlag == NULL)
        return RETURN_ERR;
    *pFlag = ssh_enabled;
    return RETURN_OK;
}

INT platform_hal_SetSSHEnable(BOOLEAN Flag)
{
    if (Flag != TRUE && Flag != FALSE)
        return RETURN_ERR;
    ssh_enabled = Flag;
    return RETURN_OK;
}

INT platform_hal_GetSNMPEnable(CHAR *pValue)
{
    return mock_copy(pValue, 32UL, snmp_mode);
}

INT platform_hal_SetSNMPEnable(CHAR *pValue)
{
    static const CHAR *const modes[] = {"rgWan", "rgDualIp", "rgLanIp"};
    size_t i;
    if (pValue == NULL)
        return RETURN_ERR;
    for (i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        if (strcmp(pValue, modes[i]) == 0) {
            snmp_mode = modes[i];
            return RETURN_OK;
        }
    }
    return RETURN_ERR;
}

/* Bounded mock table: at most 32 distinct user/interface pairs.
 * Unset pairs read as level 0. A full table rejects new pairs.
 * Both indexes accept the entire INT range specified in the header.
 */
#define MOCK_WEB_ACCESS_ENTRIES 32
static struct {
    BOOLEAN used;
    INT user;
    INT interface;
    ULONG level;
} web_access[MOCK_WEB_ACCESS_ENTRIES];

INT platform_hal_GetWebAccessLevel(INT userIndex, INT ifIndex, ULONG *pValue)
{
    size_t i;
    if (pValue == NULL)
        return RETURN_ERR;
    for (i = 0; i < MOCK_WEB_ACCESS_ENTRIES; ++i) {
        if (web_access[i].used && web_access[i].user == userIndex &&
            web_access[i].interface == ifIndex) {
            *pValue = web_access[i].level;
            return RETURN_OK;
        }
    }
    *pValue = 0UL;
    return RETURN_OK;
}

INT platform_hal_SetWebAccessLevel(INT userIndex, INT ifIndex, ULONG value)
{
    size_t i;
    size_t free_slot = MOCK_WEB_ACCESS_ENTRIES;
    if (value > 2147483647UL)
        return RETURN_ERR;
    for (i = 0; i < MOCK_WEB_ACCESS_ENTRIES; ++i) {
        if (web_access[i].used && web_access[i].user == userIndex &&
            web_access[i].interface == ifIndex) {
            web_access[i].level = value;
            return RETURN_OK;
        }
        if (!web_access[i].used && free_slot == MOCK_WEB_ACCESS_ENTRIES)
            free_slot = i;
    }
    if (free_slot == MOCK_WEB_ACCESS_ENTRIES)
        return RETURN_ERR;
    web_access[free_slot].user = userIndex;
    web_access[free_slot].interface = ifIndex;
    web_access[free_slot].level = value;
    web_access[free_slot].used = TRUE;
    return RETURN_OK;
}

/* Mock initialization is an idempotent no-op; no persistent DB is created. */
INT platform_hal_PandMDBInit(void)
{
    return RETURN_OK;
}

INT platform_hal_DocsisParamsDBInit(void)
{
    return RETURN_OK;
}

INT platform_hal_GetModelName(CHAR *pValue)
{
    return mock_copy(pValue, 256UL, "Mock-RDKB-Model");
}

INT platform_hal_GetRouterRegion(CHAR *pValue)
{
    return mock_copy(pValue, 256UL, "REGION_UNKNOWN");
}

INT platform_hal_GetSerialNumber(CHAR *pValue)
{
    return mock_copy(pValue, 256UL, "MOCK-SERIAL-0001");
}

INT platform_hal_GetHardwareVersion(CHAR *pValue)
{
    return mock_copy(pValue, 256UL, "MOCK-HW-1.0");
}

INT platform_hal_GetBaseMacAddress(CHAR *pValue)
{
    return mock_copy(pValue, 256UL, "02:00:00:00:00:01");
}

INT platform_hal_GetHardware(CHAR *pValue)
{
    return mock_copy(pValue, 256UL, "1024");
}

INT platform_hal_GetHardware_MemUsed(CHAR *pValue)
{
    return mock_copy(pValue, 1024UL, "256");
}

INT platform_hal_GetHardware_MemFree(CHAR *pValue)
{
    return mock_copy(pValue, 1024UL, "768");
}

INT platform_hal_GetSoftwareVersion(CHAR *pValue, ULONG maxSize)
{
    return mock_copy(pValue, maxSize, "MOCK-SW-1.0");
}

INT platform_hal_GetBootloaderVersion(CHAR *pValue, ULONG maxSize)
{
    return mock_copy(pValue, maxSize, "MOCK-BOOT-1.0");
}

INT platform_hal_GetFirmwareName(CHAR *pValue, ULONG maxSize)
{
    return mock_copy(pValue, maxSize, "mock-firmware.bin");
}

INT platform_hal_GetTotalMemorySize(ULONG *pulSize)
{
    if (pulSize == NULL)
        return RETURN_ERR;
    *pulSize = 1024UL;
    return RETURN_OK;
}

/* Sample factory-reset history, not a hardware reading. Process-local state
 * starts at 3 on restart. Calls must be serialized; no persistent storage.
 */
static ULONG factory_reset_count = 3UL;

INT platform_hal_GetFactoryResetCount(ULONG *pulSize)
{
    if (pulSize == NULL)
        return RETURN_ERR;
    *pulSize = factory_reset_count;
    return RETURN_OK;
}

INT platform_hal_ClearResetCount(BOOLEAN bFlag)
{
    if (bFlag != TRUE && bFlag != FALSE)
        return RETURN_ERR;
    if (bFlag == TRUE)
        factory_reset_count = 0UL;
    return RETURN_OK;
}

/* Fixed mock offset in seconds; do not read or change the system timezone.
 * Caller must provide the 256-byte buffer required by platform_hal.h.
 */
INT platform_hal_getTimeOffSet(CHAR *timeOffSet)
{
    return mock_copy(timeOffSet, 256UL, "-18000");
}

/* Process-local mock watchdog timeout: 60 minutes = 3600 seconds by default.
 * Calls must be serialized. No hardware watchdog is configured or armed.
 * Zero is accepted by the contract without assigning hardware semantics.
 */
static INT device_code_image_timeout = 3600;

INT platform_hal_SetDeviceCodeImageTimeout(INT seconds)
{
    if (seconds < 0 || seconds > 3600)
        return RETURN_ERR;
    device_code_image_timeout = seconds;
    return RETURN_OK;
}

/* Factory provisioning mocks, not hardware/NVRAM readings. Calls must be
 * serialized. The selected CM variant resets to unknown on process restart.
 */
static const CHAR *factory_cm_variant = "unknown";

INT platform_hal_getFactoryPartnerId(CHAR *pValue)
{
    /* Caller must provide at least 512 bytes, as required by the header. */
    return mock_copy(pValue, 512UL, "unknown");
}

INT platform_hal_getFactoryCmVariant(CHAR *pValue)
{
    return mock_copy(pValue, 512UL, factory_cm_variant);
}

INT platform_hal_setFactoryCmVariant(CHAR *pValue)
{
    static const CHAR *const variants[] = {
        "unknown", "pc20", "pc20genband", "pc15sip", "pc15mgcp"
    };
    size_t i;
    if (pValue == NULL)
        return RETURN_ERR;
    for (i = 0; i < sizeof(variants) / sizeof(variants[0]); ++i) {
        /* Compare only through the expected terminator; reject suffixes.
         * Store our own literal, never the lifetime-limited caller buffer.
         */
        if (strncmp(pValue, variants[i], strlen(variants[i]) + 1) == 0) {
            factory_cm_variant = variants[i];
            return RETURN_OK;
        }
    }
    return RETURN_ERR;
}

#ifdef FEATURE_RDKB_THERMAL_MANAGER
/* Fixed mock telemetry, not hardware measurements. Units follow platform_hal.h. */

/* degrees Celsius (0..100) */
INT platform_hal_getFanTemperature(INT *pTemp)
{
    if (pTemp == NULL)
        return RETURN_ERR;
    *pTemp = 45;
    return RETURN_OK;
}

/* milli-Amperes (0..20000) */
INT platform_hal_getInputCurrent(INT *pValue)
{
    if (pValue == NULL)
        return RETURN_ERR;
    *pValue = 1000;
    return RETURN_OK;
}

/* milli-Watts (0..20000) */
INT platform_hal_getInputPower(INT *pValue)
{
    if (pValue == NULL)
        return RETURN_ERR;
    *pValue = 12000;
    return RETURN_OK;
}
#endif /* FEATURE_RDKB_THERMAL_MANAGER */

/* Remaining APIs are fixtures, not hardware operations. Serialized calls only.
 * All setter state resets when the application process restarts. */
INT platform_hal_GetUsedMemorySize(ULONG *p) { if (!p) return RETURN_ERR; *p=256; return RETURN_OK; }
INT platform_hal_GetFreeMemorySize(ULONG *p) { if (!p) return RETURN_ERR; *p=768; return RETURN_OK; }

static BOOLEAN image_valid=TRUE;
INT platform_hal_SetDeviceCodeImageValid(BOOLEAN flag) {
    if (flag>TRUE) return RETURN_ERR;
    image_valid=flag; return RETURN_OK;
}
static LEDMGMT_PARAMS led={.LedColor=LED_WHITE};
#ifdef FEATURE_RDKB_LED_MANAGER
static char led_name[128], led_param[128];
int platform_hal_initLed(char *p) { return mock_copy(p,128,"mock-led-config.json"); }
#endif
INT platform_hal_setLed(PLEDMGMT_PARAMS p) {
    if (!p || p->LedColor<0 || p->LedColor>=NOT_SUPPORTED || p->State<0 ||
        p->State>1 || p->Interval<0 || (p->State && !p->Interval)) return RETURN_ERR;
#ifdef FEATURE_RDKB_LED_MANAGER
    if ((p->led_name && strlen(p->led_name)>=128) ||
        (p->led_param && strlen(p->led_param)>=128)) return RETURN_ERR;
    if (p->led_name) memmove(led_name,p->led_name,strlen(p->led_name)+1); else led_name[0]=0;
    if (p->led_param) memmove(led_param,p->led_param,strlen(p->led_param)+1); else led_param[0]=0;
#endif
    led=*p;
#ifdef FEATURE_RDKB_LED_MANAGER
    led.led_name=led_name; led.led_param=led_param;
#endif
    return RETURN_OK;
}
INT platform_hal_getLed(PLEDMGMT_PARAMS p) { if (!p) return RETURN_ERR; *p=led; return RETURN_OK; }
static UINT fan_speed[2]={50,50}; /* percentage, mock maximum 4000 RPM */
static BOOLEAN fan_override[2];
UINT platform_hal_getFanSpeed(UINT i) { return i<2 ? (fan_override[i]?100:fan_speed[i]):0; }
UINT platform_hal_getRPM(UINT i) { return platform_hal_getFanSpeed(i)*40; }
INT platform_hal_getRotorLock(UINT i) { return i<2?0:RETURN_ERR; }
BOOLEAN platform_hal_getFanStatus(UINT i) { return platform_hal_getFanSpeed(i)>0; }
INT platform_hal_setFanMaxOverride(BOOLEAN flag,UINT i) {
    if(i>=2 || flag>TRUE) return RETURN_ERR;
    fan_override[i]=flag; return RETURN_OK;
}

#ifdef FEATURE_RDKB_THERMAL_MANAGER
INT platform_hal_LoadThermalConfig(THERMAL_PLATFORM_CONFIG *p) {
    const THERMAL_PLATFORM_CONFIG defaults={2,30,35,40,30,5,1,60};
    if(!p) return RETURN_ERR;
    *p=defaults; return RETURN_OK;
}
INT platform_hal_initThermal(THERMAL_PLATFORM_CONFIG *p) { return platform_hal_LoadThermalConfig(p); }
INT platform_hal_setFanSpeed(UINT i,FAN_SPEED speed,FAN_ERR *reason) {
    static const UINT percent[]={0,25,50,75,100};
    if(!reason) return RETURN_ERR;
    *reason=FAN_ERR_HW;
    if(i>=2 || speed<0 || speed>FAN_SPEED_MAX) return RETURN_ERR;
    if(fan_override[i]) { *reason=FAN_ERR_MAX_OVERRIDE_SET; return RETURN_ERR; }
    fan_speed[i]=percent[speed]; *reason=FAN_ERR_NONE; return RETURN_OK;
}
INT platform_hal_getRadioTemperature(INT i,INT *p) {
    if(!p || i<0 || i>2) return RETURN_ERR;
    *p=42+i; return RETURN_OK;
}
INT platform_hal_getEcoModeStatus(INT i,INT *p) {
    if(!p || i<0 || i>1) return RETURN_ERR;
    *p=0; return RETURN_OK;
}
#endif
static BOOLEAN snmp_reboot=TRUE;
INT platform_hal_SetSNMPOnboardRebootEnable(CHAR *p) {
    if(!p) return RETURN_ERR;
    if(!strcasecmp(p,"enable")) snmp_reboot=TRUE;
    else if(!strcasecmp(p,"disable")) snmp_reboot=FALSE;
    else return RETURN_ERR;
    return RETURN_OK;
}
/* Process-local mock only: four zero-based ports, initially disabled/stopped.
 * Calls must be serialized. No hardware, encryption, or retry loop is used.
 */
#define MOCK_MACSEC_PORTS 4
static BOOLEAN macsec_enabled[MOCK_MACSEC_PORTS];
static BOOLEAN macsec_running[MOCK_MACSEC_PORTS];

static int valid_port(INT ethPort)
{
    return ethPort >= 0 && ethPort < MOCK_MACSEC_PORTS;
}

/* PUBLIC_INTERFACE */
/** Read mock enablement for ethPort into pFlag.
 * Return RETURN_ERR for NULL output or an unsupported port without writing.
 */
INT platform_hal_GetMACsecEnable(INT ethPort, BOOLEAN *pFlag)
{
    if (pFlag == NULL || !valid_port(ethPort))
        return RETURN_ERR;
    *pFlag = macsec_enabled[ethPort];
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/** Set mock enablement for ethPort to Boolean Flag; disabling also stops it.
 * Return RETURN_ERR for invalid inputs without changing either state.
 */
INT platform_hal_SetMACsecEnable(INT ethPort, BOOLEAN Flag)
{
    if (!valid_port(ethPort) || (Flag != TRUE && Flag != FALSE))
        return RETURN_ERR;
    macsec_enabled[ethPort] = Flag;
    if (Flag == FALSE)
        macsec_running[ethPort] = FALSE;
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/** Read mock running state for ethPort into pFlag.
 * Return RETURN_ERR for NULL output or an unsupported port without writing.
 */
INT platform_hal_GetMACsecOperationalStatus(INT ethPort, BOOLEAN *pFlag)
{
    if (pFlag == NULL || !valid_port(ethPort))
        return RETURN_ERR;
    *pFlag = macsec_running[ethPort];
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/** Start an enabled mock ethPort with timeoutSec=0; return RETURN_OK.
 * Invalid ports/timeouts or disabled ports return RETURN_ERR without mutation.
 * Unlike a hardware HAL, this mock completes immediately without retrying.
 */
INT platform_hal_StartMACsec(INT ethPort, INT timeoutSec)
{
    if (!valid_port(ethPort) || timeoutSec != 0 || !macsec_enabled[ethPort])
        return RETURN_ERR;
    macsec_running[ethPort] = TRUE;
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/** Stop mock ethPort without changing enablement.
 * Return RETURN_OK on success or RETURN_ERR for an unsupported port.
 */
INT platform_hal_StopMACsec(INT ethPort)
{
    if (!valid_port(ethPort))
        return RETURN_ERR;
    macsec_running[ethPort] = FALSE;
    return RETURN_OK;
}

INT platform_hal_GetMemoryPaths(RDK_CPUS i,PPLAT_PROC_MEM_INFO *p) {
    PPLAT_PROC_MEM_INFO r;
    if(!p) return RETURN_ERR;
    *p=NULL;
    if(i!=HOST_CPU && i!=PEER_CPU) return RETURN_ERR;
    r=calloc(1,sizeof(*r)); if(!r) return RETURN_ERR;
    strcpy(r->dramPath,"/tmp"); strcpy(r->emmcPath1,"/nvram"); strcpy(r->emmcPath2,"/nvram2");
    *p=r; return RETURN_OK; /* caller frees */
}
/* Process-local mock state, reset to AC on restart. Serialized calls only.
 * No hardware power management is performed.
 */
static PSM_STATE low_power = PSM_AC;

/* PUBLIC_INTERFACE */
/**
 * @brief Copy a declared power-state enum into the process-local mock.
 * @param[in] p Pointer to the requested PSM_STATE, including NOT_SUPPORTED.
 * @return RETURN_OK on success; RETURN_ERR for NULL or an undeclared value,
 *         leaving the current mock state unchanged.
 */
INT platform_hal_SetLowPowerModeState(PPSM_STATE p)
{
    if (p == NULL)
        return RETURN_ERR;
    if (*p < PSM_UNKNOWN || *p > PSM_NOT_SUPPORTED)
        return RETURN_ERR;

    low_power = *p;
    return RETURN_OK;
}
INT platform_hal_getCMTSMac(CHAR *p) { return mock_copy(p,100,"02:00:00:00:00:02"); }

/* PUBLIC_INTERFACE */
/**
 * @brief Copy the synthetic CPU speed fixture, not a hardware measurement.
 * @param[out] cpuSpeed Caller-owned buffer of at least 64 bytes.
 * @return RETURN_OK on success; RETURN_ERR when cpuSpeed is NULL.
 */
INT platform_hal_GetCPUSpeed(char *cpuSpeed)
{
    return mock_copy(cpuSpeed, 64UL, "1000");
}

INT platform_hal_GetFirmwareBankInfo(FW_BANK bank,PFW_BANK_INFO p) {
    if(!p || (bank!=ACTIVE_BANK && bank!=INACTIVE_BANK)) return RETURN_ERR;
    memset(p,0,sizeof(*p));
    strcpy(p->fw_name,bank==ACTIVE_BANK?"mock-firmware.bin":"mock-firmware-backup.bin");
    strcpy(p->fw_state,"confirmed"); return RETURN_OK;
}
INT platform_hal_GetInterfaceStats(const char *name,PINTF_STATS p) {
    if(!name || !p || (strcmp(name,"eth0") && strcmp(name,"erouter0") && strcmp(name,"brlan0"))) return RETURN_ERR;
    memset(p,0,sizeof(*p)); return RETURN_OK;
}

static struct { UINT count,values[63]; BOOLEAN running; } dscp[WAN_INTERFACE_MAX];
static int valid_wan(WAN_INTERFACE w) { return w>=DOCSIS && w<WAN_INTERFACE_MAX; }
INT platform_hal_setDscp(WAN_INTERFACE w,TRAFFIC_CNT_COMMAND cmd,char *values) {
    UINT parsed[63]={0},count=0;
    const char *cursor=values;
    if(!valid_wan(w) || (cmd!=TRAFFIC_CNT_START && cmd!=TRAFFIC_CNT_STOP)) return RETURN_ERR;
    if(values) {
        if(!*values || strlen(values)>195) return RETURN_ERR;
        for(;;) {
            char *end; unsigned long n; UINT i;
            if(!isdigit((unsigned char)*cursor) || count>=63) return RETURN_ERR;
            errno=0; n=strtoul(cursor,&end,10);
            if(errno || n>65534 || (*end && *end!=44)) return RETURN_ERR;
            for(i=0;i<count;++i) if(parsed[i]==n) return RETURN_ERR;
            parsed[count++]=(UINT)n;
            if(!*end) break;
            cursor=end+1;
        }
    }
    if(cmd==TRAFFIC_CNT_START) {
        dscp[w].count=count; memcpy(dscp[w].values,parsed,count*sizeof(UINT));
    }
    dscp[w].running=cmd==TRAFFIC_CNT_START; return RETURN_OK;
}
INT platform_hal_resetDscpCounts(WAN_INTERFACE w) {
    return valid_wan(w)?RETURN_OK:RETURN_ERR; /* mock counters are already zero */
}
INT platform_hal_getDscpClientList(WAN_INTERFACE w,pDSCP_list_t p) {
    UINT i;
    if(!valid_wan(w) || !p) return RETURN_ERR;
    memset(p,0,sizeof(*p)); p->numElements=dscp[w].count;
    for(i=0;i<p->numElements;++i) p->DSCP_Element[i].dscp_value=dscp[w].values[i];
    return RETURN_OK;
}
/* PUBLIC_INTERFACE */
/**
 * @brief Copy the synthetic mock PPP username, not a provisioned credential.
 * @param[out] pUserName Receives the complete NUL-terminated fixture.
 * @param[in] maxSize Output capacity in bytes, including the terminator.
 * @return RETURN_OK on success; RETURN_ERR for NULL or insufficient capacity,
 *         without modifying the output.
 */
INT platform_hal_GetPppUserName(CHAR *pUserName, ULONG maxSize)
{
    return mock_copy(pUserName, maxSize, "mock-user");
}

/* PUBLIC_INTERFACE */
/**
 * @brief Copy the synthetic mock PPP password, not a provisioned credential.
 * @param[out] pPassword Receives the complete NUL-terminated fixture.
 * @param[in] maxSize Output capacity in bytes, including the terminator.
 * @return RETURN_OK on success; RETURN_ERR for NULL or insufficient capacity,
 *         without modifying the output.
 */
INT platform_hal_GetPppPassword(CHAR *pPassword, ULONG maxSize)
{
    return mock_copy(pPassword, maxSize, "mock-password");
}
/* QoS configuration cache used by the mock DUT */
static hal_network_params_t qos;

/**
 * @brief Apply QoS configuration.
 *
 * Validates the supplied network parameters and stores the
 * configuration in the mock backend on success.
 *
 * @param[in] p Pointer to QoS network parameters.
 *
 * @return RETURN_OK  on success.
 * @return RETURN_ERR on validation failure.
 */
INT platform_hal_qos_apply(const hal_network_params_t *p)
{
    /* NULL pointer validation */
    if (p == NULL)
    {
        return RETURN_ERR;
    }

    /* Validate IP version */
    if ((p->ip_version != IP_VERSION_IPV4) &&
        (p->ip_version != IP_VERSION_IPV6))
    {
        return RETURN_ERR;
    }

    /* Validate transport protocol */
    if ((p->protocol != PROTOCOL_TCP) &&
        (p->protocol != PROTOCOL_UDP))
    {
        return RETURN_ERR;
    }

    /* Validate DSCP range (0-63) */
    if (p->dscp_value > 63)
    {
        return RETURN_ERR;
    }

    /* Save configuration for subsequent verification */
    qos = *p;

    return RETURN_OK;
}


