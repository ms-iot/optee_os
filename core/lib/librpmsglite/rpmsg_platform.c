/*
 * The Clear BSD License
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 * Copyright 2016 NXP
 * Copyright (c) Microsoft Corporation
 * All rights reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <rpmsglite/rpmsg_platform.h>
#include <rpmsglite/rpmsg_env.h>
#include <drivers/rpmsg_ipc.h>

static int isr_counter = 0;
static int disable_counter = 0;
static void *lock;

int platform_init_interrupt(int vq_id, void *isr_data)
{
	/* Register ISR to environment layer */
	env_register_isr(vq_id, isr_data);

	env_lock_mutex(lock);

	assert(0 <= isr_counter);
	if (isr_counter == 0) {
		rpmsg_ipc_rx_ipi_enable();
	}

	isr_counter++;
	env_unlock_mutex(lock);
	return 0;
}

int platform_deinit_interrupt(int vq_id)
{
	env_lock_mutex(lock);

	assert(0 < isr_counter);
	isr_counter--;
	if (isr_counter == 0) {
		rpmsg_ipc_rx_ipi_disable();
	}

	/* Unregister ISR from environment layer */
	env_unregister_isr(vq_id);
	env_unlock_mutex(lock);
	return 0;
}

void platform_notify(int vq_id)
{
	uint32_t msg;

	/* Use the upper 16-bit of the message are the virtual queue ID. The lower
	 * 16-bit are unused.
	 */
	msg = (RL_GET_Q_ID(vq_id)) << 16;
	env_lock_mutex(lock);
	if (!rpmsg_ipc_send_msg(msg)) {
		EMSG("rpmsg ipc failed to send msg over channel %d", RPMSG_IPI_CHANNEL);
	}
	env_unlock_mutex(lock);
}


/**
 * platform_interrupt_enable
 *
 * Enable peripheral-related interrupt with passed priority and type.
 *
 * @param vq_id Vector ID that need to be converted to IRQ number
 * @param trigger_type IRQ active level
 * @param trigger_type IRQ priority
 *
 * @return vq_id. Return value is never checked..
 *
 */
int platform_interrupt_enable(unsigned int vq_id)
{
	assert(0 < disable_counter);

	disable_counter--;
	if (disable_counter == 0) {
		rpmsg_ipc_ipi_enable();
	}
	return vq_id;
}

/**
 * platform_interrupt_disable
 *
 * Disable peripheral-related interrupt.
 *
 * @param vq_id Vector ID that need to be converted to IRQ number
 *
 * @return vq_id. Return value is never checked.
 *
 */
int platform_interrupt_disable(unsigned int vq_id)
{
	assert(0 <= disable_counter);

	/* if counter is set - the interrupts are disabled */
	if (disable_counter == 0) {
		rpmsg_ipc_ipi_disable();
	}
	disable_counter++;
	return vq_id;
}

static void platform_rx_ipi_handler(uint32_t ch)
{
	env_isr((int)ch);
}

/**
 * platform_init
 *
 * platform/environment init
 */
int platform_init(void)
{
	struct rpmsg_ipc_params params;

	params.rx_ipi_handler = platform_rx_ipi_handler;
	rpmsg_ipc_init(params);

	/* Create lock used in multi-instanced RPMsg */
	if (env_create_mutex(&lock, 1)) {
		return -1;
	}

	return 0;
}

/**
 * platform_deinit
 *
 * platform/environment deinit process
 */
int platform_deinit(void)
{
	/* Delete lock used in multi-instanced RPMsg */
	env_delete_mutex(lock);
	lock = NULL;
	return 0;
}
