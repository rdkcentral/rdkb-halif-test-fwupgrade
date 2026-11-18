/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
/**!
* @brief The fwupgrade_hal.h header file defines the RDK Firmware Upgrade HAL, providing functions to:
* 
* Configure: Set download URL, filename, and network interface.
* Download: Initiate, monitor, and control firmware downloads.
* Reboot: Manage device reboots after successful downloads or updates.
* Advanced Operations: Support firmware updates with factory reset and direct download/install.
*/

#ifndef __FWUPGRADE_HAL_H__
#define __FWUPGRADE_HAL_H__

/**
 * @defgroup FWUPGRADE_HAL FWUPGRADE HAL
 *
 * @defgroup FWUPGRADE_HAL_TYPES   FWUPGRADE HAL Data Types
 * @ingroup  FWUPGRADE_HAL
 * 
 * @defgroup FWUPGRADE_HAL_APIS   FWUPGRADE HAL APIs
 * @ingroup  FWUPGRADE_HAL
 *
 **/


/**
* @addtogroup FWUPGRADE_HAL_TYPES
* @{
*/
#include <sys/time.h>

/**********************************************************************
               CONSTANT DEFINITIONS
**********************************************************************/
#ifdef __cplusplus
extern "C"{
#endif

/*
* TODO: The following macros are expected to be deprecated in future versions of the interface.
*/

#ifndef CHAR
#define CHAR  char
#endif

#ifndef UCHAR
#define UCHAR unsigned char
#endif

#ifndef BOOLEAN
#define BOOLEAN  unsigned char
#endif

#ifndef USHORT
#define USHORT  unsigned short
#endif

#ifndef UINT8
#define UINT8 unsigned char
#endif

#ifndef INT
#define INT   int
#endif

#ifndef UINT
#define UINT  unsigned int
#endif

#ifndef LONG
#define LONG	long
#endif

#ifndef ULONG
#define ULONG unsigned long
#endif

#ifndef TRUE
#define TRUE     1
#endif

#ifndef FALSE
#define FALSE    0
#endif

#ifndef ENABLE
#define ENABLE   1
#endif

#ifndef RETURN_OK
#define RETURN_OK   0
#endif

#ifndef RETURN_ERR
#define RETURN_ERR   -1
#endif

/** 
* @} 
*/

/**
 * @addtogroup FWUPGRADE_HAL_APIS
 * @{
 */

/**********************************************************************************
 *
 *  Subsystem level function prototypes
 *
**********************************************************************************/

/*
 * TODO (Error Handling Enhancement):
 *   - Replace generic `RETURN_ERR` with an enumerated error code type for improved clarity and debugging.
 *   - Define specific error codes to pinpoint various failure scenarios, including:
 *       - Invalid input parameters (e.g., null pointers, out-of-range values)
 *       - Resource allocation failures (e.g., out-of-memory)
 *       - Communication or timeout issues with external systems
 *       - File system errors (e.g., file not found, permission denied)
 *       - Hardware-specific errors (e.g., SIM card not present, eUICC malfunction)
 *       - Internal module errors 
 *   - Document the new error codes thoroughly in the header file and any relevant guides.
 */

/**!
* @brief Sets the URL and filename for a firmware download.
*
* This function configures the URL from which the firmware image will be downloaded and the local filename to store it.
*
* @param[in] pUrl - The URL of the firmware image 
* @param[in] pfilename - The desired filename for the downloaded firmware image. 
*
* @returns Status of the operation:
* @retval RETURN_OK - On success.
* @retval RETURN_ERR - On failure (e.g., null pointers, failure to open configuration file).
*
* @note  Refer to relevant documentation or guidelines for specific filename format requirements and any additional error scenarios.
*/
INT fwupgrade_hal_set_download_url (char* pUrl, char* pfilename);

/**!
* @brief Retrieves the firmware download URL and filename.
*
* This function populates the provided buffers with the URL and filename of the firmware to be downloaded, as retrieved from the configuration.
*
* @param[out] pUrl         Buffer to store the firmware download URL (min. 1024 bytes).
* @param[out] pfilename    Buffer to store the firmware filename (min. 256 bytes).
*
* @returns Status of the operation:
* @retval RETURN_OK - On success.
* @retval RETURN_ERR - On failure (e.g., null pointers, configuration errors). 
*
* @note See relevant documentation for details on the expected filename format and potential error conditions. 
*/
INT fwupgrade_hal_get_download_url (char *pUrl, char* pfilename);

/**!
* @brief Configures the network interface for firmware downloads.
*
* This function sets the network interface used for downloading firmware updates over HTTP.
*
* @param[in] interface - Network interface identifier:
*                         - 0: wan0
*                         - 1: erouter0
*
* @returns Status of the operation:
* @retval RETURN_OK - On success.
* @retval RETURN_ERR - On failure (e.g., invalid interface value, configuration file error).
*/
INT fwupgrade_hal_set_download_interface (unsigned int interface);

/**!
* @brief Retrieves the configured firmware download interface.
*
* This function fetches the network interface identifier used for firmware downloads.
*
* @param[out] pinterface - Pointer to an unsigned integer to store the interface value:
*                         - 0: wan0
*                         - 1: erouter0
*
* @returns Status of the operation:
* @retval RETURN_OK - On success.
* @retval RETURN_ERR - On failure (e.g., null pointer, configuration file error).
*/
INT fwupgrade_hal_get_download_interface (unsigned int* pinterface);

/**!
* @brief Initiates firmware download and installation.
* 
* This function fetches the firmware image from the configured URL, validates it, and performs the installation process. 
*
* @returns Status of the operation:
* @retval RETURN_OK - On successful download and installation of the firmware.
* @retval RETURN_ERR - On general failure (see potential causes below).
* @retval 400 - Invalid URL or failure to resolve the hostname.
*
* **Potential Causes of RETURN_ERR:**
*   - Failure to retrieve the HTTP download URL or filename.
*   - Error downloading the image to the CPE.
*   - Failure executing the OEM-specific firmware flasher.
*   - Error setting the boot state to the new image.
*/
INT fwupgrade_hal_download ();

/**!
* @brief Retrieves the firmware (FW) download status.
*
* @returns The firmware download status:
* @retval 0 - Download not started.
* @retval 1-99 - Percentage of download completed.
* @retval 100 - Download complete, pending reboot.
* @retval 400 - Invalid HTTP server URL.
* @retval 401 - Cannot connect to HTTP server.
* @retval 402 - File not found on HTTP server.
* @retval 403 - HW Type DL Protection Failure.
* @retval 404 - HW Mask DL Protection Failure.
* @retval 405 - DL Rev Protection Failure.
* @retval 406 - DL Header Protection Failure.
* @retval 407 - DL CVC Failure.
* @retval 500 - General Download Failure.
*/
INT fwupgrade_hal_get_download_status ();


/**!
* @brief Retrieves the reboot readiness status.
*
* This function checks if the system is ready to be rebooted after a firmware update.
*
* @param[out] pValue - Pointer to an unsigned long variable where the status will be stored:
*              - 1: Ready for reboot.
*              - 2: Not ready for reboot.
*
* @returns Status of the operation:
* @retval RETURN_OK - On success.
* @retval RETURN_ERR - On failure (e.g., null pointer).
*/
INT fwupgrade_hal_reboot_ready (ULONG *pValue);

/**!
* @brief Triggers an immediate reboot to apply downloaded firmware.
* 
* @returns Status of the reboot operation:
* @retval RETURN_OK - On success.
* @retval RETURN_ERR - If another reboot is already in progress or an error occurred.
*/
INT fwupgrade_hal_download_reboot_now ();

/**!
 * @brief Performs a firmware update and factory reset on the device.
 *
 * This function initiates a firmware update process and, upon successful completion, 
 * performs a factory reset to restore the device to its default settings.
 *
 * @returns Status of the update and reset operation.
 * @retval RETURN_OK - On success.
 * @retval RETURN_ERR - On failure (e.g., download error, reboot in progress).
 */
INT fwupgrade_hal_update_and_factoryreset ();

/**!
* @brief Downloads and installs a firmware update from the specified URL.
*
* This function fetches the firmware image from the provided URL and initiates the installation process.
*
* @param[in] url - URL of the firmware image
*
* @returns Status of the firmware download and upgrade operation.
* @retval RETURN_OK - On success.
* @retval RETURN_ERR - On failure (e.g., server unreachable, invalid URL, download or installation error).
*/
INT fwupgrade_hal_download_install(const char *url);

#ifdef __cplusplus
}
#endif
#endif

/**
 * @}
 */
