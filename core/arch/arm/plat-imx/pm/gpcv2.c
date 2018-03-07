// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2017 NXP
 *
 * Peng Fan <peng.fan@nxp.com>
 */

#include <imx.h>
#include <io.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <stdint.h>

static vaddr_t gpc_base(void)
{
	return core_mmu_get_va(GPC_BASE, MEM_AREA_IO_SEC);
}

void imx_gpcv2_set_core_pgc(bool enable, uint32_t offset)
{
	uint32_t val = read32(gpc_base() + offset) & (~GPC_PGC_PCG_MASK);

	if (enable)
		val |= GPC_PGC_PCG_MASK;

	write32(val, gpc_base() + offset);
}

void imx_gpcv2_set_core1_pdn_by_software(void)
{
	uint32_t val = read32(gpc_base() + GPC_CPU_PGC_SW_PDN_REQ);

	imx_gpcv2_set_core_pgc(true, GPC_PGC_C1);

	val |= GPC_PGC_SW_PDN_PUP_REQ_CORE1_MASK;

	write32(val, gpc_base() + GPC_CPU_PGC_SW_PDN_REQ);

	while ((read32(gpc_base() + GPC_CPU_PGC_SW_PDN_REQ) &
	       GPC_PGC_SW_PDN_PUP_REQ_CORE1_MASK) != 0)
		;

	imx_gpcv2_set_core_pgc(false, GPC_PGC_C1);
}

void imx_gpcv2_set_core1_pup_by_software(void)
{
	uint32_t val = read32(gpc_base() + GPC_CPU_PGC_SW_PUP_REQ);

	imx_gpcv2_set_core_pgc(true, GPC_PGC_C1);

	val |= GPC_PGC_SW_PDN_PUP_REQ_CORE1_MASK;

	write32(val, gpc_base() + GPC_CPU_PGC_SW_PUP_REQ);

	while ((read32(gpc_base() + GPC_CPU_PGC_SW_PUP_REQ) &
	       GPC_PGC_SW_PDN_PUP_REQ_CORE1_MASK) != 0)
		;

	imx_gpcv2_set_core_pgc(false, GPC_PGC_C1);
}

void imx_gpcv2_mask_all_irqs(void)
{
	vaddr_t base = gpc_base();

	write32(~0x0, base + GPC_IMR1_CORE0_A7);
	write32(~0x0, base + GPC_IMR2_CORE0_A7);
	write32(~0x0, base + GPC_IMR3_CORE0_A7);
	write32(~0x0, base + GPC_IMR4_CORE0_A7);

	write32(~0x0, base + GPC_IMR1_CORE1_A7);
	write32(~0x0, base + GPC_IMR2_CORE1_A7);
	write32(~0x0, base + GPC_IMR3_CORE1_A7);
	write32(~0x0, base + GPC_IMR4_CORE1_A7);
}

static void imx_gpcv2_mask_irq_helper(uint32_t irq, bool mask)
{
	uint32_t val;
	vaddr_t base = gpc_base();
	uint32_t idx = (irq - 32) / 32;
	uint32_t irqmask = 1 << (irq % 32);

	val = read32(base + GPC_IMR1_CORE0_A7 + idx * 4);
	if (mask)
		val |= irqmask;
	else
		val &= ~irqmask;

	write32(val, base + GPC_IMR1_CORE0_A7 + idx * 4);
}

void imx_gpcv2_mask_irq(uint32_t irq)
{
	imx_gpcv2_mask_irq_helper(irq, true);
}

void imx_gpcv2_unmask_irq(uint32_t irq)
{
	imx_gpcv2_mask_irq_helper(irq, false);
}

bool imx_gpcv2_irq_pending(uint32_t irq)
{
	vaddr_t base = gpc_base();
	uint32_t idx = (irq - 32) / 32;
	uint32_t mask = 1 << (irq % 32);

	return (read32(base + GPC_ISR1_A7 + idx * 4) & mask) != 0;
}
