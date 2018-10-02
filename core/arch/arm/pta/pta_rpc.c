/*
 * Copyright (C) Microsoft. All rights reserved
 */

#include <kernel/msg_param.h>
#include <kernel/pseudo_ta.h>
#include <optee_msg_supplicant.h>
#include <string.h>
#include <pta_rpc.h>

/* Send RPC to the rich OS */
static TEE_Result pta_rpc_execute(
		void *sess_ctx,
		uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS]
		)
{
	uint32_t input_size, output_size, total_size;
	bool success;
	uint64_t memory_object_cookie = 0;
	struct mobj *memory_object = NULL;
	TEE_Result tee_result = TEE_SUCCESS;
	uint8_t *in_out_data = NULL;
	struct optee_msg_param rpc_msg_params[3];

	uint32_t expected_param_types = TEE_PARAM_TYPES(
			/*
			 * Opaque RPC type + RPC context values to be sent to the rich
			 * OS, along with the session ID of the caller TA.
			 */
			TEE_PARAM_TYPE_VALUE_INPUT,

			/* Buffer containing data to be sent to the rich OS */
			TEE_PARAM_TYPE_MEMREF_INPUT,

			/* Output buffer for data received from the rich OS*/
			TEE_PARAM_TYPE_MEMREF_OUTPUT,

			TEE_PARAM_TYPE_NONE);

	/* Validate the parameter types specified by the caller TA */
	if (expected_param_types != param_types) {
		EMSG("Incorrect parameter types %#x", param_types);
		tee_result = TEE_ERROR_BAD_PARAMETERS;
		goto done;
	}

	/* Allocate a single memory object for both input and output data */
	input_size = params[1].memref.size;
	output_size = params[2].memref.size;
	total_size = input_size + output_size;

	memory_object = thread_rpc_alloc_payload(total_size, &memory_object_cookie);

	if (memory_object == NULL) {
		EMSG("Failed to allocate memory object");
		tee_result = TEE_ERROR_OUT_OF_MEMORY;
		goto done;
	}

	in_out_data = mobj_get_va(memory_object, 0);

	if (in_out_data == NULL) {
		EMSG("Failed to map memory object");
		tee_result = TEE_ERROR_OUT_OF_MEMORY;
		goto done;
	}

	/* Set-up the parameters for the RPC call */
	memset(rpc_msg_params, 0, sizeof(rpc_msg_params));

	/* RPC parameter 0 - containing three values */
	rpc_msg_params[0].attr = OPTEE_MSG_ATTR_TYPE_VALUE_INPUT;
	/* RPC type value */
	rpc_msg_params[0].u.value.a = params[0].value.a;
	/* TA session ID value - see the comment from pta_rpc_open_session */
	rpc_msg_params[0].u.value.b = (vaddr_t)sess_ctx;
	/* RPC context value */
	rpc_msg_params[0].u.value.c = params[0].value.b;
	FMSG("RPC context = %#x, type = %#x, TA session %#x",
			(uint32_t)rpc_msg_params[0].u.value.c,
			(uint32_t)rpc_msg_params[0].u.value.a,
			(uint32_t)rpc_msg_params[0].u.value.b);

	/* RPC parameter 1 - input data buffer */
	success = msg_param_init_memparam(
			&rpc_msg_params[1],
			memory_object,              /* Memory object for this parameter */
			0,                          /* Start at offset 0 in the memory object */
			input_size, 		        /* Buffer size, specified by the caller TA */
			memory_object_cookie,		/* Cookie for this memory object */
			MSG_PARAM_MEM_DIR_IN);		/* RPC input parameter */

	if (!success) {
		EMSG("msg_param_init_memparam failed for input data");
		tee_result = TEE_ERROR_GENERIC;
		goto done;
	} else {
		memcpy(in_out_data, params[1].memref.buffer, input_size);
	}

	/* RPC parameter 2 - output data buffer */
	success = msg_param_init_memparam(
			&rpc_msg_params[2],
			memory_object,              /* Memory object for this parameter */
			input_size,                 /* Start at offset input_size in the memory object */
			output_size, 		        /* Buffer size, specified by the caller TA */
			memory_object_cookie,		/* Cookie for this memory object */
			MSG_PARAM_MEM_DIR_OUT);		/* RPC input parameter */

	if (!success) {
		EMSG("msg_param_init_memparam failed for output data");
		tee_result = TEE_ERROR_GENERIC;
		goto done;
	}

	/* Send RPC message to the rich OS */
	tee_result = thread_rpc_cmd(
			OPTEE_MSG_RPC_CMD_GENERIC,
			ARRAY_SIZE(rpc_msg_params),
			rpc_msg_params);
	FMSG("Returned from thread_rpc_cmd with result = %#x", tee_result);

	if (tee_result == TEE_SUCCESS)	{
		/* Copy the output data */
		params[2].memref.size = msg_param_get_buf_size(&rpc_msg_params[2]);
		assert(params[2].memref.size <= output_size);

		if (params[2].memref.size != 0) {
			memcpy(params[2].memref.buffer,
					in_out_data + input_size,
					params[2].memref.size);
		}
	}

done:

	if (memory_object != NULL) {
		thread_rpc_free_payload(memory_object_cookie, memory_object);
	}

	return tee_result;
}

/* PTA command handler */
static TEE_Result pta_rpc_invoke_command(
		void *sess_ctx,
		uint32_t cmd_id,
		uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS]
		)
{
	TEE_Result res;

	switch (cmd_id) {
	case PTA_RPC_EXECUTE:
		res = pta_rpc_execute(sess_ctx, param_types, params);
		break;

	default:
		EMSG("Command %#x is not supported\n", cmd_id);
		res = TEE_ERROR_NOT_IMPLEMENTED;
		break;
	}

	return res;
}

static TEE_Result pta_rpc_open_session(
		uint32_t param_types __unused,
		TEE_Param params[TEE_NUM_PARAMS] __unused,
		void **sess_ctx
		)
{
	/*
	 * When the rich OS opened a session to the caller TA, it received
	 * this value, that can be used as a "TA session ID".
	 */
	*sess_ctx = tee_ta_get_calling_session();

	/* Just TAs are allowed to use this PTA */
	if (*sess_ctx == NULL) {
		EMSG("Rejecting session opened from rich OS");
		return TEE_ERROR_ACCESS_DENIED;
	}

	DMSG("RPC PTA open session succeeded");
	return TEE_SUCCESS;
}

static void pta_rpc_close_session(void *sess_ctx __unused)
{
	DMSG("RPC PTA close session succeeded");
}

/* The TA manager uses a mutex to synchronize calls to any of these routines */
pseudo_ta_register(.uuid = PTA_RPC_UUID, .name = "RPC_PTA",
		.flags = PTA_DEFAULT_FLAGS,
		.open_session_entry_point = pta_rpc_open_session,
		.close_session_entry_point = pta_rpc_close_session,
		.invoke_command_entry_point = pta_rpc_invoke_command);
