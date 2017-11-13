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
#include <CyrepCommon.h>
#include <platform_config.h>
#include <stdio.h>
#include <pta_cyrep.h>

struct cyrep_pta_sess_ctx {
	CyrepKeyPair ta_key_pair;
	CyrepCertChain cert_chain;
	char cert_chain_buffer[1024];
};

static const uint8_t
fake_optee_key_pair[sizeof(CyrepKeyPair)] = {
0xc8, 0xaf, 0x8e, 0xc1, 0xd8, 0x96, 0x43, 0x1a, 0x73, 0xc9, 0x86, 0x85, 0x3c,
0x8d, 0x25, 0xc7, 0xef, 0xfc, 0x39, 0x6a, 0x57, 0x7b, 0x04, 0xb4, 0xc2, 0x01,
0xc1, 0xe0, 0x95, 0x12, 0x35, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x3d, 0xfb, 0x3a,
0x19, 0xef, 0x58, 0x8c, 0xca, 0xf9, 0xa9, 0x77, 0x63, 0x53, 0x6e, 0xce, 0x65,
0x6f, 0x75, 0x5e, 0xfb, 0xda, 0xdb, 0xbf, 0xd3, 0xdf, 0x64, 0x9b, 0x68, 0xdb,
0xcd, 0x35, 0x2d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x82,
0x5c, 0x89, 0x2c, 0xbe, 0x33, 0xa6, 0x8c, 0xfb, 0x86, 0x98, 0xb4, 0x9c, 0xc8,
0xec, 0x49, 0x85, 0x1f, 0x5c, 0x5f, 0x89, 0x11, 0xec, 0x7b, 0x47, 0x7c, 0xbe,
0xf4, 0xd2, 0x41, 0x26, 0x00, 0x00, 0x00, 0x00,
};

static const char *fake_cert_chain = R"STR(
-----BEGIN CERTIFICATE-----
MIIClDCCAjqgAwIBAgIQHbwS7zMYw2FO2j36Sq+M9TAKBggqhkjOPQQDAjAuMQwwCgYDVQQDDANTUEwxCzAJBgNVBAYMAlVTMREwDwYDVQQKDAhNU1JfVEVTVDAeFw0xNzAxMDEwMDAwMDBaFw0zNzAxMDEwMDAwMDBaMDAxDjAMBgNVBAMMBU9QVEVFMQswCQYDVQQGDAJVUzERMA8GA1UECgwITVNSX1RFU1QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAS4NRKV4MEBwrQEe1dqOfzvxyWNPIWGyXMaQ5bYwY6vyC01zdtom2Tf07/b2vtedW9lzm5TY3ep+cqMWO8ZOvs9o4IBNjCCATIwKQYDVR0OBCIEIFlCRTgRQojj88/NqGOpoa1k/9kdCK7LSK7QaJN3/Yy/MCsGA1UdAQQkMCKAINH+wKxe/7sJqOg+5CJgpn3lzbUeQjmifpx2IBITEKyGMBIGA1UdEwEB/wQIMAYBAf8CAQIwDgYDVR0PBAcDBQCGAAAAMBYGA1UdJQEB/wQMMAoGCCsGAQUFBwMCMIGbBgZngQUFBAEEgZAwgY0CAQEwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATyWkfWMLQaEwHF+beVkqria7+M/bUNTfdbt2orNGKc91aLMQRRLNnmAp0ITc5twlkeli5u5ppbA8JmzG0BNOTxMC0GCWCGSAFlAwQCAQQguTQTQafSGDIi2s9E9GG9wzlGK4kiM0PL4vlUdDxUeoEwCgYIKoZIzj0EAwIDSAAwRQIhAJhCJVhdIoXBOAM9YUDjzvi5GFlwTlPDE/i2NrpPlnZJAiBFEEmhU5B7Qt1bZlC0if8SwCOqUK1q+1SIjPNZ1TAr7Q==
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIICdzCCAh2gAwIBAgIQM/mosM3+IEYCiFUPb0JhCTAKBggqhkjOPQQDAjA3MRUwEwYDVQQDDAxDb250b3NvIEx0ZC4xCzAJBgNVBAYMAlVTMREwDwYDVQQKDAhNU1JfVEVTVDAeFw0xNzAxMDEwMDAwMDBaFw0zNzAxMDEwMDAwMDBaMC4xDDAKBgNVBAMMA1NQTDELMAkGA1UEBgwCVVMxETAPBgNVBAoMCE1TUl9URVNUMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE8lpH1jC0GhMBxfm3lZKq4mu/jP21DU33W7dqKzRinPdWizEEUSzZ5gKdCE3ObcJZHpYubuaaWwPCZsxtATTk8aOCARIwggEOMCkGA1UdDgQiBCDR/sCsXv+7CajoPuQiYKZ95c21HkI5on6cdiASExCshjArBgNVHQEEJDAigCDIcapPcwWy9ivS9Ktn3f2SZYdkUEki3xqWgOIX46k1EDASBgNVHRMBAf8ECDAGAQH/AgEDMA4GA1UdDwQHAwUAhgAAADCBjwYGZ4EFBQQBBIGEMIGBAgEBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE8lpH1jC0GhMBxfm3lZKq4mu/jP21DU33W7dqKzRinPdWizEEUSzZ5gKdCE3ObcJZHpYubuaaWwPCZsxtATTk8TAhBglghkgBZQMEAgEEFE5vdiAxNyAyMDE3LTExOjA4OjQ0MAoGCCqGSM49BAMCA0gAMEUCIQCYQiVYXSKFwTgDPWFA4874uRhZcE5TwxP4tja6T5Z2SQIgabBF7UCDCpu5/mZZRuMjNU+/WyYEKVnkK9owQPu5k/Y=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIB6jCCAZCgAwIBAgIQbeRWJ+6EW2Qr3t7M7J5etjAKBggqhkjOPQQDAjA3MRUwEwYDVQQDDAxDb250b3NvIEx0ZC4xCzAJBgNVBAYMAlVTMREwDwYDVQQKDAhNU1JfVEVTVDAeFw0xNzAxMDEwMDAwMDBaFw0zNzAxMDEwMDAwMDBaMDcxFTATBgNVBAMMDENvbnRvc28gTHRkLjELMAkGA1UEBgwCVVMxETAPBgNVBAoMCE1TUl9URVNUMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEaataJqFSD8nsXaWV9JdIuf7SPQnFnR+nUNOUScj8nOvfHluw/Z5GmmPSN+UE+9T6onBPsQWerZrVPPg5YrHOfaN+MHwwKQYDVR0OBCIEIMhxqk9zBbL2K9L0q2fd/ZJlh2RQSSLfGpaA4hfjqTUQMCsGA1UdAQQkMCKAIMhxqk9zBbL2K9L0q2fd/ZJlh2RQSSLfGpaA4hfjqTUQMA4GA1UdDwQHAwUAhgAAADASBgNVHRMBAf8ECDAGAQH/AgEEMAoGCCqGSM49BAMCA0gAMEUCIQCYQiVYXSKFwTgDPWFA4874uRhZcE5TwxP4tja6T5Z2SQIgMzCqbneueXRUUhfWUpSg++tsv9LDkon+gWfEqlcm26E=
-----END CERTIFICATE-----
)STR";

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
	CyrepKeyPair *key_pair_out;

	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);

	DMSG("PTA_CYREP_GET_PRIVATE_KEY\n");

	if (exp_pt != param_types) {
		return TEE_ERROR_BAD_PARAMETERS;
	}

	if (params[0].memref.size < sizeof(CyrepKeyPair)) {
		return TEE_ERROR_SHORT_BUFFER;
	}

	key_pair_out = params[0].memref.buffer;

	memcpy(key_pair_out, &ctx->ta_key_pair, sizeof(*key_pair_out));

	return TEE_SUCCESS;
}

static TEE_Result cyrep_get_cert_chain(
	const struct cyrep_pta_sess_ctx *ctx,
	uint32_t param_types,
	TEE_Param params[TEE_NUM_PARAMS])
{
	size_t req_len;
	uint32_t exp_pt = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE,
		TEE_PARAM_TYPE_NONE);

	DMSG("PTA_CYREP_GET_CERT_CHAIN\n");

	if (exp_pt != param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	// Compute required length of cert chain
	req_len = strlen(fake_cert_chain) +
		  strlen(ctx->cert_chain.CertBuffer) + 1;

	if (params[0].memref.size < req_len) {
		params[0].value.a = (uint32_t)req_len;
		return TEE_ERROR_SHORT_BUFFER;
	}

	strlcpy(
		params[0].memref.buffer,
		ctx->cert_chain.CertBuffer,
		params[0].memref.size);

	strlcat(
		params[0].memref.buffer,
		fake_cert_chain,
		params[0].memref.size);

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

	orig_cert_chain = (CyrepCertChain *)CYREP_CERT_CHAIN_ADDR;

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
	// XXX remove this block when real CYREP chain is taken from
	// previous boot stage 
#if 1
	memcpy(&optee_key_pair, fake_optee_key_pair, sizeof(optee_key_pair));
#else
	CyrepKeyPair *key_pair_in = (CyrepKeyPair *)phys_to_virt(
		(paddr_t)CYREP_OPTEE_KEY_PAIR_ADDR,
		MEM_AREA_IO_SEC);

	Cyrep_CapturePrivateKey(&optee_key_pair, key_pair_in);
#endif
	return TEE_SUCCESS;
}

service_init(pta_cyrep_init);

pseudo_ta_register(.uuid = PTA_CYREP_UUID, .name = "pta_cyrep",
		   .flags = PTA_DEFAULT_FLAGS,
		   .open_session_entry_point = cyrep_open_session,
		   .close_session_entry_point = cyrep_close_session,
		   .invoke_command_entry_point = cyrep_invoke_command);
