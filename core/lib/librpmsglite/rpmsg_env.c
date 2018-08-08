/*
 * Copyright (c) 2014, Mentor Graphics Corporation
 * Copyright (c) 2015 Xilinx, Inc.
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 * Copyright 2016 NXP
 * Copyright (c) Microsoft
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

/**************************************************************************
 * FILE NAME
 *
 *       bm_env.c
 *
 *
 * DESCRIPTION
 *
 *       This file is Bare Metal Implementation of env layer for OpenAMP.
 *
 *
 **************************************************************************/

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
 * Structure to keep track of registered ISRs
 */
struct isr_info {
	void *data;
};

static struct isr_info ipi_isr_table[IPI_ISR_COUNT];

struct concurrent_bounded_queue {
	size_t capacity;
	size_t count;
	size_t elem_size;
	size_t next_in;
	size_t next_out;
	void *buffer;
	struct mutex lock;
	struct condvar cv;
};

static void queue_dump(struct concurrent_bounded_queue *q)
{
	DMSG("q - capacity=%u; count=%u; elem_size=%u; next_in=%u; next_out=%u",
	     q->capacity, q->count, q->elem_size, q->next_in, q->next_out);
}

static bool queue_init(struct concurrent_bounded_queue *q, size_t capacity,
		       size_t elem_size)
{
	assert(q);
	memset(q, 0x00, sizeof(*q));

	q->capacity = capacity;
	q->count = 0;
	q->elem_size = elem_size;
	q->buffer = calloc(capacity, elem_size);
	if (!q->buffer) {
		return false;
	}

	q->next_in = 0;
	q->next_out = 0;
	mutex_init(&q->lock);
	condvar_init(&q->cv);

	return true;
}

static void queue_deinit(struct concurrent_bounded_queue *q)
{
	assert(q);
	assert(q->buffer);
	free(q->buffer);
	mutex_destroy(&q->lock);
	condvar_destroy(&q->cv);
	memset(q, 0x00, sizeof(*q));
}

static size_t queue_get_size(struct concurrent_bounded_queue *q)
{
	size_t size;

	assert(q);
	mutex_read_lock(&q->lock);
	size = q->count;
	mutex_read_unlock(&q->lock);

	return size;
}

static void queue_enqueue_helper(struct concurrent_bounded_queue *q,
				 const void *src)
{
	void *dst;

	dst = (char *)q->buffer + (q->next_in * q->elem_size);
	memcpy(dst, src, q->elem_size);
	q->next_in = (q->next_in + 1) % q->capacity;
	q->count++;
}

static bool queue_try_enqueue(struct concurrent_bounded_queue *q,
			      const void *src)
{
	assert(q);
	assert(src);

	mutex_lock(&q->lock);
	if (q->count == q->capacity) {
		mutex_unlock(&q->lock);
		return false;
	}

	queue_enqueue_helper(q, src);
	mutex_unlock(&q->lock);
	condvar_signal(&q->cv);

	return true;
}

static void queue_enqueue(struct concurrent_bounded_queue *q, const void *src)
{
	assert(q);
	assert(src);

	mutex_lock(&q->lock);
	while (q->count == q->capacity) {
		condvar_wait(&q->cv, &q->lock);
	}

	queue_enqueue_helper(q, src);
	mutex_unlock(&q->lock);
	condvar_signal(&q->cv);
}

static void queue_deqeue_helper(struct concurrent_bounded_queue *q, void *dst)
{
	void *src;

	src = (char *)q->buffer + (q->next_out * q->elem_size);
	memcpy(dst, src, q->elem_size);
	q->next_out = (q->next_out + 1) % q->capacity;
	q->count--;
}

static void queue_dequeue(struct concurrent_bounded_queue *q, void *dst)
{
	assert(q);
	assert(dst);

	mutex_lock(&q->lock);
	while (q->count == 0) {
		condvar_wait(&q->cv, &q->lock);
	}

	queue_deqeue_helper(q, dst);
	mutex_unlock(&q->lock);
}

static bool queue_try_dequeue(struct concurrent_bounded_queue *q, void *dst)
{
	assert(q);
	assert(dst);

	mutex_lock(&q->lock);
	if (q->count == 0) {
		mutex_unlock(&q->lock);
		return false;
	}

	queue_deqeue_helper(q, dst);
	mutex_unlock(&q->lock);
	return true;
}

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
	if (vector_id < IPI_ISR_COUNT) {
		DMSG("register ipi irq%d handler", vector_id);
		ipi_isr_table[vector_id].data = data;
	}
}

void env_unregister_isr(int vector_id)
{
	assert(vector_id < IPI_ISR_COUNT);
	if (vector_id < IPI_ISR_COUNT) {
		DMSG("unregister ipi irq%d handler", vector_id);
		ipi_isr_table[vector_id].data = NULL;
	}
}

void env_enable_interrupt(unsigned int vector_id)
{
	assert(vector_id < IPI_ISR_COUNT);
	DMSG("enable ipi irq%d", vector_id);
	platform_interrupt_enable(vector_id);
}

void env_disable_interrupt(unsigned int vector_id)
{
	assert(vector_id < IPI_ISR_COUNT);
	DMSG("disable ipi irq%d", vector_id);
	platform_interrupt_disable(vector_id);
}

int env_create_queue(void **queue, int length, int element_size)
{
	struct concurrent_bounded_queue *q;

	assert(queue);
	assert(length > 0);
	assert(element_size > 0);

	DMSG("create queue element size %d len %d", element_size, length);

	q = malloc(sizeof(*q));
	if (!q) {
		return -1;
	}

	if (!queue_init(q, length, (size_t)element_size)) {
		return -1;
	}

	*queue = q;

	return 0;
}

void env_delete_queue(void *queue)
{
	assert(queue);
	queue_deinit(queue);
	free(queue);
}

int env_put_queue(void *queue, void *msg, int timeout_ms)
{
	struct concurrent_bounded_queue *q;
	assert(queue);
	assert(msg);

	DMSG("env_put_queue - queue=%p; msg=%p, timeout_ms=%d", queue, msg,
	     timeout_ms);
	queue_dump(queue);

	/*
	 * only no-timeout (non-blocking) or infinite timeout (blocking) are
	 * supported for enqueue
	 */
	assert((timeout_ms == 0) || (timeout_ms == (int)RL_BLOCK));

	q = (struct concurrent_bounded_queue *)queue;
	if (timeout_ms == 0) {
		return queue_try_enqueue(q, msg) ? 1 : 0;
	}

	queue_enqueue(q, msg);
	return 1;
}

int env_get_queue(void *queue, void *msg, int timeout_ms)
{
	struct concurrent_bounded_queue *q;
	assert(queue);
	assert(msg);

	DMSG("env_get_queue - queue=%p; msg=%p, timeout_ms=%d", queue, msg,
	     timeout_ms);
	queue_dump(queue);

	/*
	 * only no-timeout (non-blocking) or infinite timeout (blocking) are
	 * supported for dequeue
	 */
	assert((timeout_ms == 0) || (timeout_ms == (int)RL_BLOCK));

	q = (struct concurrent_bounded_queue *)queue;
	if (timeout_ms == 0) {
		return queue_try_dequeue(q, msg) ? 1 : 0;
	}

	queue_dequeue(q, msg);
	return 1;
}

int env_get_current_queue_size(void *queue)
{
	assert(queue);
	return queue_get_size((struct concurrent_bounded_queue *)queue);
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
