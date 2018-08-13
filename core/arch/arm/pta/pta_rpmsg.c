/*
 * Copyright (C) Microsoft. All rights reserved
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
#include <initcall.h>
#include <keep.h>
#include <kernel/interrupt.h>
#include <kernel/misc.h>
#include <kernel/pseudo_ta.h>
#include <kernel/tee_time.h>
#include <kernel/thread.h>
#include <kernel/spinlock.h>
#include <mm/mobj.h>
#include <platform_config.h>
#include <string.h>
#include <string_ext.h>
#include <trace.h>
#include <pta_rpmsg.h>
#include <drivers/rpmsg_ipc.h>
#include <rpmsglite/rpmsg_lite.h>

/* Remote end-point address to be the destination of send requests.
 * Currently only single remote end-point is supported and is chosen to be 40,
 * future enhancement should support sending to arbitrary remote end-point and
 * to be passed as part of the send cmd parameters.
 */
#define REMOTE_EPT_ADDR 40

/* The window within which the remote core should signal the ready state in a
 * platform dependant way.
 */
#define REMOTE_BOOT_TIMEOUT_MS 1000

/* Wait interval for polling the queue for a received payload */
#define RECEIVE_POLLING_WAIT_TIME_MS 10

/* Number of times to poll the queue for received payload */
#define RECEIVE_POLLING_RETRY_COUNT 10

/* Enable verbose queue trace messages on queue operations */
#define DEBUG_QUEUE 0

/* A bounded circular queue with underlying buffer allocated from sec memory. */
struct queue {
	size_t capacity;
	size_t count;
	size_t elem_size;
	size_t next_in;
	size_t next_out;
	void *buffer;
	unsigned int lock;
};

/* A queue element holding payload information captured during the rx ISR. */
struct queue_rx_data {
	unsigned long src;
	void *data;
	size_t len;
};

/* Keep track of rpmsg framework and remote core global state. */
struct rpmsg_context {
	bool rpmsg_initialized;
	bool remote_initialized;
	struct rpmsg_lite_instance inst;
	void *shmem;
	size_t shmem_size;
};

/* Every session is represented by a dedicated end-point with an automatically
 * assigned address and will have a dedicated queue.
 */
struct session_context {
	struct rpmsg_lite_ept_static_context ept_ctx;
	struct rpmsg_lite_endpoint *ept;
	struct queue rxq;
};

static struct rpmsg_context rpmsg_ctx;

#if DEBUG_QUEUE
static void queue_dump(const struct queue *q __maybe_unused)
{
	DMSG("q - capacity=%u; count=%u; elem_size=%u; next_in=%u; next_out=%u",
	     q->capacity, q->count, q->elem_size, q->next_in, q->next_out);
}
#endif

static bool queue_push(struct queue *q, const struct queue_rx_data *data)
{
	uint32_t exceptions;
	bool success;
	void *dst;

	assert(q);
	assert(data);

	exceptions = cpu_spin_lock_xsave(&q->lock);

#if DEBUG_QUEUE
	DMSG("len:%u src:%u", data->len, (uint32_t)data->src);
	queue_dump(q);
#endif

	if (q->count == q->capacity) {
		success = false;
		goto end;
	}

	dst = (char *)q->buffer + (q->next_in * q->elem_size);
	memcpy(dst, data, q->elem_size);
	q->next_in = (q->next_in + 1) % q->capacity;
	q->count++;
	success = true;

end:
	cpu_spin_unlock_xrestore(&q->lock, exceptions);
	return success;
}

static bool queue_pop(struct queue *q, struct queue_rx_data *data)
{
	uint32_t exceptions;
	bool success;
	void *src;

	assert(q);
	assert(data);

	exceptions = cpu_spin_lock_xsave(&q->lock);

#if DEBUG_QUEUE
	queue_dump(q);
#endif

	if (q->count == 0) {
		success = false;
		goto end;
	}

	src = (char *)q->buffer + (q->next_out * q->elem_size);
	memcpy(data, src, q->elem_size);
	q->next_out = (q->next_out + 1) % q->capacity;
	q->count--;
	success = true;

end:
	cpu_spin_unlock_xrestore(&q->lock, exceptions);
	return success;
}

/* Drain a specific queue to free any held virtqueue VRING buffers */
static void queue_drain(struct queue *q)
{
	uint32_t exceptions;
	struct queue_rx_data msg;
	void *src;

	assert(q);
	while (q->count > 0) {
		exceptions = cpu_spin_lock_xsave(&q->lock);
		src = (char *)q->buffer + (q->next_out * q->elem_size);
		memcpy(&msg, src, q->elem_size);
		q->next_out = (q->next_out + 1) % q->capacity;
		q->count--;
		cpu_spin_unlock_xrestore(&q->lock, exceptions);
		rpmsg_lite_release_rx_buffer(&rpmsg_ctx.inst, msg.data);
	}
}

static bool queue_init(struct queue *q, size_t capacity, size_t elem_size)
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
	q->lock = SPINLOCK_UNLOCK;

	return true;
}

static void queue_deinit(struct queue *q)
{
	assert(q);
	if (q->buffer) {
		/* drain the queue which in turn will free each element
		 * corresponding rx buffer from the virtqueue */
		queue_drain(q);
		free(q->buffer);
		q->buffer = NULL;
	}
}

/* The end-point ISR which receives a payload and push it to the end-point
 * queue for later dequeing in a receive cmd.
 */
static int queue_rx_callback(void *payload, int payload_len, unsigned long src,
			     void *priv)
{
	struct queue_rx_data msg;
	struct queue *rxq;

	assert(payload);
	assert(priv);
	assert(payload_len > 0);

	rxq = (struct queue *)priv;
	msg.data = payload;
	msg.len = (size_t)payload_len;
	msg.src = src;

	/* if message is successfully added into queue then hold rpmsg rx buffer
	 */
	if (queue_push(rxq, &msg)) {
		return RL_HOLD;
	}

	return RL_RELEASE;
}

static TEE_Result rpmsg_remote_send(void *psess, uint32_t ptypes,
				    TEE_Param params[4])
{
	struct session_context *ctx;
	uint8_t *in_buffer;
	uint32_t in_buffer_size;
	uint32_t exp_pt = TEE_PARAM_TYPES(
		TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_MEMREF_INPUT,
		TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);
	int ret;

	assert(psess);
	ctx = (struct session_context *)psess;
	assert(ctx);

	if (exp_pt != ptypes) {
		EMSG("invalid parameters");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	in_buffer = (uint8_t *)params[1].memref.buffer;
	in_buffer_size = params[1].memref.size;

	DMSG("sending msg with size:%u from ept:%u", in_buffer_size,
	     (uint32_t)ctx->ept->addr);


	ret = rpmsg_lite_send(&rpmsg_ctx.inst, ctx->ept, REMOTE_EPT_ADDR,
			      (char *)in_buffer, in_buffer_size, RL_BLOCK);

	if (ret == RL_ERR_BUFF_SIZE) {
		EMSG("sent data exceed the max payload size %u",
		     RL_BUFFER_PAYLOAD_SIZE);

		return TEE_ERROR_EXCESS_DATA;
	} else if (ret != RL_SUCCESS) {
		EMSG("failed to send IO msg request");
		return TEE_ERROR_COMMUNICATION;
	}

	DMSG("msg sent successfully");

	return TEE_SUCCESS;
}

static TEE_Result rpmsg_remote_receive(void *psess, uint32_t ptypes,
				       TEE_Param params[4])
{
	struct session_context *ctx;
	uint8_t *out_buffer;
	uint32_t out_buffer_size;
	uint32_t exp_pt = TEE_PARAM_TYPES(
		TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_MEMREF_INOUT,
		TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);
	struct queue_rx_data rx_data;
	int timeout;

	assert(psess);
	ctx = (struct session_context *)psess;
	assert(ctx);

	if (exp_pt != ptypes) {
		EMSG("invalid parameters");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	out_buffer = (uint8_t *)params[1].memref.buffer;
	out_buffer_size = params[1].memref.size;

	DMSG("receiving msg with max size:%u", out_buffer_size);

	/* poll on the end-point queue for payloads until timeout */
	timeout = RECEIVE_POLLING_RETRY_COUNT;
	while ((timeout >= 0) && !queue_pop(&ctx->rxq, &rx_data)) {
		tee_time_wait(RECEIVE_POLLING_WAIT_TIME_MS);
		timeout--;
	}

	if (timeout < 0) {
		EMSG("timeout waiting for a msg in the queue");
		return TEE_ERROR_NO_DATA;
	}

	if (rx_data.len > out_buffer_size) {
		EMSG("provided output buffer is too short - provided:%u, expected:%u",
		     out_buffer_size, rx_data.len);

		return TEE_ERROR_SHORT_BUFFER;
	}

	memcpy(out_buffer, rx_data.data, rx_data.len);

	/* rx buffer is not needed anymore, return it back to the virtqueue */
	rpmsg_lite_release_rx_buffer(&rpmsg_ctx.inst, rx_data.data);

	DMSG("msg of size:%u received successfully", rx_data.len);
	return TEE_SUCCESS;
}

static bool rpmsg_init(void)
{
	if (rpmsg_ctx.rpmsg_initialized) {
		return true;
	}

	if (!rpmsg_lite_master_init(rpmsg_ctx.shmem, rpmsg_ctx.shmem_size,
				    RL_PLATFORM_LINK_ID, RL_NO_FLAGS,
				    &rpmsg_ctx.inst)) {

		EMSG("failed to init rpmsg master");
		return false;
	}

	IMSG("master is ready and link is up");
	rpmsg_ctx.rpmsg_initialized = true;

	return true;
}

static bool carveout_shared_mem(void)
{
	paddr_t pa;
	void *va;

	/* The shared memory region base address and size should match the same
	 * values used by the remote. This memory region has to be registered as
	 * a secure memory and be protected from normal world.
	 */
	pa = CFG_RPMSG_SHMEM_BASE;
	va = phys_to_virt(pa, MEM_AREA_IO_SEC);
	if (!va) {
		return false;
	}

	DMSG("rpmsg shmem: pa 0x%08lx va 0x%p size 0x%08x", pa, va,
	     CFG_RPMSG_SHMEM_SIZE);

	rpmsg_ctx.shmem = va;
	rpmsg_ctx.shmem_size = CFG_RPMSG_SHMEM_SIZE;
	return true;
}

static TEE_Result rpmsg_boot_remote(void)
{
	if (!carveout_shared_mem()) {
		return TEE_ERROR_OUT_OF_MEMORY;
	}

	if (!rpmsg_ipc_boot_remote()) {
		EMSG("failed to boot remote processor");
		return TEE_ERROR_COMMUNICATION;
	}

	DMSG("remote core is up and running");

	if (!rpmsg_ipc_lock_remote()) {
		EMSG("failed to lock remote processor configuratins");
		return TEE_ERROR_BAD_STATE;
	}

	DMSG("remote config is locked-down");

	if (!rpmsg_ipc_wait_remote_ready(REMOTE_BOOT_TIMEOUT_MS)) {
		EMSG("timeout waiting for remote processor to get into ready state ");
		return TEE_ERROR_TARGET_DEAD;
	}

	rpmsg_ctx.remote_initialized = true;
	IMSG("remote booted successfully");
	return TEE_SUCCESS;
}

/*
 * Trusted application entry points
 */

static TEE_Result rpmsg_open_session(uint32_t param_types __unused,
				     TEE_Param params[TEE_NUM_PARAMS] __unused,
				     void **sess_ctx __unused)
{
	struct session_context *ctx;
	struct rpmsg_lite_endpoint *ept;
	TEE_Result res;

	DMSG("rpmsg PTA open session");
	ctx = NULL;

	if (!rpmsg_ctx.remote_initialized) {
		res = rpmsg_boot_remote();
		if (res != TEE_SUCCESS) {
			EMSG("remote boot failed");
			goto end;
		}
		assert(rpmsg_ctx.remote_initialized);
	}

	if (!rpmsg_ctx.rpmsg_initialized) {
		if (!rpmsg_init()) {
			EMSG("rpmsg init failed");
			res = TEE_ERROR_BAD_STATE;
			goto end;
		}
		assert(rpmsg_ctx.rpmsg_initialized);
	}

	ctx = malloc(sizeof(struct session_context));
	if (!ctx) {
		EMSG("failed to allocate session context");
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto end;
	}
	memset(ctx, 0x00, sizeof(*ctx));

	/* create a session dedicated end-point and its associated queue */

	if (!queue_init(&ctx->rxq, rpmsg_ctx.inst.rvq->vq_nentries,
			sizeof(struct queue_rx_data))) {
		EMSG("failed to create rpmsg recv queue");
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto end;
	}

	ept = rpmsg_lite_create_ept(&rpmsg_ctx.inst, RL_ADDR_ANY,
				    queue_rx_callback, &ctx->rxq,
				    &ctx->ept_ctx);
	if (!ept) {
		EMSG("failed to create end-point");
		res = TEE_ERROR_BAD_STATE;
		goto end;
	}

	ctx->ept = ept;
	IMSG("endpoint is ready on addr:%u", (uint32_t)ept->addr);

	*sess_ctx = ctx;
	res = TEE_SUCCESS;

end:
	if ((res != TEE_SUCCESS) && ctx) {
		if (ctx->ept) {
			rpmsg_lite_destroy_ept(&rpmsg_ctx.inst, ctx->ept);
		}

		queue_deinit(&ctx->rxq);
		free(ctx);
	}

	return TEE_SUCCESS;
}

static void rpmsg_close_session(void *sess_ctx)
{
	struct session_context *ctx;

	DMSG("rpmsg PTA close session");

	ctx = (struct session_context *)sess_ctx;
	if (ctx) {
		if (ctx->ept) {
			rpmsg_lite_destroy_ept(&rpmsg_ctx.inst, ctx->ept);
		}

		queue_deinit(&ctx->rxq);
		free(ctx);
	}
}

static TEE_Result rpmsg_invoke_command(void *psess, uint32_t cmd,
				       uint32_t ptypes, TEE_Param params[4])
{
	TEE_Result res;

	switch (cmd) {
	case PTA_RPMSG_CMD_SEND:
		FMSG("PTA_RPMSG_CMD_SEND");
		res = rpmsg_remote_send(psess, ptypes, params);
		break;
	case PTA_RPMSG_CMD_RECEIVE:
		FMSG("PTA_RPMSG_CMD_RECEIVE");
		res = rpmsg_remote_receive(psess, ptypes, params);
		break;
	default:
		EMSG("command not implemented %d", cmd);
		res = TEE_ERROR_NOT_IMPLEMENTED;
		break;
	}

	return res;
}

static TEE_Result pta_rpmsg_init(void)
{
	memset(&rpmsg_ctx, 0x00, sizeof(rpmsg_ctx));

	return TEE_SUCCESS;
}

service_init_late(pta_rpmsg_init);

pseudo_ta_register(.uuid = PTA_RPMSG_UUID, .name = "pta_rpmsg",
		   .flags = PTA_DEFAULT_FLAGS,
		   .open_session_entry_point = rpmsg_open_session,
		   .close_session_entry_point = rpmsg_close_session,
		   .invoke_command_entry_point = rpmsg_invoke_command);
