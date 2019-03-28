// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * Copyright (c) 2016, Wind River Systems.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <arm32.h>
#include <console.h>
#include <drivers/gic.h>
#include <drivers/imx_uart.h>
#include <drivers/tzc380.h>
#include <io.h>
#include <kernel/generic_boot.h>
#include <kernel/misc.h>
#include <kernel/panic.h>
#include <kernel/pm_stubs.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <stdint.h>
#include <sm/optee_smc.h>
#include <tee/entry_fast.h>
#include <tee/entry_std.h>


static void main_fiq(void);
struct gic_data gic_data;

static const struct thread_handlers handlers = {
	.std_smc = tee_entry_std,
	.fast_smc = tee_entry_fast,
	.nintr = main_fiq,
	.cpu_on = pm_panic,
	.cpu_off = pm_panic,
	.cpu_suspend = pm_panic,
	.cpu_resume = pm_panic,
	.system_off = pm_panic,
	.system_reset = pm_panic,
};

static struct imx_uart_data console_data;

#ifdef CONSOLE_UART_BASE
register_phys_mem(MEM_AREA_IO_NSEC, CONSOLE_UART_BASE, CORE_MMU_DEVICE_SIZE);
#endif
#ifdef GIC_BASE
register_phys_mem(MEM_AREA_IO_SEC, GIC_BASE, CORE_MMU_DEVICE_SIZE);
#endif
#ifdef ANATOP_BASE
register_phys_mem(MEM_AREA_IO_SEC, ANATOP_BASE, CORE_MMU_DEVICE_SIZE);
#endif
#ifdef GICD_BASE
register_phys_mem(MEM_AREA_IO_SEC, GICD_BASE, 0x10000);
#endif
#ifdef AIPS1_BASE
register_phys_mem(MEM_AREA_IO_SEC, AIPS1_BASE,
		  ROUNDUP(AIPS1_SIZE, CORE_MMU_DEVICE_SIZE));
#endif
#ifdef AIPS2_BASE
register_phys_mem(MEM_AREA_IO_SEC, AIPS2_BASE,
		  ROUNDUP(AIPS2_SIZE, CORE_MMU_DEVICE_SIZE));
#endif
#ifdef AIPS3_BASE
register_phys_mem(MEM_AREA_IO_SEC, AIPS3_BASE,
		  ROUNDUP(AIPS3_SIZE, CORE_MMU_DEVICE_SIZE));
#endif
#ifdef IRAM_BASE
register_phys_mem(MEM_AREA_TEE_COHERENT,
		  ROUNDDOWN(IRAM_BASE, CORE_MMU_DEVICE_SIZE),
		  CORE_MMU_DEVICE_SIZE);
#endif
#ifdef IRAM_S_BASE
register_phys_mem(MEM_AREA_TEE_COHERENT,
		  ROUNDDOWN(IRAM_S_BASE, CORE_MMU_DEVICE_SIZE),
		  CORE_MMU_DEVICE_SIZE);
#endif

#if defined(CFG_PL310)
register_phys_mem(MEM_AREA_IO_SEC,
		  ROUNDDOWN(PL310_BASE, CORE_MMU_DEVICE_SIZE),
		  CORE_MMU_DEVICE_SIZE);
#endif
#ifdef CFG_TZC380
register_phys_mem(MEM_AREA_IO_SEC,
		  ROUNDDOWN(TZASC_BASE, CORE_MMU_DEVICE_SIZE),
		  CORE_MMU_DEVICE_SIZE);
#endif

#ifdef CAAM_BASE
register_phys_mem(MEM_AREA_IO_SEC, CAAM_BASE, CORE_MMU_DEVICE_SIZE);
#endif

#ifdef OCOTP_BASE
register_phys_mem(MEM_AREA_IO_SEC,
		  ROUNDDOWN(OCOTP_BASE, CORE_MMU_DEVICE_SIZE),
		  CORE_MMU_DEVICE_SIZE);
#endif

#ifdef CCM_BASE
register_phys_mem(MEM_AREA_IO_SEC,
		  ROUNDDOWN(CCM_BASE, CORE_MMU_DEVICE_SIZE),
		  CORE_MMU_DEVICE_SIZE);
#endif

const struct thread_handlers *generic_boot_get_handlers(void)
{
	return &handlers;
}

static void main_fiq(void)
{
	gic_it_handle(&gic_data);
}

void console_init(void)
{
	imx_uart_init(&console_data, CONSOLE_UART_BASE);
	register_serial_console(&console_data.chip);
}

void main_init_gic(void)
{
	vaddr_t gicc_base;
	vaddr_t gicd_base;

	gicc_base = core_mmu_get_va(GIC_BASE + GICC_OFFSET, MEM_AREA_IO_SEC);
	gicd_base = core_mmu_get_va(GIC_BASE + GICD_OFFSET, MEM_AREA_IO_SEC);

	if (!gicc_base || !gicd_base)
		panic();

	/* Initialize GIC */
	gic_init(&gic_data, gicc_base, gicd_base);
	itr_init(&gic_data.chip);
}

#if defined(CFG_MX6Q) || defined(CFG_MX6D) || defined(CFG_MX6DL) || \
	defined(CFG_MX7)
void main_secondary_init_gic(void)
{
	gic_cpu_init(&gic_data);
}
#endif

#ifdef CFG_TZC380
/* Translation from TZC region sizes to their TZC register encoding values */
struct tzc_encoding {
	size_t size_bytes;
	size_t size_encoding;
};

static struct tzc_encoding tzc_encodings[] = {
	{32 * 1024 * 1024,  TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_32M)},
	{16 * 1024 * 1024,  TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_16M)},
	{8 * 1024 * 1024,   TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_8M)},
	{4 * 1024 * 1024,   TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_4M)},
	{2 * 1024 * 1024,   TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_2M)},
	{1 * 1024 * 1024,   TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_1M)},
	{512 * 1024,        TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_512K)},
	{256 * 1024,        TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_256K)},
	{128 * 1024,        TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_128K)},
	{64 * 1024,         TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_64K)},
	{32 * 1024,         TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_32K)},
};

/* Memory areas reserved just for Secure/TZ access */
struct secure_area {
	paddr_t paddr;
	size_t size_bytes;
};

static struct secure_area secure_areas[] = {
#ifdef TZSRAM_BASE
	{TZSRAM_BASE, TZSRAM_SIZE},
#endif
	{TZDRAM_BASE, TZDRAM_SIZE},
};

static void allow_unsecure_readwrite_entire_memory(void)
{
	// Region 0 always includes the entire physical memory.
	tzc_configure_region(0, 0, TZC_ATTR_SP_ALL);
}

/* Deny rich OS access to TZ memory */
static void protect_tz_memory(void)
{
	uint8_t secure_index = 0, tzc_index = 0, encoding_index = 0;
	size_t remaining_bytes = 0, aligned_bytes = 0;
	uint32_t size_encoding = 0;
	paddr_t base = 0;

	/* Enable TZC memory region */
	uint32_t attributes = (TZC_ATTR_REGION_ENABLE << TZC_ATTR_REGION_EN_SHIFT);

	/* Allow just Secure/TZ R+W access, and no access from the rich OS  */
	attributes |= TZC_ATTR_SP_S_RW;

	/* Region 0 is reserved for the entire physical memory, so start from 1 here */
	tzc_index = 1;

	/* Secure all TZ memory areas */
	for (secure_index = 0; secure_index < ARRAY_SIZE(secure_areas); secure_index++) {
		base = secure_areas[secure_index].paddr;
		remaining_bytes = secure_areas[secure_index].size_bytes;

		DMSG("pa 0x%08" PRIxPA " size 0x%08zx needs TZC protection",
				base,
				remaining_bytes);

		/*
		 * If this assert fails, add larger TZC region sizes to the
		 * tzc_encodings array.
		 */
		assert(remaining_bytes <= tzc_encodings[0].size_bytes);

		while (remaining_bytes != 0) {
			/* Find the largest and aligned TZC region size <= remaining_bytes */
			for (encoding_index = 0;
					encoding_index < ARRAY_SIZE(tzc_encodings);
					encoding_index++) {

				aligned_bytes = tzc_encodings[encoding_index].size_bytes;

				if (remaining_bytes >= aligned_bytes) {
					if ((base < aligned_bytes) || ((base % aligned_bytes) != 0)) {
						FMSG("Unaligned pa 0x%08" PRIxPA " size 0x%08zx",
								base,
								aligned_bytes);
						continue;
					}

					/* Found an appropriate TZC region size */
					size_encoding = tzc_encodings[encoding_index].size_encoding;
					break;
				}
			}

			if (encoding_index >= ARRAY_SIZE(tzc_encodings)) {
				EMSG("Unsupported TZ memory size = 0x%08zx, unprotected = 0x%08zx",
						secure_areas[secure_index].size_bytes,
						remaining_bytes);
				panic();
			}

			DMSG("Protecting pa 0x%08" PRIxPA " size 0x%08zx",
					base,
					aligned_bytes);

			tzc_configure_region(tzc_index, base, attributes | size_encoding);

			tzc_index++;
			base += aligned_bytes;
			remaining_bytes -= aligned_bytes;
		}
	}
}

static TEE_Result init_tzc380(void)
{
	void *va;

	va = phys_to_virt(TZASC_BASE, MEM_AREA_IO_SEC);
	if (!va) {
		EMSG("TZASC1 not mapped");
		panic();
	}

	tzc_init((vaddr_t)va);
	tzc_set_action(TZC_ACTION_ERR);

	// Start by allowing both TZ and the normal world to read and write, thus
	// simulating the behavior of systems where the TZASC_ENABLE fuse has not
	// been burnt.
	allow_unsecure_readwrite_entire_memory();

	// Reserve some of the memory regions for Secure (TZ) access only.
	protect_tz_memory();

	return TEE_SUCCESS;
}

service_init(init_tzc380);
#endif // #ifdef CFG_TZC380

