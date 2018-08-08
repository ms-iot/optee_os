/** @file
 *
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *
 *  This program and the accompanying materials
 *  are licensed and made available under the terms and conditions of the BSD
 *  License which accompanies this distribution.  The full text of the license may
 *  be found at http://opensource.org/licenses/bsd-license.php
 *
 *  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR
 *  IMPLIED.
 *
 **/
#include <io.h>
#include <platform_config.h>
#include <stdint.h>
#include <assert.h>
#include <mu-imx.h>

void mu_init(struct mu_regs *mu)
{
	uint32_t cr;

	cr = read32((vaddr_t)&mu->cr);
	cr &= ~(MU_CR_GIEn_MASK | MU_CR_RIEn_MASK | MU_CR_TIEn_MASK
		| MU_CR_GIRn_MASK | MU_CR_Fn_MASK);
	write32(cr, (vaddr_t)&mu->cr);
}

void mu_enable_rx_full_int(struct mu_regs *mu, uint32_t channel)
{
	uint32_t cr;

	assert(channel < MU_CHANNEL_COUNT);

	cr = read32((vaddr_t)&mu->cr);
	/* Clear General Interrupt request for all channels (GIRn) */
	cr &= ~MU_CR_GIRn_MASK;
	/* Enable Receive Interrupt for channel */
	cr |= MU_CR_RIE(channel);
	write32(cr, (vaddr_t)&mu->cr);
}

void mu_disable_rx_full_int(struct mu_regs *mu, uint32_t channel)
{
	uint32_t cr;

	assert(channel < MU_CHANNEL_COUNT);

	cr = read32((vaddr_t)&mu->cr);

	/* Clear General Interrupt request for all channels (GIRn) */
	cr &= ~MU_CR_GIRn_MASK;
	/* Disable Receive Interrupt for channel */
	cr &= ~MU_CR_RIE(channel);
	write32(cr, (vaddr_t)&mu->cr);
}

bool mu_is_tx_empty(struct mu_regs *mu, uint32_t channel)
{
	uint32_t sr;
	uint32_t mask;

	assert(channel < MU_CHANNEL_COUNT);

	sr = read32((vaddr_t)&mu->sr);
	mask = MU_SR_TE(channel);
	return ((sr & mask) != 0);
}

bool mu_is_rx_full(struct mu_regs *mu, uint32_t channel)
{
	uint32_t sr;
	uint32_t mask;

	assert(channel < MU_CHANNEL_COUNT);

	sr = read32((vaddr_t)&mu->sr);
	mask = MU_SR_RF(channel);
	return ((sr & mask) != 0);
}

bool mu_send_msg(struct mu_regs *mu, uint32_t channel, uint32_t msg)
{
	int timeout = MU_POLL_TIMEOUT;

	assert(channel < MU_CHANNEL_COUNT);

	while ((!mu_is_tx_empty(mu, channel) || mu_is_event_pending(mu))
	       && (timeout > 0)) {
		timeout--;
	}

	if (timeout <= 0) {
		EMSG("timeout waiting on TE/EP");
		return false;
	}

	write32(msg, (vaddr_t)&mu->tr[channel]);

	timeout = MU_POLL_TIMEOUT;
	while (mu_is_event_pending(mu) && (timeout > 0)) {
		timeout--;
	}

	if (timeout <= 0) {
		EMSG("timeout waiting on EP");
		return false;
	}

	return true;
}

bool mu_try_receive_msg(struct mu_regs *mu, uint32_t channel, uint32_t *msg)
{
	assert(channel < MU_CHANNEL_COUNT);
	assert(msg);

	if (!mu_is_rx_full(mu, channel)) {
		return false;
	}

	*msg = read32((vaddr_t)&mu->rr[channel]);
	return true;
}

bool mu_get_flag(struct mu_regs *mu, uint32_t fn)
{
	uint32_t sr;
	assert(fn < MU_SR_Fn_BIT_COUNT);
	sr = read32((vaddr_t)&mu->sr);
	return ((sr & MU_SR_Fn(fn)) != 0);
}

bool mu_set_flag(struct mu_regs *mu, uint32_t fn, bool val)
{
	int timeout = MU_POLL_TIMEOUT;
	uint32_t cr;

	assert(fn < MU_SR_Fn_BIT_COUNT);

	while (mu_is_flags_pending(mu) && (timeout > 0)) {
		timeout--;
	}

	if (timeout <= 0) {
		EMSG("timeout waiting on flags to get unset");
		return false;
	}

	cr = read32((vaddr_t)&mu->cr);
	cr &= ~MU_CR_GIRn_MASK;
	if (val) {
		cr |= MU_SR_Fn(fn);
	} else {
		cr &= ~MU_SR_Fn(fn);
	}
	write32(cr, (vaddr_t)&mu->cr);

	return true;
}

bool mu_is_event_pending(struct mu_regs *mu)
{
	uint32_t sr;
	sr = read32((vaddr_t)&mu->sr);
	return (sr & MU_SR_EP_MASK) != 0;
}

bool mu_is_flags_pending(struct mu_regs *mu)
{
	uint32_t sr;
	sr = read32((vaddr_t)&mu->sr);
	return (sr & MU_SR_FUP_MASK) != 0;
}

void mu_enable_general_int(struct mu_regs *mu, uint32_t idx)
{
	uint32_t cr;
	assert(idx < MU_SR_GIPn_BIT_COUNT);
	cr = read32((vaddr_t)&mu->cr);
	cr &= ~MU_CR_GIEn_MASK;
	cr |= MU_CR_GIEn(idx);
	write32(cr, (vaddr_t)&mu->cr);
}

void mu_disable_general_int(struct mu_regs *mu, uint32_t idx)
{
	uint32_t cr;
	assert(idx < MU_SR_GIPn_BIT_COUNT);
	cr = read32((vaddr_t)&mu->cr);
	cr &= ~(MU_CR_GIRn_MASK | MU_CR_GIEn(idx));
	write32(cr, (vaddr_t)&mu->cr);
}

bool mu_is_general_int_pending(struct mu_regs *mu, uint32_t idx)
{
	uint32_t sr;
	assert(idx < MU_SR_GIPn_BIT_COUNT);
	sr = read32((vaddr_t)&mu->sr);
	return ((sr & MU_SR_GIPn(idx)) != 0);
}

void mu_clear_general_int_pending(struct mu_regs *mu, uint32_t idx)
{
	uint32_t sr;
	assert(idx < MU_SR_GIPn_BIT_COUNT);
	sr = read32((vaddr_t)&mu->sr);
	sr |= MU_SR_GIPn(idx);
	write32(sr, (vaddr_t)&mu->sr);
}