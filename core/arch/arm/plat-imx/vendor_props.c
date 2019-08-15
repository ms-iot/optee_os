// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2016, Linaro Limited.
 */
#include <tee/tee_svc.h>
#include <user_ta_header.h>
#include <util.h>
#include <kernel/tee_ta_manager.h>
#include <kernel/tee_common_otp.h>
#include <tee/tee_cryp_utl.h>

/*
 * The data to hash is 64 bytes made up of:
 * - 16 bytes: the UUID of the calling TA.
 * - 16 bytes: the hardware unique key
 * - 32 bytes: the die ID
 *
 * For future-proofing the resulting seed is hashed to 64 bytes.
 */
static TEE_Result get_prop_endorsement(struct tee_ta_session *sess,
				       void *buf, size_t *blen)
{
	const uint32_t algo = TEE_ALG_SHA512;
	void *ctx = NULL;
	uint8_t die_id[TEE_SHA256_HASH_SIZE];
	uint8_t digest[TEE_SHA512_HASH_SIZE];
	struct tee_hw_unique_key hwkey = {0};
	TEE_Result res = TEE_SUCCESS;
	const TEE_UUID *uuid = NULL;

	EMSG("Generating a new endorsement seed for the fTPM");

	if (*blen < sizeof(digest)) {
		*blen = sizeof(digest);
		res = TEE_ERROR_SHORT_BUFFER;
		goto out;
	}
	*blen = sizeof(digest);

	// Generate a new endorsement seed
	res = crypto_hash_alloc_ctx(&ctx, algo);
	if (res)
		goto out;

	res = crypto_hash_init(ctx, algo);
	if (res)
		goto out;

	// Gather the data to hash
	uuid = &(sess->ctx->uuid);
	res = crypto_hash_update(ctx, algo, (uint8_t *)uuid, sizeof(*uuid));
	if (res)
		goto out;

	res = tee_otp_get_hw_unique_key(&hwkey);
	if (res)
		goto out;
	res = crypto_hash_update(ctx, algo, (uint8_t *)&hwkey, sizeof(hwkey));
	if (res)
		goto out;

	res = tee_otp_get_die_id(die_id, sizeof(die_id));
	if (res)
		goto out;
	res = crypto_hash_update(ctx, algo, (uint8_t *)die_id, sizeof(die_id));
	if (res)
		goto out;

	res = crypto_hash_final(ctx, algo, digest, sizeof(digest));
	if (res)
		goto out;

out:
	if (ctx)
		crypto_hash_free_ctx(ctx, algo);

	if (res)
		return res;
	else
		return tee_svc_copy_to_user((void *)buf, digest, sizeof(digest));
}

static const struct tee_props vendor_propset_array_tee[] = {
	{
		.name = "com.microsoft.ta.endorsementSeed",
		.prop_type = USER_TA_PROP_TYPE_BINARY_BLOCK,
		.get_prop_func = get_prop_endorsement
	},
};

const struct tee_vendor_props vendor_props_tee = {
	.props = vendor_propset_array_tee,
	.len = ARRAY_SIZE(vendor_propset_array_tee),
};
