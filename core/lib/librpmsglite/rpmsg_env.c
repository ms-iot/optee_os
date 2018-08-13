/*
 * Copyright (c) 2014, Mentor Graphics Corporation
 * Copyright (c) 2015 Xilinx, Inc.
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 * Copyright 2016 NXP
 * Copyright (c) Microsoft Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#include <rpmsglite/rpmsg_lite.h>
#include <rpmsglite/rpmsg_env.h>
#include <rpmsglite/rpmsg_platform.h>
#include <rpmsglite/virtqueue.h>
#include <rpmsglite/rpmsg_compiler.h>
#include <mm/core_memprot.h>
#include <kernel/tee_time.h>
#include <kernel/mutex.h>

#include <stdlib.h>
#include <string.h>
#include <string_ext.h>

static int env_init_counter = 0;

/*
 * Max supported ISR counts which equals to the max number of supported
 * virtual queues
 */
#define IPI_ISR_COUNT 4

/*
 * Structure to keep track of registered ISRs and their private data to be
 * passed to handlers.
 */
struct isr_info {
	void *data;
};

static struct isr_info ipi_isr_table[IPI_ISR_COUNT];

int env_init(void)
{
	assert(env_init_counter >= 0);
	if (env_init_counter < 0) {
		return -1;
	}

	/*
	 * calling evn_init multiple times is ok, but should be matched with
	 * same number of env deinit
	 */
	env_init_counter++;
	if (env_init_counter > 1) {
		return 0;
	}

	memset(ipi_isr_table, 0, sizeof(ipi_isr_table));
	return platform_init();
}

int env_deinit(void)
{
	assert(env_init_counter > 0);
	if (env_init_counter <= 0) {
		return -1;
	}

	env_init_counter--;
	if (env_init_counter > 0) {
		return 0;
	}
	return platform_deinit();
}

void *env_allocate_memory(unsigned int size)
{
	return malloc(size);
}

void env_free_memory(void *ptr)
{
	if (ptr != NULL) {
		free(ptr);
	}
}

void env_memset(void *ptr, int value, unsigned long size)
{
	memset(ptr, value, size);
}

void env_memcpy(void *dst, void const *src, unsigned long len)
{
	memcpy(dst, src, len);
}

int env_strcmp(const char *dst, const char *src)
{
	return strcmp(dst, src);
}

void env_strncpy(char *dest, const char *src, unsigned long len)
{
	strlcpy(dest, src, len);
}

int env_strncmp(char *dest, const char *src, unsigned long len)
{
	return strncmp(dest, src, len);
}

void env_mb(void)
{
	MEM_BARRIER();
}

void env_rmb(void)
{
	MEM_BARRIER();
}

void env_wmb(void)
{
	MEM_BARRIER();
}

unsigned long env_map_vatopa(void *address)
{
	assert(address);
	return (unsigned long)virt_to_phys(address);
}

void *env_map_patova(unsigned long address)
{
	assert(address);
	return (void *)phys_to_virt((paddr_t)address, MEM_AREA_IO_SEC);
}

int env_create_mutex(void **lock, int count)
{
	struct mutex *m;

	assert(lock);
	assert(count == 1 || count == 0);

	m = malloc(sizeof(struct mutex));
	if (!m) {
		return -1;
	}

	mutex_init(m);

	if (count == 0) {
		mutex_lock(m);
	}

	FMSG("created mutex %p count %d", (void *)m, count);
	*lock = m;
	return 0;
}

void env_delete_mutex(void *lock)
{
	assert(lock);
	FMSG("destroy mutex %p", lock);
	mutex_destroy((struct mutex *)lock);
	free(lock);
}

void env_lock_mutex(void *lock)
{
	assert(lock);
	FMSG("+mutex lock %p", lock);
	mutex_lock((struct mutex *)lock);
}

void env_unlock_mutex(void *lock)
{
	assert(lock);
	FMSG("-mutex unlock %p", lock);
	mutex_unlock((struct mutex *)lock);
}

void env_sleep_msec(int num_msec)
{
	assert(num_msec >= 0);
	tee_time_wait(num_msec);
}

void env_register_isr(int vector_id, void *data)
{
	assert(vector_id < IPI_ISR_COUNT);
	FMSG("register ipi irq%d handler", vector_id);
	if (vector_id < IPI_ISR_COUNT) {
		ipi_isr_table[vector_id].data = data;
	}
}

void env_unregister_isr(int vector_id)
{
	assert(vector_id < IPI_ISR_COUNT);
	FMSG("unregister ipi irq%d handler", vector_id);
	if (vector_id < IPI_ISR_COUNT) {
		ipi_isr_table[vector_id].data = NULL;
	}
}

void env_enable_interrupt(unsigned int vector_id)
{
	assert(vector_id < IPI_ISR_COUNT);
	FMSG("enable ipi irq%d", vector_id);
	platform_interrupt_enable(vector_id);
}

void env_disable_interrupt(unsigned int vector_id)
{
	assert(vector_id < IPI_ISR_COUNT);
	FMSG("disable ipi irq%d", vector_id);
	platform_interrupt_disable(vector_id);
}

void env_isr(int vector)
{
	struct isr_info *info;
	assert(vector < IPI_ISR_COUNT);
	if (vector < IPI_ISR_COUNT) {
		info = &ipi_isr_table[vector];
		virtqueue_notification((struct virtqueue *)info->data);
	}
}
