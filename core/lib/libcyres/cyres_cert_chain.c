// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Microsoft Corporation. All rights reserved.
 *
 * Contains functions to manipulate certificate chains. Certificate chains
 * are passed between boot stages, and each stage must agree on it's format.
 * We use FDT to store a certificate chain.
 */

#ifdef CFG_OPTEE_REVISION_MAJOR
#define __OPTEE__
#endif

#ifdef __UBOOT__
#include <common.h>
#include <linux/libfdt.h>
#include <fdt_support.h>
#endif

#ifdef __OPTEE__
#include <types_ext.h>
#include <stdio.h>
#include <string.h>
#include <string_ext.h>
#include <libfdt.h>
#endif

#include <malloc.h>

#include <RiotTarget.h>
#include <RiotStatus.h>
#include <RiotSha256.h>
#include <RiotHmac.h>
#include <RiotKdf.h>
#include <RiotAes128.h>
#include <RiotKdf.h>
#include <RiotEcc.h>
#include <RiotDerEnc.h>
#include <RiotX509Bldr.h>
#include <RiotCrypt.h>
#include <TcpsId.h>

#include <cyres_cert_chain.h>

#ifdef __UBOOT__
#define _NEED_STRLCAT
#endif

#ifdef __OPTEE__
#define _NEED_FDT_CREATE_PHANDLE
#define _NEED_FDT_SETPROP_PLACEHOLDER
#endif

#define STR_LITERAL_LEN(s) (sizeof(s) - 1)

struct cyres_cert_blob {
	struct fdt_header fdt;	/* must be first member of struct */
	char _body[1];
};

struct cyres_cert {
	char subject_name[128];	/* DERAddUTF8String asserts less than 127 */
	char issuer_name[128];
	DERBuilderContext context;
	uint8_t buf[DER_MAX_TBS];
};

/*
 * Structure for passing keys between boot stages. Stored separately
 * from the cert chain because a module's private key must be hidden
 * by the module, whereas the certficate chain is public.
 *
 * We do not use FDT for the key blob because we need guarantees
 * around clearing of memory, and libfdt does not make any promises
 * in that regard.
 */
struct cyres_key_blob {
	uint32_t magic;
	uint32_t version;
	struct cyres_key_pair key;
};

/* Static data fields that make up the "root signer" Cert */
static const RIOT_X509_TBS_DATA root_cert_tbs_template = { { 0 },
	"Manufacturer", "MSR_TEST", "US",
	"170101000000Z", "370101000000Z",
	"Manufacturer", "MSR_TEST", "US" };

/* Static data fields that make up the DeviceID Cert "to be signed" region */
static const RIOT_X509_TBS_DATA device_cert_tbs_template = { { 0 },
	"Manufacturer", "MSR_TEST", "US",
	"170101000000Z", "370101000000Z",
	"RIoT Device", "MSR_TEST", "US" };

/* Static data fields that make up the Alias Cert "to be signed" region */
static const RIOT_X509_TBS_DATA alias_cert_tbs_template = { { 0 },
	"RIoT Device", "MSR_TEST", "US",
	"170101000000Z", "370101000000Z",
	"RIoT Fw", "MSR_TEST", "US" };

/*
 * The "root" signing key. This is intended for development purposes only.
 * This key is used to sign the DeviceID certificate, the certificiate for
 * this "root" key represents the "trusted" CA for the developer-mode DPS
 * server(s). Again, this is for development purposes only and (obviously)
 * provides no meaningful security whatsoever.
 */
static const uint8_t test_ecc_pub[sizeof(ecc_publickey)] = {
	0xeb, 0x9c, 0xfc, 0xc8, 0x49, 0x94, 0xd3, 0x50, 0xa7, 0x1f, 0x9d, 0xc5,
	0x09, 0x3d, 0xd2, 0xfe, 0xb9, 0x48, 0x97, 0xf4, 0x95, 0xa5, 0x5d, 0xec,
	0xc9, 0x0f, 0x52, 0xa1, 0x26, 0x5a, 0xab, 0x69, 0x00, 0x00, 0x00, 0x00,
	0x7d, 0xce, 0xb1, 0x62, 0x39, 0xf8, 0x3c, 0xd5, 0x9a, 0xad, 0x9e, 0x05,
	0xb1, 0x4f, 0x70, 0xa2, 0xfa, 0xd4, 0xfb, 0x04, 0xe5, 0x37, 0xd2, 0x63,
	0x9a, 0x46, 0x9e, 0xfd, 0xb0, 0x5b, 0x1e, 0xdf, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00 };

static const uint8_t test_ecc_priv[sizeof(ecc_privatekey)] = {
	0xe3, 0xe7, 0xc7, 0x13, 0x57, 0x3f, 0xd9, 0xc8, 0xb8, 0xe1, 0xea, 0xf4,
	0x53, 0xf1, 0x56, 0x15, 0x02, 0xf0, 0x71, 0xc0, 0x53, 0x49, 0xc8, 0xda,
	0xe6, 0x26, 0xa9, 0x0b, 0x17, 0x88, 0xe5, 0x70, 0x00, 0x00, 0x00, 0x00
};

#ifdef _NEED_STRLCAT
static size_t strlcat(char *dst, const char *src, size_t size)
{
	size_t len;

	if (size == 0)
		return 0;

	len = strnlen(dst, size - 1);
	dst += len;
	size -= len;
	return len + strlcpy(dst, src, size);
}
#endif

#ifdef _NEED_FDT_CREATE_PHANDLE
static unsigned int fdt_create_phandle(void *fdt, int nodeoffset)
{
	(void)fdt;
	(void)nodeoffset;
	return 0;
}
#endif

static cyres_result cyres_result_from_riot(RIOT_STATUS status)
{
	// TODO fill out conversion
	switch (status) {
	case RIOT_SUCCESS: return CYRES_SUCCESS;
	default:
		break;
	}

	return CYRES_ERROR_GENERIC;
}

static cyres_result cyres_result_from_x509(int val)
{
	// TODO fill out conversion table
	switch (val) {
	case 0: return CYRES_SUCCESS;
	default:
		break;
	}

	return CYRES_ERROR_GENERIC;
}

static cyres_result cyres_result_from_fdt(int val)
{
	/* error values are compatible; pass through unmodified */
	return (cyres_result)val;
}

static cyres_result cyres_alloc_cert(const char *issuer_name,
				     const char *subject_name,
				     struct cyres_cert **cert_out)
{
	struct cyres_cert *cert;

	cert = (struct cyres_cert *)malloc(sizeof(struct cyres_cert));
	if (!cert)
		return CYRES_ERROR_OUT_OF_MEMORY;

	if (strlcpy(cert->subject_name, subject_name,
		    sizeof(cert->subject_name)) >=
	    sizeof(cert->subject_name)) {
		free(cert);
		return CYRES_ERROR_OVERFLOW;
	}

	if (strlcpy(cert->issuer_name, issuer_name,
		    sizeof(cert->issuer_name)) >=
	    sizeof(cert->issuer_name)) {
		free(cert);
		return CYRES_ERROR_OVERFLOW;
	}

	DERInitContext(&cert->context, cert->buf, sizeof(cert->buf));

	*cert_out = cert;
	return CYRES_SUCCESS;
}

static cyres_result cyres_derive_cert_serial_num(const uint8_t *source,
						 size_t source_size,
						 RIOT_X509_TBS_DATA *tbs_data
						)
{
	RIOT_STATUS status;
	uint8_t digest[RIOT_DIGEST_LENGTH];

	// Generate serial number
	status = RiotCrypt_Kdf(digest,
			       sizeof(digest),
			       source,
			       source_size,
			       NULL,
			       0,
			       (const uint8_t *)RIOT_LABEL_SERIAL,
			       STR_LITERAL_LEN(RIOT_LABEL_SERIAL),
			       sizeof(digest));

	if (status != RIOT_SUCCESS)
		return cyres_result_from_riot(status);

	/*
	 * Ensure that serial number is positive and does not
	 * have leading zeros
	 */
	digest[0] &= 0x7F;
	digest[0] |= 0x01;

	memcpy(tbs_data->SerialNum, digest, sizeof(tbs_data->SerialNum));

	return CYRES_SUCCESS;
}

static cyres_result cyres_gen_root_cert(const char *issuer_name,
					const char *subject_name, int path_len,
					struct cyres_cert **cert_out)
{
	cyres_result res;
	int ret;
	RIOT_STATUS status;
	struct cyres_cert *cert;
	RIOT_ECC_SIGNATURE tbs_sig;
	RIOT_X509_TBS_DATA tbs_data;
	struct cyres_key_pair test_key;

	memcpy(&test_key.pub, test_ecc_pub, sizeof(test_key.pub));
	memcpy(&test_key.priv, test_ecc_priv, sizeof(test_key.priv));

	res = cyres_alloc_cert(issuer_name, subject_name, &cert);
	if (res)
		return res;

	res = cyres_derive_cert_serial_num((const uint8_t *)&test_key.pub,
					   sizeof(test_key.pub), &tbs_data);
	if (res)
		goto end;

	memcpy(&tbs_data, &root_cert_tbs_template, sizeof(tbs_data));
	tbs_data.IssuerCommon = issuer_name;
	tbs_data.SubjectCommon = subject_name;

	ret = X509GetRootCertTBS(&cert->context, &tbs_data, &test_key.pub,
				 path_len);
	if (ret) {
		res = cyres_result_from_x509(ret);
		goto end;
	}

	/* sign the certificate's TBS region */
	status = RiotCrypt_Sign(&tbs_sig, cert->context.Buffer,
				cert->context.Position, &test_key.priv);
	if (status != RIOT_SUCCESS) {
		res = cyres_result_from_riot(status);
		goto end;
	}

	ret = X509MakeRootCert(&cert->context, &tbs_sig);
	if (ret) {
		res = cyres_result_from_x509(ret);
		goto end;
	}

	*cert_out = cert;
	res = CYRES_SUCCESS;

end:
	if (res)
		if (cert)
			cyres_free_cert(cert);

	return res;
}

static cyres_result cyres_gen_device_cert(const struct cyres_root_cert_args *args,
					  const char *issuer_name,
					  const RIOT_ECC_PUBLIC *device_id_pub,
					  struct cyres_cert **cert_out)
{
	cyres_result res;
	int ret;
	RIOT_STATUS status;
	struct cyres_cert *cert;
	RIOT_ECC_SIGNATURE tbs_sig;
	RIOT_X509_TBS_DATA tbs_data;
	struct cyres_key_pair test_key;
	uint32_t claim_len;
	uint8_t claim[1024];

	memcpy(&test_key.pub, test_ecc_pub, sizeof(test_key.pub));
	memcpy(&test_key.priv, test_ecc_priv, sizeof(test_key.priv));

	res = cyres_alloc_cert(issuer_name, args->device_cert_subject, &cert);
	if (res)
		return res;

	res = cyres_derive_cert_serial_num((const uint8_t *)device_id_pub,
					   sizeof(RIOT_ECC_PUBLIC), &tbs_data);
	if (res)
		goto end;

	memcpy(&tbs_data, &device_cert_tbs_template, sizeof(tbs_data));
	tbs_data.IssuerCommon = issuer_name;
	tbs_data.SubjectCommon = args->device_cert_subject;

	status = BuildDeviceClaim(device_id_pub,
				  args->auth_key_pub,
				  (uint32_t)args->auth_key_pub_size,
				  args->fwid,
				  (uint32_t)args->fwid_size,
				  claim,
				  sizeof(claim),
				  &claim_len);
	if (status != RIOT_SUCCESS) {
		res = cyres_result_from_riot(status);
		goto end;
	}

	ret = X509GetDeviceCertTBS(&cert->context,
				   &tbs_data,
				   (RIOT_ECC_PUBLIC *)
				   device_id_pub, // discard const
				   &test_key.pub,
				   claim,
				   claim_len,
				   args->root_path_len - 1);
	if (ret) {
		res = cyres_result_from_x509(ret);
		goto end;
	}

	// Sign the certificate's TBS region
	status = RiotCrypt_Sign(&tbs_sig, cert->context.Buffer,
				cert->context.Position, &test_key.priv);
	if (status != RIOT_SUCCESS) {
		res = cyres_result_from_riot(status);
		goto end;
	}

	ret = X509MakeDeviceCert(&cert->context, &tbs_sig);
	if (ret) {
		res = cyres_result_from_x509(ret);
		goto end;
	}

	*cert_out = cert;
	res = CYRES_SUCCESS;

end:
	if (res)
		if (cert)
			cyres_free_cert(cert);

	return res;
}

cyres_result cyres_init_cert_blob(void *buf, size_t buf_size,
				  struct cyres_cert_blob **cert_blob)
{
	int ret;

	ret = fdt_create_empty_tree(buf, buf_size);
	if (ret < 0)
		return cyres_result_from_fdt(ret);

	/*
	 * set version so consumers know which version of this library
	 * built the store
	 */
	ret = fdt_setprop_u32(buf, /*root*/ 0, "version",
			      CYRES_CERT_CHAIN_VERSION);
	if (ret < 0)
		return cyres_result_from_fdt(ret);

	/* create empty 'certs' node */
	ret = fdt_add_subnode(buf, 0, "certs");
	if (ret < 0)
		return cyres_result_from_fdt(ret);

	*cert_blob = buf;
	return CYRES_SUCCESS;
}

cyres_result cyres_open_cert_blob(void *buf,
				  struct cyres_cert_blob **cert_blob)
{
	int ret;
	int len;
	uint32_t version;
	const void *ptr;

	ret = fdt_check_header(buf);
	if (ret)
		return cyres_result_from_fdt(ret);

	ptr = fdt_getprop(buf, /*root*/ 0, "version", &len);
	if (!ptr)
		return cyres_result_from_fdt(len);

	if (len != sizeof(uint32_t))
		return CYRES_ERROR_BAD_FORMAT;

	version = fdt32_to_cpu(*(const uint32_t *)ptr);
	if (version != CYRES_CERT_CHAIN_VERSION)
		return CYRES_ERROR_BAD_VERSION;

	*cert_blob = (struct cyres_cert_blob *)buf;

	return CYRES_SUCCESS;
}

size_t cyres_get_cert_blob_size(const struct cyres_cert_blob *cert_blob)
{
	return fdt_totalsize(&cert_blob->fdt);
}

cyres_result cyres_move_cert_blob(const struct cyres_cert_blob *in_blob,
				  void *buf, size_t size,
				  struct cyres_cert_blob **out_blob)
{
	int ret;

	ret = fdt_open_into(&in_blob->fdt, buf, size);
	if (ret)
		return cyres_result_from_fdt(ret);

	*out_blob = (struct cyres_cert_blob *)buf;

	return CYRES_SUCCESS;
}

cyres_result
cyres_insert_root_and_device_certs(struct cyres_cert_blob *blob,
				   const struct cyres_root_cert_args *args,
				   struct cyres_key_pair *device_key_pair)
{
	cyres_result res;
	RIOT_STATUS status;
	struct cyres_cert *root_cert = NULL;
	struct cyres_cert *device_cert = NULL;
	uint8_t uds_digest[RIOT_DIGEST_LENGTH];
	uint8_t cdi[RIOT_DIGEST_LENGTH];

	/* don't use device identity directly */
	status = RiotCrypt_Hash(uds_digest,
				sizeof(uds_digest),
				args->identity,
				args->identity_size);
	if (status != RIOT_SUCCESS) {
		res = cyres_result_from_riot(status);
		goto end;
	}

	status = RiotCrypt_Hash2(cdi,
				 sizeof(cdi),
				 uds_digest,
				 sizeof(uds_digest),
				 args->fwid,
				 args->fwid_size);

	/* derive DeviceID key pair from device identity */
	status = RiotCrypt_DeriveEccKey(&device_key_pair->pub,
					&device_key_pair->priv,
					cdi,
					sizeof(cdi),
					(const uint8_t *)RIOT_LABEL_IDENTITY,
					STR_LITERAL_LEN(RIOT_LABEL_IDENTITY));

	if (status != RIOT_SUCCESS) {
		res = cyres_result_from_riot(status);
		goto end;
	}

	/* copy device public key to the cert chain */
	//memcpy(
	//		&CertChain->DeviceIDPub,
	//		&device_key_pair->pub,
	//		sizeof(RIOT_ECC_PUBLIC));

	res = cyres_gen_root_cert("Contoso Ltd.", "Contoso Ltd.",
				  args->root_path_len, &root_cert);
	if (res)
		goto end;

	/* append root cert */
	res = cyres_insert_cert(blob, root_cert);
	cyres_free_cert(root_cert);
	if (res)
		goto end;

	/* generate device cert */
	res = cyres_gen_device_cert(args,
				    "Contoso Ltd.",
				    &device_key_pair->pub,
				    &device_cert);
	if (res)
		goto end;

	/* append device cert */
	res = cyres_insert_cert(blob, device_cert);
	cyres_free_cert(device_cert);
	device_cert = NULL;
	if (res)
		goto end;

end:
	cyres_zero_mem(uds_digest, sizeof(uds_digest));
	cyres_zero_mem(cdi, sizeof(cdi));

	return res;
}

cyres_result cyres_gen_alias_cert(const struct cyres_gen_alias_cert_args *args,
				  struct cyres_cert **cert_out,
				  struct cyres_key_pair *subject_key_pair)
{
	cyres_result res;
	int ret;
	RIOT_STATUS status;
	uint32_t claim_len;
	struct cyres_cert *cert = NULL;
	RIOT_ECC_SIGNATURE tbs_sig;
	RIOT_X509_TBS_DATA tbs_data;
	uint8_t digest[RIOT_DIGEST_LENGTH];
	uint8_t claim[1024];

	// hash seed data to 256-bit digest
	status = RiotCrypt_Hash(digest, sizeof(digest),
				(const uint8_t *)args->seed_data,
				args->seed_data_size);

	if (status != RIOT_SUCCESS)
		return cyres_result_from_riot(status);

	// combine hashed seed data and image digest
	status = RiotCrypt_Hash2(digest,
				 sizeof(digest),
				 digest,
				 sizeof(digest),
				 args->subject_digest,
				 args->subject_digest_size);

	if (status != RIOT_SUCCESS)
		return cyres_result_from_riot(status);

	// derive key pair from hashed seed data and image digest
	status = RiotCrypt_DeriveEccKey(&subject_key_pair->pub,
					&subject_key_pair->priv,
					digest,
					sizeof(digest),
					(const uint8_t *)RIOT_LABEL_ALIAS,
					STR_LITERAL_LEN(RIOT_LABEL_ALIAS));

	if (status != RIOT_SUCCESS)
		return cyres_result_from_riot(status);

	memcpy(&tbs_data, &alias_cert_tbs_template, sizeof(tbs_data));
	tbs_data.IssuerCommon = args->issuer_name;
	tbs_data.SubjectCommon = args->subject_name;
	status = cyres_derive_cert_serial_num((const uint8_t *)
					      &subject_key_pair->pub,
					      sizeof(RIOT_ECC_PUBLIC),
					      &tbs_data);

	if (status != RIOT_SUCCESS)
		return cyres_result_from_riot(status);

	res = cyres_alloc_cert(args->issuer_name, args->subject_name, &cert);
	if (res)
		return res;

	status = BuildAliasClaim(args->auth_key_pub,
				 args->auth_key_pub_size,
				 args->subject_digest,
				 args->subject_digest_size,
				 claim,
				 sizeof(claim),
				 &claim_len);

	if (status != RIOT_SUCCESS) {
		res = cyres_result_from_riot(status);
		goto end;
	}

	ret = X509GetAliasCertTBS(&cert->context,
				  &tbs_data,
				  &subject_key_pair->pub,
				  (RIOT_ECC_PUBLIC *)
				  &args->issuer_key_pair->pub,
				  /* discard const */
				  (uint8_t *)args->subject_digest,
				  /* discard const */
				  args->subject_digest_size,
				  claim,
				  claim_len,
				  args->path_len);

	if (ret) {
		res = cyres_result_from_x509(ret);
		goto end;
	}

	// sign the Alias Key Certificate's TBS region.
	status = RiotCrypt_Sign(&tbs_sig,
				cert->context.Buffer,
				cert->context.Position,
				&args->issuer_key_pair->priv);

	if (status != RIOT_SUCCESS) {
		res = cyres_result_from_riot(status);
		goto end;
	}

	// Generate Alias Key Certificate by signing the TBS region.
	ret = X509MakeAliasCert(&cert->context, &tbs_sig);
	if (ret) {
		res = cyres_result_from_x509(ret);
		goto end;
	}

	*cert_out = cert;
	res = CYRES_SUCCESS;

end:

	if (res != CYRES_SUCCESS)
		if (cert)
			free(cert);

	// Clean up potentially sensative data.
	cyres_zero_mem(digest, sizeof(digest));

	return res;
}

cyres_result cyres_insert_cert(struct cyres_cert_blob *blob,
			       const struct cyres_cert *cert)
{
	cyres_result res;
	size_t pem_len;
	int ret;
	int addr = 0;
	int certs_node;
	int cert_node;
	uint32_t phandle;
	char *pem_buf;
	void *strict_alias_workaround;
	char node_name[20];

	/* get length needed for PEM string */
	res = cyres_cert_to_pem(cert, NULL, &pem_len);
	if (res != CYRES_ERROR_SHORT_BUFFER)
		return CYRES_ERROR_GENERIC;

	/* get the 'certs' node */
	certs_node = fdt_subnode_offset(&blob->fdt, /* root */ 0, "certs");
	if (certs_node < 0)
		return cyres_result_from_fdt(certs_node);

	do {
		addr++;

		/* create what is hopefully a unique name */
		snprintf(node_name, sizeof(node_name), "cert@%d", addr);

		/* attempt to create the node */
		ret = fdt_add_subnode(&blob->fdt, certs_node, node_name);
	} while (ret == -FDT_ERR_EXISTS);

	if (ret < 0)
		return cyres_result_from_fdt(ret);

	cert_node = ret;

	ret = fdt_create_phandle(&blob->fdt, cert_node);
	if (!ret)
		return cyres_result_from_fdt(-FDT_ERR_BADPHANDLE);

	/* allocate space for the PEM property in the node */
	ret = fdt_setprop_placeholder(&blob->fdt, cert_node, "pem", pem_len,
				      &strict_alias_workaround);
	pem_buf = strict_alias_workaround;

	if (ret)
		return cyres_result_from_fdt(ret);

	/* convert to PEM and insert in the FDT */
	res = cyres_cert_to_pem(cert, pem_buf, &pem_len);
	if (res)
		return res;

	/* find this cert's issuer */
	ret = fdt_node_offset_by_prop_value(&blob->fdt, certs_node,
					    "subject-name", cert->issuer_name,
					    strlen(cert->issuer_name) + 1);
	if (ret >= 0) {
		/* if we found an issuer, get the phandle */
		phandle = fdt_get_phandle(&blob->fdt, ret);
		if (phandle == 0 || phandle == 0xffffffff)
			return CYRES_ERROR_GENERIC;

		/* create the issuer property */
		ret = fdt_setprop_u32(&blob->fdt, cert_node, "issuer",
				      phandle);
		if (ret)
			return cyres_result_from_fdt(ret);
	}

	/* so far so good; set the subject name */
	ret = fdt_setprop_string(&blob->fdt, cert_node, "subject-name",
				 cert->subject_name);
	if (ret)
		return cyres_result_from_fdt(ret);

	return CYRES_SUCCESS;
}

cyres_result cyres_priv_key_to_pem(const struct cyres_key_pair *key,
				   char *buf, size_t *buf_size)
{
	DERBuilderContext context;
	uint32_t length;
	int ret;
	uint8_t der_buf[DER_MAX_TBS];

	DERInitContext(&context, der_buf, sizeof(der_buf));
	ret = X509GetDEREcc(&context, key->pub, key->priv);
	if (ret)
		return cyres_result_from_x509(ret);

	if (buf && *buf_size > 0)
		length = *buf_size - 1;
	else
		length = 0;

	ret = DERtoPEM(&context, R_ECC_PRIVATEKEY_TYPE, buf, &length);
	if (ret) {
		*buf_size = length + 1;
		return CYRES_ERROR_SHORT_BUFFER;
	}

	buf[length] = '\0';

	return CYRES_SUCCESS;
}

cyres_result cyres_pub_key_to_pem(const RIOT_ECC_PUBLIC *key,
				  char *buf, size_t *buf_size)
{
	DERBuilderContext context;
	uint32_t length;
	int ret;
	uint8_t der_buf[DER_MAX_TBS];

	DERInitContext(&context, der_buf, sizeof(der_buf));
	ret = X509GetDEREccPub(&context, *key);
	if (ret)
		return cyres_result_from_x509(ret);

	if (buf && *buf_size > 0)
		length = *buf_size - 1;
	else
		length = 0;

	ret = DERtoPEM(&context, R_PUBLICKEY_TYPE, buf, &length);
	if (ret) {
		*buf_size = length + 1;
		return CYRES_ERROR_SHORT_BUFFER;
	}

	buf[length] = '\0';

	return CYRES_SUCCESS;
}

cyres_result cyres_cert_to_pem(const struct cyres_cert *cert,
			       char *buf, size_t *buf_size)
{
	int ret;
	uint32_t len;

	if (!buf || *buf_size == 0)
		len = 0;
	else
		len = (uint32_t)*buf_size - 1;

	ret = DERtoPEM((DERBuilderContext *)&cert->context, // discard const
		       R_CERT_TYPE, buf, &len);
	if (ret) {
		*buf_size = len + 1;
		return CYRES_ERROR_SHORT_BUFFER;
	}

	buf[len] = '\0';

	return CYRES_SUCCESS;
}

cyres_result cyres_get_cert_chain_pem(const struct cyres_cert_blob *blob,
				      const char *subject,
				      char *buf,
				      size_t *buf_size)
{
	size_t req_len = 1;
	int certs_node;
	int cert;
	int issuer;
	int len;
	const void *ptr;
	uint32_t phandle;
	const char *pem;

	if (buf && *buf_size > 0)
		buf[0] = '\0';

	/* get the 'certs' node */
	certs_node = fdt_subnode_offset(&blob->fdt, /* root */ 0, "certs");
	if (certs_node < 0)
		return cyres_result_from_fdt(certs_node);

	/* find cert node with the given subject name */
	issuer = fdt_node_offset_by_prop_value(&blob->fdt,
					       certs_node,
					       "subject-name",
					       subject,
					       strlen(subject) + 1);
	if (issuer < 0)
		return cyres_result_from_fdt(issuer);

	/* walk the chain of certs using the 'issuer' phandle */
	cert = -1;
	while (cert != issuer) {
		cert = issuer;

		/* get the cert's PEM string */
		pem = fdt_getprop(&blob->fdt, cert, "pem", &len);
		if (!pem)
			return cyres_result_from_fdt(len);

		if (len == 0 || (pem[len - 1] != '\0'))
			return CYRES_ERROR_BAD_FORMAT;

		/* account for null terminator */
		req_len += len - 1;

		if (buf)
			if (strlcat(buf, pem, *buf_size) >= *buf_size)
				return CYRES_ERROR_SHORT_BUFFER;

		ptr = fdt_getprop(&blob->fdt, cert, "issuer", &len);
		if (!ptr) {
			if (len == -FDT_ERR_NOTFOUND)
				break;
			return cyres_result_from_fdt(len);
		}

		if (len != sizeof(uint32_t))
			return CYRES_ERROR_BAD_FORMAT;

		phandle = fdt32_to_cpu(*(const uint32_t *)ptr);

		/* get the issuing cert */
		issuer = fdt_node_offset_by_phandle(&blob->fdt, phandle);
		if (issuer < 0)
			return cyres_result_from_fdt(issuer);
	}

	if (!buf || req_len > *buf_size) {
		*buf_size = req_len;
		return CYRES_ERROR_SHORT_BUFFER;
	}

	return CYRES_SUCCESS;
}

void cyres_free_cert(struct cyres_cert *cert)
{
	cyres_zero_mem(cert, sizeof(struct cyres_cert));
	free(cert);
}

/* memset implementation that will not be optimized away */
void cyres_zero_mem(void *mem, size_t size)
{
	volatile char *p;

	if (!mem)
		return;

	/*
	 * XXX - Consider writing a random value over the
	 * memory region before zeroing.
	 */
	p = (volatile char *)mem;
	while (size > 0) {
		*p = 0;
		p++;
		size--;
	}
}

cyres_result cyres_make_key_blob_inplace(void *buf, size_t buf_size,
					 const struct cyres_key_pair *key)
{
	struct cyres_key_blob *blob;

	if (buf_size < sizeof(struct cyres_key_blob))
		return CYRES_ERROR_SHORT_BUFFER;

	blob = buf;
	blob->magic = CYRES_KEY_BLOB_MAGIC;
	blob->version = CYRES_KEY_BLOB_VERSION;
	memcpy(&blob->key, key, sizeof(blob->key));

	return CYRES_SUCCESS;
}

cyres_result cyres_take_key_from_blob(void *keyblob,
				      struct cyres_key_pair *key)
{
	struct cyres_key_blob *blob = keyblob;

	if (blob->magic != CYRES_KEY_BLOB_MAGIC)
		return CYRES_ERROR_BAD_FORMAT;

	if (blob->version != CYRES_KEY_BLOB_VERSION)
		return CYRES_ERROR_BAD_FORMAT;

	memcpy(key, &blob->key, sizeof(*key));
	cyres_zero_mem(&blob->key, sizeof(blob->key));

	return CYRES_SUCCESS;
}
