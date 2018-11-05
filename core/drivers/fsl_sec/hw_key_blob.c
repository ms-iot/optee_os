// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2017 NXP
 *
 *  Pankaj Gupta <pankaj.gupta@nxp.com>
 */

#include <trace.h>
#include <kernel/tee_common_otp.h>
#include "fsl_sec.h"
#include "sec_hw_specific.h"
#include "jobdesc.h"
#include "malloc.h"
#include "string.h"

//-----------------------------------------------------------------------------

#define BLOB_PROTO_INFO 0x00000002

//-----------------------------------------------------------------------------

static TEE_Result set_master_key_source_otpmk(void)
{
	uint32_t val;

	val = sec_in32(ptov(SNVS_BASE + SNVS_LPMKCR));
	if ((val & ~0x3) == SNVS_LPMKCR_MK_OTP) {
		DMSG("Master key source is already OTPMK");
		return TEE_SUCCESS;
	}

	/* check MKS_SL and MKS_HL bits */
	val = sec_in32(ptov(SNVS_BASE + SNVS_LPLR));
	if ((val & SNVS_LPLR_MKEYSEL_LCK) != 0) {
		DMSG("MASTER_KEY_SEL is hard locked!");
		return TEE_ERROR_SECURITY;
	}

	val = sec_in32(ptov(SNVS_BASE + SNVS_HPLR));
	if ((val & SNVS_HPLR_MKEYSEL_LCK) != 0) {
		DMSG("MASTER_KEY_SEL is soft locked!");
		return TEE_ERROR_SECURITY;
	}

	/* Set master key source to OTPMK */
	DMSG("Setting master key source to OTPMK\n");
	val = sec_in32(ptov(SNVS_BASE + SNVS_LPMKCR));
	val = (val & ~0x3) | SNVS_LPMKCR_MK_OTP;
	sec_out32(val, ptov(SNVS_BASE + SNVS_LPMKCR));

	return TEE_SUCCESS;
}

/* Verify that the CAAM is in secure/trusted mode and priblob is accessible */
static TEE_Result verify_priblob_trusted(void)
{
	uint32_t val;

	/* We can only read true OTPMK when CAAM is operating in secure
	 *	or trusted mode. This is a warning and does not prevent boot.
	 */
	val = sec_in32(ptov(CAAM_BASE + SEC_REG_CSTA_OFFSET));
	switch (val & CSTA_MOO_MASK) {
	case CSTA_MOO_SECURE:
	case CSTA_MOO_TRUSTED:
		break;
	default:
		EMSG("CAAM not secure/trusted; OTPMK inaccessible");
		return TEE_ERROR_SECURITY;
	}

	return TEE_SUCCESS;
}

static void roll_forward_master_key(void)
{
	uint32_t val;

	DMSG("Setting priblob configuration forward to normal mode");

	val = sec_in32(ptov(CAAM_BASE + SEC_REG_SCFGR_OFFSET));
	sec_out32((ptov(CAAM_BASE + SEC_REG_SCFGR_OFFSET)),
		  val | SCFGR_PRIBLOB_NORMAL);
}

static TEE_Result init_and_roll_forward_master_key(void)
{
	TEE_Result res;

	// Ensure that the fsl_sec driver has been initialized
	sec_init();

	res = set_master_key_source_otpmk();
	if (res != TEE_SUCCESS) {
		DMSG("Failed to set master key source to otpmk");
		return res;
	}

	res = verify_priblob_trusted();
	if (res != TEE_SUCCESS) {
		DMSG("priblob not in a trusted or secure state");
		return res;
	}

	roll_forward_master_key();

	return res;
}

// Callback function after Instantiation decsriptor is submitted to SEC
static void blob_done(uint32_t *desc, uint32_t status, void *arg,
		      void *job_ring)
{
	(void)desc;
	(void)status;
	(void)arg;
	(void)job_ring;
	DMSG("Desc SUCCESS\n");
	DMSG("status: %x", status);
}

TEE_Result get_hw_unq_key_blob_hw(uint8_t *hw_key, int size)
{
	TEE_Result res;
	int i;

	uint32_t key_sz = KEY_IDNFR_SZ_BYTES;
	uint8_t *key_data = NULL;

	uint32_t in_sz = 16;
	uint8_t *in_data = NULL;

	// output blob will have 32 bytes key blob in beginning and
	// 16 byte HMAC identifier at end of data blob
	uint32_t out_sz = in_sz + KEY_BLOB_SIZE + MAC_SIZE;
	uint8_t *out_data = NULL;

	uint32_t operation = CMD_OPERATION |
			     OP_TYPE_ENCAP_PROTOCOL |
			     OP_PCLID_BLOB |
			     BLOB_PROTO_INFO;

	// TBD - Current allocator doesn't have a free function
	// Remove static once free implementation is available
	static struct job_descriptor *jobdesc;

	key_data = memalign(64, key_sz);

	if (key_data == NULL) {
		DMSG("Key data buffer alloc failed\n");
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto clean_up;
	}
	memset(key_data, 0x00, key_sz);

	in_data = memalign(64, in_sz);

	if (in_data == NULL) {
		DMSG("In data buffer alloc failed\n");
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto clean_up;
	}
	memset(in_data, 0x00, in_sz);

	out_data = memalign(64, out_sz);

	if (out_data == NULL) {
		DMSG("Out data buffer alloc failed\n");
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto clean_up;
	}
	memset(out_data, 0x00, out_sz);

	jobdesc = memalign(64, sizeof(struct job_descriptor));
	if (jobdesc == NULL) {
		DMSG("desc allocation failed\n");
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto clean_up;
	}

	jobdesc->arg = NULL;
	jobdesc->callback = blob_done;

	DMSG("\nGenerating Master Key Verification Blob.\n");

	// create the hw_encap_blob descriptor
	res = cnstr_hw_encap_blob_jobdesc(jobdesc->desc,
					  key_data, key_sz, CLASS_2,
					  in_data, in_sz,
					  out_data, out_sz,
					  operation);

	// Finally, generate the blob
	res = run_descriptor_jr(jobdesc);
	if (res) {
		DMSG("Error in running hw unq key blob descriptor\n");
		res = TEE_ERROR_GENERIC;
		goto clean_up;
	}

	// Copying alternate bytes of the Master Key Verification Blob.
	for (i = 0; i < size; i++)
		hw_key[i] = out_data[i];

clean_up:
	if (key_data != NULL)
		free(key_data);
	if (in_data != NULL)
		free(in_data);
	if (out_data != NULL)
		free(out_data);
	if (jobdesc != NULL)
		free(jobdesc);
	return res;
}

TEE_Result tee_otp_get_hw_unique_key(struct tee_hw_unique_key *hwkey)
{
	TEE_Result res;

	res = init_and_roll_forward_master_key();
	if (res != TEE_SUCCESS) {
		EMSG("Failed to init normal priblob otpmk identity from CAAM");
		return res;
	}

	res = get_hw_unq_key_blob_hw(&hwkey->data[0], HW_UNIQUE_KEY_LENGTH);

	if (res != TEE_SUCCESS) {
		EMSG("Hardware Unique Key Failed");
	} else {
		DMSG("Hardware Unique Key Retrieved:");
		DHEXDUMP(&hwkey->data[0], HW_UNIQUE_KEY_LENGTH);
	}

	return res;
}
