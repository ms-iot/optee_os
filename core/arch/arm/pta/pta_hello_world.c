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

#include <arm.h>
#include <kernel/misc.h>
#include <kernel/pseudo_ta.h>
#include <kernel/user_ta.h>
#include <kernel/thread.h>
#include <mm/core_memprot.h>
#include <mm/tee_mmu.h>
#include <optee_msg_supplicant.h>
#include <utee_defines.h>
#include <string.h>

#include <pta_hello_world.h>


/*
 * Trusted application command handlers
 */

static TEE_Result hellow_world_cmd1(uint32_t param_types,
    TEE_Param params[TEE_NUM_PARAMS])
{
    uint8_t* in_buffer;
    uint8_t* out_buffer;
    uint32_t out_buffer_size;

    uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
        TEE_PARAM_TYPE_MEMREF_INOUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);

    DMSG("PTA_HELLO_WORLD_CMD1\n");

    if (exp_pt != param_types) {
        return TEE_ERROR_BAD_PARAMETERS;
    }

    in_buffer = (uint8_t*)params[0].memref.buffer;
    out_buffer_size = params[0].memref.size;
    out_buffer = (uint8_t*)params[1].memref.buffer;
    if (out_buffer_size > params[1].memref.size) {
        out_buffer_size = params[1].memref.size;
    }

    for (uint32_t i = 0; i < out_buffer_size; ++i) {
        out_buffer[i] = in_buffer[out_buffer_size - i - 1];
    }

    return TEE_SUCCESS;
}

/*
 * Trusted application entry points
 */

static TEE_Result hellow_world_open_session(uint32_t param_types __unused,
                    TEE_Param params[TEE_NUM_PARAMS] __unused,
                    void **sess_ctx __unused)
{
    DMSG("Hello world PTA open session succeeded!\n");
    return TEE_SUCCESS;
}

static void hellow_world_close_session(void *sess_ctx __unused)
{
    DMSG("Hello world PTA close session succeeded!\n");
}

static TEE_Result hellow_world_invoke_command(void *sess_ctx __unused, uint32_t cmd_id,
				 uint32_t param_types,
				 TEE_Param params[TEE_NUM_PARAMS])
{
    TEE_Result res;
    DMSG("Hello world PTA invoke command %d\n", cmd_id);

    switch (cmd_id) {
    case PTA_HELLO_WORLD_CMD1:
        res = hellow_world_cmd1(param_types, params);
        break;

    default:
        EMSG("Command not implemented %d\n", cmd_id);
        res = TEE_ERROR_NOT_IMPLEMENTED;
        break;
    }

    return res;
}

pseudo_ta_register(.uuid = PTA_HELLO_WORLD_UUID, .name = "pta_hello_world",
		   .flags = PTA_DEFAULT_FLAGS,
		   .open_session_entry_point = hellow_world_open_session,
           .close_session_entry_point = hellow_world_close_session,
		   .invoke_command_entry_point = hellow_world_invoke_command);
