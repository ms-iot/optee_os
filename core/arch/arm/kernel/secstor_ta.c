// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2017, Linaro Limited
 */

#include <tee/tadb.h>
#include <kernel/user_ta.h>
#include <kernel/user_ta_store.h>
#include <initcall.h>
#include <crypto/crypto.h>
#include <ta_pub_key.h>

static TEE_Result secstor_ta_open(const TEE_UUID *uuid,
				  struct user_ta_store_handle **handle)
{
	TEE_Result res;
	struct tee_tadb_ta_read *ta;
	size_t l;
	const struct tee_tadb_property *prop;

	res = tee_tadb_ta_open(uuid, &ta);
	if (res)
		return res;
	prop = tee_tadb_ta_get_property(ta);

	l = prop->custom_size;
	res = tee_tadb_ta_read(ta, NULL, &l);
	if (res)
		goto err;
	if (l != prop->custom_size) {
		res = TEE_ERROR_CORRUPT_OBJECT;
		goto err;
	}

	*handle = (struct user_ta_store_handle *)ta;

	return TEE_SUCCESS;
err:
	tee_tadb_ta_close(ta);
	return res;
}

static TEE_Result secstor_ta_get_size(const struct user_ta_store_handle *h,
				      size_t *size)
{
	struct tee_tadb_ta_read *ta = (struct tee_tadb_ta_read *)h;
	const struct tee_tadb_property *prop = tee_tadb_ta_get_property(ta);

	*size = prop->bin_size;

	return TEE_SUCCESS;
}

static TEE_Result secstor_ta_get_tag(const struct user_ta_store_handle *h,
				     uint8_t *tag, unsigned int *tag_len)
{
	return tee_tadb_get_tag((struct tee_tadb_ta_read *)h, tag, tag_len);
}

static TEE_Result secstor_ta_read(struct user_ta_store_handle *h, void *data,
				  size_t len)
{
	struct tee_tadb_ta_read *ta = (struct tee_tadb_ta_read *)h;
	size_t l = len;
	TEE_Result res = tee_tadb_ta_read(ta, data, &l);

	if (res)
		return res;
	if (l != len)
		return TEE_ERROR_BAD_PARAMETERS;

	return TEE_SUCCESS;
}

static void secstor_ta_close(struct user_ta_store_handle *h)
{
	struct tee_tadb_ta_read *ta = (struct tee_tadb_ta_read *)h;

	tee_tadb_ta_close(ta);
}

#ifdef CFG_ATTESTATION_MEASURE
static TEE_Result secstor_ta_get_measurement(struct user_ta_store_handle *h,
					     uint8_t *binary_measurement,
					     size_t *measurement_len)
{
	struct tee_tadb_ta_read *ta = (struct tee_tadb_ta_read *)h;

	return tee_tadb_get_measurement(ta, binary_measurement,
					measurement_len);
}

static TEE_Result secstor_ta_get_signer(struct user_ta_store_handle *h __unused,
					uint8_t *signer_measurement,
					size_t *measurement_len)
{
	uint32_t hash_algo = ATTESTATION_MEASUREMENT_ALGO;
	void *hash_ctx = NULL;
	TEE_Result res = TEE_SUCCESS;

	if (!signer_measurement ||
	    *measurement_len < ATTESTATION_MEASUREMENT_SIZE) {
		*measurement_len = ATTESTATION_MEASUREMENT_SIZE;
		return TEE_ERROR_SHORT_BUFFER;
	}
	*measurement_len = ATTESTATION_MEASUREMENT_SIZE;

	res = crypto_hash_alloc_ctx(&hash_ctx, hash_algo);
	if (res != TEE_SUCCESS)
		goto error_free_hash;

	res = crypto_hash_init(hash_ctx, hash_algo);
	if (res != TEE_SUCCESS)
		goto error_free_hash;

	res = crypto_hash_update(hash_ctx, hash_algo,
				 (uint8_t *)&ta_pub_key_exponent,
				 sizeof(ta_pub_key_exponent));
	if (res != TEE_SUCCESS)
		goto error_free_hash;

	res = crypto_hash_update(hash_ctx, hash_algo,
				 (uint8_t *)ta_pub_key_modulus,
				 ta_pub_key_modulus_size);
	if (res != TEE_SUCCESS)
		goto error_free_hash;

	res = crypto_hash_final(hash_ctx, hash_algo,
				signer_measurement,
				ATTESTATION_MEASUREMENT_SIZE);
error_free_hash:
	crypto_hash_free_ctx(hash_ctx, hash_algo);
	return res;
}

static TEE_Result secstor_ta_get_version(struct user_ta_store_handle *h,
					 uint32_t *version)
{
	struct tee_tadb_ta_read *ta = (struct tee_tadb_ta_read *)h;

	if (!version)
		return TEE_ERROR_BAD_PARAMETERS;

	*version = tee_tadb_get_version(ta);
	return TEE_SUCCESS;
}
#endif /* CFG_ATTESTATION_MEASURE */

TEE_TA_REGISTER_TA_STORE(4) = {
	.description = "Secure Storage TA",
	.open = secstor_ta_open,
	.get_size = secstor_ta_get_size,
	.get_tag = secstor_ta_get_tag,
	.read = secstor_ta_read,
	.close = secstor_ta_close,
#ifdef CFG_ATTESTATION_MEASURE
	.get_measurement = secstor_ta_get_measurement,
	.get_version = secstor_ta_get_version,
	.get_signer = secstor_ta_get_signer,
#endif
};
