/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2016, 2017 NXP
 *
 *  York Sun <york.sun@nxp.com>
 */

#ifndef __FSL_SEC_IO_H__
#define __FSL_SEC_IO_H__

#include <stdint.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <tee/cache.h>
#include "io.h"

#define ptov(a) ((vaddr_t)phys_to_virt((paddr_t)(a), MEM_AREA_IO_SEC))
#define vtop(a) ((phys_addr_t *)virt_to_phys((void *)(a)))

#ifdef CONFIG_PHYS_64BIT
typedef uint64_t phys_addr_t;
typedef uint64_t phys_size_t;

 // Return higher 32 bits of physical address
#define PHYS_ADDR_HI(phys_addr) \
	    (uint32_t)(((uint64_t)phys_addr) >> 32)

 // Return lower 32 bits of physical address
#define PHYS_ADDR_LO(phys_addr) \
	    (uint32_t)(((uint64_t)phys_addr) & 0xFFFFFFFF)
#else
typedef uint32_t phys_addr_t;
typedef uint32_t phys_size_t;

 // 32 bit address has no PHYS_ADDR_HI component
#define PHYS_ADDR_HI(phys_addr) 0

 // Return lower 32 bits of a 32 bit physical address
#define PHYS_ADDR_LO(phys_addr) phys_addr

#endif

#include "soc.h"

/*
 * These macros are for ARM-based SoCs.
 * Raw IO access is presumed to be in little-endian.
 */
#define out8(a, v)	write8(v, a)
#define out16(a, v)	write16(v, a)
#define out32(a, v)	write32(v, a)
#define out64(a, v)	write32(v, a)
#define in8(a)		read8(a)
#define in16(a)		read16(a)
#define in32(a)		read32(a)
#define in64(a)		read32(a)

#define in_le16(a)	in16(a)
#define in_le32(a)	in32(a)
#define in_le64(a)	in64(a)
#define out_le16(a, v)	out16(a, v)
#define out_le32(a, v)	out32(a, v)
#define out_le64(a, v)	out64(a, v)

#define uswap16(v)	((((v) & 0xff00) >> 8) | (((v) & 0xff) << 8))
#define uswap32(v)	((((v) & 0xff000000) >> 24)	|\
			 (((v) & 0x00ff0000) >> 8)	|\
			 (((v) & 0x0000ff00) << 8)	|\
			 (((v) & 0x000000ff) << 24))
#define uswap64(v)	((((v) & 0xff00000000000000ULL) >> 56)	|\
			 (((v) & 0x00ff000000000000ULL) >> 40)	|\
			 (((v) & 0x0000ff0000000000ULL) >> 24)	|\
			 (((v) & 0x000000ff00000000ULL) >> 8)	|\
			 (((v) & 0x00000000ff000000ULL) << 8)	|\
			 (((v) & 0x0000000000ff0000ULL) << 24)	|\
			 (((v) & 0x000000000000ff00ULL) << 40)	|\
			 (((v) & 0x00000000000000ffULL) << 56))

#define out_be16(a, v)	out16(a, uswap16(v))
#define out_be32(a, v)	out32(a, uswap32(v))
#define out_be64(a, v)	out64(a, uswap64(v))
#define in_be16(a)	uswap16(in16(a))
#define in_be32(a)	uswap32(in32(a))
#define in_be64(a)	uswap64(in64(a))
#define setbits_be32(a, v)	out_be32((a), in_be32(a) | (set))
#define clrbits_be32(a, c)	out_be32((a), in_be32(a) & ~(c))
#define clrsetbits_be32(a, c, s)	out_be32((a), (in_be32(a) & ~(c)) | (s))

#define uart_in		in8
#define uart_out	out8
#define i2c_in		in8
#define i2c_out		out8

#define CONFIG_SYS_FSL_CCSR_GUR_LE
#define CONFIG_SYS_FSL_CCSR_SEC_LE
#define CONFIG_SYS_FSL_CCSR_SFP_LE
#define CONFIG_SYS_FSL_CCSR_ESDHC_LE

#ifdef CONFIG_SYS_FSL_CCSR_GUR_BE
#define gur_in32(a)	in_be32(a)
#define gur_out32(a, v)	out_be32(a, v)
#elif defined(CONFIG_SYS_FSL_CCSR_GUR_LE)
#define gur_in32(a)	in_le32(a)
#define gur_out32(a, v)	out_le32(a, v)
#else
#error Please define CCSR GUR register endianness
#endif

#ifdef CONFIG_SYS_FSL_CCSR_SEC_BE
#define sec_in32(a)	in_be32(a)
#define sec_out32(a, v)	out_be32(a, v)
#define sec_in64(addr)	(((uint64_t)sec_in32((uint32_t *)(addr)) << 32) | \
			(sec_in32((uint32_t *)(addr) + 1)))
#define sec_out64(addr, val) do {				\
	sec_out32((uint32_t *)(addr), (uint32_t)((val) >> 32));	\
	sec_out32((uint32_t *)(addr) + 1, (uint32_t)(val));	\
	} while (0)
#elif defined(CONFIG_SYS_FSL_CCSR_SEC_LE)
#define sec_in32(a)	in_le32(a)
#define sec_out32(a, v)	out_le32(a, v)
#define sec_in64(addr)	(((uint64_t)sec_in32((uint32_t *)(addr) + 1) << 32) | \
			(sec_in32((uint32_t *)(addr))))
#define sec_out64(addr, val) do {					\
	sec_out32((uint32_t *)(addr) + 1, (uint32_t)((val) >> 32));	\
	sec_out32((uint32_t *)(addr), (uint32_t)(val));			\
	} while (0)
#else
#error Please define CCSR SEC register endianness
#endif

#ifdef CONFIG_SYS_FSL_CCSR_SFP_BE
#define sfp_in32(a)	in_be32(a)
#define sfp_out32(a, v)	out_be32(a, v)
#elif defined(CONFIG_SYS_FSL_CCSR_SFP_LE)
#define sfp_in32(a)	in_le32(a)
#define sfp_out32(a, v)	out_le32(a, v)
#else
#error Please define CCSR SFP register endianness
#endif

#ifdef CONFIG_SYS_FSL_CCSR_ESDHC_BE
#define esdhc_in32(a)		in_be32(a)
#define esdhc_out32(a, v)	out_be32(a, v)
#elif defined(CONFIG_SYS_FSL_CCSR_ESDHC_LE)
#define esdhc_in32(a)		in_le32(a)
#define esdhc_out32(a, v)	out_le32(a, v)
#else
#error Please define CCSR ESDHC register endianness
#endif

#endif /* __FSL_SEC_IO_H__ */
