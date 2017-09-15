#ifndef __COMMON_INC_H__
#define __COMMON_INC_H__

#define COMPILE_UBOOT 0
#define COMPILE_OPTEE 1
#define COMPILE_UEFI 0

#if COMPILE_UBOOT
#include <common.h>
#elif COMPILE_OPTEE
#include <types_ext.h>
#include <string.h>
#include <assert.h>
#elif COMPILE_UEFI
#endif

#endif // __COMMON_INC_H__