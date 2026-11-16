#define _POSIX_C_SOURCE 200809L
#include "platformhal_apis.h"
#include "platform_hal.h"
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Typed HAL objects stay here; only text crosses IPC. */
static platform_vl_status_t invalid_request(char *response, size_t capacity,
                                            const char *message)
{
    snprintf(response, capacity, "status: -1\nerror: %s\n", message);
    return PLATFORM_VL_INVALID_REQUEST;
}


/* Command adapters own every output object; only bounded text crosses IPC. */
#define BAD_ARGS (-3)
#define ARG_COUNT 16
struct api_args { const char *key[ARG_COUNT]; char *value[ARG_COUNT]; size_t count; };
static char *arg(const struct api_args *a,const char *key) {
    size_t i;
    for(i=0;i<a->count;++i) if(!strcmp(a->key[i],key)) return a->value[i];
    return NULL;
}
static int number(const struct api_args *a,const char *key,long min,long max,long *out) {
    const char *p=arg(a,key),*digit=p; char *end; long value;
    if(!p || !*p) return 0;
    if(*digit==45 && min<0) ++digit;
    if(!*digit) return 0;
    for(;*digit;++digit) if(*digit<48 || *digit>57) return 0;
    errno=0; value=strtol(p,&end,10);
    if(errno || *end || value<min || value>max) return 0;
    *out=value; return 1;
}
#define NUM(key,min,max,var) long var; if(!number(a,key,min,max,&var)) return BAD_ARGS
static int ulong_number(const struct api_args *a, const char *key, ULONG *out)
{
    const char *text = arg(a, key);
    const char *digit;
    char *end;
    unsigned long parsed;

    if (text == NULL || *text == '\0')
        return 0;
    for (digit = text; *digit != '\0'; ++digit) {
        if (*digit < '0' || *digit > '9')
            return 0;
    }
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0')
        return 0;
    if ((unsigned long)(ULONG)parsed != parsed)
        return 0;
    *out = (ULONG)parsed;
    return 1;
}
/* Incremental formatter rejects overflow rather than returning truncated data. */
static int append_text(char *out,size_t cap,size_t *used,const char *format,...) {
    int n; va_list ap;
    if(*used>=cap) return 0;
    va_start(ap,format); n=vsnprintf(out+*used,cap-*used,format,ap); va_end(ap);
    if(n<0 || (size_t)n>=cap-*used) return 0;
    *used+=(size_t)n; return 1;
}

/* COMMAND GetDeviceConfigStatus|?output-pointer */
static INT call_GetDeviceConfigStatus(
    const struct api_args *a,
    char *out,
    size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[128] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetDeviceConfigStatus(output);

    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %.*s\n", (int)sizeof(value), value);

    return rc;
}
/* COMMAND GetTelnetEnable| */
static INT call_GetTelnetEnable(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    BOOLEAN value = 0;
    BOOLEAN *output = &value;
    INT rc;
    /* Test tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    rc = platform_hal_GetTelnetEnable(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %d\n", (int)value);
    return rc;
}
/* COMMAND SetTelnetEnable|value */
static INT call_SetTelnetEnable(const struct api_args *a,char *out,size_t cap) {
    (void)a; (void)out; (void)cap;
    NUM("flag",0,1,flag); return platform_hal_SetTelnetEnable((BOOLEAN)flag);
}
/* COMMAND GetSSHEnable| */
static INT call_GetSSHEnable(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    BOOLEAN value = 0;
    BOOLEAN *output = &value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetSSHEnable(output);

    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %d\n", (int)value);

    return rc;
}
/* COMMAND SetSSHEnable|flag */
static INT call_SetSSHEnable(const struct api_args *a,char *out,size_t cap) {
    (void)a; (void)out; (void)cap;
    NUM("flag",0,1,flag); return platform_hal_SetSSHEnable((BOOLEAN)flag);
}
/* COMMAND GetSNMPEnable|?output-pointer */
static INT call_GetSNMPEnable(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[32] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetSNMPEnable(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND SetSNMPEnable|pvalue */
static INT call_SetSNMPEnable(
    const struct api_args *a, char *out, size_t cap)
{
    CHAR *pvalue = arg(a, "pvalue");

    (void)out;
    (void)cap;

    /* Missing input is a malformed request, not a HAL NULL test. */
    if (pvalue == NULL)
        return BAD_ARGS;

    /* Convert the test token into an actual local C NULL pointer. */
    if (strcmp(pvalue, "NULL") == 0)
        pvalue = NULL;

    return platform_hal_SetSNMPEnable(pvalue);
}
/* COMMAND GetcaTimeout| */
static INT call_GetWebUITimeout(const struct api_args *a,char *out,size_t cap) {
    (void)a; (void)out; (void)cap;
    ULONG value=0; INT rc=platform_hal_GetWebUITimeout(&value); if(rc==RETURN_OK) snprintf(out,cap,"value: %lu\n",(unsigned long)value); return rc;
}
/* COMMAND SetWebUITimeout|value */
static INT call_SetWebUITimeout(const struct api_args *a,char *out,size_t cap) {
    (void)out; (void)cap;
    /* Parse nonnegative long values; the HAL validates timeout boundaries. */
    NUM("value",0,LONG_MAX,value);
    return platform_hal_SetWebUITimeout((ULONG)value);
}
/* COMMAND PandMDBInit| */
static INT call_PandMDBInit(const struct api_args *a,char *out,size_t cap) {
    (void)a; (void)out; (void)cap;
    return platform_hal_PandMDBInit();
}
/* COMMAND DocsisParamsDBInit| */
static INT call_DocsisParamsDBInit(const struct api_args *a,char *out,size_t cap) {
    (void)a; (void)out; (void)cap;
    return platform_hal_DocsisParamsDBInit();
}
/* COMMAND GetModelName|?output-pointer */
static INT call_GetModelName(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[1024] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetModelName(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetRouterRegion|?output-pointer */
static INT call_GetRouterRegion(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[1024] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetRouterRegion(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetSerialNumber|?output-pointer */
static INT call_GetSerialNumber(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[1024] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetSerialNumber(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetHardwareVersion|?output-pointer */
static INT call_GetHardwareVersion(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[1024] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetHardwareVersion(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetSoftwareVersion|?output-pointer */
static INT call_GetSoftwareVersion(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[1024] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    /* Keep a valid capacity so the NULL case isolates pointer handling. */
    rc = platform_hal_GetSoftwareVersion(output, (ULONG)sizeof(value));
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetBootloaderVersion|?output-pointer */
static INT call_GetBootloaderVersion(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[1024] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    /* Keep a valid capacity so the NULL case isolates pointer handling. */
    rc = platform_hal_GetBootloaderVersion(output, (ULONG)sizeof(value));
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetFirmwareName|?output-pointer */
static INT call_GetFirmwareName(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[1024] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    /* Keep a valid capacity so the NULL case isolates pointer handling. */
    rc = platform_hal_GetFirmwareName(output, (ULONG)sizeof(value));
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetBaseMacAddress|?output-pointer */
static INT call_GetBaseMacAddress(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[1024] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetBaseMacAddress(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetHardware|?output-pointer */
static INT call_GetHardware(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[1024] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetHardware(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetHardware_MemUsed|?output-pointer */
static INT call_GetHardware_MemUsed(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[1024] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetHardware_MemUsed(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetHardware_MemFree|?output-pointer */
static INT call_GetHardware_MemFree(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[1024] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetHardware_MemFree(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetTotalMemorySize|?output-pointer */
static INT call_GetTotalMemorySize(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    ULONG value = 0;
    ULONG *output = &value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetTotalMemorySize(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %lu\n", (unsigned long)value);
    return rc;
}
/* COMMAND GetUsedMemorySize|?output-pointer */
static INT call_GetUsedMemorySize(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    ULONG value = 0;
    ULONG *output = &value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetUsedMemorySize(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %lu\n", (unsigned long)value);
    return rc;
}
/* COMMAND GetFreeMemorySize|?output-pointer */
static INT call_GetFreeMemorySize(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    ULONG value = 0;
    ULONG *output = &value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetFreeMemorySize(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %lu\n", (unsigned long)value);
    return rc;
}
/* COMMAND GetFactoryResetCount|?output-pointer */
static INT call_GetFactoryResetCount(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    ULONG value = 0;
    ULONG *output = &value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetFactoryResetCount(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %lu\n", (unsigned long)value);
    return rc;
}
/* COMMAND ClearResetCount|flag */
static INT call_ClearResetCount(const struct api_args *a, char *out, size_t cap)
{
    (void)out;
    (void)cap;

    /* BOOLEAN is a scalar flag, not an output pointer. */
    NUM("flag", 0, 1, flag);
    return platform_hal_ClearResetCount((BOOLEAN)flag);
}
/* COMMAND getTimeOffSet|?output-pointer */
static INT call_getTimeOffSet(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[256] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_getTimeOffSet(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND SetDeviceCodeImageValid|value */
static INT call_SetDeviceCodeImageValid(
    const struct api_args *a, char *out, size_t cap)
{
    (void)out;
    (void)cap;

    /* BOOLEAN is a scalar: FALSE=0 or TRUE=1. */
    NUM("value", 0, 1, value);
    return platform_hal_SetDeviceCodeImageValid((BOOLEAN)value);
}

/* COMMAND getFactoryPartnerId|?output-pointer */
static INT call_getFactoryPartnerId(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[512] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_getFactoryPartnerId(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);

    return rc;
}

/* COMMAND getFactoryCmVariant|?output-pointer */
static INT call_getFactoryCmVariant(
    const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[512] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_getFactoryCmVariant(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);

    return rc;
}
/* COMMAND setFactoryCmVariant|value */
static INT call_setFactoryCmVariant(
    const struct api_args *a, char *out, size_t cap)
{
    CHAR *value = arg(a, "value");

    (void)out;
    (void)cap;

    /* Missing input is a malformed request, not a HAL NULL test. */
    if (value == NULL)
        return BAD_ARGS;

    /* Convert the test token into an actual local C NULL pointer. */
    if (strcmp(value, "NULL") == 0)
        value = NULL;

    return platform_hal_setFactoryCmVariant(value);
}
#ifdef FEATURE_RDKB_THERMAL_MANAGER
/* COMMAND getFanTemperature|?output-pointer */
static INT call_getFanTemperature(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    INT value = 0;
    INT *output = &value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_getFanTemperature(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %d\n", (int)value);
    return rc;
}
#endif
#ifdef FEATURE_RDKB_THERMAL_MANAGER
/* COMMAND getInputCurrent|?output-pointer */
static INT call_getInputCurrent(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    INT value = 0;
    INT *output = &value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_getInputCurrent(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %d\n", (int)value);
    return rc;
}
#endif
#ifdef FEATURE_RDKB_THERMAL_MANAGER
/* COMMAND getInputPower|?output-pointer */
static INT call_getInputPower(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    INT value = 0;
    INT *output = &value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_getInputPower(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %d\n", (int)value);
    return rc;
}
#endif
/* COMMAND SetSNMPOnboardRebootEnable|pvalue */
static INT call_SetSNMPOnboardRebootEnable(const struct api_args *a,char *out,size_t cap) {
    CHAR *pvalue = arg(a, "pvalue");
    (void)out; (void)cap;
    if (pvalue == NULL)
        return BAD_ARGS;
    return platform_hal_SetSNMPOnboardRebootEnable(pvalue);
}
/* COMMAND getCMTSMac|?output-pointer */
static INT call_getCMTSMac(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[100] = {0};
    CHAR *output = value;
    INT rc;
    size_t used = 0;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    rc = platform_hal_getCMTSMac(output);
    if (rc != RETURN_OK || output == NULL) return rc;
    if (!append_text(out, cap, &used, "value: %.*s\n",
                     (int)sizeof(value), value))
        return RETURN_ERR;
    return RETURN_OK;
}
/* COMMAND GetCPUSpeed|?output-pointer */
static INT call_GetCPUSpeed(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[64] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_GetCPUSpeed(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetPppUserName|?output-pointer */
static INT call_GetPppUserName(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[256] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    /* Keep a valid capacity so the NULL case isolates pointer handling. */
    rc = platform_hal_GetPppUserName(output, (ULONG)sizeof(value));
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}
/* COMMAND GetPppPassword|?output-pointer */
static INT call_GetPppPassword(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR value[256] = {0};
    CHAR *output = value;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    /* Keep a valid capacity so the NULL case isolates pointer handling. */
    rc = platform_hal_GetPppPassword(output, (ULONG)sizeof(value));
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "value: %s\n", value);
    return rc;
}

/* Use LEDMGMT_PARAMS from platform_hal.h; never send pointers over IPC. */
static INT call_setLed(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "pValue");
    LEDMGMT_PARAMS pValue = {0};
    INT rc;
    size_t used = 0;
    /* Select a local input pointer; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            return platform_hal_setLed(NULL);
        if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    /* Require representable integers; HAL validates LED semantics. */
    NUM("LedColor", INT_MIN, INT_MAX, color);
    NUM("State", INT_MIN, INT_MAX, state);
    NUM("Interval", INT_MIN, INT_MAX, interval);
    pValue.LedColor = (LED_COLOR)color;
    pValue.State = (INT)state;
    pValue.Interval = (INT)interval;
#ifdef FEATURE_RDKB_LED_MANAGER
    pValue.led_name = arg(a, "led_name");
    pValue.led_param = arg(a, "led_param");
#endif
    rc = platform_hal_setLed(&pValue);
    if (rc != RETURN_OK) return rc;
    if (!append_text(out, cap, &used,
                     "LedColor: %d\nState: %d\nInterval: %d\n",
                     (int)pValue.LedColor, pValue.State, pValue.Interval))
        return RETURN_ERR;
    return RETURN_OK;
}

static INT call_getLed(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    LEDMGMT_PARAMS pValue = {0};
    PLEDMGMT_PARAMS output = &pValue;
    INT rc;
    size_t used = 0;
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    rc = platform_hal_getLed(output);
    if (output == NULL) return rc;
    if (rc != RETURN_OK) return rc;
    if (!append_text(out, cap, &used,
                     "LedColor: %d\nState: %d\nInterval: %d\n",
                     (int)pValue.LedColor, pValue.State, pValue.Interval))
        return RETURN_ERR;
    return RETURN_OK;
}

/* COMMAND GetWebAccessLevel|userIndex ifIndex ?output-pointer */
static INT call_GetWebAccessLevel(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    ULONG value = 0;
    ULONG *output = &value;
    INT rc;
    size_t used = 0;
    NUM("userIndex", INT_MIN, INT_MAX, userIndex);
    NUM("ifIndex", INT_MIN, INT_MAX, ifIndex);
    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    rc = platform_hal_GetWebAccessLevel((INT)userIndex, (INT)ifIndex, output);
    if (rc != RETURN_OK || output == NULL) return rc;
    if (!append_text(out, cap, &used, "value: %lu\n", (unsigned long)value))
        return RETURN_ERR;
    return RETURN_OK;
}

/* COMMAND SetWebAccessLevel|userIndex ifIndex value */
static INT call_SetWebAccessLevel(const struct api_args *a, char *out, size_t cap)
{
    ULONG value;
    (void)out; (void)cap;
    NUM("userIndex", INT_MIN, INT_MAX, userIndex);
    NUM("ifIndex", INT_MIN, INT_MAX, ifIndex);
    /* Parse ULONG storage; HAL validates the documented access-level range. */
    if (!ulong_number(a, "value", &value))
        return BAD_ARGS;
    return platform_hal_SetWebAccessLevel((INT)userIndex, (INT)ifIndex, value);
}

/* COMMAND SetDeviceCodeImageTimeout|seconds */
static INT call_SetDeviceCodeImageTimeout(
    const struct api_args *a, char *out, size_t cap)
{
    (void)out;
    (void)cap;

    /* Parse representable INT values; the HAL validates the 0–3600 range. */
    NUM("seconds", INT_MIN, INT_MAX, seconds);
    return platform_hal_SetDeviceCodeImageTimeout((INT)seconds);
}

/* COMMAND GetFirmwareBankInfo|bankIndex ?output-pointer */
static INT call_GetFirmwareBankInfo(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    FW_BANK_INFO info = {0};
    PFW_BANK_INFO output = &info;
    INT rc;
    size_t used = 0;
    /* Include invalid boundary cases; HAL validates supported banks. */
    NUM("bankIndex", -1, 2, bankIndex);
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    rc = platform_hal_GetFirmwareBankInfo((FW_BANK)bankIndex, output);
    if (rc != RETURN_OK || output == NULL) return rc;
    if (!append_text(out, cap, &used, "fw_name: %.*s\nfw_state: %.*s\n",
                     (int)sizeof(info.fw_name), info.fw_name,
                     (int)sizeof(info.fw_state), info.fw_state))
        return RETURN_ERR;
    return RETURN_OK;
}

/* COMMAND GetInterfaceStats|?ifname ?output-pointer */
static INT call_GetInterfaceStats(const struct api_args *a, char *out, size_t cap)
{
    const char *ifname = arg(a, "ifname");
    const char *mode = arg(a, "output-pointer");
    INTF_STATS stats = {0};
    PINTF_STATS output = &stats;
    INT rc;
    size_t used = 0;

    /* Missing ifname is actual NULL; pointer tokens are resolved locally. */
    if (ifname != NULL && strcmp(ifname, "NULL") == 0)
        ifname = NULL;
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    rc = platform_hal_GetInterfaceStats(ifname, output);
    if (rc != RETURN_OK || output == NULL) return rc;
    if (!append_text(out, cap, &used,
                     "rx_packet: %llu\ntx_packet: %llu\nrx_bytes: %llu\ntx_bytes: %llu\n",
                     (unsigned long long)stats.rx_packet,
                     (unsigned long long)stats.tx_packet,
                     (unsigned long long)stats.rx_bytes,
                     (unsigned long long)stats.tx_bytes))
        return RETURN_ERR;
    return RETURN_OK;
}

/* Fan getters return readings, not HAL operation status. */
/* COMMAND getFanSpeed|fanIndex */
static INT call_getFanSpeed(const struct api_args *a, char *out, size_t cap)
{
    UINT value;
    size_t used = 0;
    NUM("fanIndex", 0, 1, fanIndex);
    value = platform_hal_getFanSpeed((UINT)fanIndex);
    if (!append_text(out, cap, &used, "value: %u\n", (unsigned int)value))
        return RETURN_ERR;
    return RETURN_OK;
}

/* COMMAND getRPM|fanIndex */
static INT call_getRPM(const struct api_args *a, char *out, size_t cap)
{
    UINT value;
    size_t used = 0;
    NUM("fanIndex", 0, 1, fanIndex);
    value = platform_hal_getRPM((UINT)fanIndex);
    if (!append_text(out, cap, &used, "value: %u\n", (unsigned int)value))
        return RETURN_ERR;
    return RETURN_OK;
}

/* COMMAND getRotorLock|fanIndex */
static INT call_getRotorLock(const struct api_args *a, char *out, size_t cap)
{
    INT value;
    size_t used = 0;
    NUM("fanIndex", 0, 1, fanIndex);
    /* -1 means not applicable and remains response data. */
    value = platform_hal_getRotorLock((UINT)fanIndex);
    if (!append_text(out, cap, &used, "value: %d\n", (int)value))
        return RETURN_ERR;
    return RETURN_OK;
}

/* COMMAND getFanStatus|fanIndex */
static INT call_getFanStatus(const struct api_args *a, char *out, size_t cap)
{
    BOOLEAN value;
    size_t used = 0;
    NUM("fanIndex", 0, 1, fanIndex);
    value = platform_hal_getFanStatus((UINT)fanIndex);
    if (!append_text(out, cap, &used, "value: %d\n", (int)value))
        return RETURN_ERR;
    return RETURN_OK;
}

/* COMMAND setFanMaxOverride|bOverrideFlag fanIndex */
static INT call_setFanMaxOverride(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    NUM("bOverrideFlag", 0, 1, flag);
    /* The HAL checks supported indexes; avoid overflow in parsing/casting. */
    NUM("fanIndex", 0,
        (UINT_MAX > LONG_MAX ? LONG_MAX : UINT_MAX), fanIndex);
    return platform_hal_setFanMaxOverride((BOOLEAN)flag, (UINT)fanIndex);
}

#ifdef FEATURE_RDKB_THERMAL_MANAGER
/* COMMAND setFanSpeed|fanIndex fanSpeed ?output-pointer */
static INT call_setFanSpeed(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    FAN_ERR reason = FAN_ERR_NONE;
    FAN_ERR *output = &reason;
    INT rc;
    size_t used = 0;
    NUM("fanIndex", 0, 1, fanIndex);
    /* Parse numeric speeds; HAL validates supported FAN_SPEED values. */
    NUM("fanSpeed", INT_MIN, INT_MAX, fanSpeed);
    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    rc = platform_hal_setFanSpeed((UINT)fanIndex, (FAN_SPEED)fanSpeed, output);
    /* Keep the HAL reason on success and failure only for a valid object. */
    if (output != NULL &&
        !append_text(out, cap, &used, "reason: %d\n", (int)reason))
        return RETURN_ERR;
    return rc;
}

/* Shared serialization for the two thermal configuration getters. */
static INT thermal_config_text(const THERMAL_PLATFORM_CONFIG *p,
                               char *out, size_t cap)
{
    size_t used = 0;
    if (!append_text(out, cap, &used,
                     "FanCount: %u\nSlowSpeedThresh: %u\nMediumSpeedThresh: %u\n"
                     "FastSpeedThresh: %u\nFanMinRunTime: %u\nMonitoringDelay: %u\n"
                     "PowerMonitoring: %u\nLogInterval: %u\n",
                     p->FanCount, p->SlowSpeedThresh, p->MediumSpeedThresh,
                     p->FastSpeedThresh, p->FanMinRunTime, p->MonitoringDelay,
                     p->PowerMonitoring, p->LogInterval))
        return RETURN_ERR;
    return RETURN_OK;
}

/* COMMAND initThermal|?output-pointer */
static INT call_initThermal(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    THERMAL_PLATFORM_CONFIG config = {0};
    THERMAL_PLATFORM_CONFIG *output = &config;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    rc = platform_hal_initThermal(output);
    if (rc != RETURN_OK || output == NULL) return rc;
    return thermal_config_text(output, out, cap);
}

/* COMMAND LoadThermalConfig|?output-pointer */
static INT call_LoadThermalConfig(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    THERMAL_PLATFORM_CONFIG config = {0};
    THERMAL_PLATFORM_CONFIG *output = &config;
    INT rc;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    rc = platform_hal_LoadThermalConfig(output);
    if (rc != RETURN_OK || output == NULL) return rc;
    return thermal_config_text(output, out, cap);
}

#endif /* FEATURE_RDKB_THERMAL_MANAGER */

#ifdef FEATURE_RDKB_THERMAL_MANAGER
/* COMMAND getRadioTemperature|radioIndex ?output-pointer */
static INT call_getRadioTemperature(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    INT value = 0;
    INT *output = &value;
    INT rc;
    size_t used = 0;
    /* Include invalid upper boundary; HAL validates supported indexes. */
    NUM("radioIndex", 0, 3, radioIndex);

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_getRadioTemperature((INT)radioIndex, output);
    if (rc != RETURN_OK || output == NULL) return rc;
    if (!append_text(out, cap, &used, "value: %d\n", (int)value))
        return RETURN_ERR;
    return RETURN_OK;
}

/* COMMAND getEcoModeStatus|radioIndex ?output-pointer */
static INT call_getEcoModeStatus(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    INT value = 0;
    INT *output = &value;
    INT rc;
    size_t used = 0;
    /* Parse representable INT values; the HAL validates supported indexes. */
    NUM("radioIndex", INT_MIN, INT_MAX, radioIndex);

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = platform_hal_getEcoModeStatus((INT)radioIndex, output);
    if (rc != RETURN_OK || output == NULL) return rc;
    if (!append_text(out, cap, &used, "value: %d\n", (int)value))
        return RETURN_ERR;
    return RETURN_OK;
}

#endif /* FEATURE_RDKB_THERMAL_MANAGER */

/* COMMAND GetMACsecEnable|ethPort ?output-pointer */
static INT call_GetMACsecEnable(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    BOOLEAN value = 0;
    BOOLEAN *output = &value;
    INT rc;
    size_t used = 0;
    /* Parse signed INT ports; HAL validates the supported port range. */
    NUM("ethPort", INT_MIN, INT_MAX, ethPort);
    /* Tokens select local pointers; scalar NULL ports remain invalid inputs. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    rc = platform_hal_GetMACsecEnable((INT)ethPort, output);
    if (rc != RETURN_OK || output == NULL) return rc;
    if (!append_text(out, cap, &used, "value: %d\n", (int)value))
        return RETURN_ERR;
    return RETURN_OK;
}

/* COMMAND GetMACsecOperationalStatus|ethPort ?output-pointer */
static INT call_GetMACsecOperationalStatus(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    BOOLEAN value = 0;
    BOOLEAN *output = &value;
    INT rc;
    size_t used = 0;
    /* Parse signed INT ports; HAL validates the supported port range. */
    NUM("ethPort", INT_MIN, INT_MAX, ethPort);
    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    rc = platform_hal_GetMACsecOperationalStatus((INT)ethPort, output);
    if (rc != RETURN_OK || output == NULL) return rc;
    if (!append_text(out, cap, &used, "value: %d\n", (int)value))
        return RETURN_ERR;
    return RETURN_OK;
}

/* COMMAND SetMACsecEnable|ethPort Flag */
static INT call_SetMACsecEnable(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    /* Parse signed INT ports; HAL validates the supported port range. */
    NUM("ethPort", INT_MIN, INT_MAX, ethPort);
    NUM("Flag", 0, 1, flag);
    return platform_hal_SetMACsecEnable((INT)ethPort, (BOOLEAN)flag);
}

/* COMMAND StartMACsec|ethPort timeoutSec */
static INT call_StartMACsec(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    /* Parse signed INT ports; HAL validates the supported port range. */
    NUM("ethPort", INT_MIN, INT_MAX, ethPort);
    NUM("timeoutSec", 0, 0, timeoutSec);
    /* Contract: blocking, timeout 0 retries forever on a real HAL. */
    return platform_hal_StartMACsec((INT)ethPort, (INT)timeoutSec);
}

/* COMMAND StopMACsec|ethPort */
static INT call_StopMACsec(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    /* Parse signed INT ports; HAL validates the supported port range. */
    NUM("ethPort", INT_MIN, INT_MAX, ethPort);
    return platform_hal_StopMACsec((INT)ethPort);
}

/* COMMAND GetMemoryPaths|index */
static INT call_GetMemoryPaths(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    PPLAT_PROC_MEM_INFO info = NULL;
    PPLAT_PROC_MEM_INFO *output = &info;
    INT rc;
    size_t used = 0;
    /* Parse signed INT indices; HAL validates supported CPU enum values. */
    NUM("index", INT_MIN, INT_MAX, index);
    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    /* Current backend allocates one record with calloc; caller frees it.
     * The HAL header exposes no array length. Serialize only record zero;
     * confirm allocation/count semantics before linking a vendor backend. */
    rc = platform_hal_GetMemoryPaths((RDK_CPUS)index, output);
    if (rc == RETURN_OK) {
        if (output == NULL || !info || !append_text(out, cap, &used,
                "dramPath: %.*s\nemmcPath1: %.*s\nemmcPath2: %.*s\nemmcPath3: %.*s\n",
                (int)sizeof(info->dramPath), info->dramPath,
                (int)sizeof(info->emmcPath1), info->emmcPath1,
                (int)sizeof(info->emmcPath2), info->emmcPath2,
                (int)sizeof(info->emmcPath3), info->emmcPath3))
            rc = RETURN_ERR;
    }
    free(info);
    return rc;
}

typedef INT (*dhcp_options_getter)(
    dhcp_opt_list **req_opt_list,
    dhcp_opt_list **send_opt_list);

/*
 * EMPTY-LIST MOCK ONLY. Populated vendor lists require type-aware
 * serialization and their documented cleanup mechanism.
 */
static INT call_empty_dhcp_options(
    const struct api_args *a,
    char *out,
    size_t cap,
    dhcp_options_getter getter)
{
    const char *req_mode = arg(a, "req-pointer");
    const char *send_mode = arg(a, "send-pointer");
    dhcp_opt_list *req_list = NULL;
    dhcp_opt_list *send_list = NULL;
    dhcp_opt_list **req_output = &req_list;
    dhcp_opt_list **send_output = &send_list;
    INT rc;
    size_t used = 0;

    /* Omitted or BUFFER selects a valid local pointer-to-pointer. */
    if (req_mode != NULL) {
        if (strcmp(req_mode, "NULL") == 0)
            req_output = NULL;
        else if (strcmp(req_mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    if (send_mode != NULL) {
        if (strcmp(send_mode, "NULL") == 0)
            send_output = NULL;
        else if (strcmp(send_mode, "BUFFER") != 0)
            return BAD_ARGS;
    }

    rc = getter(req_output, send_output);
    if (rc != RETURN_OK)
        return rc;

    if (req_output == NULL || send_output == NULL)
        return RETURN_ERR;

    /*
     * These mocks always return NULL lists and allocate nothing.
     * Do not reuse this adapter for populated vendor lists.
     */
    if (req_list != NULL || send_list != NULL)
        return RETURN_ERR;

    if (!append_text(
            out, cap, &used,
            "req_count: 0\n"
            "send_count: 0\n"))
        return RETURN_ERR;

    return RETURN_OK;
}

/* COMMAND GetDhcpv6_Options|?req-pointer ?send-pointer */
static INT call_GetDhcpv6_Options(
    const struct api_args *a,
    char *out,
    size_t cap)
{
    return call_empty_dhcp_options(
        a, out, cap, platform_hal_GetDhcpv6_Options);
}

/* COMMAND GetDhcpv4_Options|?req-pointer ?send-pointer */
static INT call_GetDhcpv4_Options(
    const struct api_args *a,
    char *out,
    size_t cap)
{
    return call_empty_dhcp_options(
        a, out, cap, platform_hal_GetDhcpv4_Options);
}

/* COMMAND SetLowPowerModeState|state */
static INT call_SetLowPowerModeState(const struct api_args *a, char *out, size_t cap)
{
    const char *input = arg(a, "state");
    PSM_STATE state;
    (void)out; (void)cap;

    if (input == NULL)
        return BAD_ARGS;
    /* Only the token crosses IPC; the HAL receives a local input pointer. */
    if (strcmp(input, "NULL") == 0)
        return platform_hal_SetLowPowerModeState(NULL);

    /* HAL validates whether the numeric state is supported. */
    NUM("state", INT_MIN, INT_MAX, value);
    state = (PSM_STATE)value;
    return platform_hal_SetLowPowerModeState(&state);
}

#ifdef FEATURE_RDKB_LED_MANAGER
/* COMMAND initLed|?output-pointer */
static INT call_initLed(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    CHAR config_file_name[128] = {0};
    char *output = config_file_name;
    INT rc;
    size_t used = 0;

    /* Tokens select local pointers; no addresses cross IPC. */
    if (mode != NULL) {
        if (strcmp(mode, "NULL") == 0)
            output = NULL;
        else if (strcmp(mode, "BUFFER") != 0)
            return BAD_ARGS;
    }
    rc = platform_hal_initLed(output);
    if (rc != RETURN_OK || output == NULL) return rc;
    if (!append_text(out, cap, &used, "config_file_name: %.*s\n",
                     (int)sizeof(config_file_name), config_file_name))
        return RETURN_ERR;
    return RETURN_OK;
}
#endif /* FEATURE_RDKB_LED_MANAGER */

/* COMMAND setDscp|interfaceType cmd ?pDscpVals */
static INT call_setDscp(const struct api_args *a, char *out, size_t cap)
{
    char *values = arg(a, "pDscpVals");
    (void)out; (void)cap;
    /* Parse numeric inputs; the HAL validates supported enum values. */
    NUM("interfaceType", 0, INT_MAX, interfaceType);
    NUM("cmd", 0, INT_MAX, cmd);
    /* Forward text unchanged for HAL validation; omission means C NULL. */
    return platform_hal_setDscp((WAN_INTERFACE)interfaceType,
                                (TRAFFIC_CNT_COMMAND)cmd, values);
}

/* COMMAND resetDscpCounts|interfaceType */
static INT call_resetDscpCounts(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    NUM("interfaceType", 0, WAN_INTERFACE_MAX, interfaceType);
    return platform_hal_resetDscpCounts((WAN_INTERFACE)interfaceType);
}

/* Read an optional bounded numeric page parameter with a fixed default. */
static int page_number(const struct api_args *a, const char *key,
                       long min, long max, long fallback, long *value)
{
    if (!arg(a, key)) { *value = fallback; return 1; }
    return number(a, key, min, max, value);
}

struct dscp_page {
    size_t elementOffset, elementLimit, clientOffset, clientLimit;
};

/* Serialize a bounded page; never report a truncated list as success. */
static INT dscp_page_text(const DSCP_list_t *list, const struct dscp_page *page,
                          char *out, size_t cap)
{
    size_t used = 0, i, j, end, count;
    if (list->numElements > 64) return RETURN_ERR;
    for (i = 0; i < list->numElements; ++i)
        if (list->DSCP_Element[i].numClients > 256) return RETURN_ERR;
    if (page->elementOffset > list->numElements) return BAD_ARGS;
    count = list->numElements - page->elementOffset;
    if (count > page->elementLimit) count = page->elementLimit;
    end = page->elementOffset + count;
    if (!append_text(out, cap, &used,
                     "numElements: %u\nelementOffset: %zu\nelementCount: %zu\n",
                     list->numElements, page->elementOffset, count)) goto overflow;
    for (i = page->elementOffset; i < end; ++i) {
        const DSCP_Element_t *element = &list->DSCP_Element[i];
        size_t start = page->clientOffset;
        size_t clients;
        if (start > element->numClients) start = element->numClients;
        clients = element->numClients - start;
        if (clients > page->clientLimit) clients = page->clientLimit;
        if (!append_text(out, cap, &used,
                "element[%zu].dscp_value: %u\nelement[%zu].numClients: %u\n"
                "element[%zu].clientOffset: %zu\nelement[%zu].clientCount: %zu\n",
                i, element->dscp_value, i, element->numClients,
                i, start, i, clients)) goto overflow;
        for (j = start; j < start + clients; ++j) {
            const Traffic_client_t *client = &element->Client[j];
            if (!append_text(out, cap, &used,
                    "element[%zu].client[%zu].mac: %.*s\n"
                    "element[%zu].client[%zu].rxBytes: %lu\n"
                    "element[%zu].client[%zu].txBytes: %lu\n",
                    i, j, (int)sizeof(client->mac), client->mac,
                    i, j, (unsigned long)client->rxBytes,
                    i, j, (unsigned long)client->txBytes)) goto overflow;
        }
    }
    return RETURN_OK;
overflow:
    if (cap) out[0] = 0;
    return RETURN_ERR;
}

/* COMMAND getDscpClientList|interfaceType ?output-pointer ?elementOffset ?elementLimit ?clientOffset ?clientLimit */
static INT call_getDscpClientList(const struct api_args *a, char *out, size_t cap)
{
    const char *mode = arg(a, "output-pointer");
    DSCP_list_t *list;
    struct dscp_page page;
    long elementOffset, elementLimit, clientOffset, clientLimit;
    INT rc;
    NUM("interfaceType", 0, WAN_INTERFACE_MAX, interfaceType);
    if (mode && strcmp(mode, "NULL") && strcmp(mode, "BUFFER"))
        return BAD_ARGS;
    if (!page_number(a, "elementOffset", 0, 64, 0, &elementOffset) ||
        !page_number(a, "elementLimit", 1, 64, 1, &elementLimit) ||
        !page_number(a, "clientOffset", 0, 256, 0, &clientOffset) ||
        !page_number(a, "clientLimit", 1, 256, 16, &clientLimit))
        return BAD_ARGS;
    page.elementOffset = (size_t)elementOffset;
    page.elementLimit = (size_t)elementLimit;
    page.clientOffset = (size_t)clientOffset;
    page.clientLimit = (size_t)clientLimit;
    if (mode && !strcmp(mode, "NULL"))
        return platform_hal_getDscpClientList((WAN_INTERFACE)interfaceType, NULL);
    list = calloc(1, sizeof(*list));
    if (!list) return RETURN_ERR;
    rc = platform_hal_getDscpClientList((WAN_INTERFACE)interfaceType, list);
    if (rc == RETURN_OK) rc = dscp_page_text(list, &page, out, cap);
    free(list);
    return rc;
}

/* A MAC address is exactly six hexadecimal octets separated by colons. */
static int hex_digit(unsigned char c)
{
    if (c >= 48 && c <= 57) return c - 48;
    if (c >= 65 && c <= 70) return c - 65 + 10;
    if (c >= 97 && c <= 102) return c - 97 + 10;
    return -1;
}

static int parse_mac(const char *text, UINT8_t mac[6])
{
    size_t i;
    if (!text || strlen(text) != 17) return 0;
    for (i = 0; i < 6; ++i) {
        int high = hex_digit((unsigned char)text[i * 3]);
        int low = hex_digit((unsigned char)text[i * 3 + 1]);
        if (high < 0 || low < 0 || (i < 5 && text[i * 3 + 2] != 58))
            return 0;
        mac[i] = (UINT8_t)((high << 4) | low);
    }
    return 1;
}

/* COMMAND qos_apply|src_mac ip_version src_ip dest_ip dest_port protocol dscp_value ?src_port ?params-pointer */
static INT call_qos_apply(const struct api_args *a, char *out, size_t cap)
{
    hal_network_params_t params = {0};
    const char *src_ip = arg(a, "src_ip");
    const char *dest_ip = arg(a, "dest_ip");
    const char *mode = arg(a, "params");
    long source_port = 0;
    int family;
    void *src, *dest;
    (void)out; (void)cap;
    /* Input pointers are selected locally; addresses never cross IPC. */
    if (mode != NULL && strcmp(mode, "NULL") != 0 &&
        strcmp(mode, "STRUCT") != 0)
        return BAD_ARGS;
    if (mode != NULL && strcmp(mode, "NULL") == 0)
        return platform_hal_qos_apply(NULL);
    if (arg(a, "src_port") != NULL &&
        !number(a, "src_port", 0, 65535, &source_port))
        return BAD_ARGS;
    NUM("ip_version", IP_VERSION_IPV4, IP_VERSION_IPV6, version);
    NUM("dest_port", 0, 65535, port);
    /* The HAL validates supported protocols and the DSCP contract (0–63). */
    NUM("protocol", 0, INT_MAX, protocol);
    NUM("dscp_value", 0, 255, dscp);
    if (!parse_mac(arg(a, "src_mac"), params.src_mac) || !src_ip || !dest_ip)
        return BAD_ARGS;
    params.ip_version = (ip_version_t)version;
    params.protocol = (net_proto_t)protocol;
    params.dscp_value = (UINT8_t)dscp;
    params.dest_port = htons((UINT16_t)port);
    params.src_port = htons((UINT16_t)source_port);
    family = version == IP_VERSION_IPV4 ? AF_INET : AF_INET6;
    src = version == IP_VERSION_IPV4 ? (void *)&params.src_ip.ipv4 :
                                      (void *)params.src_ip.ipv6;
    dest = version == IP_VERSION_IPV4 ? (void *)&params.dest_ip.ipv4 :
                                       (void *)params.dest_ip.ipv6;
    if (inet_pton(family, src_ip, src) != 1 ||
        inet_pton(family, dest_ip, dest) != 1)
        return BAD_ARGS;
    return platform_hal_qos_apply(&params);
}

/* Register adapters with required keys and ?optional keys. */
struct api_entry {
    const char *name;
    const char *options;
    INT (*call)(const struct api_args *,char *,size_t);
};
static const struct api_entry api_entries[] = {
#ifdef FEATURE_RDKB_LED_MANAGER
    {"platform_hal_setLed", "?LedColor ?State ?Interval ?pValue ?led_name ?led_param", call_setLed},
#else
    {"platform_hal_setLed", "?LedColor ?State ?Interval ?pValue", call_setLed},
#endif
    {"platform_hal_getLed", "?output-pointer", call_getLed},
    {"platform_hal_GetDeviceConfigStatus","?output-pointer",call_GetDeviceConfigStatus},
    {"platform_hal_GetTelnetEnable","?output-pointer",call_GetTelnetEnable},
    {"platform_hal_SetTelnetEnable","flag",call_SetTelnetEnable},
    {"platform_hal_GetSSHEnable", "?output-pointer", call_GetSSHEnable},
    {"platform_hal_SetSSHEnable","flag",call_SetSSHEnable},
    {"platform_hal_GetSNMPEnable","?output-pointer",call_GetSNMPEnable},
    {"platform_hal_SetSNMPEnable","pvalue",call_SetSNMPEnable},
    {"platform_hal_GetWebUITimeout","",call_GetWebUITimeout},
    {"platform_hal_SetWebUITimeout","value",call_SetWebUITimeout},
    {"platform_hal_PandMDBInit","",call_PandMDBInit},
    {"platform_hal_DocsisParamsDBInit","",call_DocsisParamsDBInit},
    {"platform_hal_GetModelName","?output-pointer",call_GetModelName},
    {"platform_hal_GetRouterRegion", "?output-pointer", call_GetRouterRegion},
    {"platform_hal_GetSerialNumber", "?output-pointer", call_GetSerialNumber},
    {"platform_hal_GetHardwareVersion", "?output-pointer", call_GetHardwareVersion},
    {"platform_hal_GetSoftwareVersion", "?output-pointer", call_GetSoftwareVersion},
    {"platform_hal_GetBootloaderVersion", "?output-pointer", call_GetBootloaderVersion},
    {"platform_hal_GetFirmwareName", "?output-pointer", call_GetFirmwareName},
    {"platform_hal_GetBaseMacAddress", "?output-pointer", call_GetBaseMacAddress},
    {"platform_hal_GetHardware", "?output-pointer", call_GetHardware},
    {"platform_hal_GetHardware_MemUsed", "?output-pointer", call_GetHardware_MemUsed},
    {"platform_hal_GetHardware_MemFree", "?output-pointer", call_GetHardware_MemFree},
    {"platform_hal_GetTotalMemorySize", "?output-pointer", call_GetTotalMemorySize},
    {"platform_hal_GetUsedMemorySize", "?output-pointer", call_GetUsedMemorySize},
    {"platform_hal_GetFreeMemorySize", "?output-pointer", call_GetFreeMemorySize},
    {"platform_hal_GetFactoryResetCount", "?output-pointer", call_GetFactoryResetCount},
    {"platform_hal_ClearResetCount", "flag", call_ClearResetCount},
    {"platform_hal_getTimeOffSet", "?output-pointer", call_getTimeOffSet},
    {"platform_hal_SetDeviceCodeImageValid", "value", call_SetDeviceCodeImageValid},
    {"platform_hal_getFactoryPartnerId", "?output-pointer", call_getFactoryPartnerId},
    {"platform_hal_getFactoryCmVariant", "?output-pointer", call_getFactoryCmVariant},
    {"platform_hal_setFactoryCmVariant", "value", call_setFactoryCmVariant},
#ifdef FEATURE_RDKB_THERMAL_MANAGER
    {"platform_hal_getFanTemperature","?output-pointer",call_getFanTemperature},
#endif
#ifdef FEATURE_RDKB_THERMAL_MANAGER
    {"platform_hal_getInputCurrent","?output-pointer",call_getInputCurrent},
#endif
#ifdef FEATURE_RDKB_THERMAL_MANAGER
    {"platform_hal_getInputPower","?output-pointer",call_getInputPower},
#endif
    {"platform_hal_SetSNMPOnboardRebootEnable","pvalue",call_SetSNMPOnboardRebootEnable},
    {"platform_hal_getCMTSMac","?output-pointer",call_getCMTSMac},
    {"platform_hal_GetCPUSpeed","?output-pointer",call_GetCPUSpeed},
    {"platform_hal_GetPppUserName","?output-pointer",call_GetPppUserName},
    {"platform_hal_GetPppPassword","?output-pointer",call_GetPppPassword},
    {"platform_hal_qos_apply", "src_mac ip_version src_ip dest_ip dest_port protocol dscp_value ?src_port ?params", call_qos_apply},
    {"platform_hal_GetWebAccessLevel", "userIndex ifIndex ?output-pointer", call_GetWebAccessLevel},
    {"platform_hal_SetWebAccessLevel", "userIndex ifIndex value", call_SetWebAccessLevel},
    {"platform_hal_SetDeviceCodeImageTimeout", "seconds", call_SetDeviceCodeImageTimeout},
    {"platform_hal_GetFirmwareBankInfo", "bankIndex ?output-pointer", call_GetFirmwareBankInfo},
    {"platform_hal_GetInterfaceStats", "?ifname ?output-pointer", call_GetInterfaceStats},
    {"platform_hal_getFanSpeed", "fanIndex", call_getFanSpeed},
    {"platform_hal_getRPM", "fanIndex", call_getRPM},
    {"platform_hal_getRotorLock", "fanIndex", call_getRotorLock},
    {"platform_hal_getFanStatus", "fanIndex", call_getFanStatus},
    {"platform_hal_setFanMaxOverride", "bOverrideFlag fanIndex", call_setFanMaxOverride},
#ifdef FEATURE_RDKB_THERMAL_MANAGER
    {"platform_hal_setFanSpeed", "fanIndex fanSpeed ?output-pointer", call_setFanSpeed},
#endif
#ifdef FEATURE_RDKB_THERMAL_MANAGER
    {"platform_hal_initThermal", "?output-pointer", call_initThermal},
#endif
#ifdef FEATURE_RDKB_THERMAL_MANAGER
    {"platform_hal_LoadThermalConfig", "?output-pointer", call_LoadThermalConfig},
#endif
#ifdef FEATURE_RDKB_THERMAL_MANAGER
    {"platform_hal_getRadioTemperature", "radioIndex ?output-pointer", call_getRadioTemperature},
#endif
#ifdef FEATURE_RDKB_THERMAL_MANAGER
    {"platform_hal_getEcoModeStatus", "radioIndex ?output-pointer", call_getEcoModeStatus},
#endif
    {"platform_hal_GetMACsecEnable", "ethPort ?output-pointer", call_GetMACsecEnable},
    {"platform_hal_GetMACsecOperationalStatus", "ethPort ?output-pointer", call_GetMACsecOperationalStatus},
    {"platform_hal_SetMACsecEnable", "ethPort Flag", call_SetMACsecEnable},
    {"platform_hal_StartMACsec", "ethPort timeoutSec", call_StartMACsec},
    {"platform_hal_StopMACsec", "ethPort", call_StopMACsec},
    {"platform_hal_GetMemoryPaths", "index ?output-pointer", call_GetMemoryPaths},
    {"platform_hal_GetDhcpv6_Options", "?req-pointer ?send-pointer", call_GetDhcpv6_Options},
    {"platform_hal_GetDhcpv4_Options", "?req-pointer ?send-pointer", call_GetDhcpv4_Options},
    {"platform_hal_SetLowPowerModeState", "state", call_SetLowPowerModeState},
#ifdef FEATURE_RDKB_LED_MANAGER
    {"platform_hal_initLed", "?output-pointer", call_initLed},
#endif
    {"platform_hal_setDscp", "interfaceType cmd ?pDscpVals", call_setDscp},
    {"platform_hal_resetDscpCounts", "interfaceType", call_resetDscpCounts},
    {"platform_hal_getDscpClientList", "interfaceType ?output-pointer ?elementOffset ?elementLimit ?clientOffset ?clientLimit", call_getDscpClientList},
};


/* Validate required and optional fields independently of their argv order. */
static int valid_options(const struct api_entry *entry, const struct api_args *a)
{
    const char *cursor = entry->options;
    size_t matched = 0;

    /*
     * QoS NULL-pointer testing supplies only --params=NULL.
     * Structured requests still use the normal required-option checks.
     */
    if (strcmp(entry->name, "platform_hal_qos_apply") == 0) {
        const char *mode = arg(a, "params");

        if (mode != NULL && strcmp(mode, "NULL") == 0)
            return a->count == 1;
    }

    while (*cursor) {
        const char *key;
        size_t length, i;
        int optional = *cursor == 63;
        int found = 0;
        if (optional) ++cursor;
        key = cursor;
        while (*cursor && *cursor != 32) ++cursor;
        length = (size_t)(cursor - key);
        for (i = 0; i < a->count; ++i) {
            if (strlen(a->key[i]) == length && !strncmp(a->key[i], key, length)) {
                found = 1; ++matched; break;
            }
        }
        if (!optional && !found) return 0;
        while (*cursor == 32) ++cursor;
    }
    return matched == a->count;
}

static platform_vl_status_t format_api_result(INT rc, const char *result,
                                               char *response, size_t cap)
{
    size_t used = 0;
    if(rc==BAD_ARGS) return invalid_request(response,cap,"invalid argument value");
    /* Adapter output can contain diagnostics (for example FAN_ERR) even
     * when the HAL fails. Keep that data while preserving the failure status. */
    if(!append_text(response,cap,&used,"status: %d\n",rc) ||
       (rc!=RETURN_OK && !append_text(response,cap,&used,"error: HAL operation failed\n")) ||
       !append_text(response,cap,&used,"%s",result)) {
        snprintf(response,cap,"status: -1\nerror: response too large\n");
        return PLATFORM_VL_HAL_ERROR;
    }
    return rc==RETURN_OK?PLATFORM_VL_OK:PLATFORM_VL_HAL_ERROR;
}

/* The frame owns this mutable command buffer for the duration of dispatch. */
platform_vl_status_t platformhal_dispatch_command(char *command,char *response,size_t cap)
{
    struct api_args args={0};
    const char *name=NULL;
    char *save=NULL,*token;
    const struct api_entry *entry=NULL;
    size_t i,used=0;
    INT rc;
    char result[PLATFORM_VL_MAX_PAYLOAD]={0};
    if (!response || cap == 0) return PLATFORM_VL_INVALID_REQUEST;
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
            /* All HAL APIs use the same --platform-api selector. */
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

            if (strcmp(key, "platform-api") == 0) {
                if (name)
                    return invalid_request(response, cap, "multiple commands");
                name = value;
            } else {
                if (!*key || args.count == ARG_COUNT || arg(&args, key))
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
    if(!strcmp(name,"list")) {
        if(args.count) return invalid_request(response,cap,"list takes no arguments");
        if(!append_text(response,cap,&used,"status: 0\n")) return PLATFORM_VL_HAL_ERROR;
        for(i=0;i<sizeof(api_entries)/sizeof(api_entries[0]);++i)
            if(!append_text(response,cap,&used,"api: %s options: %s\n",api_entries[i].name,api_entries[i].options)) {
                snprintf(response,cap,"status: -1\nerror: response too large\n");
                return PLATFORM_VL_HAL_ERROR;
            }
        return PLATFORM_VL_OK;
    }


    for(i=0;i<sizeof(api_entries)/sizeof(api_entries[0]);++i)
        if(!strcmp(name,api_entries[i].name)) { entry=&api_entries[i]; break; }
    if(!entry) return invalid_request(response,cap,"unknown or disabled API; use --list-apis");
    if (!valid_options(entry, &args))
        return invalid_request(response, cap,
            "missing required or unknown API options; use --list-apis");
    rc=entry->call(&args,result,sizeof(result));
    return format_api_result(rc, result, response, cap);
}


/* Deliberately narrow JSON schema: API-specific literal keys and integer values.
 * No nested objects, strings, booleans, nulls, escapes or extra fields.
 * Parse the entire document before invoking any HAL operation.
 */
static char *json_space(char *cursor)
{
    return cursor + strspn(cursor, " \t\r\n");
}

static int parse_integer_json(char *json, struct api_args *args,
                              const char *const *keys, size_t count)
{
    char *cursor = json_space(json);
    size_t i;
    if (!count || count > ARG_COUNT || *cursor != 123) return 0;
    ++cursor;
    for (i = 0; i < count; ++i) {
        char *key, *end, *value;
        int separator;
        size_t k;
        cursor = json_space(cursor);
        if (*cursor != 34) return 0;
        key = ++cursor;
        end = strchr(cursor, 34);
        if (!end) return 0;
        *end = 0;
        cursor = json_space(end + 1);
        for (k = 0; k < count; ++k)
            if (!strcmp(key, keys[k])) break;
        if (k == count || arg(args, key)) return 0;
        if (*cursor != 58) return 0;
        value = cursor = json_space(cursor + 1);
        if (*cursor == 45) ++cursor;
        if (*cursor < 48 || *cursor > 57) return 0;
        /* JSON forbids leading zeroes; values such as 01 fail at delimiter. */
        if (*cursor == 48) ++cursor;
        else while (*cursor >= 48 && *cursor <= 57) ++cursor;
        end = cursor;
        cursor = json_space(cursor);
        separator = *cursor;
        if (separator != (i + 1 == count ? 125 : 44)) return 0;
        *end = 0;
        /* Normalize JSON negative zero before existing numeric validation. */
        if (value[0] == 45 && value[1] == 48 && end - value == 2) ++value;
        args->key[args->count] = key;
        args->value[args->count++] = value;
        ++cursor;
    }
    return *json_space(cursor) == 0;
}

/*
 * Decode ASCII strings in place. Common JSON escapes are supported;
 * Unicode escapes and raw non-ASCII bytes are deliberately rejected.
 * Decoding cannot expand the original buffer.
 */
static int platform_json_read_string(char **cursor, char **value)
{
    char *src = *cursor;
    char *dst;
    if (*src != '"') return 0;
    *value = dst = ++src;
    while (*src != '"') {
        unsigned char ch = (unsigned char)*src++;
        if (ch < 0x20 || ch >= 0x80) return 0;
        if (ch == '\\') {
            ch = (unsigned char)*src++;
            switch (ch) {
                case '"': case '\\': case '/': break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                default: return 0;
            }
        }
        *dst++ = (char)ch;
    }
    ++src;
    *dst = '\0';
    *cursor = src;
    return 1;
}

/* Exactly one required string field; pointers remain within mutable json. */
static int platform_parse_string_json(
    char *json, struct api_args *args, const char *expected_key)
{
    struct api_args parsed = {0};
    char *cursor, *key, *value;
    if (!json || !args || !expected_key) return 0;
    cursor = json_space(json);
    if (*cursor != '{') return 0;
    cursor = json_space(cursor + 1);
    if (!platform_json_read_string(&cursor, &key)) return 0;
    if (strcmp(key, expected_key) != 0) return 0;
    cursor = json_space(cursor);
    if (*cursor != ':') return 0;
    cursor = json_space(cursor + 1);
    if (!platform_json_read_string(&cursor, &value)) return 0;
    cursor = json_space(cursor);
    if (*cursor != '}') return 0;
    if (*json_space(cursor + 1) != '\0') return 0;
    parsed.key[0] = key;
    parsed.value[0] = value;
    parsed.count = 1;
    *args = parsed;
    return 1;
}

/* Required integer interfaceType/cmd and optional ASCII string pDscpVals. */
static int platform_parse_dscp_json(char *json, struct api_args *args)
{
    struct api_args parsed = {0};
    char *cursor = json_space(json);
    if (*cursor != '{') return 0;
    cursor = json_space(cursor + 1);
    for (;;) {
        char *key, *value, *end;
        int separator;
        if (parsed.count >= 3 || parsed.count >= ARG_COUNT) return 0;
        if (!platform_json_read_string(&cursor, &key)) return 0;
        if (strcmp(key, "interfaceType") && strcmp(key, "cmd") &&
            strcmp(key, "pDscpVals")) return 0;
        if (arg(&parsed, key)) return 0;
        cursor = json_space(cursor);
        if (*cursor != ':') return 0;
        cursor = json_space(cursor + 1);
        if (!strcmp(key, "pDscpVals")) {
            if (!platform_json_read_string(&cursor, &value)) return 0;
            cursor = json_space(cursor);
            separator = *cursor;
        } else {
            value = cursor;
            if (*cursor == '-') ++cursor;
            if (*cursor < '0' || *cursor > '9') return 0;
            if (*cursor == '0') ++cursor;
            else while (*cursor >= '0' && *cursor <= '9') ++cursor;
            end = cursor;
            cursor = json_space(cursor);
            separator = *cursor;
            if (separator != ',' && separator != '}') return 0;
            *end = '\0';
            if (value[0] == '-' && value[1] == '0' && end - value == 2)
                ++value;
        }
        parsed.key[parsed.count] = key;
        parsed.value[parsed.count++] = value;
        if (separator == '}') {
            if (*json_space(cursor + 1) != '\0') return 0;
            if (!arg(&parsed, "interfaceType") || !arg(&parsed, "cmd"))
                return 0;
            *args = parsed;
            return 1;
        }
        if (separator != ',') return 0;
        cursor = json_space(cursor + 1);
    }
}

/* Flat QoS object; src_port is optional, all other fields are required. */
static int platform_parse_qos_json(char *json, struct api_args *args)
{
    const char *const keys[] = {
        "src_mac", "src_ip", "dest_ip", "ip_version",
        "dest_port", "protocol", "dscp_value", "src_port"
    };
    struct api_args parsed = {0};
    char *cursor = json_space(json);
    size_t k;
    if (*cursor != '{') return 0;
    cursor = json_space(cursor + 1);
    for (;;) {
        char *key, *value, *end;
        int separator;
        if (parsed.count >= 8 || parsed.count >= ARG_COUNT) return 0;
        if (!platform_json_read_string(&cursor, &key)) return 0;
        for (k = 0; k < 8; ++k)
            if (!strcmp(key, keys[k])) break;
        if (k == 8 || arg(&parsed, key)) return 0;
        cursor = json_space(cursor);
        if (*cursor != ':') return 0;
        cursor = json_space(cursor + 1);
        if (k < 3) {
            if (!platform_json_read_string(&cursor, &value)) return 0;
            cursor = json_space(cursor);
            separator = *cursor;
        } else {
            value = cursor;
            if (*cursor == '-') ++cursor;
            if (*cursor < '0' || *cursor > '9') return 0;
            if (*cursor == '0') ++cursor;
            else while (*cursor >= '0' && *cursor <= '9') ++cursor;
            end = cursor;
            cursor = json_space(cursor);
            separator = *cursor;
            if (separator != ',' && separator != '}') return 0;
            *end = '\0';
            if (value[0] == '-' && value[1] == '0' && end - value == 2)
                ++value;
        }
        parsed.key[parsed.count] = key;
        parsed.value[parsed.count++] = value;
        if (separator == '}') {
            if (*json_space(cursor + 1) != '\0') return 0;
            for (k = 0; k < 7; ++k)
                if (!arg(&parsed, keys[k])) return 0;
            *args = parsed;
            return 1;
        }
        if (separator != ',') return 0;
        cursor = json_space(cursor + 1);
    }
}

/* PUBLIC_INTERFACE */
platform_vl_status_t platformhal_dispatch_json(char *request, char *response,
                                              size_t cap)
{
    struct api_args args = {0};
    char result[PLATFORM_VL_MAX_PAYLOAD] = {0};
    char json_scratch[PLATFORM_VL_MAX_PAYLOAD];
    char *json;
    INT rc;
    if (!response || cap == 0) return PLATFORM_VL_INVALID_REQUEST;
    if (!request || !(json = strchr(request, 10)))
        return invalid_request(response, cap, "missing JSON API envelope");
    *json++ = 0;
    if (!strcmp(request, "platform_hal_setLed")) {
        const char *const keys[] = {"LedColor", "State", "Interval"};
        if (!parse_integer_json(json, &args, keys, 3))
            return invalid_request(response, cap, "expected JSON integer fields LedColor, State, Interval");
        rc = call_setLed(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_SetTelnetEnable")) {
        const char *const keys[] = {"flag"};
        if (!parse_integer_json(json, &args, keys, 1))
            return invalid_request(response, cap, "expected JSON integer field flag");
        rc = call_SetTelnetEnable(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_SetSSHEnable")) {
        const char *const keys[] = {"flag"};
        if (!parse_integer_json(json, &args, keys, 1))
            return invalid_request(response, cap, "expected JSON integer field flag");
        rc = call_SetSSHEnable(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_SetWebUITimeout")) {
        const char *const keys[] = {"value"};
        if (!parse_integer_json(json, &args, keys, 1))
            return invalid_request(response, cap, "expected JSON integer field value");
        rc = call_SetWebUITimeout(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_SetWebAccessLevel")) {
        const char *const keys[] = {"userIndex", "ifIndex", "value"};
        if (!parse_integer_json(json, &args, keys, 3))
            return invalid_request(response, cap, "expected JSON integer fields userIndex, ifIndex, value");
        rc = call_SetWebAccessLevel(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_ClearResetCount")) {
        const char *const keys[] = {"flag"};
        if (!parse_integer_json(json, &args, keys, 1))
            return invalid_request(response, cap, "expected JSON integer field flag");
        rc = call_ClearResetCount(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_setFanMaxOverride")) {
        const char *const keys[] = {"bOverrideFlag", "fanIndex"};
        if (!parse_integer_json(json, &args, keys, 2))
            return invalid_request(response, cap, "expected JSON integer fields bOverrideFlag, fanIndex");
        rc = call_setFanMaxOverride(&args, result, sizeof(result));
#ifdef FEATURE_RDKB_THERMAL_MANAGER
    } else if (!strcmp(request, "platform_hal_setFanSpeed")) {
        const char *const keys[] = {"fanIndex", "fanSpeed"};
        if (!parse_integer_json(json, &args, keys, 2))
            return invalid_request(response, cap, "expected JSON integer fields fanIndex, fanSpeed; use named parameters for output-pointer tests");
        rc = call_setFanSpeed(&args, result, sizeof(result));
#endif
    } else if (!strcmp(request, "platform_hal_setDscp")) {
        size_t length = strlen(json);
        if (length >= sizeof(json_scratch))
            return invalid_request(response, cap, "JSON payload is too large");
        memcpy(json_scratch, json, length + 1);
        if (!platform_parse_dscp_json(json_scratch, &args))
            return invalid_request(response, cap, "expected integer interfaceType, cmd and optional string pDscpVals; omit pDscpVals for NULL");
        rc = call_setDscp(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_resetDscpCounts")) {
        const char *const keys[] = {"interfaceType"};
        if (!parse_integer_json(json, &args, keys, 1))
            return invalid_request(response, cap, "expected JSON integer field interfaceType");
        rc = call_resetDscpCounts(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_qos_apply")) {
        size_t length = strlen(json);
        if (length >= sizeof(json_scratch))
            return invalid_request(response, cap, "JSON payload is too large");
        memcpy(json_scratch, json, length + 1);
        if (!platform_parse_qos_json(json_scratch, &args))
            return invalid_request(response, cap, "expected flat QoS JSON fields: string src_mac, src_ip, dest_ip; integer ip_version, dest_port, protocol, dscp_value; optional integer src_port; use --params=NULL for NULL input");
        rc = call_qos_apply(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_SetMACsecEnable")) {
        const char *const keys[] = {"ethPort", "Flag"};
        if (!parse_integer_json(json, &args, keys, 2))
            return invalid_request(response, cap, "expected JSON integer fields ethPort, Flag");
        rc = call_SetMACsecEnable(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_StartMACsec")) {
        const char *const keys[] = {"ethPort", "timeoutSec"};
        if (!parse_integer_json(json, &args, keys, 2))
            return invalid_request(response, cap, "expected JSON integer fields ethPort, timeoutSec");
        rc = call_StartMACsec(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_StopMACsec")) {
        const char *const keys[] = {"ethPort"};
        if (!parse_integer_json(json, &args, keys, 1))
            return invalid_request(response, cap, "expected JSON integer field ethPort");
        rc = call_StopMACsec(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_SetLowPowerModeState")) {
        const char *const keys[] = {"state"};
        if (!parse_integer_json(json, &args, keys, 1))
            return invalid_request(response, cap, "expected JSON integer field state");
        rc = call_SetLowPowerModeState(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_SetDeviceCodeImageTimeout")) {
        const char *const keys[] = {"seconds"};
        if (!parse_integer_json(json, &args, keys, 1))
            return invalid_request(response, cap, "expected JSON integer field seconds");
        rc = call_SetDeviceCodeImageTimeout(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_SetDeviceCodeImageValid")) {
        const char *const keys[] = {"value"};
        if (!parse_integer_json(json, &args, keys, 1))
            return invalid_request(response, cap, "expected JSON integer field value");
        rc = call_SetDeviceCodeImageValid(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_SetSNMPEnable")) {
        size_t length = strlen(json);
        if (length >= sizeof(json_scratch))
            return invalid_request(response, cap, "JSON payload is too large");
        memcpy(json_scratch, json, length + 1);
        if (!platform_parse_string_json(json_scratch, &args, "pvalue"))
            return invalid_request(response, cap, "expected exactly one JSON string field pvalue");
        if (strcmp(arg(&args, "pvalue"), "NULL") == 0)
            return invalid_request(response, cap, "use --pvalue=NULL for NULL input");
        rc = call_SetSNMPEnable(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_SetSNMPOnboardRebootEnable")) {
        size_t length = strlen(json);
        if (length >= sizeof(json_scratch))
            return invalid_request(response, cap, "JSON payload is too large");
        memcpy(json_scratch, json, length + 1);
        if (!platform_parse_string_json(json_scratch, &args, "pvalue"))
            return invalid_request(response, cap, "expected exactly one JSON string field pvalue");
        rc = call_SetSNMPOnboardRebootEnable(&args, result, sizeof(result));
    } else if (!strcmp(request, "platform_hal_setFactoryCmVariant")) {
        size_t length = strlen(json);
        if (length >= sizeof(json_scratch))
            return invalid_request(response, cap, "JSON payload is too large");
        memcpy(json_scratch, json, length + 1);
        if (!platform_parse_string_json(json_scratch, &args, "value"))
            return invalid_request(response, cap, "expected exactly one JSON string field value");
        if (strcmp(arg(&args, "value"), "NULL") == 0)
            return invalid_request(response, cap, "use --value=NULL for NULL input");
        rc = call_setFactoryCmVariant(&args, result, sizeof(result));
    } else {
        return invalid_request(response, cap, "JSON is not supported for this API");
    }
    return format_api_result(rc, result, response, cap);
}
