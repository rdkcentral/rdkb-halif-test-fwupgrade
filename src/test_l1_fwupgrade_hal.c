/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:*
* Copyright 2016 RDK Management
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
 
/**
* @file test_fwupgrade_hal.c
* @page fwupgrade_hal Level 1 Tests
*
* ## Module's Role
* This module includes Level 1 functional tests (success and failure scenarios).
* This is to ensure that the fwupgrade_hal APIs meet the requirements across all vendors.
*
* **Pre-Conditions:**  None@n
* **Dependencies:** None@n
*
* Ref to API Definition specification documentation : [halSpec.md](../../../docs/halSpec.md)
*/

#include <ut.h>
#include <ut_log.h>
#include "fwupgrade_hal.h"
#include <string.h>
#include <ut_kvp.h>
#include <ut_kvp_profile.h>
#include <stdlib.h>

char *Valid_Image_name;
char *Valid_url;

/**
* @brief This test case is used to verify the functionality of the fwupgrade_hal_set_download_url() API, when valid pUrl and pfilename are passed as input.
*
* The objective of this test case is to verify that the fwupgrade_hal_set_download_url() API successfully sets the download URL and filename with the provided values.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 001
* **Priority:** High
*
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Set download URL and filename | pUrl = "http://dac15cdlserver.ae.ccp.xcal.tv:8080/Images", pfilename = "CGM4140COM_6.2p10s1_PROD_sey.bin" | SUCCESS | The fwupgrade_hal_set_download_url() API should successfully set the download URL and filename |
*/
void test_l1_fwupgrade_hal_positive1_set_download_url(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_positive1_set_download_url...");

    char pUrl[1024];
    char pfilename[256];

    strcpy(pUrl, Valid_url);
    strcpy(pfilename, Valid_Image_name);

    INT result = fwupgrade_hal_set_download_url(pUrl, pfilename);

    UT_LOG("Invoking fwupgrade_hal_set_download_url with valid pUrl and pfilename...");
    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("Result: %d", result);

    UT_ASSERT_EQUAL(result, RETURN_OK);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive1_set_download_url...");
}

/**
 * @brief This test is to verify the functionality of the fwupgrade_hal_set_download_url function.
 *
 * The objective of this test is to check whether the fwupgrade_hal_set_download_url function sets the download URL and filename correctly.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 002
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
 *
 * **Test Procedure:**
 *  | Variation / Step | Description                                    | Test Data                                                  | Expected Result                                                        | Notes                |
 *  | :--------------: | ---------------------------------------------- | --------------------------------------------------------- | ----------------------------------------------------------------------- | ---------------------|
 *  |       01         | Set download URL and filename with minimum length | pUrl = "http://a.com", pfilename = "SR203_a_PROD_sey.bin" | fwupgrade_hal_set_download_url should return RETURN_OK                  | Should be successful |
 */
void test_l1_fwupgrade_hal_positive2_set_download_url(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_positive2_set_download_url...");

    char pUrl[1024];
    char pfilename[256];
    strcpy(pUrl, "http://a.com");
    strcpy(pfilename, "SR203_a_PROD_sey.bin");

    INT result = fwupgrade_hal_set_download_url(pUrl, pfilename);

    UT_LOG("Invoking fwupgrade_hal_set_download_url with minimum length pUrl and pfilename...");
    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("Result: %d", result);

    UT_ASSERT_EQUAL(result, RETURN_OK);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive2_set_download_url...");
}

/**
* @brief This test case verifies the functionality of the fwupgrade_hal_set_download_url API.
*
* The fwupgrade_hal_set_download_url API is called with maximum length pUrl and valid pfilename to test if it sets the download URL correctly.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 003
* **Priority:** High
*
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Invoking fwupgrade_hal_set_download_url with maximum length pUrl and valid pfilename | pUrl = "aaaaaaaaaaaaaaaaaaaaaaaaa...", pfilename = "WNXL11BWL_6.2p10s1_PROD_sey.bin" | Result = RETURN_OK | Should be successful |
*/

void test_l1_fwupgrade_hal_positive3_set_download_url(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_positive3_set_download_url...");

    char pUrl[1024];
    char pfilename[256];
    // Create a pUrl with maximum buffer size
    memset(pUrl, 'a', 1023);
    pUrl[1023] = '\0';
    strcpy(pfilename, Valid_Image_name);

    INT result = fwupgrade_hal_set_download_url(pUrl, pfilename);

    UT_LOG("Invoking fwupgrade_hal_set_download_url with maximum length pUrl and valid pfilename...");
    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("Result: %d", result);

    UT_ASSERT_EQUAL(result, RETURN_OK);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive3_set_download_url...");
}

/**
* @brief Test to validate the behavior of fwupgrade_hal_set_download_url() when the input URL is NULL
*
* This test is designed to check if the fwupgrade_hal_set_download_url() function behaves as expected when the input URL is NULL. The function expects a valid URL and a filename as input parameters and if the URL is NULL, it should return an error.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 004
* **Priority:** High
*
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- | -------------- | ----- |
* | 01 | Setting pUrl to NULL | pUrl = NULL, pfilename = TG4482A_6.2p10s1_PROD_sey.bin | RETURN_ERR | Should return an error |
*/
void test_l1_fwupgrade_hal_negative1_set_download_url(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_negative1_set_download_url...");

    char* pUrl = NULL;
    char pfilename[256];
    strcpy(pfilename, Valid_Image_name);

    INT result = fwupgrade_hal_set_download_url(pUrl, pfilename);

    UT_LOG("Invoking fwupgrade_hal_set_download_url with pUrl set to NULL...");
    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("Result: %d", result);

    UT_ASSERT_EQUAL(result, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative1_set_download_url...");
}

/**
* @brief Test case to validate the negative scenario of setting the download URL.
*
* This test case checks the functionality of the fwupgrade_hal_set_download_url() API when the file name is set to NULL.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 005
* **Priority:** High
*
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If the user chooses to run the test in interactive mode, the test case has to be selected via console. 
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Set download URL with NULL file name | pUrl = "http://dac15cdlserver.ae.ccp.xcal.tv:8080/Images", pfilename = NULL | RETURN_ERR | Should return an error |
*/
void test_l1_fwupgrade_hal_negative2_set_download_url(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_negative2_set_download_url...");

    char pUrl[1024];
    char* pfilename = NULL;
    strcpy(pUrl, Valid_url);

    INT result = fwupgrade_hal_set_download_url(pUrl, pfilename);

    UT_LOG("Invoking fwupgrade_hal_set_download_url with pfilename set to NULL...");
    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("Result: %d", result);

    UT_ASSERT_EQUAL(result, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative2_set_download_url...");
}

/**
 * @brief Test case to verify the behavior of fwupgrade_hal_set_download_url() when both pUrl and pfilename are NULL.
 * 
 * This test case checks if the fwupgrade_hal_set_download_url() function returns an error (RETURN_ERR) correctly when both pUrl and pfilename are NULL.
 * 
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 006
 * **Priority:** High
 * 
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console.
 * 
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- | --------------- | ----- |
 * | 01 | Setting both pUrl and pfilename as NULL | pUrl = NULL, pfilename = NULL | RETURN_ERR | Should return error |
 */

void test_l1_fwupgrade_hal_negative3_set_download_url(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_negative3_set_download_url...");

    char* pUrl = NULL;
    char* pfilename = NULL;

    INT result = fwupgrade_hal_set_download_url(pUrl, pfilename);

    UT_LOG("Invoking fwupgrade_hal_set_download_url with pUrl and pfilename set to NULL...");
    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("Result: %d", result);

    UT_ASSERT_EQUAL(result, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative3_set_download_url...");
}

/**
* @brief Test case to verify the behavior of the fwupgrade_hal_set_download_url function when the download URL exceeds the buffer size limit.
*
* The purpose of this test is to ensure that the fwupgrade_hal_set_download_url function properly handles a download URL string that exceeds the maximum buffer size limit. The function should return an error code in this case.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 007
* **Priority:** High
* 
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If the user chooses to run the test in interactive mode, then the test case has to be selected via the console.
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Set download URL exceeding buffer size limit | pUrl = "a" repeated 1025 times, pfilename = "SR203_6.2p10s1_PROD_sey.bin" | RETURN_ERR | The function should return an error code |
*/
void test_l1_fwupgrade_hal_negative4_set_download_url(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_negative4_set_download_url...");

    char pUrl[1025];
    char pfilename[256];
    memset(pUrl, 'a', 1024);
    pUrl[1024] = '\0';
    strcpy(pfilename, Valid_Image_name);

    INT result = fwupgrade_hal_set_download_url(pUrl, pfilename);

    UT_LOG("Invoking fwupgrade_hal_set_download_url with pUrl exceeding buffer size limit...");
    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("length of purl: %d", strlen(pUrl));
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("Result: %d", result);

    UT_ASSERT_EQUAL(result, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative4_set_download_url...");
}

/**
 * @brief Test case to verify the behavior when setting the download URL with an exceeding buffer size for the filename.
 *
 * The objective of this test is to ensure that the function fwupgrade_hal_set_download_url handles the case when the filename provided exceeds the buffer size limit correctly. It tests the boundary condition where the length of the filename is greater than the maximum allowed limit.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 008
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** If the user chooses to run the test in interactive mode, they need to select this test case via the console.
 *
 * **Test Procedure:**
 * - | Variation / Step | Description                                              | Test Data                                                          | Expected Result                              | Notes              |
 * - | :---------------: | -------------------------------------------------------- | ------------------------------------------------------------------ | -------------------------------------------- | ------------------ |
 * - |       01          | Test setting the download URL with a filename exceeding   | pUrl = "http://dac15cdlserver.ae.ccp.xcal.tv:8080/Images"         | result = fwupgrade_hal_set_download_url(...) | Should be failure |
 *                      | the buffer size limit                                    | pfilename = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" |                                                  |                    |
 */
void test_l1_fwupgrade_hal_negative5_set_download_url(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_negative5_set_download_url...");

    char pUrl[1024];
    char pfilename[257];
    strcpy(pUrl, Valid_url);
    memset(pfilename, 'a', 256);
    pfilename[256] = '\0';

    INT result = fwupgrade_hal_set_download_url(pUrl, pfilename);

    UT_LOG("Invoking fwupgrade_hal_set_download_url with pfilename exceeding buffer size limit...");
    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("Result: %d", result);

    UT_ASSERT_EQUAL(result, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative5_set_download_url...");
}

/**
 * @brief Test case to verify the functionality of fwupgrade_hal_get_download_url API.
 *
 * This test case checks whether the fwupgrade_hal_get_download_url API returns the expected download URL for a given URL and filename.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 009
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Invoke the API with valid buffers for pUrl and pfilename | pUrl = "http://dac15cdlserver.ae.ccp.xcal.tv:8080/Images", pfilename = "CGM4140COM_6.2p10s1_PROD_sey.bin" | Return status should be RETURN_OK | Should be successful |
 */
void test_l1_fwupgrade_hal_positive1_get_download_url(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_positive1_get_download_url...");
    char pUrl[1024];
    char pfilename[256];

    strcpy(pUrl, Valid_url);
    strcpy(pfilename, Valid_Image_name);
    INT result = fwupgrade_hal_set_download_url(pUrl, pfilename);
    // Declare and initialize variables for input parameters
    char pUrl2[1024];
    char pfilename2[256];

    // Invoke the API with valid buffers for pUrl and pfilename
    INT result2 = fwupgrade_hal_get_download_url(pUrl2, pfilename2);

    // Log the test details, output values, and return status
    UT_LOG("Invoking fwupgrade_hal_get_download_url with valid buffers for pUrl and pfilename:");
    UT_LOG("pUrl: %s", pUrl2);
    UT_LOG("pfilename: %s", pfilename2);
    UT_LOG("Return Status: %d", result2);

    // Check the return status
    UT_ASSERT_EQUAL(result2, RETURN_OK);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive1_get_download_url...");
}

/**
 * @brief This function tests the negative scenario of the fwupgrade_hal_get_download_url function.
 *
 * This test verifies the behavior of the fwupgrade_hal_get_download_url function when NULL is passed as the pUrl parameter. 
 * It checks whether the function returns an error status.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 010
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Passing NULL pUrl | pUrl = NULL, pfilename = "TG4482A_6.2p10s1_PROD_sey.bin" | RETURN_ERR | Should return error status |
 */

void test_l1_fwupgrade_hal_negative1_get_download_url(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_negative1_get_download_url...");

    // Declare and initialize variables for input parameters
    char* pUrl = NULL;
    char pfilename[256];

    // Invoke the API with NULL pUrl
    INT result = fwupgrade_hal_get_download_url(pUrl, pfilename);

    // Log the test details, output values, and return status
    UT_LOG("Invoking fwupgrade_hal_get_download_url with NULL pUrl:");
    UT_LOG("pUrl: NULL");
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("Return Status: %d", result);

    // Check the return status
    UT_ASSERT_EQUAL(result, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative1_get_download_url...");
}

/**
* @brief This test case is used to verify the behavior of the fwupgrade_hal_get_download_url function when it is invoked with NULL pfilename.
*
* The test case is designed to check whether the function returns the expected return status when invoked with NULL pfilename. The input is a valid pUrl and NULL pfilename. The expected result is that the function should return RETURN_ERR.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 011
* **Priority:** High
* 
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console.
* 
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Invoke the API with NULL pfilename | pUrl = "http://dac15cdlserver.ae.ccp.xcal.tv:8080/Images", pfilename = NULL | RETURN_ERR | Should be successful |
*/
void test_l1_fwupgrade_hal_negative2_get_download_url(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_negative2_get_download_url...");

    // Declare and initialize variables for input parameters
    char pUrl[1024];
    char* pfilename = NULL;

    // Invoke the API with NULL pfilename
    INT result = fwupgrade_hal_get_download_url(pUrl, pfilename);

    // Log the test details, output values, and return status
    UT_LOG("Invoking fwupgrade_hal_get_download_url with NULL pfilename:");
    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("pfilename: NULL");
    UT_LOG("Return Status: %d", result);

    // Check the return status
    UT_ASSERT_EQUAL(result, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative2_get_download_url...");
}

/**
* @brief This test case is used to test the fwupgrade_hal_get_download_url API when both pUrl and pfilename are NULL.
*
* The objective of this test is to verify the behavior of the fwupgrade_hal_get_download_url API when both pUrl and pfilename parameters are NULL. The expected result is that the API should return the error code RETURN_ERR.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 012
* **Priority:** High
*
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Both pUrl and pfilename are NULL | pUrl: NULL, pfilename: NULL| RETURN_ERR | Should return RETURN_ERR |
*/

void test_l1_fwupgrade_hal_negative3_get_download_url(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_negative3_get_download_url...");

    // Declare and initialize variables for input parameters
    char* pUrl = NULL;
    char* pfilename = NULL;

    // Invoke the API with both pUrl and pfilename NULL
    INT result = fwupgrade_hal_get_download_url(pUrl, pfilename);

    // Log the test details, output values, and return status
    UT_LOG("Invoking fwupgrade_hal_get_download_url with both pUrl and pfilename NULL:");
    UT_LOG("pUrl: NULL");
    UT_LOG("pfilename: NULL");
    UT_LOG("Return Status: %d", result);

    // Check the return status
    UT_ASSERT_EQUAL(result, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative3_get_download_url...");
}

/**
 * @brief This test function verifies the functionality of fwupgrade_hal_set_download_interface with WAN0 interface.
 *
 * Test Group ID: Basic: 01
 * Test Case ID: 013
 * Priority: High
 *
 * Pre-Conditions: None
 * Dependencies: None
 * User Interaction: If user chose to run the test in interactive mode, then the test case has to be selected via console.
 *
 * Test Procedure:
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check fwupgrade_hal_set_download_interface with WAN0 interface | interface = 0 | RETURN_OK (0) | Should be successful |
 */
void test_l1_fwupgrade_hal_positive1_set_download_interface_wan0( void )
{
    UT_LOG("Entering test_l1_fwupgrade_hal_positive1_set_download_interface_wan0...");

    unsigned int interface = 0;

    int status = fwupgrade_hal_set_download_interface(interface);

    UT_LOG("Invoking fwupgrade_hal_set_download_interface with interface = %u", interface);
    UT_LOG("Expected Result: RETURN_OK (0)");
    UT_LOG("Actual Result: %d", status);

    UT_ASSERT_EQUAL(status, RETURN_OK);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive1_set_download_interface_wan0...");
}

/**
 * @brief This test case is used to verify the functionality of the "fwupgrade_hal_set_download_interface" API by setting the download interface for the erouter0.
 *
 * The objective of this test is to ensure that the API is able to set the download interface correctly for the erouter0.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 014
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console.
 *
 * **Test Procedure:**
 * The following test variations will be used:
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- | -------------- | ----- |
 * | 01 | Set download interface for erouter0 | interface = 1 | RETURN_OK (0) | Should be successful |
 */

void test_l1_fwupgrade_hal_positive2_set_download_interface_erouter0( void )
{
    UT_LOG("Entering test_l1_fwupgrade_hal_positive2_set_download_interface_erouter0...");

    unsigned int interface = 1;

    int status = fwupgrade_hal_set_download_interface(interface);

    UT_LOG("Invoking fwupgrade_hal_set_download_interface with interface = %u", interface);
    UT_LOG("Expected Result: RETURN_OK (0)");
    UT_LOG("Actual Result: %d", status);

    UT_ASSERT_EQUAL(status, RETURN_OK);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive2_set_download_interface_erouter0...");
}

/** 
 * @brief Test the negative functionality of fwupgrade_hal_set_download_interface when an invalid interface value is used. 
 * 
 * This test checks the behavior of the fwupgrade_hal_set_download_interface function when an invalid interface value is passed as the argument. The expected behavior is that the function should return the error code RETURN_ERR (-1).
 *
 * **Test Group ID:** Basic (for L1): 01
 * **Test Case ID:** 015
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
 *
 * **Test Procedure:**
 *  | Variation / Step | Description | Test Data | Expected Result | Notes |
 *  | :----: | --------- | ---------- |-------------- | ----- |
 *  | 01 | First set of conditions | interface = 2 | RETURN_ERR (-1) | Should return the error code RETURN_ERR (-1) |
 */
void test_l1_fwupgrade_hal_negative1_set_download_interface_invalid_interface_value( void )
{
    UT_LOG("Entering test_l1_fwupgrade_hal_negative1_set_download_interface_invalid_interface_value...");

    unsigned int interface = 2;

    int status = fwupgrade_hal_set_download_interface(interface);

    UT_LOG("Invoking fwupgrade_hal_set_download_interface with interface = %u", interface);
    UT_LOG("Expected Result: RETURN_ERR (-1)");
    UT_LOG("Actual Result: %d", status);

    UT_ASSERT_EQUAL(status, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative1_set_download_interface_invalid_interface_value...");
}

/**
 * @brief Test case to verify the behavior of fwupgrade_hal_set_download_interface when a very large interface value is passed.
 *
 * This test is to verify the behavior of fwupgrade_hal_set_download_interface function when a very large interface value is passed as an argument. The expected result is that the function should return -1 (RETURN_ERR).
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 016
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console.
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data                           | Expected Result | Notes                 |
 * | :--------------: | ----------- | ----------------------------------- | --------------- | --------------------- |
 * |       01         | First set of conditions | interface = 4294967295    | RETURN_ERR       | Should be unsuccessful |
 *
 */
void test_l1_fwupgrade_hal_negative2_set_download_interface_very_large_interface_value( void )
{
    UT_LOG("Entering test_l1_fwupgrade_hal_negative2_set_download_interface_very_large_interface_value...");

    unsigned int interface = 4294967295;

    int status = fwupgrade_hal_set_download_interface(interface);

    UT_LOG("Invoking fwupgrade_hal_set_download_interface with interface = %u", interface);
    UT_LOG("Expected Result: RETURN_ERR (-1)");
    UT_LOG("Actual Result: %d", status);

    UT_ASSERT_EQUAL(status, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative2_set_download_interface_very_large_interface_value...");
}

/**
* @brief This test case verifies the behavior of fwupgrade_hal_get_download_interface function when called with a valid memory location for pinterface.
*
* The function fwupgrade_hal_get_download_interface is tested to ensure that it returns the expected result when provided with a valid memory location for pinterface.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 017
* **Priority:** High
* 
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- | -------------- | ----- |
* | 01 | Invoking fwupgrade_hal_get_download_interface with valid memory location for pinterface | pinterface = valid memory location | success variable = RETURN_OK | Should be successful |
*/

void test_l1_fwupgrade_hal_positive1_get_download_interface_valid_memory_location(void)
{
    UT_LOG("Entering test_l1_fwupgrade_hal_positive1_get_download_interface_valid_memory_location...");
    unsigned int pinterface;
    
    int result = fwupgrade_hal_get_download_interface(&pinterface);
    
    UT_LOG("Invoking fwupgrade_hal_get_download_interface with valid memory location for pinterface");
    UT_LOG("Returned Value: %d", result);
    
    UT_ASSERT_EQUAL(result, RETURN_OK);
    
    UT_LOG("Exiting test_l1_fwupgrade_hal_positive1_get_download_interface_valid_memory_location...");
}

/**
* @brief Test case to verify the behavior of the fwupgrade_hal_get_download_interface function when pinterface is NULL.
*
* This test case checks the return value of the fwupgrade_hal_get_download_interface function when the pinterface argument is NULL.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 018
* **Priority:** High
*
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Verify behavior when pinterface is NULL | pinterface = NULL | RETURN_ERR | Should return RETURN_ERR |
*/
void test_l1_fwupgrade_hal_negative1_get_download_interface_NULL_pinterface(void)
{
    UT_LOG("Entering test_l1_fwupgrade_hal_negative1_get_download_interface_NULL_pinterface...");

    int result = fwupgrade_hal_get_download_interface(NULL);

    UT_LOG("Invoking fwupgrade_hal_get_download_interface with NULL pinterface");
    UT_LOG("Returned Value: %d", result);

    UT_ASSERT_EQUAL(result, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative1_get_download_interface_NULL_pinterface...");
}

/**
 * @brief This test case is used to verify the successful download of firmware using the fwupgrade_hal_download function.
 *
 * The objective of this test is to ensure that the fwupgrade_hal_download function returns RETURN_OK (0) upon successful firmware download.
 *
 * **Test Group ID:** Basic (for L1): 01
 * **Test Case ID:** 019
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
 *
 * **Test Procedure:**
 * 
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- | -------------- | ----- |
 * | 01 | First set of conditions | - | - | Should be successful |
 */
void test_l1_fwupgrade_hal_positive1_fwupgrade_hal_download_success() {
    UT_LOG("Entering test_l1_fwupgrade_hal_positive1_fwupgrade_hal_download_success...");

    char pUrl[1024];
    char pfilename[256];
    strcpy(pfilename, Valid_Image_name);
    sprintf(pUrl, " %s/Images/%s ", Valid_url, Valid_Image_name);
    
    INT result = fwupgrade_hal_set_download_url(pUrl, pfilename);

    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("Result: %d", result);

    INT status = fwupgrade_hal_download();
    UT_LOG("Invoking fwupgrade_hal_download");

    UT_LOG("Expected Result: RETURN_OK (0)");
    UT_LOG("Return Status: %d", status);
    UT_ASSERT_EQUAL(status, RETURN_OK);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive1_fwupgrade_hal_download_success...");
}

/**
* @brief This function tests the scenario where a firmware upgrade download fails.
*
* This test case checks the behavior of the firmware upgrade API when the download process fails. The download process is expected to return an error status.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 020
* **Priority:** High
*
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console.
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Invoking fwupgrade_hal_download | None | RETURN_ERR (-1) | Should return an error status |
*/
void test_l1_fwupgrade_hal_negative1_fwupgrade_hal_download_error() {
    UT_LOG("Entering test_l1_fwupgrade_hal_negative1_fwupgrade_hal_download_error...");

    char pUrl[1024];
    char pfilename[256];

    sprintf(pUrl, "%s/Images/%s", Valid_url, Valid_Image_name);
    strcpy(pfilename, Valid_Image_name);

    INT result = fwupgrade_hal_set_download_url(pUrl, pfilename);

    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("Result: %d", result);
    
    INT status = fwupgrade_hal_download();
    UT_LOG("Invoking fwupgrade_hal_download");

    UT_LOG("Expected Result: RETURN_ERR (-1)");
    UT_LOG("Return Status: %d", status);
    UT_ASSERT_EQUAL(status, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative1_fwupgrade_hal_download_error...");
}

/**
* @brief Test to verify the behavior of fwupgrade_hal_get_download_status when the download has not started.
*
* This test is to verify the behavior of fwupgrade_hal_get_download_status function when the download has not started. The expected status should be 0. 
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 021
* **Priority:** High
* 
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :---: | --- | --- | --- | --- |
* | 01 | Invoke fwupgrade_hal_get_download_status when the download has not started | None | 0 | Should return 0 |
*/
void test_l1_fwupgrade_hal_positive_1_get_download_status_not_started(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_positive_1_get_download_status_not_started...");

    INT status = fwupgrade_hal_get_download_status();

    UT_LOG("Invoking fwupgrade_hal_get_download_status - Download not started. Expected Status: 0");
    UT_ASSERT_EQUAL(status, 0);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive_1_get_download_status_not_started...");
}

/**
* @brief Test case to check the get_download_status() function when firmware download is in progress.
*
* This test case verifies the behavior of the get_download_status() function when the firmware download is in progress.
*
* **Test Group ID:** Basic: 01 / Module: 02
* **Test Case ID:** 022
* **Priority:** High
*
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console.
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Verify the get_download_status() function when firmware download is in progress | status = 50 | The status returned by get_download_status() should be equal to 50 | Should be successful |
* | 02 | Verify the get_download_status() function when another stage of firmware download is in progress | status = 75 | The status returned by get_download_status() should be equal to 75 | Should be successful |
*/
void test_l1_fwupgrade_hal_positive_2_get_download_status_in_progress(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_positive_2_get_download_status_in_progress...");

    INT status;

    // Simulate firmware download in progress
    // api will return 100 if the download is in progress
    status = 100;

    UT_LOG("Invoking fwupgrade_hal_get_download_status - Download in progress. Expected Status: 100");
    UT_ASSERT_EQUAL(status, fwupgrade_hal_get_download_status());


    UT_LOG("Exiting test_l1_fwupgrade_hal_positive_2_get_download_status_in_progress...");
}

/**
 * @brief This test case verifies the functionality of the fwupgrade_hal_get_download_status API when the download is completed and waiting for reboot.
 *
 * The purpose of this test is to ensure that the fwupgrade_hal_get_download_status API returns the expected status when the download is completed and waiting for reboot.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 023
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** If the user chooses to run the test in interactive mode, then the test case has to be selected via console.
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- | -------------- | ----- |
 * | 01 | Verify the fwupgrade_hal_get_download_status API when the download is completed and waiting for reboot. | None | The API should return the expected status of 200. | This test case checks if the API is returning the expected status when the download is completed and waiting for reboot. |
 */
void test_l1_fwupgrade_hal_positive_3_get_download_status_completed_waiting_for_reboot(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_positive_3_get_download_status_completed_waiting_for_reboot...");
    INT status = fwupgrade_hal_get_download_status();
    UT_LOG("Invoking fwupgrade_hal_get_download_status - Download completed, waiting for reboot. Expected Status: 200");
    UT_ASSERT_EQUAL(status, 200);
    UT_LOG("Exiting test_l1_fwupgrade_hal_positive_3_get_download_status_completed_waiting_for_reboot...");
}

/**
* @brief Test case to verify the reboot readiness of the firmware upgrade HAL.
*
* This test case verifies the functionality of the fwupgrade_hal_reboot_ready() API by checking if the firmware upgrade HAL is ready for reboot.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 024
* **Priority:** High
*
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console.
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Invoking fwupgrade_hal_reboot_ready with pValue: valid memory location | pValue = &value | result = RETURN_OK, *pValue = 1 | Should be successful |
*/

void test_l1_fwupgrade_hal_positive_1_reboot_ready(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_positive_1_reboot_ready...");

    ULONG value = 0;
    INT result = fwupgrade_hal_reboot_ready(&value);

    UT_LOG("Invoking fwupgrade_hal_reboot_ready with pValue: valid memory location");
    UT_LOG("Return value: %d, *pValue: %lu", result, value);

    UT_ASSERT_EQUAL(result, RETURN_OK);
    UT_ASSERT_EQUAL(value, 1);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive_1_reboot_ready...");
}

/**
* @brief This test case is used to verify the functionality of the fwupgrade_hal_reboot_ready API when the reboot is ready.
*
* This test case checks whether the fwupgrade_hal_reboot_ready API returns the expected result and updates the value parameter correctly when the reboot is ready.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 025
* **Priority:** High
* 
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
* 
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Invoking fwupgrade_hal_reboot_ready with pValue: valid memory location | pValue = valid memory location | Result is RETURN_OK, *pValue is 2 | Should be successful |
*/

void test_l1_fwupgrade_hal_positive_2_reboot_ready(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_positive_2_reboot_ready...");

    ULONG value = 0;
    v_secure_system("echo \"Active\" > /tmp/.voice_call_status");
    INT result = fwupgrade_hal_reboot_ready(&value);

    UT_LOG("Invoking fwupgrade_hal_reboot_ready with pValue: valid memory location");
    UT_LOG("Return value: %d, *pValue: %lu", result, value);

    UT_ASSERT_EQUAL(result, RETURN_OK);
    UT_ASSERT_EQUAL(value, 2);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive_2_reboot_ready...");
}

/**
* @brief This test case verifies the behavior of the fwupgrade_hal_reboot_ready function when invoked with a NULL pValue.
*
* This test checks whether the fwupgrade_hal_reboot_ready function returns RETURN_ERR when called with a NULL pValue as input.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 026
* **Priority:** High
*
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console.
*
* **Test Procedure:** 
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Invoking fwupgrade_hal_reboot_ready with NULL pValue | pValue = NULL | RETURN_ERR is returned | Should be unsuccessful |
*/
void test_l1_fwupgrade_hal_negative_1_reboot_ready(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_negative_1_reboot_ready...");

    INT result = fwupgrade_hal_reboot_ready(NULL);

    UT_LOG("Invoking fwupgrade_hal_reboot_ready with NULL pValue");
    UT_LOG("Return value: %d", result);

    UT_ASSERT_EQUAL(result, RETURN_ERR);

    UT_LOG("Exiting test_l1_fwupgrade_hal_negative_1_reboot_ready...");
}

/**
 * @brief Test the fwupgrade_hal_download_reboot_now API.
 *
 * This test case validates the functionality of the fwupgrade_hal_download_reboot_now API by verifying the return value.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 027
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
 *
 * **Test Procedure:**
 * | Variation / Step | Description                                          | Test Data                                                | Expected Result        | Notes            |
 * | :--------------: | ---------------------------------------------------- | -------------------------------------------------------- | ---------------------- | ----------------- |
 * |        01        | Invoke the fwupgrade_hal_download_reboot_now API     |                                                        | RETURN_OK              | Should be successful |
 */
void test_l1_fwupgrade_hal_positive1_fwupgrade_hal_download_reboot_now(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_positive1_fwupgrade_hal_download_reboot_now...");

    // Invoke the fwupgrade_hal_download_reboot_now API
    INT result = fwupgrade_hal_download_reboot_now();

    // Check the return value
    UT_LOG("Invoking fwupgrade_hal_download_reboot_now");

    // Assert the return value
    UT_ASSERT_EQUAL(result, RETURN_OK);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive1_fwupgrade_hal_download_reboot_now...");
}

/**
* @brief This test case checks the functionality of the fwupgrade_hal_update_and_factoryreset API.
*
* The objective of this test is to verify whether the fwupgrade_hal_update_and_factoryreset API is working correctly and returning the anticipated result.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 028
* **Priority:** High
*
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Invoking the API with no parameters | - | RETURN_OK | Should be successful |
*/
void test_l1_fwupgrade_hal_positive1_update_and_factoryreset(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_positive1_update_and_factoryreset...");
    
    char pUrl[1024];
    char pfilename[256];

    sprintf(pUrl, " %s/Images/%s ", Valid_url, Valid_Image_name);
    strcpy(pfilename, Valid_Image_name);
    
    INT status = fwupgrade_hal_set_download_url(pUrl, pfilename);

    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("status: %d", status);

    // Invoke the API being tested
    INT result = fwupgrade_hal_update_and_factoryreset();

    // Check the return value
    UT_ASSERT_EQUAL(result, RETURN_OK);

    // Log the test description, output values, and return status
    UT_LOG("Invoked fwupgrade_hal_update_and_factoryreset with no parameters");
    UT_LOG("Return status: %d", result);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive1_update_and_factoryreset...");
}

/**
* @brief This test case checks the functionality of the fwupgrade_hal_update_and_factoryreset API.
*
* The objective of this test is to verify whether the fwupgrade_hal_update_and_factoryreset API is working correctly and returning the anticipated result.
*
* **Test Group ID:** Basic: 01
* **Test Case ID:** 029
* **Priority:** High
*
* **Pre-Conditions:** None
* **Dependencies:** None
* **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console
*
* **Test Procedure:**
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Invoking the API with no parameters | - | RETURN_OK | Should be successful |
*/
void test_l1_fwupgrade_hal_negative1_update_and_factoryreset(void) {
    UT_LOG("Entering test_l1_fwupgrade_hal_negative1_update_and_factoryreset...");

    char pUrl[1024];
    char pfilename[256];

    sprintf(pUrl, "%s/Images/%s", Valid_url, Valid_Image_name);
    strcpy(pfilename, Valid_Image_name);

    INT result = fwupgrade_hal_set_download_url(pUrl, pfilename);

    UT_LOG("pUrl: %s", pUrl);
    UT_LOG("pfilename: %s", pfilename);
    UT_LOG("Result: %d", result);

    // Invoke the API being tested
    INT status = fwupgrade_hal_update_and_factoryreset();

    // Check the return value
    UT_LOG("Expected Result: RETURN_ERR (-1)");
    UT_LOG("Return Status: %d", status);
    UT_ASSERT_EQUAL(status, RETURN_ERR);


    UT_LOG("Exiting test_l1_fwupgrade_hal_negative1_update_and_factoryreset...");
}


/**
 * @brief Test case for fwupgrade_hal_download_install function.
 *
 * This test case is used to verify the functionality of the fwupgrade_hal_download_install function.
 *
 * **Test Group ID:** Basic: 01
 * **Test Case ID:** 030
 * **Priority:** High
 *
 * **Pre-Conditions:** None
 * **Dependencies:** None
 * **User Interaction:** If user chose to run the test in interactive mode, then the test case has to be selected via console.
 *
 * **Test Procedure:**
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | First set of conditions | url = http://dac15cdlserver.ae.ccp.xcal.tv:8080/Images | Should successfully download and install firmware | Should be successful |
 */

void test_l1_fwupgrade_hal_positive1_fwupgrade_hal_download_install(void)
{
    UT_LOG("Entering test_l1_fwupgrade_hal_positive1_fwupgrade_hal_download_install...");

    const char *url = Valid_url;
    INT result = fwupgrade_hal_download_install(url);

    UT_LOG("Invoking fwupgrade_hal_download_install with input parameters (url: %s)", url);
    UT_ASSERT_EQUAL(result, RETURN_OK);

    UT_LOG("Exiting test_l1_fwupgrade_hal_positive1_fwupgrade_hal_download_install...");
}


static UT_test_suite_t * pSuite = NULL;

/**
 * @brief Register the main tests for this module
 *
 * @return int - 0 on success, otherwise failure
 */
int test_fwupgrade_hal_l1_register(void)
{
    // Create the test suite
    pSuite = UT_add_suite("[L1 fwupgrade_hal]", NULL, NULL);
    if (pSuite == NULL) {
        return -1;
    }

    Valid_Image_name = (char *)malloc(256 * sizeof(char));
    Valid_url = (char *)malloc(1024 * sizeof(char));


    // Get the Image name from user input
    printf("Enter the valid image name: ");
    if (fgets(Valid_Image_name, 256, stdin) != NULL) {
        // Remove newline character if present
        size_t len = strlen(Valid_Image_name);
        if (len > 0 && Valid_Image_name[len-1] == '\n') {
            Valid_Image_name[len-1] = '\0';
        }
    }

    // Get the URL from user input
    printf("Enter the valid URL: ");
    if (fgets(Valid_url, 1024, stdin) != NULL) {
        // Remove newline character if present
        size_t len = strlen(Valid_url);
        if (len > 0 && Valid_url[len-1] == '\n') {
            Valid_url[len-1] = '\0';
        }
    }

    // Print the values to confirm input
    printf("Valid_Image_name: %s\n", Valid_Image_name);
    printf("Valid_url: %s\n", Valid_url);
    // List of test function names and strings
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive1_set_download_url" ,test_l1_fwupgrade_hal_positive1_set_download_url );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive2_set_download_url" ,test_l1_fwupgrade_hal_positive2_set_download_url );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive3_set_download_url" ,test_l1_fwupgrade_hal_positive3_set_download_url );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative1_set_download_url" ,test_l1_fwupgrade_hal_negative1_set_download_url );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative2_set_download_url" ,test_l1_fwupgrade_hal_negative2_set_download_url );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative3_set_download_url" ,test_l1_fwupgrade_hal_negative3_set_download_url );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative4_set_download_url" ,test_l1_fwupgrade_hal_negative4_set_download_url );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative5_set_download_url" ,test_l1_fwupgrade_hal_negative5_set_download_url );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive1_get_download_url" ,test_l1_fwupgrade_hal_positive1_get_download_url );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative1_get_download_url" ,test_l1_fwupgrade_hal_negative1_get_download_url );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative2_get_download_url" ,test_l1_fwupgrade_hal_negative2_get_download_url );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative3_get_download_url" ,test_l1_fwupgrade_hal_negative3_get_download_url );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive1_set_download_interface_wan0" ,test_l1_fwupgrade_hal_positive1_set_download_interface_wan0 );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive2_set_download_interface_erouter0" ,test_l1_fwupgrade_hal_positive2_set_download_interface_erouter0 );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative1_set_download_interface_invalid_interface_value" ,test_l1_fwupgrade_hal_negative1_set_download_interface_invalid_interface_value );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative2_set_download_interface_very_large_interface_value" ,test_l1_fwupgrade_hal_negative2_set_download_interface_very_large_interface_value );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive1_get_download_interface_valid_memory_location" ,test_l1_fwupgrade_hal_positive1_get_download_interface_valid_memory_location );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative1_get_download_interface_NULL_pinterface" ,test_l1_fwupgrade_hal_negative1_get_download_interface_NULL_pinterface );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive_1_get_download_status_not_started" ,test_l1_fwupgrade_hal_positive_1_get_download_status_not_started );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive1_fwupgrade_hal_download_success" ,test_l1_fwupgrade_hal_positive1_fwupgrade_hal_download_success );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive_2_get_download_status_in_progress" ,test_l1_fwupgrade_hal_positive_2_get_download_status_in_progress );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive_3_get_download_status_completed_waiting_for_reboot" ,test_l1_fwupgrade_hal_positive_3_get_download_status_completed_waiting_for_reboot );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative1_fwupgrade_hal_download_error" ,test_l1_fwupgrade_hal_negative1_fwupgrade_hal_download_error );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive_1_reboot_ready" ,test_l1_fwupgrade_hal_positive_1_reboot_ready );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive_2_reboot_ready" ,test_l1_fwupgrade_hal_positive_2_reboot_ready );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative_1_reboot_ready" ,test_l1_fwupgrade_hal_negative_1_reboot_ready );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive1_fwupgrade_hal_download_reboot_now" ,test_l1_fwupgrade_hal_positive1_fwupgrade_hal_download_reboot_now );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive1_update_and_factoryreset" ,test_l1_fwupgrade_hal_positive1_update_and_factoryreset );
    UT_add_test( pSuite, "l1_fwupgrade_hal_negative1_update_and_factoryreset" ,test_l1_fwupgrade_hal_negative1_update_and_factoryreset );
    UT_add_test( pSuite, "l1_fwupgrade_hal_positive1_fwupgrade_hal_download_install" ,test_l1_fwupgrade_hal_positive1_fwupgrade_hal_download_install );
    
    return 0;
}
