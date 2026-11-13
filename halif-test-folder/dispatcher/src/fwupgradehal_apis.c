#define _POSIX_C_SOURCE 200809L
#include "fwupgradehal_apis.h"
#include "fwupgrade_hal.h"
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Buffer sizes required by fwupgrade_hal.h for the GET URL API. */
#define URL_BUFFER_SIZE 1024
#define FILENAME_BUFFER_SIZE 256

/* Typed HAL objects stay here; only text crosses IPC. */
static fwupgrade_vl_status_t invalid_request(char *response, size_t capacity,
                                             const char *message)
{
    snprintf(response, capacity, "status: -1\nerror: %s\n", message);
    return FWUPGRADE_VL_INVALID_REQUEST;
}

/* Command adapters own every output object; only bounded text crosses IPC. */
#define BAD_ARGS (-3)
#define ARG_COUNT 16
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
 * Optional --output-pointer test token. It selects a real local pointer:
 *   BUFFER (default) valid output object, NULL pointer to the HAL otherwise.
 * No address ever crosses IPC.
 */
enum pointer_mode { MODE_BUFFER, MODE_NULL, MODE_NULL_SECOND, MODE_BAD };

static enum pointer_mode pointer_mode(const struct api_args *a, int allow_second)
{
    const char *mode = arg(a, "output-pointer");
    if (mode == NULL || !strcmp(mode, "BUFFER") || !strcmp(mode, "DEFAULT"))
        return MODE_BUFFER;
    if (!strcmp(mode, "NULL")) return MODE_NULL;
    if (allow_second && !strcmp(mode, "NULL_FILENAME")) return MODE_NULL_SECOND;
    return MODE_BAD;
}

/* COMMAND set_download_url|url filename */
static INT call_set_download_url(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    /* Either argument may be a JSON null, which the HAL must reject. */
    return fwupgrade_hal_set_download_url(arg(a, "url"), arg(a, "filename"));
}

/* COMMAND get_download_url|?output-pointer */
static INT call_get_download_url(const struct api_args *a, char *out, size_t cap)
{
    char url[URL_BUFFER_SIZE] = {0};
    char filename[FILENAME_BUFFER_SIZE] = {0};
    char *url_out = url, *filename_out = filename;
    INT rc;
    switch (pointer_mode(a, 1)) {
    case MODE_BUFFER: break;
    case MODE_NULL: url_out = NULL; break;
    case MODE_NULL_SECOND: filename_out = NULL; break;
    default: return BAD_ARGS;
    }
    rc = fwupgrade_hal_get_download_url(url_out, filename_out);
    if (rc == RETURN_OK)
        snprintf(out, cap, "url: %s\nfilename: %s\n", url, filename);
    return rc;
}

/* COMMAND set_download_interface|interface */
static INT call_set_download_interface(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    NUM("interface", 0, INT_MAX, interface);
    return fwupgrade_hal_set_download_interface((unsigned int)interface);
}

/* COMMAND get_download_interface|?output-pointer */
static INT call_get_download_interface(const struct api_args *a, char *out, size_t cap)
{
    unsigned int value = 0;
    unsigned int *output = &value;
    INT rc;
    switch (pointer_mode(a, 0)) {
    case MODE_BUFFER: break;
    case MODE_NULL: output = NULL; break;
    default: return BAD_ARGS;
    }
    rc = fwupgrade_hal_get_download_interface(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "interface: %u\n", value);
    return rc;
}

/* COMMAND download| */
static INT call_download(const struct api_args *a, char *out, size_t cap)
{
    (void)a; (void)out; (void)cap;
    return fwupgrade_hal_download();
}

/* The HAL returns the download status itself (0..100, 4xx, 500), not RETURN_*. */
/* COMMAND get_download_status| */
static INT call_get_download_status(const struct api_args *a, char *out, size_t cap)
{
    (void)a;
    snprintf(out, cap, "download_status: %d\n", (int)fwupgrade_hal_get_download_status());
    return RETURN_OK;
}

/* COMMAND reboot_ready|?output-pointer */
static INT call_reboot_ready(const struct api_args *a, char *out, size_t cap)
{
    ULONG value = 0;
    ULONG *output = &value;
    INT rc;
    switch (pointer_mode(a, 0)) {
    case MODE_BUFFER: break;
    case MODE_NULL: output = NULL; break;
    default: return BAD_ARGS;
    }
    rc = fwupgrade_hal_reboot_ready(output);
    if (rc == RETURN_OK && output != NULL)
        snprintf(out, cap, "reboot_ready: %lu\n", (unsigned long)value);
    return rc;
}

/* COMMAND download_reboot_now| */
static INT call_download_reboot_now(const struct api_args *a, char *out, size_t cap)
{
    (void)a; (void)out; (void)cap;
    return fwupgrade_hal_download_reboot_now();
}

/* COMMAND update_and_factoryreset| */
static INT call_update_and_factoryreset(const struct api_args *a, char *out, size_t cap)
{
    (void)a; (void)out; (void)cap;
    return fwupgrade_hal_update_and_factoryreset();
}

/* COMMAND download_install|url */
static INT call_download_install(const struct api_args *a, char *out, size_t cap)
{
    (void)out; (void)cap;
    return fwupgrade_hal_download_install(arg(a, "url"));
}

/* Register adapters with required keys and ?optional keys. */
struct api_entry {
    const char *name;
    const char *options;
    INT (*call)(const struct api_args *, char *, size_t);
};
static const struct api_entry api_entries[] = {
    {"fwupgrade_hal_set_download_url", "url filename", call_set_download_url},
    {"fwupgrade_hal_get_download_url", "?output-pointer", call_get_download_url},
    {"fwupgrade_hal_set_download_interface", "interface", call_set_download_interface},
    {"fwupgrade_hal_get_download_interface", "?output-pointer", call_get_download_interface},
    {"fwupgrade_hal_download", "", call_download},
    {"fwupgrade_hal_get_download_status", "", call_get_download_status},
    {"fwupgrade_hal_reboot_ready", "?output-pointer", call_reboot_ready},
    {"fwupgrade_hal_download_reboot_now", "", call_download_reboot_now},
    {"fwupgrade_hal_update_and_factoryreset", "", call_update_and_factoryreset},
    {"fwupgrade_hal_download_install", "url", call_download_install},
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

static fwupgrade_vl_status_t format_api_result(INT rc, const char *result,
                                               char *response, size_t cap)
{
    size_t used = 0;
    if (rc == BAD_ARGS) return invalid_request(response, cap, "invalid argument value");
    /* Non-zero HAL codes (-1, 400, ...) are reported verbatim in "status:". */
    if (!append_text(response, cap, &used, "status: %d\n", rc) ||
        (rc == RETURN_OK && !append_text(response, cap, &used, "%s", result)) ||
        (rc != RETURN_OK && !append_text(response, cap, &used, "error: HAL operation failed\n"))) {
        snprintf(response, cap, "status: -1\nerror: response too large\n");
        return FWUPGRADE_VL_HAL_ERROR;
    }
    return rc == RETURN_OK ? FWUPGRADE_VL_OK : FWUPGRADE_VL_HAL_ERROR;
}

/* The frame owns this mutable command buffer for the duration of dispatch. */
fwupgrade_vl_status_t fwupgradehal_dispatch_command(char *command, char *response, size_t cap)
{
    struct api_args args = {0};
    const char *name = NULL;
    char *save = NULL, *token;
    const struct api_entry *entry = NULL;
    size_t i, used = 0;
    INT rc;
    char result[FWUPGRADE_VL_MAX_PAYLOAD] = {0};
    if (!response || cap == 0) return FWUPGRADE_VL_INVALID_REQUEST;
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
            /* All HAL APIs use the same --fwupgrade-api selector. */
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

            if (strcmp(key, "fwupgrade-api") == 0) {
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
        if (!append_text(response, cap, &used, "status: 0\n")) return FWUPGRADE_VL_HAL_ERROR;
        for (i = 0; i < sizeof(api_entries) / sizeof(api_entries[0]); ++i)
            if (!append_text(response, cap, &used, "api: %s options: %s\n",
                             api_entries[i].name, api_entries[i].options)) {
                snprintf(response, cap, "status: -1\nerror: response too large\n");
                return FWUPGRADE_VL_HAL_ERROR;
            }
        return FWUPGRADE_VL_OK;
    }

    for (i = 0; i < sizeof(api_entries) / sizeof(api_entries[0]); ++i)
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
static const struct json_field set_url_fields[] = {{"url", 's'}, {"filename", 's'}};
static const struct json_field interface_fields[] = {{"interface", 'i'}};
static const struct json_field install_fields[] = {{"url", 's'}};
static const struct json_api json_apis[] = {
    {"fwupgrade_hal_set_download_url", set_url_fields, 2, call_set_download_url,
     "expected JSON string-or-null fields url, filename"},
    {"fwupgrade_hal_set_download_interface", interface_fields, 1, call_set_download_interface,
     "expected JSON integer field interface"},
    {"fwupgrade_hal_download_install", install_fields, 1, call_download_install,
     "expected JSON string-or-null field url"},
};

/* PUBLIC_INTERFACE */
fwupgrade_vl_status_t fwupgradehal_dispatch_json(char *request, char *response, size_t cap)
{
    struct api_args args = {0};
    char result[FWUPGRADE_VL_MAX_PAYLOAD] = {0};
    char *json;
    size_t i;
    INT rc;
    if (!response || cap == 0) return FWUPGRADE_VL_INVALID_REQUEST;
    if (!request || !(json = strchr(request, '\n')))
        return invalid_request(response, cap, "missing JSON API envelope");
    *json++ = 0;
    for (i = 0; i < sizeof(json_apis) / sizeof(json_apis[0]); ++i) {
        const struct json_api *api = &json_apis[i];
        if (strcmp(request, api->name)) continue;
        if (!parse_json(json, &args, api->fields, api->count))
            return invalid_request(response, cap, api->error);
        rc = api->call(&args, result, sizeof(result));
        return format_api_result(rc, result, response, cap);
    }
    return invalid_request(response, cap,
        "JSON is supported only for set_download_url, set_download_interface and download_install");
}

