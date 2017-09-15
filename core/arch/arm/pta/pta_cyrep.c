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
#include <string_ext.h>
#include <initcall.h>
#include <RiotTarget.h>
#include <RiotStatus.h>
#include <RiotEcc.h>
#include <RiotCrypt.h>
#include <RiotSha256.h>
#include <RiotDerEnc.h>
#include <RiotX509Bldr.h>
#include <CyrepCommon.h>
#include <platform_config.h>
#include <stdio.h>
#include <pta_cyrep.h>

struct cyrep_pta_sess_ctx {
	CyrepKeyPair ta_key_pair;
	CyrepCertChain cert_chain;
	char cert_chain_buffer[1024];
};

static CyrepKeyPair optee_key_pair;

static void uuid_to_string(const TEE_UUID *uuid, char s[64])
{
	snprintf(s, 64, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
		uuid->timeLow,
		uuid->timeMid,
		uuid->timeHiAndVersion,
		uuid->clockSeqAndNode[0],
		uuid->clockSeqAndNode[1],
		uuid->clockSeqAndNode[2],
		uuid->clockSeqAndNode[3],
		uuid->clockSeqAndNode[4],
		uuid->clockSeqAndNode[5],
		uuid->clockSeqAndNode[6],
		uuid->clockSeqAndNode[7]);
}

/*
 * Trusted application command handlers
 */

static TEE_Result cyrep_get_private_key(
	const struct cyrep_pta_sess_ctx *ctx,
	uint32_t param_types,
	TEE_Param params[TEE_NUM_PARAMS])
{
	DERBuilderContext cer_ctx;
	uint8_t cer_buf[DER_MAX_TBS];
	size_t out_buf_size;
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);

	DMSG("PTA_CYREP_GET_PRIVATE_KEY\n");

	if (exp_pt != param_types) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (params[0].memref.size < 1) {
		return TEE_ERROR_SHORT_BUFFER;
	}

	DERInitContext(&cer_ctx, cer_buf, sizeof(cer_buf));
	if (X509GetDEREcc(
		&cer_ctx,
		ctx->ta_key_pair.PublicKey,
		ctx->ta_key_pair.PrivateKey) != 0) {

		EMSG("Failed to encode key pair");
		return TEE_ERROR_GENERIC;
	}

	out_buf_size = params[0].memref.size - 1;
	if (DERtoPEM(
		&cer_ctx,
		ECC_PRIVATEKEY_TYPE,
		(char *)params[0].memref.buffer,
		&out_buf_size) != 0) {

		EMSG("Failed to convert DER to PEM");
		params[0].value.a = out_buf_size + 1;
		return TEE_ERROR_SHORT_BUFFER;
	}

	((char *)params[0].memref.buffer)[out_buf_size] = '\0';

	return TEE_SUCCESS;
}

static TEE_Result cyrep_get_cert_chain(
	const struct cyrep_pta_sess_ctx *ctx,
	uint32_t param_types,
	TEE_Param params[TEE_NUM_PARAMS])
{
	CyrepCertChain *orig_cert_chain;
	size_t req_len;
	size_t charsCopied;
	size_t index;
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);

	DMSG("PTA_CYREP_GET_CERT_CHAIN\n");

	orig_cert_chain = (CyrepCertChain *)phys_to_virt(
		CYREP_CERT_CHAIN_ADDR,
		MEM_AREA_IO_SEC);

	if (exp_pt != param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	index = Cyrep_FindCertBySubjectName(orig_cert_chain, "OPTEE");
	if (index == CYREP_CERT_INDEX_INVALID)
		return TEE_ERROR_ITEM_NOT_FOUND;

	req_len = Cyrep_GetCertAndIssuersLength(orig_cert_chain, index) +
		  strnlen(ctx->cert_chain.CertBuffer,
		  ctx->cert_chain.CertBufferSize) + 1;

	if (params[0].memref.size < req_len) {
		params[0].value.a = (uint32_t)req_len;
		return TEE_ERROR_SHORT_BUFFER;
	}

	charsCopied = strlcpy(
		params[0].memref.buffer,
		ctx->cert_chain.CertBuffer,
		params[0].memref.size);

	if (!Cyrep_GetCertAndIssuers(
		orig_cert_chain,
		index,
		(char *)params[0].memref.buffer + charsCopied,
		params[0].memref.size - charsCopied)) {

		return TEE_ERROR_GENERIC;
	}

	return TEE_SUCCESS;
}

static TEE_Result get_second_ta_session(struct tee_ta_session **sess)
{
	const struct thread_specific_data *tsd = thread_get_tsd();
	const struct tee_ta_session *first = TAILQ_FIRST(&tsd->sess_stack);
	struct tee_ta_session *second;

	second = TAILQ_NEXT(first, link_tsd);
	if (!second)
		return TEE_ERROR_ITEM_NOT_FOUND;

	*sess = second;

	return TEE_SUCCESS;
}

/*
 * Trusted application entry points
 */

static TEE_Result cyrep_open_session(
	uint32_t param_types __unused,
	TEE_Param params[TEE_NUM_PARAMS] __unused,
	void **sess_ctx)
{
	TEE_Result res;
	struct cyrep_pta_sess_ctx *ctx = NULL;
	CyrepCertChain *orig_cert_chain;
	CyrepDigest digest;
	struct tee_ta_session *ta_session;
	struct user_ta_ctx *utc;
	size_t cert_chain_size;
	char ta_guid_str[64];

	orig_cert_chain = (CyrepCertChain *)phys_to_virt(
		CYREP_CERT_CHAIN_ADDR,
		MEM_AREA_IO_SEC);

	if (!Cyrep_ValidateCertChainVersion(orig_cert_chain)) {
		EMSG("Cyrep: bad cert chain version\n");
		return TEE_ERROR_BAD_FORMAT;
	}

	// Make sure the calling TA is a user TA
	res = get_second_ta_session(&ta_session);
	if (res != TEE_SUCCESS)
		return res;

	if (!is_user_ta_ctx(ta_session->ctx))
		return TEE_ERROR_NOT_SUPPORTED;

	utc = to_user_ta_ctx(ta_session->ctx);
	memcpy(&digest, utc->ta_image_sha256, TEE_SHA256_HASH_SIZE);

	uuid_to_string(&ta_session->ctx->uuid, ta_guid_str);

	ctx = malloc(sizeof(struct cyrep_pta_sess_ctx));
	if (ctx == NULL) {
		EMSG("Failed to allocate cyrep PTA context structure\n");
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto end;
	}

	cert_chain_size = sizeof(struct cyrep_pta_sess_ctx) -
			offsetof(struct cyrep_pta_sess_ctx, cert_chain);

	if (!Cyrep_InitCertChain(
			1,
			&ctx->cert_chain,
			cert_chain_size)) {

		EMSG("Failed to initialize per-TA cert chain\n");
		res = TEE_ERROR_GENERIC;
		goto end;
	}

	ctx->cert_chain.DeviceIDPub = orig_cert_chain->DeviceIDPub;

	// append certificate for the TA to the cert chain
	if (!Cyrep_MeasureImageAndAppendCert(
			&optee_key_pair,
			&optee_key_pair.PrivateKey,
			sizeof(RIOT_ECC_PRIVATE),
			&digest,
			ta_guid_str,
			"OPTEE",
			&ctx->cert_chain,
			&ctx->ta_key_pair)) {

		EMSG("Failed to append TA certificate to cert chain\n");
		res = TEE_ERROR_SECURITY;
		goto end;
	}

	*sess_ctx = ctx;
	res = TEE_SUCCESS;

end:
	if (res != TEE_SUCCESS) {
		if (ctx != NULL)
			free(ctx);
	}

	return res;
}

static void cyrep_close_session(void *sess_ctx)
{
	if (sess_ctx != NULL) {
		free(sess_ctx);
	}
	DMSG("Cyrep PTA close session succeeded!\n");
}

static TEE_Result cyrep_invoke_command(void *sess_ctx, uint32_t cmd_id,
				 uint32_t param_types,
				 TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Result res;
	struct cyrep_pta_sess_ctx *ctx = sess_ctx;

	switch (cmd_id) {
	case PTA_CYREP_GET_PRIVATE_KEY:
		res = cyrep_get_private_key(ctx, param_types, params);
		break;
	case PTA_CYREP_GET_CERT_CHAIN:
		res = cyrep_get_cert_chain(ctx, param_types, params);
		break;
	default:
		EMSG("Command not implemented %d\n", cmd_id);
		res = TEE_ERROR_NOT_IMPLEMENTED;
		break;
	}

	return res;
}

static TEE_Result pta_cyrep_init(void)
{
	CyrepKeyPair *key_pair_in = (CyrepKeyPair *)phys_to_virt(
		(paddr_t)CYREP_OPTEE_KEY_PAIR_ADDR,
		MEM_AREA_IO_SEC);

	Cyrep_CapturePrivateKey(&optee_key_pair, key_pair_in);

	return TEE_SUCCESS;
}

service_init(pta_cyrep_init);

pseudo_ta_register(.uuid = PTA_CYREP_UUID, .name = "pta_cyrep",
		   .flags = PTA_DEFAULT_FLAGS,
		   .open_session_entry_point = cyrep_open_session,
		   .close_session_entry_point = cyrep_close_session,
		   .invoke_command_entry_point = cyrep_invoke_command);
