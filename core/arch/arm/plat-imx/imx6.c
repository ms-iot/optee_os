// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * All rights reserved.
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

#include <compiler.h>
#include <drivers/gic.h>
#include <io.h>
#include <kernel/generic_boot.h>
#include <kernel/misc.h>
#include <kernel/tz_ssvce_pl310.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <platform_config.h>

register_phys_mem(MEM_AREA_IO_SEC, SRC_BASE, CORE_MMU_DEVICE_SIZE);

/* Bits included in the CFG_TZ_SPI_CONTROLLERS build parameter */
#define TZ_SPI1         0x01
#define TZ_SPI2         0x02
#define TZ_SPI3         0x04
#define TZ_SPI4         0x08
#define TZ_SPI5         0x10
#define TZ_SPI_ALL      (TZ_SPI1 | TZ_SPI2 | TZ_SPI3 | TZ_SPI4 | TZ_SPI5)

/* SPI controllers reserved using CSU_CSL18, CSU_CSL19 and CSU_CSL20 */
#define TZ_CSL18_SPI    TZ_SPI1
#define TZ_CSL19_SPI    (TZ_SPI2 | TZ_SPI3)
#define TZ_CSL20_SPI    (TZ_SPI4 | TZ_SPI5)

#if (CFG_TZ_SPI_CONTROLLERS & (~TZ_SPI_ALL))
    #error "Unsupported CFG_TZ_SPI_CONTROLLERS value"
#endif

struct csu_csl_access_control {
    uint32_t csl_index;
    uint32_t csl_value;
};

/*
 * Peripherals that are not present in the access_control array can be
 * accessed from any execution mode.
 */
static struct csu_csl_access_control access_control[] = {

    /* TZASC1   - CSL16 [7:0] */
    /* TZASC2   - CSL16 [23:16] */
    {16, ((CSU_TZ_SUPERVISOR << 0)  | (CSU_TZ_SUPERVISOR << 16))},

    /* SPDIF    - CSL18 [7:0] */
    /* eCSPI1   - CSL18 [23:16] */
#if     ((CFG_TZ_SPI_CONTROLLERS & TZ_CSL18_SPI) == TZ_SPI1)
    {18, ((CSU_ALL_MODES << 0)      | (CSU_TZ_SUPERVISOR << 16))},
#endif

    /* eCSPI2   - CSL19 [7:0] */
    /* eCSPI3   - CSL19 [23:16] */
#if     ((CFG_TZ_SPI_CONTROLLERS & TZ_CSL19_SPI) == TZ_SPI2)
    {19, ((CSU_TZ_SUPERVISOR << 0)  | (CSU_ALL_MODES << 16))},
#elif   ((CFG_TZ_SPI_CONTROLLERS & TZ_CSL19_SPI) == TZ_SPI3)
    {19, ((CSU_ALL_MODES << 0)      | (CSU_TZ_SUPERVISOR << 16))},
#elif   ((CFG_TZ_SPI_CONTROLLERS & TZ_CSL19_SPI) == (TZ_SPI2 | TZ_SPI3))
    {19, ((CSU_TZ_SUPERVISOR << 0)  | (CSU_TZ_SUPERVISOR << 16))},
#endif

    /* eCSPI4   - CSL20 [7:0] */
    /* eCSPI5   - CSL20 [23:16] */
#if     ((CFG_TZ_SPI_CONTROLLERS & TZ_CSL20_SPI) == TZ_SPI4)
    {20, ((CSU_TZ_SUPERVISOR << 0)  | (CSU_ALL_MODES << 16))},
#elif   ((CFG_TZ_SPI_CONTROLLERS & TZ_CSL20_SPI) == TZ_SPI5)
    {20, ((CSU_ALL_MODES << 0)      | (CSU_TZ_SUPERVISOR << 16))},
#elif   ((CFG_TZ_SPI_CONTROLLERS & TZ_CSL20_SPI) == (TZ_SPI4 | TZ_SPI5))
    {20, ((CSU_TZ_SUPERVISOR << 0)  | (CSU_TZ_SUPERVISOR << 16))},
#endif

};

static uint32_t get_csl_value(uint32_t csl_index __maybe_unused)
{
    uint32_t i;

    /* Check if this csl_index corresponds to any protected peripherals */
    for (i = 0; i < ARRAY_SIZE(access_control); i++) {
        if (access_control[i].csl_index == csl_index) {
            return access_control[i].csl_value;
        }
    }

    return CSU_ACCESS_ALL;
}

void plat_cpu_reset_late(void)
{
	uintptr_t addr;
	uint32_t pa __maybe_unused;

	if (!get_core_pos()) {
		/* primary core */
#if defined(CFG_BOOT_SYNC_CPU)
		pa = virt_to_phys((void *)TEE_TEXT_VA_START);
		/* set secondary entry address and release core */
		write32(pa, SRC_BASE + SRC_GPR1 + 8);
		write32(pa, SRC_BASE + SRC_GPR1 + 16);
		write32(pa, SRC_BASE + SRC_GPR1 + 24);

		write32(SRC_SCR_CPU_ENABLE_ALL, SRC_BASE + SRC_SCR);
#endif

		/* SCU config */
		write32(SCU_INV_CTRL_INIT, SCU_BASE + SCU_INV_SEC);
		write32(SCU_SAC_CTRL_INIT, SCU_BASE + SCU_SAC);
		write32(SCU_NSAC_CTRL_INIT, SCU_BASE + SCU_NSAC);

		/* SCU enable */
		write32(read32(SCU_BASE + SCU_CTRL) | 0x1,
			SCU_BASE + SCU_CTRL);

		/* configure imx6 CSU */

		/* first grant all peripherals */
		for (addr = CSU_BASE + CSU_CSL_START;
			 addr != CSU_BASE + CSU_CSL_END;
			 addr += 4)
			write32(CSU_ACCESS_ALL, addr);

		/* lock the settings */
		for (addr = CSU_BASE + CSU_CSL_START;
			 addr != CSU_BASE + CSU_CSL_END;
			 addr += 4)
			write32(read32(addr) | CSU_SETTING_LOCK, addr);
	}
}
