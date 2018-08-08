/** @file
 *
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *
 *  This program and the accompanying materials
 *  are licensed and made available under the terms and conditions of the BSD
 *License which accompanies this distribution.  The full text of the license may
 *be found at http://opensource.org/licenses/bsd-license.php
 *
 *  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR
 *IMPLIED.
 *
 **/
#include <kernel/interrupt.h>
#include <drivers/rpmsg_ipc.h>
#include <mm/core_memprot.h>
#include <keep.h>
#include <mu-imx.h>
#include <platform_config.h>
#include <assert.h>
#include <io.h>

register_phys_mem(MEM_AREA_IO_SEC, CFG_MCU_FW_TEXT_BASE, CFG_MCU_FW_TEXT_SIZE);
register_phys_mem(MEM_AREA_IO_SEC, CFG_MCU_FW_DATA_BASE, CFG_MCU_FW_DATA_SIZE);
register_phys_mem(MEM_AREA_IO_SEC, CFG_RPMSG_SHMEM_BASE, CFG_RPMSG_SHMEM_SIZE);
register_phys_mem(MEM_AREA_IO_SEC, M4_BOOTROM_BASE, M4_BOOTROM_SIZE);

/* Use TR1/RR1 for IPI kick notifications */
#define RPMSG_IPI_MU_CHANNEL 1

/* MU CR.F0 is used by the M4 to indicate to A7 that it is up & running and that
 * M4 has moved itself to domain0 and that all the M4 app required resource has
 * been assigned to the M4 domain by the M4 app.
 * It is safe after this event for the A7 to lock the M4 reset configurations.
 */
#define M4_MU_READY_FLAG_IDX 0

struct rpmsg_ipc_ctx {
	bool initialized;
	struct rpmsg_ipc_params params;
};

struct rpmsg_ipc_ctx ctx = {.initialized = false};

static struct mu_regs *get_mu(void)
{
	struct mu_regs *mu;
	mu = (struct mu_regs *)core_mmu_get_va(MU_A_BASE, MEM_AREA_IO_SEC);
	if (!mu) {
		EMSG("failed to get VA for MU PA 0x%p", (void *)MU_A_BASE);
	}
	return mu;
}

static enum itr_return __maybe_unused ipi_isr(struct itr_handler *handler)
{
	uint32_t msg;
	uint32_t ch;
	struct mu_regs *mu;

	if (handler->it != IMX_MU_A_IRQn) {
		EMSG("unhandled itr");
		return ITRR_NONE;
	}

	mu = get_mu();
	if (mu_is_rx_full(mu, RPMSG_IPI_MU_CHANNEL)) {
		if (mu_try_receive_msg(mu, RPMSG_IPI_MU_CHANNEL, &msg)) {
			ch = msg >> 16;
			ctx.params.rx_ipi_handler(ch);
		}
	} else {
		EMSG("unhandled mu event");
	}

	return ITRR_HANDLED;
}
KEEP_PAGER(ipi_isr);

struct itr_handler ipi_it_handler = {.it = IMX_MU_A_IRQn, .handler = ipi_isr};

bool rpmsg_ipc_init(struct rpmsg_ipc_params params)
{
	if (ctx.initialized) {
		assert(!"rpmsg_ipc already initialized and an IPI handler is already registered");
		return false;
	}

	assert(params.rx_ipi_handler);
	ctx.params = params;
	ctx.initialized = true;

	mu_init(get_mu());
	itr_add(&ipi_it_handler);

	return true;
}

void rpmsg_ipc_ipi_enable(void)
{
	assert(ctx.initialized);
	itr_enable(IMX_MU_A_IRQn);
}

void rpmsg_ipc_ipi_disable(void)
{
	assert(ctx.initialized);
	itr_disable(IMX_MU_A_IRQn);
}

void rpmsg_ipc_rx_ipi_enable(void)
{
	assert(ctx.initialized);
	mu_enable_rx_full_int(get_mu(), RPMSG_IPI_MU_CHANNEL);
}

void rpmsg_ipc_rx_ipi_disable(void)
{
	assert(ctx.initialized);
	mu_disable_rx_full_int(get_mu(), RPMSG_IPI_MU_CHANNEL);
}

bool rpmsg_ipc_send_msg(uint32_t data)
{
	assert(ctx.initialized);
	if (!mu_send_msg(get_mu(), RPMSG_IPI_MU_CHANNEL, data)) {
		return false;
	}

	return true;
}

bool rpmsg_ipc_boot_remote(void)
{
	vaddr_t m4_text_base;
	vaddr_t m4_bootrom_base;
	vaddr_t src_base;
	uint32_t src_m4rcr;
	uint32_t m4_sp;
	uint32_t m4_pc;

	m4_text_base =
		core_mmu_get_va((paddr_t)CFG_MCU_FW_TEXT_BASE, MEM_AREA_IO_SEC);
	if (!m4_text_base) {
		return false;
	}

	m4_bootrom_base =
		core_mmu_get_va((paddr_t)M4_BOOTROM_BASE, MEM_AREA_IO_SEC);
	if (!m4_bootrom_base) {
		return false;
	}

	src_base = core_mmu_get_va((paddr_t)SRC_BASE, MEM_AREA_IO_SEC);
	if (!src_base) {
		return false;
	}

	/*
	 * this is required because the M4 is not starting from the TCM, but from
	 * the DRAM instead. The M4 expects its starting SP and PC to be at present
	 * as 2 32-bit words at m4_bootrom_base.
	 * The binary format of the M4 fw will have these 2 pieces of information
	 * at the begining of the M4 firmware image.
	 */
	m4_sp = read32((vaddr_t)m4_text_base);
	m4_pc = read32((vaddr_t)m4_text_base + 4);
	write32(m4_sp, (vaddr_t)m4_bootrom_base);
	write32(m4_pc, (vaddr_t)m4_bootrom_base + 4);

	IMSG("booting the M4 core with sp(pa:0x%p):0x%x pc(pa:0x%p):0x%x",
	     (void *)M4_BOOTROM_BASE, m4_sp, (void *)(M4_BOOTROM_BASE + 4),
	     m4_pc);

	/* de-assert M4 RST and enable the M4 core */
	src_m4rcr = read32(src_base + SRC_M4RCR);
	src_m4rcr &= ~SRC_M4RCR_SW_M4C_NON_SCLR_RST;
	src_m4rcr |= SRC_M4RCR_ENABLE_M4;
	write32(src_m4rcr, src_base + SRC_M4RCR);

	return true;
}

bool rpmsg_ipc_wait_remote_ready(uint32_t timeout_ms)
{
	struct mu_regs *mu;

	/* only infinite timeout is supported */
	if (timeout_ms != 0) {
		return false;
	}

	mu = get_mu();

	/* consider timing out based on some counter, ms resolution should be
	 * enough
	 */

	while (!mu_get_flag(mu, M4_MU_READY_FLAG_IDX)) {
	}

	/* acknowledge the ready flag */
	if (!mu_set_flag(mu, M4_MU_READY_FLAG_IDX, true)) {
		return false;
	}
	return true;
}


bool rpmsg_ipc_lock_remote(void)
{
	vaddr_t src_base;
	uint32_t src_m4rcr;

	src_base = core_mmu_get_va((paddr_t)SRC_BASE, MEM_AREA_IO_SEC);
	if (!src_base) {
		return false;
	}

	/* M4 control locking logic is as follows:
	 * - enable domain assignment to M4 reset-control register (M4RCR).
	 * - assign this register to domain1 which is the M4 assigned domain.
	 * - lock M4RCR making its bit unmodifiable, expect after POR.
	 * the effect of this locking is that no once can reset the M4 except
	 * itself.
	 */
	src_m4rcr = read32(src_base + SRC_M4RCR);
	src_m4rcr |= (SRC_M4RCR_DOM_EN | SRC_M4RCR_DOMAIN1 | SRC_M4RCR_LOCK);
	write32(src_m4rcr, src_base + SRC_M4RCR);

	return true;
}
