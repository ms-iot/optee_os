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
 * environment (i.e u-boot, OPTEE, or UEFI).
 */

#if defined(CONFIG_CYREP_UBOOT_BUILD)
#include <common.h>

#define CYREP_PLATFORM_ASSERT(...) \
    assert(__VA_ARGS__)

#define CYREP_PLATFORM_TRACE_ERROR(...)\
    printf("ERROR: " __VA_ARGS__)

#elif defined(CONFIG_CYREP_OPTEE_BUILD)
#include <types_ext.h>
#include <string.h>
#include <assert.h>

#define CYREP_PLATFORM_TRACE_ERROR(...)\
    EMSG(__VA_ARGS__)

#define CYREP_PLATFORM_ASSERT(...) \
    assert(__VA_ARGS__)

#elif defined(CONFIG_CYREP_UEFI_BUILD)
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <Library/DebugLib.h>

#define CYREP_PLATFORM_ASSERT(...) \
    ASSERT(__VA_ARGS__)

#define CYREP_PLATFORM_TRACE_ERROR(...) \
    DEBUG((DEBUG_ERROR, __VA_ARGS__))

#endif

#define CYREP_ASSERT(...) \
    do { \
        CYREP_PLATFORM_ASSERT (__VA_ARGS__); \
    } while (0)

#define CYREP_INTERNAL_ERROR(...) \
    do { \
        CYREP_PLATFORM_TRACE_ERROR("Cyrep Internal Error: "__VA_ARGS__); \
    } while(0)

#endif // __CYREP_COMMON_H__