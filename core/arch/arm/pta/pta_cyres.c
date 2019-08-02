// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) Microsoft. All rights reserved
 */

#include <kernel/pseudo_ta.h>
#include <kernel/user_ta.h>
#include <kernel/thread.h>
#include <kernel/panic.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <crypto/crypto.h>
#include <tomcrypt.h>
#include <ta_pub_key.h>
#include <initcall.h>
#include <stdio.h>
#include <cyres_cert_chain.h>
#include <RiotCrypt.h>
#include <pta_cyres.h>

struct cyres_pta_sess_ctx {
	struct cyres_cert *ta_cert;
	struct cyres_key_pair ta_key_pair;
};

static struct cyres_cert_blob *cert_blob;
static struct cyres_key_pair optee_key_pair;

static TEE_Result tee_result_from_cyres(cyres_result cy_res)
{
	/*
	 * return values are compatible. 0 is success, and CYRES_ERROR codes
	 * are defined with same underlying values as TEE_ERROR codes.
	 */
	return (TEE_Result)cy_res;
}

static TEE_Result tee_result_from_riot(RIOT_STATUS status)
{
	switch (status) {
	case RIOT_SUCCESS: return TEE_SUCCESS;
	case RIOT_FAILURE: return TEE_ERROR_GENERIC;
	case RIOT_INVALID_PARAMETER: return TEE_ERROR_BAD_PARAMETERS;
	case RIOT_BAD_FORMAT: return TEE_ERROR_BAD_FORMAT;
	default:
		break;
	}

	return TEE_ERROR_SECURITY;
}

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

static TEE_Result get_calling_ta_session(struct tee_ta_session **sess)
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

static TEE_Result export_ta_pub_key(unsigned char *buf, unsigned long *len)
{
	rsa_key key;
	TEE_Result res;
	int ret;
	uint32_t e = TEE_U32_TO_BIG_ENDIAN(ta_pub_key_exponent);

	ret = rsa_set_key(ta_pub_key_modulus, ta_pub_key_modulus_size,
			  (const unsigned char *)&e, sizeof(e),
			  NULL, 0,
			  &key);

	if (ret != CRYPT_OK) {
		DMSG("rsa_set_key failed (0x%x)", ret);
		return TEE_ERROR_SECURITY;
	}

	ret = rsa_export(buf, len, PK_PUBLIC, &key);
	if (ret != CRYPT_OK) {
		DMSG("rsa_export failed (0x%x)", ret);
		res = TEE_ERROR_SECURITY;
		goto end;
	}

	res = TEE_SUCCESS;

end:
	rsa_free(&key);
	return res;
}

static TEE_Result gen_ta_cert(struct cyres_pta_sess_ctx *ctx)
{
	TEE_Result res;
	cyres_result cy_res;
	unsigned long key_size;
	struct tee_ta_session *ta_session;
	struct user_ta_ctx *utc;
	struct cyres_cert *cert = NULL;
	struct cyres_gen_alias_cert_args args;
	char ta_guid_str[64];
	unsigned char key_buf[1024];

	// Make sure the calling TA is a user TA
	res = get_calling_ta_session(&ta_session);
	if (res != TEE_SUCCESS)
		return res;

	if (!is_user_ta_ctx(ta_session->ctx))
		return TEE_ERROR_NOT_SUPPORTED;

	utc = to_user_ta_ctx(ta_session->ctx);
	uuid_to_string(&ta_session->ctx->uuid, ta_guid_str);

	/* export TA public key */
	key_size = sizeof(key_buf);
	res = export_ta_pub_key(key_buf, &key_size);
	if (res != TEE_SUCCESS)
		return res;

	/* generate a certificate for the TA */
	memset(&args, 0, sizeof(args));
	args.issuer_key_pair = &optee_key_pair;
	args.seed_data = &optee_key_pair.priv;
	args.seed_data_size = sizeof(optee_key_pair.priv);
	args.subject_digest = utc->ta_image_sha256;
	args.subject_digest_size = sizeof(utc->ta_image_sha256);
	args.auth_key_pub = key_buf;
	args.auth_key_pub_size = key_size;
	args.subject_name = ta_guid_str;
	args.issuer_name = "OP-TEE";
	args.path_len = 1;
	cy_res = cyres_gen_alias_cert(&args, &cert, &ctx->ta_key_pair);
	if (cy_res) {
		res = tee_result_from_cyres(cy_res);
		goto end;
	}

	ctx->ta_cert = cert;
	res = TEE_SUCCESS;

end:
	if (res)
		if (cert)
			cyres_free_cert(cert);

	return res;
}

static TEE_Result compute_ta_cert_chain_buffer_size(
		const struct cyres_cert_blob *chain,
		const struct cyres_cert *ta_cert,
		size_t *size)
{
	cyres_result cy_res;
	size_t ta_size = 0;
	size_t chain_size = 0;

	cy_res = cyres_cert_to_pem(ta_cert, NULL, &ta_size);
	if (cy_res != CYRES_ERROR_SHORT_BUFFER) {
		EMSG("Failed to get TA cert required buffer size");
		return (cy_res == CYRES_SUCCESS) ? TEE_ERROR_GENERIC :
			tee_result_from_cyres(cy_res);
	}

	cy_res = cyres_get_cert_chain_pem(chain, "OP-TEE", NULL, &chain_size);
	if (cy_res != CYRES_ERROR_SHORT_BUFFER) {
		EMSG("Failed to get cert chain required buffer size (0x%x)",
				cy_res);
		return (cy_res == CYRES_SUCCESS) ? TEE_ERROR_GENERIC :
			tee_result_from_cyres(cy_res);
	}

	/* account for extra null terminator */
	*size = ta_size + chain_size - 1;

	return TEE_SUCCESS;
}

static TEE_Result get_ta_private_key(
		const struct cyres_pta_sess_ctx *ctx,
		char *buf, size_t *buf_size)
{
	cyres_result cy_res;

	cy_res = cyres_priv_key_to_pem(&ctx->ta_key_pair, buf, buf_size);

	return tee_result_from_cyres(cy_res);
}

static TEE_Result get_ta_public_key(
		const struct cyres_pta_sess_ctx *ctx,
		char *buf, size_t *buf_size)
{
	cyres_result cy_res;

	cy_res = cyres_pub_key_to_pem(&ctx->ta_key_pair.pub, buf, buf_size);

	return tee_result_from_cyres(cy_res);
}

static TEE_Result get_ta_cert_chain(
		const struct cyres_pta_sess_ctx *ctx,
		char *buf, size_t *buf_size)
{
	cyres_result cy_res;
	TEE_Result res;
	size_t required_size = (size_t)-1; /* work around compiler warning */
	size_t ta_cert_len;
	size_t remaining_size;

	res = compute_ta_cert_chain_buffer_size(cert_blob, ctx->ta_cert,
			&required_size);
	if (res)
		return res;

	if (!buf || *buf_size < required_size) {
		*buf_size = required_size;
		return TEE_ERROR_SHORT_BUFFER;
	}

	/* copy the TA cert to the buffer */
	cy_res = cyres_cert_to_pem(ctx->ta_cert, buf, buf_size);
	if (cy_res)
		return tee_result_from_cyres(cy_res);

	/* compute remaining buffer size, accounting for extra NULL*/
	ta_cert_len = strlen(buf);
	remaining_size = *buf_size - ta_cert_len;

	/* append the rest of the cert chain to the buffer */
	cy_res = cyres_get_cert_chain_pem(cert_blob, "OP-TEE",
			buf + ta_cert_len, &remaining_size);

	if (cy_res)
		return tee_result_from_cyres(cy_res);

	return TEE_SUCCESS;
}

static TEE_Result get_seal_key(struct cyres_pta_sess_ctx *ctx,
			       uint8_t *secret,
			       uint32_t secret_size,
			       const uint8_t *key_selector,
			       uint32_t key_selector_size)
{
	TEE_Result res;
	RIOT_STATUS status;
	void *hash_ctx = NULL;
	uint8_t hash[TEE_SHA256_HASH_SIZE];

	res = crypto_hash_alloc_ctx(&hash_ctx, TEE_ALG_SHA256);
	if (res)
		goto end;

	res = crypto_hash_init(hash_ctx, TEE_ALG_SHA256);
	if (res)
		goto end;

	/* hash the TA private key */
	res = crypto_hash_update(hash_ctx,
				 TEE_ALG_SHA256,
				 (const uint8_t *)&ctx->ta_key_pair.priv,
				 sizeof(ctx->ta_key_pair.priv));
	if (res)
		goto end;

	res = crypto_hash_final(hash_ctx, TEE_ALG_SHA256, hash, sizeof(hash));
	if (res)
		goto end;

	status = RiotCrypt_Kdf(secret,
			       secret_size,
			       hash,
			       sizeof(hash),
			       key_selector,
			       key_selector_size,
			       (const uint8_t *)"PTA_SEAL_KDF",
			       sizeof("PTA_SEAL_KDF") - 1,
			       secret_size);

	if (status != RIOT_SUCCESS) {
		EMSG("RiotCrypt_Kdf() failed: 0x%x", status);
		res = tee_result_from_riot(status);
		goto end;
	}

end:
	if (hash_ctx)
		crypto_hash_free_ctx(hash_ctx, TEE_ALG_SHA256);

	return res;
}
static TEE_Result handle_get_private_key_size(
		struct cyres_pta_sess_ctx *ctx,
		uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Result res;
	size_t size;
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
			TEE_PARAM_TYPE_NONE,
			TEE_PARAM_TYPE_NONE,
			TEE_PARAM_TYPE_NONE);

	if (exp_pt != param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	res = get_ta_private_key(ctx, NULL, &size);
	if (res == TEE_ERROR_SHORT_BUFFER) {
		params[0].value.a = (uint32_t)size;
		res = TEE_SUCCESS;
	}
	return res;
}

static TEE_Result handle_get_private_key(
		struct cyres_pta_sess_ctx *ctx,
		uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Result res;
	size_t size;
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
			TEE_PARAM_TYPE_NONE,
			TEE_PARAM_TYPE_NONE,
			TEE_PARAM_TYPE_NONE);

	if (exp_pt != param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	size = params[0].memref.size;
	res = get_ta_private_key(ctx, (char *)params[0].memref.buffer, &size);
	return res;
}

static TEE_Result handle_get_public_key_size(
		struct cyres_pta_sess_ctx *ctx,
		uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Result res;
	size_t size;
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
			TEE_PARAM_TYPE_NONE,
			TEE_PARAM_TYPE_NONE,
			TEE_PARAM_TYPE_NONE);

	if (exp_pt != param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	res = get_ta_public_key(ctx, NULL, &size);
	if (res == TEE_ERROR_SHORT_BUFFER) {
		params[0].value.a = (uint32_t)size;
		res = TEE_SUCCESS;
	}

	return res;
}

static TEE_Result handle_get_public_key(
		struct cyres_pta_sess_ctx *ctx,
		uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Result res;
	size_t size;
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
			TEE_PARAM_TYPE_NONE,
			TEE_PARAM_TYPE_NONE,
			TEE_PARAM_TYPE_NONE);

	if (exp_pt != param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	size = params[0].memref.size;
	res = get_ta_public_key(ctx, (char *)params[0].memref.buffer, &size);
	return res;
}

static TEE_Result handle_get_cert_chain_size(
		struct cyres_pta_sess_ctx *ctx,
		uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Result res;
	size_t size = 0;
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
			TEE_PARAM_TYPE_NONE,
			TEE_PARAM_TYPE_NONE,
			TEE_PARAM_TYPE_NONE);

	if (exp_pt != param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	res = get_ta_cert_chain(ctx, NULL, &size);
	if (res == TEE_ERROR_SHORT_BUFFER) {
		params[0].value.a = (uint32_t)size;
		res = TEE_SUCCESS;
	}

	return res;
}

static TEE_Result handle_get_cert_chain(
		struct cyres_pta_sess_ctx *ctx,
		uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Result res;
	size_t size;
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,
			TEE_PARAM_TYPE_NONE,
			TEE_PARAM_TYPE_NONE,
			TEE_PARAM_TYPE_NONE);

	if (exp_pt != param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	size = params[0].memref.size;
	res = get_ta_cert_chain(ctx, (char *)params[0].memref.buffer, &size);
	return res;
}

static TEE_Result handle_get_seal_key(
		struct cyres_pta_sess_ctx *ctx,
		uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Result res;
	uint32_t type;

	type = TEE_PARAM_TYPE_GET(param_types, 0);
	if (type != TEE_PARAM_TYPE_MEMREF_OUTPUT)
		return TEE_ERROR_BAD_PARAMETERS;

	type = TEE_PARAM_TYPE_GET(param_types, 1);
	if (type != TEE_PARAM_TYPE_NONE &&
	    type != TEE_PARAM_TYPE_MEMREF_INPUT)
		return TEE_ERROR_BAD_PARAMETERS;

	res = get_seal_key(ctx,
			   params[0].memref.buffer, params[0].memref.size,
			   params[1].memref.buffer, params[1].memref.size);
	return res;
}

static TEE_Result cyres_invoke_command(void *sess_ctx, uint32_t cmd_id,
		uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Result res;
	struct cyres_pta_sess_ctx *ctx = sess_ctx;

	switch (cmd_id) {
	case PTA_CYRES_GET_PRIVATE_KEY_SIZE:
		return handle_get_private_key_size(ctx, param_types, params);
	case PTA_CYRES_GET_PRIVATE_KEY:
		return handle_get_private_key(ctx, param_types, params);
	case PTA_CYRES_GET_PUBLIC_KEY_SIZE:
		return handle_get_public_key_size(ctx, param_types, params);
	case PTA_CYRES_GET_PUBLIC_KEY:
		return handle_get_public_key(ctx, param_types, params);
	case PTA_CYRES_GET_CERT_CHAIN_SIZE:
		return handle_get_cert_chain_size(ctx, param_types, params);
	case PTA_CYRES_GET_CERT_CHAIN:
		return handle_get_cert_chain(ctx, param_types, params);
	case PTA_CYRES_GET_SEAL_KEY:
		return handle_get_seal_key(ctx, param_types, params);
	default:
		EMSG("Command not implemented %d", cmd_id);
		res = TEE_ERROR_NOT_IMPLEMENTED;
		break;
	}

	return res;
}

static TEE_Result cyres_open_session(
		uint32_t param_types __unused,
		TEE_Param params[TEE_NUM_PARAMS] __unused,
		void **sess_ctx)
{
	TEE_Result res;
	struct cyres_pta_sess_ctx *ctx;

	if (!cert_blob) {
		EMSG("Failed to import cert chain, Cyres unavailable");
		return TEE_ERROR_BAD_STATE;
	}

	ctx = (struct cyres_pta_sess_ctx *)malloc(
			sizeof(struct cyres_pta_sess_ctx));

	if (!ctx)
		return TEE_ERROR_OUT_OF_MEMORY;

	/*
	 * let close_session handle cleanup, which is called even in
	 * case of failure
	 */
	memset(ctx, 0, sizeof(struct cyres_pta_sess_ctx));
	*sess_ctx = ctx;

	res = gen_ta_cert(ctx);

	return res;
}

/* close session is always called, even if open failed */
static void cyres_close_session(void *sess_ctx)
{
	struct cyres_pta_sess_ctx *ctx = sess_ctx;

	if (ctx) {
		if (ctx->ta_cert)
			cyres_free_cert(ctx->ta_cert);
		free(ctx);
	}
}

/* copy the certificate chain to OPTEE-owned memory */
static TEE_Result capture_cert_blob(void)
{
	cyres_result cy_res;
	struct cyres_cert_blob *in_blob;
	void *virt;
	void *buf;
	size_t size;

	if (!core_mmu_add_mapping(MEM_AREA_IO_NSEC, CFG_CYRES_CERT_CHAIN_ADDR,
				CFG_DTB_MAX_SIZE))
		panic("failed to map memory for cert blob");

	virt = phys_to_virt(CFG_CYRES_CERT_CHAIN_ADDR, MEM_AREA_IO_NSEC);
	if (!virt)
		panic();

	cy_res = cyres_open_cert_blob(virt, &in_blob);
	if (cy_res)
		return tee_result_from_cyres(cy_res);

	/* allocate space to hold the certificate chain */
	size = cyres_get_cert_blob_size(in_blob);
	buf = malloc(size);
	if (!buf)
		return TEE_ERROR_OUT_OF_MEMORY;

	cy_res = cyres_move_cert_blob(in_blob, buf, size, &cert_blob);
	if (cy_res) {
		free(buf);
		return tee_result_from_cyres(cy_res);
	}

	return TEE_SUCCESS;
}

static TEE_Result capture_private_key(void)
{
	cyres_result cy_res;
	void *virt;

	if (!core_mmu_add_mapping(MEM_AREA_IO_NSEC, CFG_CYRES_KEY_ADDR,
				CORE_MMU_PGDIR_SIZE))
		panic("failed to map key store");

	virt = phys_to_virt(CFG_CYRES_KEY_ADDR, MEM_AREA_IO_NSEC);
	if (!virt)
		panic();

	cy_res = cyres_take_key_from_blob(virt, &optee_key_pair);
	if (cy_res)
		return tee_result_from_cyres(cy_res);

	return TEE_SUCCESS;
}

static TEE_Result pta_cyres_init(void)
{
	TEE_Result res;

	res = capture_cert_blob();
	if (res == TEE_SUCCESS)
		IMSG("Successfully captured Cyres certificate chain");
	else
		EMSG("Failed to capture Cyres certificate chain (0x%x)", res);

	res = capture_private_key();
	if (res == TEE_SUCCESS)
		IMSG("Successfully captured Cyres private key");
	else
		EMSG("Failed to capture Cyres private key (0x%x)", res);

	return TEE_SUCCESS;
}

service_init_late(pta_cyres_init);

pseudo_ta_register(.uuid = PTA_CYRES_UUID, .name = "pta_cyres",
		.flags = PTA_DEFAULT_FLAGS,
		.open_session_entry_point = cyres_open_session,
		.close_session_entry_point = cyres_close_session,
		.invoke_command_entry_point = cyres_invoke_command);
