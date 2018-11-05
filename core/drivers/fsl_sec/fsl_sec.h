/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2017 NXP
 *
 *  Ruchika Gupta <ruchika.gupta@nxp.com>
 */

#ifndef FSL_SEC_H
#define FSL_SEC_H

#include "sec_jr_driver.h"

#define CAAM_JR0_OFFSET   0x1000
#define CAAM_JR0_ADDR     (CAAM_BASE_ADDR + CAAM_JR0_OFFSET)

 // This function does basic SEC Initilaization
int sec_init(void);

 // This function is used to submit jobs to JR
int run_descriptor_jr(struct job_descriptor *desc);

 // This function is used to instatiate the HW RNG if not already instantiated
int hw_rng_instantiate(void);

 // This function is used to return random bytes of byte_len from HW RNG
int get_rand_bytes_hw(uint8_t *bytes, int byte_len);

 // This function is used to set the hw unique key from HW CAAM
TEE_Result get_hw_unq_key_blob_hw(uint8_t *hw_key, int size);

#endif // FSL_SEC_H
