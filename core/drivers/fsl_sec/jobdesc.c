// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2017 NXP
 *
 *  Ruchika Gupta <ruchika.gupta@nxp.com>
 */

#include <trace.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <tee/cache.h>
#include "fsl_sec_io.h"
#include "sec_hw_specific.h"
#include "jobdesc.h"

#define DESC_LEN_MASK 0x7f
#define DESC_START_SHIFT 16

// Return Length of desctiptr from first word
uint32_t desc_length(uint32_t *desc)
{
	return desc[0] & DESC_LEN_MASK;
}

//Update start index in first word of descriptor
void desc_update_start_index(uint32_t *desc, uint32_t index)
{
	desc[0] |= (index << DESC_START_SHIFT);
}

// Initialize the descriptor
void desc_init(uint32_t *desc)
{
	*desc = 0;
}

// Add word in the descriptor and increment the length
void desc_add_word(uint32_t *desc, uint32_t word)
{
	uint32_t len = desc_length(desc);

	// Add Word at Last
	uint32_t *last = desc + len;
	*last = word;

	// Increase the length
	desc[0] += 1;
}

// Add Pointer to the descriptor
void desc_add_ptr(uint32_t *desc, phys_addr_t *ptr)
{
	uint32_t len = desc_length(desc);

	// Add Word at Last
	phys_addr_t *last = (phys_addr_t *)(desc + len);
#ifdef CONFIG_PHYS_64BIT
	union ptr_addr_t *ptr_addr = (union ptr_addr_t *)last;

	ptr_addr->m_halves.high = PHYS_ADDR_HI(ptr);
	ptr_addr->m_halves.low = PHYS_ADDR_LO(ptr);
#else
	*last = (uint32_t) ptr;
#endif

	// Increase the length
	desc[0] += (uint32_t)(sizeof(phys_addr_t) / sizeof(uint32_t));
}

// Descriptor to generate Random words
int cnstr_rng_jobdesc(uint32_t *desc, uint32_t state_handle,
		      uint32_t *add_inp __unused, uint32_t add_ip_len,
		      uint8_t *out_data, uint32_t len)
{
	phys_addr_t *phys_addr_out = vtop(out_data);
	unsigned long start, end;

	// Current descriptor support only 64K length
	if (len > 0xffff)
		return -1;

	// Additional Input not supported by current descriptor
	if (add_ip_len > 0)
		return -1;

	desc_init(desc);
	// Class1 Alg Operation,RNG Optype, Generate
	desc_add_word(desc, 0xb0800000);
	desc_add_word(desc, 0x82500000 | (state_handle << ALG_AAI_SH_SHIFT));
	desc_add_word(desc, 0x60340000 | len);
	desc_add_ptr(desc, phys_addr_out);

	start = (unsigned long)out_data;
	end = ROUNDUP(start + len, STACK_ALIGNMENT);
	cache_operation(TEE_CACHEFLUSH, (void *) start, (size_t)(end - start));

	return 0;
}

// Construct descriptor to instantiate RNG
int cnstr_rng_instantiate_jobdesc(uint32_t *desc)
{
	desc_init(desc);
	desc_add_word(desc, 0xb0800000);
	// Class1 Alg Operation,RNG Optype, Instantiate
	desc_add_word(desc, 0x82500004);
	// Wait for done
	desc_add_word(desc, 0xa2000001);
	//Load to clear written
	desc_add_word(desc, 0x10880004);
	//Pri Mode Reg clear
	desc_add_word(desc, 0x00000001);
	// Generate secure keys
	desc_add_word(desc, 0x82501000);

	return 0;
}

// Construct descriptor to generate hw key blob
int cnstr_hw_encap_blob_jobdesc(uint32_t *desc, uint8_t *key_idnfr,
				uint32_t key_sz, uint32_t key_class,
				uint8_t *plain_txt, uint32_t in_sz,
				uint8_t *enc_blob, uint32_t out_sz,
				uint32_t operation)
{
	phys_addr_t *phys_key_idnfr, *phys_addr_in, *phys_addr_out;
	unsigned long start, end;

	phys_key_idnfr = vtop((void *)key_idnfr);
	phys_addr_in = vtop((void *)plain_txt);
	phys_addr_out = vtop((void *)enc_blob);

	desc_init(desc);

	desc_add_word(desc, 0xb0800000);

	//Key Identifier
	desc_add_word(desc, (key_class | key_sz));
	desc_add_ptr(desc, phys_key_idnfr);
	//desc_add_word(desc, 0x00000000); //None Hi loc
	//desc_add_word(desc, 0x00000000); //None Lo loc

	//Source Address
	desc_add_word(desc, 0xf0400000);
	desc_add_ptr(desc, phys_addr_in);
	//desc_add_word(desc, 0x00000000); //None Hi loc
	//desc_add_word(desc, 0x00000000); //None Lo loc

	//In Size = 0x10
	desc_add_word(desc, in_sz);

	//Out Address
	desc_add_word(desc, 0xf8400000);
	desc_add_ptr(desc, phys_addr_out);

	//Out Size = 0x10
	desc_add_word(desc, out_sz);

	//Operation
	desc_add_word(desc, operation);

	start = (unsigned long)key_idnfr;
	end = ROUNDUP(start + key_sz, STACK_ALIGNMENT);
	cache_operation(TEE_CACHEFLUSH, (void *) start, (size_t)(end - start));

	start = (unsigned long)plain_txt;
	end = ROUNDUP(start + in_sz, STACK_ALIGNMENT);
	cache_operation(TEE_CACHEFLUSH, (void *) start, (size_t)(end - start));

	start = (unsigned long)enc_blob;
	end = ROUNDUP(start + out_sz, STACK_ALIGNMENT);
	cache_operation(TEE_CACHEFLUSH, (void *) start, (size_t)(end - start));

	return 0;
}
