/*
 * fwupgrade_hal.c
 *
 * Stateful FWUpgrade HAL stand-in (libfwupgrade_hal.so)
 *
 * Implements only the public fwupgrade_hal.h API, so it can be replaced by the
 * development team's libfwupgrade_hal.so without any test-side change.
 */

#include "fwupgrade_hal.h"

#include <string.h>

/*--------------------------------------------------
 * Constants
 *-------------------------------------------------*/

#define MAX_URL_LEN          1024
#define MAX_FILENAME_LEN      256

#define DOWNLOAD_NOT_STARTED    0
#define DOWNLOAD_COMPLETED    100

/*--------------------------------------------------
 * Firmware State Machine
 *-------------------------------------------------*/

typedef enum
{
    FW_STATE_IDLE = 0,
    FW_STATE_CONFIGURED,
    FW_STATE_DOWNLOADING,
    FW_STATE_VALIDATING,
    FW_STATE_INSTALLED,
    FW_STATE_REBOOT_READY,
    FW_STATE_ERROR
} fw_state_t;

/*--------------------------------------------------
 * Stored URL Configuration
 *-------------------------------------------------*/

/* Power-on defaults: a configured URL/filename pair and interface 0. */
static char g_url[MAX_URL_LEN] = "https://fwserver/image.wic";
static char g_filename[MAX_FILENAME_LEN] = "image.wic";

static BOOLEAN g_url_configured = TRUE;
static unsigned int g_interface = 0;

/*--------------------------------------------------
 * Direct Install State
 *-------------------------------------------------*/

static char g_direct_install_url[MAX_URL_LEN];
static BOOLEAN g_direct_install_active = FALSE;

/*--------------------------------------------------
 * Firmware State
 *-------------------------------------------------*/

static fw_state_t g_fw_state = FW_STATE_IDLE;

static INT g_download_status = DOWNLOAD_NOT_STARTED;

static BOOLEAN g_reboot_in_progress = FALSE;
static BOOLEAN g_factory_reset_requested = FALSE;

/*==================================================
 * Internal Helpers
 *=================================================*/

static void fw_reset_upgrade_state(void)
{
    g_fw_state = FW_STATE_CONFIGURED;

    g_download_status = DOWNLOAD_NOT_STARTED;

    g_reboot_in_progress = FALSE;
    g_factory_reset_requested = FALSE;

    g_direct_install_active = FALSE;
}

/*
 * A direct-install URL must be http(s)://<host>[...] and must not contain
 * whitespace, control, non-ASCII or URL-unsafe characters (< > double-quote
 * backslash ^ ` { | }). This rejects a URL without a host, a blank URL and
 * a URL such as image<1>.wic.
 */
static BOOLEAN fw_is_valid_url(const char *url)
{
    const char *host;
    size_t i;

    if (strncmp(url, "http://", 7) == 0)
    {
        host = url + 7;
    }
    else if (strncmp(url, "https://", 8) == 0)
    {
        host = url + 8;
    }
    else
    {
        return FALSE;
    }

    if ((host[0] == '\0') || (host[0] == '/') ||
        (host[0] == ':') || (host[0] == '?') || (host[0] == '#'))
    {
        return FALSE;
    }

    for (i = 0; url[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)url[i];

        if ((c <= 0x20) || (c >= 0x7F))
        {
            return FALSE;
        }

        switch (c)
        {
            case '<':
            case '>':
            case 0x22: /* double quote */
            case 0x5C: /* backslash */
            case '^':
            case '`':
            case '{':
            case '|':
            case '}':
                return FALSE;

            default:
                break;
        }
    }

    return TRUE;
}

/*
 * The stand-in knows a single firmware server. Any other host is reported as
 * unreachable (download status 401), as a real HAL would on a failed connect.
 */
#define STANDIN_SERVER_HOST "fwserver"

static BOOLEAN fw_is_reachable(const char *url)
{
    const char *scheme = strstr(url, "://");
    const char *host = (scheme != NULL) ? scheme + 3 : url;
    size_t len = strcspn(host, "/:?#");

    return ((len == strlen(STANDIN_SERVER_HOST)) &&
            (strncmp(host, STANDIN_SERVER_HOST, len) == 0)) ? TRUE : FALSE;
}

static INT fwupgrade_run_upgrade(const char *url)
{
    g_fw_state = FW_STATE_DOWNLOADING;

    if (!fw_is_reachable(url))
    {
        g_download_status = 401;
        g_fw_state = FW_STATE_ERROR;
        return RETURN_ERR;
    }

    g_fw_state = FW_STATE_VALIDATING;
    g_download_status = DOWNLOAD_COMPLETED;
    g_fw_state = FW_STATE_REBOOT_READY;

    return RETURN_OK;
}

/*==================================================
 * URL APIs
 *=================================================*/

INT fwupgrade_hal_set_download_url(char *pUrl,
                                   char *pfilename)
{
    size_t url_len;
    size_t filename_len;

    if ((pUrl == NULL) || (pfilename == NULL))
    {
        return RETURN_ERR;
    }

    if ((pUrl[0] == '\0') ||
        (pfilename[0] == '\0'))
    {
        return RETURN_ERR;
    }

    url_len = strlen(pUrl);
    filename_len = strlen(pfilename);

    if (url_len >= MAX_URL_LEN)
    {
        return RETURN_ERR;
    }

    if (filename_len >= MAX_FILENAME_LEN)
    {
        return RETURN_ERR;
    }

    strcpy(g_url, pUrl);
    strcpy(g_filename, pfilename);

    g_url_configured = TRUE;

    fw_reset_upgrade_state();

    return RETURN_OK;
}

INT fwupgrade_hal_get_download_url(char *pUrl,
                                   char *pfilename)
{
    if ((pUrl == NULL) || (pfilename == NULL))
    {
        return RETURN_ERR;
    }

    if (!g_url_configured)
    {
        return RETURN_ERR;
    }

    strcpy(pUrl, g_url);
    strcpy(pfilename, g_filename);

    return RETURN_OK;
}

/*==================================================
 * Interface APIs
 *=================================================*/

INT fwupgrade_hal_set_download_interface(unsigned int interface)
{
    if ((interface != 0) &&
        (interface != 1))
    {
        return RETURN_ERR;
    }

    g_interface = interface;

    fw_reset_upgrade_state();

    return RETURN_OK;
}

INT fwupgrade_hal_get_download_interface(unsigned int *pinterface)
{
    if (pinterface == NULL)
    {
        return RETURN_ERR;
    }

    *pinterface = g_interface;

    return RETURN_OK;
}

/*==================================================
 * Firmware Download APIs
 *=================================================*/

INT fwupgrade_hal_download(void)
{
    if (!g_url_configured)
    {
        return RETURN_ERR;
    }

    /*
     * Reset previous operation state.
     *
     * This ensures:
     * - reboot flag is cleared
     * - factory reset flag is cleared
     * - status is reset
     * - direct-install activity is cleared
     */
    fw_reset_upgrade_state();

    return fwupgrade_run_upgrade(g_url);
}


INT fwupgrade_hal_download_install(const char *url)
{
    if ((url == NULL) ||
        (url[0] == '\0'))
    {
        return RETURN_ERR;
    }

    if (strlen(url) >= MAX_URL_LEN)
    {
        return RETURN_ERR;
    }

    if (!fw_is_valid_url(url))
    {
        return RETURN_ERR;
    }

    /*
     * Keep direct-install URL separate from the
     * configured URL/filename pair.
     */
    strcpy(g_direct_install_url, url);

    /*
     * Reset previous workflow state.
     */
    fw_reset_upgrade_state();

    g_direct_install_active = TRUE;

    return fwupgrade_run_upgrade(url);
}

INT fwupgrade_hal_get_download_status(void)
{
    return g_download_status;
}

/*==================================================
 * Reboot APIs
 *=================================================*/

INT fwupgrade_hal_reboot_ready(ULONG *pValue)
{
    if (pValue == NULL)
    {
        return RETURN_ERR;
    }

    if (g_fw_state == FW_STATE_REBOOT_READY)
    {
        *pValue = 1;
    }
    else
    {
        *pValue = 2;
    }

    return RETURN_OK;
}

INT fwupgrade_hal_download_reboot_now(void)
{
    if (g_reboot_in_progress)
    {
        return RETURN_ERR;
    }

    if (g_fw_state != FW_STATE_REBOOT_READY)
    {
        return RETURN_ERR;
    }

    g_reboot_in_progress = TRUE;

    /*
     * Simulated reboot accepted.
     * Reset firmware state.
     */

    g_fw_state = FW_STATE_IDLE;

    g_download_status = DOWNLOAD_NOT_STARTED;

    return RETURN_OK;
}

/*==================================================
 * Factory Reset API
 *=================================================*/

INT fwupgrade_hal_update_and_factoryreset(void)
{
    INT status;

    /*
     * A factory reset that is already pending must not be requested twice.
     * Checked before the upgrade state below is reset.
     */
    if (g_factory_reset_requested)
    {
        return RETURN_ERR;
    }

    /*
     * Like fwupgrade_hal_download(), the update is performed here from the
     * configured URL; no separate download call is required beforehand.
     */
    if (!g_url_configured)
    {
        return RETURN_ERR;
    }

    fw_reset_upgrade_state();

    status = fwupgrade_run_upgrade(g_url);
    if (status != RETURN_OK)
    {
        return status;
    }

    g_factory_reset_requested = TRUE;

    return RETURN_OK;
}
