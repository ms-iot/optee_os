// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Peng Fan <peng.fan@nxp.com>
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
#include <console.h>
#include <drivers/imx_uart.h>
#include <drivers/imx_wdog.h>
#include <io.h>
#include <imx.h>
#include <imx_pm.h>
#include <imx-regs.h>
#include <kernel/generic_boot.h>
#include <kernel/misc.h>
#include <kernel/panic.h>
#include <kernel/pm_stubs.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <stdint.h>
#include <sm/optee_smc.h>
#include <sm/psci.h>
#include <tee/entry_std.h>
#include <tee/entry_fast.h>
#include <atomic.h>
#include "imx_pl310.h"

#ifdef CFG_PL310
#define CORE_IDX_L2CACHE                   0x00100000
#endif

uint32_t active_cores = 1;

int psci_features(uint32_t psci_fid)
{
	switch (psci_fid) {
#ifdef CFG_BOOT_SECONDARY_REQUEST
	case PSCI_CPU_ON:
		return 0;
#endif
	case PSCI_PSCI_FEATURES:
	case PSCI_SYSTEM_OFF:
	case PSCI_SYSTEM_RESET:
	case PSCI_VERSION:
		return 0;

	case PSCI_CPU_SUSPEND:
		return PSCI_OS_INITIATED | PSCI_EXTENDED_STATE_ID;

	default:
		return PSCI_RET_NOT_SUPPORTED;
	}
}

uint32_t psci_version(void)
{
	return PSCI_VERSION_1_0;
}

#ifdef CFG_BOOT_SECONDARY_REQUEST
int psci_cpu_on(uint32_t core_idx, uint32_t entry,
		uint32_t context_id)
{
	uint32_t val;
	vaddr_t va;

#ifdef CFG_PL310
	if (core_idx == CORE_IDX_L2CACHE)
		return l2cache_op(entry);
#endif

	va = core_mmu_get_va(SRC_BASE, MEM_AREA_IO_SEC);
	if (!va)
		EMSG("No SRC mapping\n");

	if ((core_idx == 0) || (core_idx >= CFG_TEE_CORE_NB_CORE))
		return PSCI_RET_INVALID_PARAMETERS;

	atomic_inc32(&active_cores);

	/* set secondary cores' NS entry addresses */
	generic_boot_set_core_ns_entry(core_idx, entry, context_id);

	val = virt_to_phys((void *)TEE_TEXT_VA_START);
	if (soc_is_imx7ds()) {
		write32(val, va + SRC_GPR1_MX7 + core_idx * 8);

		imx_gpcv2_set_core1_pup_by_software();

		/* release secondary core */
		val = read32(va + SRC_A7RCR1);
		val |=  BIT32(SRC_A7RCR1_A7_CORE1_ENABLE_OFFSET +
			      (core_idx - 1));
		write32(val, va + SRC_A7RCR1);

		return PSCI_RET_SUCCESS;
	}

	/* boot secondary cores from OP-TEE load address */
	write32(val, va + SRC_GPR1 + core_idx * 8);

	/* release secondary core */
	val = read32(va + SRC_SCR);
	val |=  BIT32(SRC_SCR_CORE1_ENABLE_OFFSET + (core_idx - 1));
	val |=  BIT32(SRC_SCR_CORE1_RST_OFFSET + (core_idx - 1));
	write32(val, va + SRC_SCR);

	imx_set_src_gpr(core_idx, 0);

	return PSCI_RET_SUCCESS;
}

int psci_cpu_off(void)
{
	uint32_t core_id;

	core_id = get_core_pos();

	DMSG("core_id: %" PRIu32, core_id);

	atomic_dec32(&active_cores);

	psci_armv7_cpu_off();

	imx_set_src_gpr(core_id, UINT32_MAX);

	thread_mask_exceptions(THREAD_EXCP_ALL);

	while (true)
		wfi();

	return PSCI_RET_INTERNAL_FAILURE;
}

int psci_affinity_info(uint32_t affinity,
		       uint32_t lowest_affnity_level __unused)
{
	vaddr_t va = core_mmu_get_va(SRC_BASE, MEM_AREA_IO_SEC);
	vaddr_t gpr5 = core_mmu_get_va(IOMUXC_BASE, MEM_AREA_IO_SEC) +
				       IOMUXC_GPR5_OFFSET;
	uint32_t cpu, val;
	bool wfi;

	cpu = affinity;

	if (soc_is_imx7ds())
		wfi = true;
	else
		wfi = read32(gpr5) & ARM_WFI_STAT_MASK(cpu);

	if ((imx_get_src_gpr(cpu) == 0) || !wfi)
		return PSCI_AFFINITY_LEVEL_ON;

	DMSG("cpu: %" PRIu32 "GPR: %" PRIx32, cpu, imx_get_src_gpr(cpu));
	/*
	 * Wait secondary cpus ready to be killed
	 * TODO: Change to non dead loop
	 */
	if (soc_is_imx7ds()) {
		while (read32(va + SRC_GPR1_MX7 + cpu * 8 + 4) != UINT_MAX)
			;

		val = read32(va + SRC_A7RCR1);
		val &=  ~BIT32(SRC_A7RCR1_A7_CORE1_ENABLE_OFFSET + (cpu - 1));
		write32(val, va + SRC_A7RCR1);
	} else {
		while (read32(va + SRC_GPR1 + cpu * 8 + 4) != UINT32_MAX)
			;

		/* Kill cpu */
		val = read32(va + SRC_SCR);
		val &= ~BIT32(SRC_SCR_CORE1_ENABLE_OFFSET + cpu - 1);
		val |=  BIT32(SRC_SCR_CORE1_RST_OFFSET + cpu - 1);
		write32(val, va + SRC_SCR);
	}

	/* Clean arg */
	imx_set_src_gpr(cpu, 0);

	return PSCI_AFFINITY_LEVEL_OFF;
}
#endif

__weak int imx7_cpu_suspend(uint32_t power_state __unused,
			    uintptr_t entry __unused,
			    uint32_t context_id __unused,
			    struct sm_nsec_ctx *nsec __unused)
{
	return 0;
}

int psci_cpu_suspend(uint32_t power_state __maybe_unused,
		     uintptr_t entry __maybe_unused,
		     uint32_t context_id __maybe_unused,
		     struct sm_nsec_ctx *nsec __maybe_unused)
{
	int ret = PSCI_RET_INVALID_PARAMETERS;

#ifdef CFG_MX7
	ret = imx7_cpu_suspend(power_state, entry, context_id, nsec);
#endif

	return ret;
}

void psci_system_reset(void)
{
	imx_wdog_restart();
}

void __attribute__((noreturn)) psci_system_off(void)
{
	vaddr_t snvs = core_mmu_get_va(SNVS_BASE, MEM_AREA_IO_SEC);
	uint32_t val;

	/* Reset power glitch detector */
	write32(SNVS_LPPGDR_INIT, snvs + SNVS_LPPGDR);
	write32(SNVS_LPSR_PGD, snvs + SNVS_LPSR);

	/* Set dumb power manager mode to 1 and turn off power */
	val = read32(snvs + SNVS_LPCR);
	val |= SNVS_LPCR_DP_EN;
	val |= SNVS_LPCR_TOP;
	write32(val, snvs + SNVS_LPCR);

	/* Wait for the end */
	for (;;) wfi();
}

