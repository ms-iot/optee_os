// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2017 NXP
 *
 *  Ruchika Gupta <ruchika.gupta@nxp.com>
 */

#include <trace.h>
#include <kernel/panic.h>
#include <kernel/spinlock.h>
#include <tee_api_types.h>
#include <rng_support.h>
#include <crypto/crypto.h>
#include "fsl_sec.h"
#include "sec_hw_specific.h"
#include "jobdesc.h"
#include "string.h"
#include "malloc.h"

// Callback function after Instantiation decsriptor is submitted to SEC
static void rng_done(uint32_t *desc, uint32_t status, void *arg, void *job_ring)
{
	(void)desc;
	(void)status;
	(void)arg;
	(void)job_ring;
	//DMSG("Desc SUCCESS\n");
	//DMSG("status: %x", status);
}

// RNG operates with a global aligned jobdesc and output buffer.
// This is to avoid repeated memalign calls on every RNG creation and to allow
//  for a larger RNG buffer that can dispatch numbers without CAAM calls for
//  each small request.
#define RNGBUF_SIZE 256
static unsigned int rng_lock = SPINLOCK_UNLOCK;
static struct job_descriptor *rng_jobdesc;
static uint8_t *rng_buf;
static uint32_t rng_rem;

// Is the HW RNG instantiated?
// Return code:
// 0 - Not in the instantiated state
// 1 - In the instantiated state
// state_handle - 0 for SH0, 1 for SH1
static int is_hw_rng_instantiated(uint32_t *state_handle)
{
	int ret_code = 0;
	uint32_t rdsta;

	rdsta = sec_in32(ptov(CAAM_BASE + RNG_REG_RDSTA_OFFSET));

	//Check if either of the two state handles has been instantiated
	if (rdsta & RNG_STATE0_HANDLE_INSTANTIATED) {
		*state_handle = 0;
		ret_code = 1;
	} else if (rdsta & RNG_STATE0_HANDLE_INSTANTIATED) {
		*state_handle = 1;
		ret_code = 1;
	}

	return ret_code;
}

// @brief Kick the TRNG block of the RNG HW Engine
// @param [in] ent_delay	Entropy delay to be used
//      By default, the TRNG runs for 200 clocks per sample;
//      1200 clocks per sample generates better entropy.
// @retval 0 on success
// @retval -1 on error
static void kick_trng(int ent_delay)
{
	uint32_t val;

	// put RNG4 into program mode
	val = sec_in32(ptov(CAAM_BASE + RNG_REG_RTMCTL_OFFSET));
	val = val | RTMCTL_PRGM;
	sec_out32(ptov(CAAM_BASE + RNG_REG_RTMCTL_OFFSET), val);

	// rtsdctl bits 0-15 contain "Entropy Delay, which defines the
	//  length (in system clocks) of each Entropy sample taken
	val = sec_in32(ptov(CAAM_BASE + RNG_REG_RTSDCTL_OFFSET));
	val = (val & ~RTSDCTL_ENT_DLY_MASK) |
	      (ent_delay << RTSDCTL_ENT_DLY_SHIFT);
	sec_out32(ptov(CAAM_BASE + RNG_REG_RTSDCTL_OFFSET), val);
	// min. freq. count, equal to 1/4 of the entropy sample length
	sec_out32(ptov(CAAM_BASE + RNG_REG_RTFRQMIN_OFFSET), ent_delay >> 2);
	// disable maximum frequency count
	sec_out32(ptov(CAAM_BASE + RNG_REG_RTFRQMAX_OFFSET), RTFRQMAX_DISABLE);

	// select raw sampling in both entropy shifter
	//  and statistical checker
	val = sec_in32(ptov(CAAM_BASE + RNG_REG_RTMCTL_OFFSET));
	val = val | RTMCTL_SAMP_MODE_RAW_ES_SC;
	sec_out32(ptov((CAAM_BASE + RNG_REG_RTMCTL_OFFSET)), val);

	// put RNG4 into run mode
	val = sec_in32(ptov(CAAM_BASE + RNG_REG_RTMCTL_OFFSET));
	val = val & ~RTMCTL_PRGM;
	sec_out32(ptov((CAAM_BASE + RNG_REG_RTMCTL_OFFSET)), val);
}

// @brief Submit descriptor to instantiate the RNG
// @retval 0 on success
// @retval -1 on error
static int instantiate_rng(void)
{
	int ret = 0;
	// TBD - Current allocator doesn't have a free function
	// Remove static once free implementation is available
	static struct job_descriptor *jobdesc;

	if (!jobdesc) {
		jobdesc = fsl_sec_memalign(64, sizeof(struct job_descriptor));
		if (!jobdesc) {
			DMSG("desc allocation failed\n");
			return -1;
		}
	}
	jobdesc->arg = NULL;
	jobdesc->callback = rng_done;

	// create the hw_rng descriptor
	cnstr_rng_instantiate_jobdesc(jobdesc->desc);

	// Finally, generate the requested random data bytes
	ret = run_descriptor_jr(jobdesc);
	if (ret) {
		DMSG("Error in running descriptor\n");
		ret = -1;
	}

	fsl_sec_free(jobdesc);
	return ret;
}

// Generate Random Data using HW RNG
// Parameters:
// uint8_t* add_input     - unused
// uint32_t add_input_len - unused
// uint8_t* out           - user specified output byte array
// uint32_t out_len       - number of bytes to store in output byte array
// Return code:
// 0 - SUCCESS
// -1 - ERROR
static int
hw_rng_generate(uint32_t *add_input __unused, uint32_t add_input_len __unused,
		uint8_t *out, uint32_t out_len, uint32_t state_handle)
{
	int ret = 0;

	cpu_spin_lock(&rng_lock);
	if (rng_jobdesc == NULL) {
		DMSG("rng_jobdesc buffer not initialized\n");
		ret = -1;
		goto exit;
	}

	if (rng_buf == NULL) {
		DMSG("rng_buf buffer not initialized");
		ret = -1;
		goto exit;
	}

	while (out_len > 0) {
		if (rng_rem == 0) {
			rng_jobdesc->arg = NULL;
			rng_jobdesc->callback = rng_done;

			ret = cnstr_rng_jobdesc(rng_jobdesc->desc, state_handle,
					0, 0, rng_buf, RNGBUF_SIZE);
			if (ret) {
				DMSG("Descriptor construction failed\n");
				ret = -1;
				goto exit;
			}

			// Finally, generate the requested random data bytes
			ret = run_descriptor_jr(rng_jobdesc);
			if (ret) {
				DMSG("Error in running descriptor\n");
				ret = -1;
				goto exit;
			}
			rng_rem = RNGBUF_SIZE;
		}

		if (out_len > rng_rem) {
			memcpy(out, &rng_buf[RNGBUF_SIZE-rng_rem], rng_rem);
			out_len -= rng_rem;
			out += rng_rem;
			rng_rem = 0;
		} else {
			memcpy(out, &rng_buf[RNGBUF_SIZE-rng_rem], out_len);
			rng_rem -= out_len;
			break;
		}
	}

exit:
	cpu_spin_unlock(&rng_lock);
	return ret;
}

// this function instantiates the rng
//
// Return code:
//  0 - All is well
// <0 - Error occurred somewhere
int hw_rng_instantiate(void)
{
	int ret;
	int ent_delay = RTSDCTL_ENT_DLY_MIN;
	uint32_t state_handle;

	cpu_spin_lock(&rng_lock);
	if (rng_jobdesc == NULL) {
		rng_jobdesc = fsl_sec_memalign(STACK_ALIGNMENT,
				sizeof(struct job_descriptor));
		if (rng_jobdesc == NULL) {
			DMSG("Failed to allocate rng_jobdesc");
			cpu_spin_unlock(&rng_lock);
			return -1;
		}
	}

	if (rng_buf == NULL) {
		rng_buf = fsl_sec_memalign(STACK_ALIGNMENT, RNGBUF_SIZE);
		if (rng_buf == NULL) {
			DMSG("Failed to allocate rng_buf");
			cpu_spin_unlock(&rng_lock);
			return -1;
		}
	}
	cpu_spin_unlock(&rng_lock);

	ret = is_hw_rng_instantiated(&state_handle);
	if (ret) {
		DMSG("RNG already instantiated\n");
		return 0;
	}
	do {
		kick_trng(ent_delay);
		ent_delay += 400;
		//if instantiate_rng(...) fails, the loop will rerun
		//and the kick_trng(...) function will modify the
		//upper and lower limits of the entropy sampling
		//interval, leading to a sucessful initialization of
		ret = instantiate_rng();
	} while ((ret == -1) && (ent_delay < RTSDCTL_ENT_DLY_MAX));
	if (ret) {
		DMSG("RNG: Failed to instantiate RNG\n");
		return ret;
	}

	DMSG("RNG: INSTANTIATED\n");

	// Enable RDB bit so that RNG works faster
	//sec_setbits32(&sec->scfgr, SEC_SCFGR_RDBENABLE);

	return ret;
}

// Generate random bytes, and stuff them into the bytes buffer
//
// If the HW RNG has not already been instantiated,
//  it will be instantiated before data is generated.
//
// Parameters:
// uint8_t* bytes  - byte buffer large enough to hold the requested random date
// int    byte_len - number of random bytes to generate
//
// Return code:
//  0 - All is well
//  ~0 - Error occurred somewhere
int get_rand_bytes_hw(uint8_t *bytes, int byte_len)
{
	int ret_code = 0;
	uint32_t state_handle;

	// If this is the first time this routine is called,
	//  then the hash_drbg will not already be instantiated.
	// Therefore, before generating data, instantiate the hash_drbg
	ret_code = is_hw_rng_instantiated(&state_handle);
	if (!ret_code) {
		DMSG("Instantiating the HW RNG\n");

		// Instantiate the hw RNG
		ret_code = hw_rng_instantiate();
		if (ret_code) {
			DMSG("HW RNG Instantiate failed\n");
			return ret_code;
		}
	}

	if (!is_hw_rng_instantiated(&state_handle)) {
		DMSG("HW RNG is in an Error state, and cannot be used\n");
		return -1;
	}

	ret_code = hw_rng_generate(0, 0, bytes, byte_len, state_handle);

	if (ret_code) {
		DMSG("HW RNG Generate failed\n");
		return ret_code;
	}

	return ret_code;
}


// RNG APIs called elsewhere in OP-TEE
uint8_t hw_get_random_byte(void)
{
	uint8_t byte;
	TEE_Result res;

	res = crypto_rng_read((void *)&byte, 1);
	if (res != TEE_SUCCESS) {
		DMSG("Failed to get random byte!");
		byte = 0;
	}

	return byte;
}

TEE_Result crypto_rng_read(void *buf, size_t blen)
{
	TEE_Result res;

	if (buf == NULL)
		return TEE_ERROR_BAD_PARAMETERS;

	res = get_rand_bytes_hw((uint8_t *) buf, blen);
	if (res != TEE_SUCCESS)
		DMSG("Failed to get random bytes of length %d", blen);

	return res;
}
