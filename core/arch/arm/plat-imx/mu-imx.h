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
#ifndef __MU_IMX_H
#define __MU_IMX_H

#include <stdint.h>
#include <stdbool.h>

#define MU_CHANNEL_COUNT 4u

#define MU_POLL_TIMEOUT 10000

#define MU_CR_GIEn_SHIFT 28
#define MU_CR_GIEn_MASK (0xF << MU_CR_GIEn_SHIFT)
#define MU_CR_GIEn(IDX) (((0x8 >> (IDX)) << MU_CR_GIEn_SHIFT) & MU_CR_GIEn_MASK)

#define MU_CR_RIEn_SHIFT 24
#define MU_CR_RIEn_MASK (0xF << MU_CR_RIEn_SHIFT)
#define MU_CR_RIE(CH) (((0x8 >> (CH)) << MU_CR_RIEn_SHIFT) & MU_CR_RIEn_MASK)

#define MU_CR_TIEn_SHIFT 20
#define MU_CR_TIEn_MASK (0xF << MU_CR_TIEn_SHIFT)
#define MU_CR_TIE(CH) (((0x8 >> (CH)) << MU_CR_TIEn_SHIFT) & MU_CR_TIEn_MASK)

#define MU_CR_GIRn_SHIFT 16
#define MU_CR_GIRn_MASK (0xF << MU_CR_GIRn_SHIFT)
#define MU_CR_Fn_SHIFT 0
#define MU_CR_Fn_MASK (0x7 << MU_CR_Fn_SHIFT)
#define MU_CR_Fn(IDX) ((0x1 << ((IDX) + MU_CR_Fn_SHIFT) & MU_CR_Fn_MASK)

#define MU_SR_Fn_SHIFT 0
#define MU_SR_Fn_MASK (0x7 << MU_SR_Fn_SHIFT)
#define MU_SR_Fn(IDX) ((0x1 << ((IDX) + MU_SR_Fn_SHIFT)) & MU_SR_Fn_MASK)
#define MU_SR_Fn_BIT_COUNT 3

#define MU_SR_TEn_SHIFT 20
#define MU_SR_TEn_MASK (0xF << MU_SR_TEn_SHIFT)
#define MU_SR_TE(CH) (((0x8 >> (CH)) << MU_SR_TEn_SHIFT) & MU_SR_TEn_MASK)

#define MU_SR_RFn_SHIFT 24
#define MU_SR_RFn_MASK (0xF << MU_SR_RFn_SHIFT)
#define MU_SR_RF(CH) (((0x8 >> (CH)) << MU_SR_RFn_SHIFT) & MU_SR_RFn_MASK)

#define MU_SR_GIPn_SHIFT 28
#define MU_SR_GIPn_MASK (0xF << MU_SR_GIPn_SHIFT)
#define MU_SR_GIPn(IDX) (((0x8 >> (IDX)) << MU_SR_GIPn_SHIFT) & MU_SR_GIPn_MASK)
#define MU_SR_GIPn_BIT_COUNT 4

#define MU_SR_EP_SHIFT 4
#define MU_SR_EP_MASK (0x1 << MU_SR_EP_SHIFT)

#define MU_SR_FUP_SHIFT 8
#define MU_SR_FUP_MASK (0x1 << MU_SR_FUP_SHIFT)

struct mu_regs {
	/* Processor Transmit Registers */
	uint32_t tr[MU_CHANNEL_COUNT];
	/* Processor Receive Registers */
	uint32_t rr[MU_CHANNEL_COUNT];
	/* Processor Status Register */
	uint32_t sr;
	/* Processor Control Register */
	uint32_t cr;
};

void mu_init(struct mu_regs *mu);
void mu_enable_rx_full_int(struct mu_regs *mu, uint32_t channel);
void mu_disable_rx_full_int(struct mu_regs *mu, uint32_t channel);
bool mu_send_msg(struct mu_regs *mu, uint32_t channel, uint32_t msg);
bool mu_try_receive_msg(struct mu_regs *mu, uint32_t channel, uint32_t *msg);
bool mu_is_rx_full(struct mu_regs *mu, uint32_t channel);
bool mu_is_tx_empty(struct mu_regs *mu, uint32_t channel);
bool mu_get_flag(struct mu_regs *mu, uint32_t fn);
bool mu_set_flag(struct mu_regs *mu, uint32_t fn, bool val);
bool mu_is_event_pending(struct mu_regs *mu);
bool mu_is_flags_pending(struct mu_regs *mu);
void mu_enable_general_int(struct mu_regs *mu, uint32_t idx);
void mu_disable_general_int(struct mu_regs *mu, uint32_t idx);
bool mu_is_general_int_pending(struct mu_regs *mu, uint32_t idx);
void mu_clear_general_int_pending(struct mu_regs *mu, uint32_t idx);

#endif /* __MU_IMX_H */