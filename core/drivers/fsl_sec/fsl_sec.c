// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2017 NXP
 *
 *  Ruchika Gupta <ruchika.gupta@nxp.com>
 */

#include <stdint.h>
#include <trace.h>
#include <tee/tee_cryp_utl.h>
#include <kernel/spinlock.h>
#include "fsl_sec.h"
#include "sec_jr_driver.h"
#include "sec_hw_specific.h"
#include "jobdesc.h"
#include "fsl_sec_io.h"
#include "string.h"

 // Job ring 0 is reserved for usage by sec firmware
#define DEFAULT_JR	0
static void *job_ring;
static bool sec_initialized;
static unsigned int jr_lock = SPINLOCK_UNLOCK;

 // This function sets the TZ bit for the Job ring number passed as @num
static int config_tz(int num)
{
	uint32_t jricid;

	// Setting TZ bit of job ring
	switch (num) {
	case 0:
		jricid = sec_in32(ptov(CAAM_BASE + SEC_REG_JR0ICIDR_MS_OFFSET));
		sec_out32(ptov(CAAM_BASE + SEC_REG_JR0ICIDR_MS_OFFSET),
			  jricid | JRICID_MS_TZ);
		break;
	case 1:
	case 2:
	case 3:
	default:
		DMSG("JR%d selected, only JR0 is supported", num);
		return -1;
	}
	return 0;
}

 // This function checks if Virtualization is enabled for JR and
 // accordingly sets the bot for starting JR<num> in JRSTARTR register
static inline int start_jr(int num)
{
	uint32_t ctpr = sec_in32(ptov((CAAM_BASE + SEC_REG_CTPR_MS_OFFSET)));
	uint32_t tmp = sec_in32(ptov((CAAM_BASE + SEC_REG_JRSTARTR_OFFSET)));
	uint32_t scfgr = sec_in32(ptov((CAAM_BASE + SEC_REG_SCFGR_OFFSET)));
	bool start = false;

	if (ctpr & CTPR_VIRT_EN_INC) {
		if ((ctpr & CTPR_VIRT_EN_POR) || (scfgr & SCFGR_VIRT_EN))
			start = true;
	} else {
		if (ctpr & CTPR_VIRT_EN_POR)
			start = true;
	}

	if (start == true) {
		switch (num) {
		case 0:
			tmp |= JRSTARTR_STARTJR0;
			break;
		case 1:
		case 2:
		case 3:
		default:
			DMSG("JR%d selected, only JR0 is supported", num);
			return -1;
		}
	}
	sec_out32(ptov(CAAM_BASE + SEC_REG_JRSTARTR_OFFSET), tmp);

	return 0;
}

 // This functions configures the Job Ring
 // JR3 is reserved for use by Secure world
static int configure_jr(int num)
{
	int ret;
	void *reg_base_addr;

	switch (num) {
	case 0:
		reg_base_addr = (void *)(CAAM_BASE + CAAM_JR0_OFFSET);
		break;
	case 1:
	case 2:
	case 3:
	default:
		DMSG("JR%d selected, only JR0 is supported", num);
		return -1;
	}

	// Initialize the JR library
	ret = sec_jr_lib_init();
	if (ret != 0) {
		DMSG("Error in sec_jr_lib_init");
		return -1;
	}

	ret = start_jr(num);
	if (ret != 0) {
		DMSG("Error starting job ring");
		return -1;
	}

	// Do HW configuration of the JR
	job_ring = init_job_ring(SEC_NOTIFICATION_TYPE_POLL, 0, 0,
				 reg_base_addr, 0);

	if (!job_ring) {
		DMSG("Error in init_job_ring");
		return -1;
	}

	return ret;
}

 // This function initializes SEC block, does basic parameter setting
 // configures the default Job ring assigned to TZ /secure world
 // Instantiates the RNG
int sec_init(void)
{
	int ret;
	uint32_t mcfgr;

	if (sec_initialized == true)
		return 0;

	mcfgr = sec_in32(ptov(CAAM_BASE + SEC_REG_MCFGR_OFFSET));

	// Modify CAAM Read/Write attributes
	// AXI Write - Cacheable, WB and WA
	// AXI Read - Cacheable, RA
#if defined(CONFIG_ARCH_LS2080A) || defined(CONFIG_ARCH_LS2088A)
	mcfgr = (mcfgr & ~MCFGR_AWCACHE_MASK) | (0xb << MCFGR_AWCACHE_SHIFT);
	mcfgr = (mcfgr & ~MCFGR_ARCACHE_MASK) | (0x6 << MCFGR_ARCACHE_SHIFT);
#else
	mcfgr = (mcfgr & ~MCFGR_AWCACHE_MASK) | (0x2 << MCFGR_AWCACHE_SHIFT);
#endif

	// Set PS bit to 1
#ifdef CONFIG_PHYS_64BIT
	mcfgr |= (1 << MCFGR_PS_SHIFT);
#endif

	sec_out32(ptov(CAAM_BASE + SEC_REG_MCFGR_OFFSET), mcfgr);

	DMSG("MCFGR: 0x%x", mcfgr);

	// Configure the default JR for usage

	ret = configure_jr(DEFAULT_JR);
	if (ret != 0) {
		DMSG("FSL_JR: configuraiton failure\n");
		return -1;
	}

	// Do TZ configuration of default JR for sec firmware
	ret = config_tz(DEFAULT_JR);
	if (ret != 0) {
		DMSG("TZ onfiguraiton failure\n");
		return -1;
	}

	// Instantiate the RNG

	ret = hw_rng_instantiate();
	if (ret != 0) {
		EMSG("RNG instantiation failure\n");
		return -1;
	}

	sec_initialized = true;

	return ret;
}

 // This function is used for sumbitting job to the Job Ring
 // [param] [in] - jobdesc to be submitted
 // Return - -1 in case of error and 0 in case of SUCCESS
int run_descriptor_jr(struct job_descriptor *jobdesc)
{
	int ret = 0;

	cpu_spin_lock(&jr_lock);

	ret = enq_jr_desc(job_ring, jobdesc);
	if (ret == 0) {
		DMSG("JR enqueue done...");
	} else {
		DMSG("Error in Enqueue");
		ret = -1;
		goto exit;
	}

	ret = dequeue_jr(job_ring, -1);
	if (ret > 0) {
		DMSG("Dequeue of success 0x%x", ret);
		ret = 0;
	} else {
		DMSG("deq_ret 0x%x", ret);
		ret = -1;
	}

exit:
	cpu_spin_unlock(&jr_lock);
	return ret;
}

void plat_rng_init(void)
{
	int ret;

	ret = sec_init();
	if (ret == 0)
		DMSG("sec_init succeeded");
	else
		EMSG("sec_init failed");
}
