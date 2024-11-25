/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:*
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

#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include "fwupgrade_hal.h"


INT fwupgrade_hal_set_download_url(char* pUrl, char* pfilename)
{
  /*TODO: Implement Me!*/
  (void)pUrl;
  (void)pfilename;
  return (INT)0;
}

INT fwupgrade_hal_get_download_url(char* pUrl, char* pfilename)
{
  /*TODO: Implement Me!*/
  (void)pUrl;
  (void)pfilename;
  return (INT)0;
}

INT fwupgrade_hal_set_download_interface(unsigned int interface)
{
  /*TODO: Implement Me!*/
  (void)interface;
  return (INT)0;
}

INT fwupgrade_hal_get_download_interface(unsigned int* pinterface)
{
  /*TODO: Implement Me!*/
  (void)pinterface;
  return (INT)0;
}

INT fwupgrade_hal_download(void)
{
  /*TODO: Implement Me!*/
  return (INT)0;
}

INT fwupgrade_hal_get_download_status(void)
{
  /*TODO: Implement Me!*/
  return (INT)0;
}

INT fwupgrade_hal_reboot_ready(ULONG* pValue)
{
  /*TODO: Implement Me!*/
  (void)pValue;
  return (INT)0;
}

INT fwupgrade_hal_download_reboot_now(void)
{
  /*TODO: Implement Me!*/
  return (INT)0;
}

INT fwupgrade_hal_update_and_factoryreset(void)
{
  /*TODO: Implement Me!*/
  return (INT)0;
}

INT fwupgrade_hal_download_install(const char* url)
{
  /*TODO: Implement Me!*/
  (void)url;
  return (INT)0;
}

