/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Microsoft Corporation. All rights reserved.
 *
 * Routines for building and manipulating Cyres certificate chains.
 */

#ifndef __CYRES_CERT_CHAIN_H__
#define __CYRES_CERT_CHAIN_H__

#include <RiotTarget.h>
#include <RiotStatus.h>
#include <RiotEcc.h>

#define CYRES_SUCCESS			0x00000000
#define CYRES_ERROR_GENERIC		0xFFFF0000
#define CYRES_ERROR_BAD_FORMAT		0xFFFF0005
#define CYRES_ERROR_BAD_PARAMETERS	0xFFFF0006
#define CYRES_ERROR_NOT_IMPLEMENTED	0xFFFF0009
#define CYRES_ERROR_SHORT_BUFFER	0xFFFF0010
#define CYRES_ERROR_OUT_OF_MEMORY	0xFFFF000C
#define CYRES_ERROR_OVERFLOW		0xFFFF300F
#define CYRES_ERROR_BAD_VERSION		0xF0010001

#define CYRES_KEY_BLOB_MAGIC		0x43595253 /* CYRS */
#define CYRES_KEY_BLOB_VERSION		0x00000001
#define CYRES_CERT_CHAIN_VERSION	0x00000001

typedef uint32_t cyres_result;

struct cyres_key_pair {
	RIOT_ECC_PUBLIC pub;
	RIOT_ECC_PRIVATE priv;
};

struct cyres_cert_blob;
struct cyres_cert;
struct cyres_key_blob;

cyres_result cyres_init_cert_blob(void *buf, size_t buf_size,
				  struct cyres_cert_blob **blob);

cyres_result cyres_open_cert_blob(void *buf,
				  struct cyres_cert_blob **blob);

size_t cyres_get_cert_blob_size(const struct cyres_cert_blob *blob);

cyres_result cyres_move_cert_blob(const struct cyres_cert_blob *in_blob,
				  void *buf, size_t size,
				  struct cyres_cert_blob **out_blob);

struct cyres_root_cert_args {
	const uint8_t *identity;
	size_t identity_size;
	const uint8_t *fwid;
	size_t fwid_size;
	const uint8_t *auth_key_pub;
	size_t auth_key_pub_size;
	const char *device_cert_subject;
	int root_path_len;
};

cyres_result
cyres_insert_root_and_device_certs(struct cyres_cert_blob *blob,
				   const struct cyres_root_cert_args *args,
				   struct cyres_key_pair *device_key_pair);

struct cyres_gen_alias_cert_args {
	const struct cyres_key_pair *issuer_key_pair;
	const void *seed_data;
	size_t seed_data_size;
	const void *subject_digest;
	size_t subject_digest_size;
	const uint8_t *auth_key_pub;
	size_t auth_key_pub_size;
	const char *subject_name;
	const char *issuer_name;
	uint32_t path_len;
};

cyres_result cyres_gen_alias_cert(const struct cyres_gen_alias_cert_args *args,
				  struct cyres_cert **cert,
				  struct cyres_key_pair *subject_key_pair);

cyres_result cyres_insert_cert(struct cyres_cert_blob *blob,
			       const struct cyres_cert *cert);

cyres_result cyres_cert_to_pem(const struct cyres_cert *cert,
			       char *buf, size_t *buf_size);

cyres_result cyres_priv_key_to_pem(const struct cyres_key_pair *key,
				   char *buf, size_t *buf_size);

cyres_result cyres_pub_key_to_pem(const RIOT_ECC_PUBLIC *key,
				  char *buf, size_t *buf_size);

cyres_result cyres_get_cert_chain_pem(const struct cyres_cert_blob *blob,
				      const char *subject, char *buf,
				      size_t *buf_size);

void cyres_free_cert(struct cyres_cert *cert);

void cyres_zero_mem(void *mem, size_t size);

cyres_result cyres_make_key_blob_inplace(void *buf, size_t buf_size,
					 const struct cyres_key_pair *key);

cyres_result cyres_take_key_from_blob(void *keyblob,
				      struct cyres_key_pair *key);

#endif /* __CYRES_CERT_CHAIN_H__ */
