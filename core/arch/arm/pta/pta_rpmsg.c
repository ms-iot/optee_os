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
#include <mm/mobj.h>
#include <platform_config.h>
#include <string.h>
#include <string_ext.h>
#include <trace.h>
#include <pta_rpmsg.h>
#include <drivers/rpmsg_ipc.h>
#include <rpmsglite/rpmsg_lite.h>
#include <rpmsglite/rpmsg_queue.h>
#include <rpmsglite/rpmsg_ns.h>

/* Local and remote end-points addresses, on the remote side, the 2 addresses
 * should be swapped.
 */
#define LOCAL_EPT_ADDR 30
#define REMOTE_EPT_ADDR 40

struct rpmsg_context {
	bool rpmsg_initialized;
	struct rpmsg_lite_instance inst;
	struct rpmsg_lite_ept_static_context ept_ctx;
	struct rpmsg_lite_endpoint *ept;
	rpmsg_queue_handle q;
	void *shmem;
	size_t shmem_size;
};

static struct rpmsg_context rpmsg_ctx;

static int rpmsg_init(void)
{
	struct rpmsg_lite_endpoint *ept;
	rpmsg_queue_handle q;
	int ret;

	if (rpmsg_ctx.rpmsg_initialized) {
		ret = 0;
		goto end;
	}

	if (!rpmsg_lite_master_init(rpmsg_ctx.shmem, rpmsg_ctx.shmem_size,
				    RL_PLATFORM_LINK_ID, RL_NO_FLAGS,
				    &rpmsg_ctx.inst)) {

		EMSG("failed to init RPMsg");
		ret = -1;
		goto end;
	}

	IMSG("master is ready and link is up");

	q = rpmsg_queue_create(&rpmsg_ctx.inst);
	if (!q) {
		EMSG("failed to create RPMsg recv queue");
		ret = -1;
		goto end;
	}

	rpmsg_ctx.q = q;

	ept = rpmsg_lite_create_ept(&rpmsg_ctx.inst, LOCAL_EPT_ADDR,
				    rpmsg_queue_rx_cb, q, &rpmsg_ctx.ept_ctx);
	if (!ept) {
		EMSG("failed to create end-point");
		ret = -1;
		goto end;
	}

	rpmsg_ctx.ept = ept;

	ret = 0;
	IMSG("RPMsg is ready. local ept addr:%u remote ept addr:%u",
	     LOCAL_EPT_ADDR, REMOTE_EPT_ADDR);

	rpmsg_ctx.rpmsg_initialized = true;

end:
	if (ret) {
		if (rpmsg_ctx.ept) {
			rpmsg_lite_destroy_ept(&rpmsg_ctx.inst, rpmsg_ctx.ept);
		}

		if (rpmsg_ctx.q) {
			rpmsg_queue_destroy(&rpmsg_ctx.inst, rpmsg_ctx.q);
		}

		rpmsg_lite_deinit(&rpmsg_ctx.inst);
	}

	return ret;
}

static int carveout_shared_mem(void)
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
		return -1;
	}

	DMSG("RPMsg shmem: pa 0x%08lx va 0x%p size 0x%08x", pa, va,
	     CFG_RPMSG_SHMEM_SIZE);

	rpmsg_ctx.shmem = va;
	rpmsg_ctx.shmem_size = CFG_RPMSG_SHMEM_SIZE;
	return 0;
}

static TEE_Result rpmsg_boot_remote(void *psess __unused,
				  uint32_t ptypes,
				  TEE_Param params[4])
{
	uint32_t exp_pt = TEE_PARAM_TYPES(
		TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);

	if (exp_pt != ptypes) {
		EMSG("invalid parameters");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/* currently a single remote is supported and core identification is not
	 * implemented. This param is expected to be 0 and should be treated as
	 * reserved.
	 */
	if (params[0].value.a != 0) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (carveout_shared_mem()) {
		return TEE_ERROR_OUT_OF_MEMORY;
	}

	if (!rpmsg_ipc_boot_remote()) {
		EMSG("Failed to boot remote processor");
		return TEE_ERROR_COMMUNICATION;
	}

	DMSG("remote core is up and running");

	if (!rpmsg_ipc_lock_remote()) {
		EMSG("Failed to lock remote processor configuratins");
		return TEE_ERROR_BAD_STATE;
	}

	DMSG("remote config is locked-down");

	/* consider waiting a fixed amout of time instead of blocking */
	if (!rpmsg_ipc_wait_remote_ready(0)) {
		EMSG("timeout waiting for remote processor to get into ready state ");
		return TEE_ERROR_TARGET_DEAD;
	}

	if (rpmsg_init() != TEE_SUCCESS) {
		EMSG("rpmsg init failed");
		return TEE_ERROR_BAD_STATE;
	}

	DMSG("remote booted successfully");
	return TEE_SUCCESS;
}

static TEE_Result rpmsg_remote_io(void *psess __unused,
				  uint32_t ptypes,
				  TEE_Param params[4])
{
	uint8_t *in_buffer;
	uint32_t in_buffer_size;
	uint8_t *out_buffer;
	uint32_t out_buffer_size;
	unsigned long msg_src_ept_addr;
	int total_received;
	uint32_t exp_pt = TEE_PARAM_TYPES(
		TEE_PARAM_TYPE_VALUE_INPUT, TEE_PARAM_TYPE_MEMREF_INPUT,
		TEE_PARAM_TYPE_MEMREF_INOUT, TEE_PARAM_TYPE_NONE);

	if (exp_pt != ptypes) {
		EMSG("invalid parameters");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	in_buffer = (uint8_t *)params[1].memref.buffer;
	out_buffer = (uint8_t *)params[2].memref.buffer;
	in_buffer_size = params[1].memref.size;
	out_buffer_size = params[2].memref.size;

	DMSG("sending IO msg with input size:%u output size:%d", in_buffer_size,
	     out_buffer_size);

	if (rpmsg_lite_send(&rpmsg_ctx.inst, rpmsg_ctx.ept, REMOTE_EPT_ADDR,
			    (char *)in_buffer, in_buffer_size, RL_BLOCK)) {

		EMSG("failed to send IO msg request");
		return TEE_ERROR_COMMUNICATION;
	}

	DMSG("IO msg sent, waiting for response...");
	if (rpmsg_queue_recv(&rpmsg_ctx.inst, rpmsg_ctx.q, &msg_src_ept_addr,
			     (char *)out_buffer, out_buffer_size,
			     &total_received, RL_BLOCK)) {

		EMSG("failed to receive IO msg response");
		return TEE_ERROR_COMMUNICATION;
	}

	DMSG("received IO msg response with size:%d", total_received);
	return TEE_SUCCESS;
}

/*
 * Trusted application entry points
 */

static TEE_Result rpmsg_open_session(uint32_t param_types __unused,
				     TEE_Param params[TEE_NUM_PARAMS] __unused,
				     void **sess_ctx __unused)
{
	*sess_ctx = NULL;
	DMSG("RPMsg PTA open session");
	return TEE_SUCCESS;
}

static void rpmsg_close_session(void *sess_ctx)
{
	if (sess_ctx != NULL) {
		free(sess_ctx);
	}
	DMSG("RPMsg PTA close session");
}

static TEE_Result rpmsg_invoke_command(void *psess,
				       uint32_t cmd,
				       uint32_t ptypes,
				       TEE_Param params[4])
{
	TEE_Result res;

	switch (cmd) {
	case PTA_RPMSG_CMD_BOOT_REMOTE:
		DMSG("PTA_RPMSG_CMD_BOOT_REMOTE");
		res = rpmsg_boot_remote(psess, ptypes, params);
		break;
	case PTA_RPMSG_CMD_REMOTE_IO:
		DMSG("PTA_RPMSG_CMD_REMOTE_IO");
		res = rpmsg_remote_io(psess, ptypes, params);
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
