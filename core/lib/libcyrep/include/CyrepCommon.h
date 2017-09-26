/** @file
*
*  Copyright (c) Microsoft Corporation. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/
#ifndef __CYREP_COMMON_H__
#define __CYREP_COMMON_H__

/*
 * Define the appropriate compile time CONFIG_CYREP_* based on the target
 * environment (i.e u-boot, OPTEE, or UEFI) which are the currently supported
 * targets.
 */

#if defined(CONFIG_CYREP_UBOOT_BUILD)
#include <common.h>
#elif defined(CONFIG_CYREP_OPTEE_BUILD)
#include <types_ext.h>
#include <string.h>
#include <assert.h>
#elif defined(CONFIG_CYREP_UEFI_BUILD)
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#endif

#endif // __CYREP_COMMON_H__