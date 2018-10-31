// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2017 NXP
 *
 *  Alexandru Porosanu <alexandru.porosanu@nxp.com>
 *  Ruchika Gupta <ruchika.gupta@nxp.com>
 */

#include <stdint.h>
#include <trace.h>
#include "sec_hw_specific.h"

// Used to retry resetting a job ring in SEC hardware.
#define SEC_TIMEOUT 100000

// Job rings used for communication with SEC HW
extern struct sec_job_ring_t g_job_rings[MAX_SEC_JOB_RINGS];

// The current state of SEC user space driver
extern volatile enum sec_driver_state_e g_driver_state;

// The number of job rings used by SEC user space driver
extern int g_job_rings_no;

// LOCAL FUNCTIONS
// =============================================================================
static inline void hw_set_input_ring_start_addr(struct jr_regs *regs,
						phys_addr_t *start_addr)
{
	DMSG("HW_SET_INPUT_RING_START_ADDR: %x", (unsigned int) start_addr);
#if defined(CONFIG_PHYS_64BIT)
	sec_out32(ptov(&regs->irba_h), (uint32_t) PHYS_ADDR_HI(start_addr));
#else
	sec_out32(ptov(&regs->irba_h), 0);
#endif
	sec_out32(ptov(&regs->irba_l), (uint32_t) PHYS_ADDR_LO(start_addr));
}

static inline void hw_set_output_ring_start_addr(struct jr_regs *regs,
						 phys_addr_t *start_addr)
{
#if defined(CONFIG_PHYS_64BIT)
	sec_out32(ptov(&regs->orba_h), PHYS_ADDR_HI(start_addr));
#else
	sec_out32(ptov(&regs->orba_h), 0);
#endif
	sec_out32(ptov(&regs->orba_l), (uint32_t) PHYS_ADDR_LO(start_addr));
}

// ORJR - Output Ring Jobs Removed Register shows how many jobs were
//removed from the Output Ring for processing by software. This is done after
//the software has processed the entries.
static inline void hw_remove_entries(struct sec_job_ring_t *jr, int num)
{
	struct jr_regs *regs = (struct jr_regs *)jr->register_base_addr;

	sec_out32(ptov(&regs->orjr), num);
}

// IRSA - Input Ring Slots Available register holds the number of entries in
//the Job Ring's input ring. Once a job is enqueued, the value returned is
//decremented by the hardware by the number of jobs enqueued.
static inline int hw_get_available_slots(struct sec_job_ring_t *jr)
{
	struct jr_regs *regs = (struct jr_regs *)jr->register_base_addr;

	return sec_in32(ptov(&regs->irsa));
}

// ORSFR - Output Ring Slots Full register holds the number of jobs which were
//processed by the SEC and can be retrieved by the software. Once a job has
//been processed by software, the user will call hw_remove_one_entry in order
//to notify the SEC that the entry was processed
static inline int hw_get_no_finished_jobs(struct sec_job_ring_t *jr)
{
	struct jr_regs *regs = (struct jr_regs *)jr->register_base_addr;

	return sec_in32(ptov(&regs->orsf));
}

// @brief Process Jump Halt Condition related errors
//@param [in]  error_code        The error code in the descriptor status word

static inline void hw_handle_jmp_halt_cond_err(union hw_error_code
						   error_code __maybe_unused)
{
	DMSG("JMP %x", error_code.error_desc.jmp_halt_cond_src.jmp);
	DMSG("Descriptor Index: %x",
	     error_code.error_desc.jmp_halt_cond_src.desc_idx);
	DMSG(" Condition %x", error_code.error_desc.jmp_halt_cond_src.cond);
}

// @brief Process DECO related errors
//@param [in]  error_code        The error code in the descriptor status word

static inline void hw_handle_deco_err(union hw_error_code
					  error_code)
{
	DMSG("JMP %x", error_code.error_desc.deco_src.jmp);
	DMSG("Desc Index: 0x%x", error_code.error_desc.deco_src.desc_idx);

	switch (error_code.error_desc.deco_src.desc_err) {
	case SEC_HW_ERR_DECO_HFN_THRESHOLD:
		DMSG(" Warning: Desc success, but 3GPP HFN at threshold");
		break;
	default:
		DMSG("Error 0x%04x not implemented",
		     error_code.error_desc.deco_src.desc_err);
		break;
	}
}

// @brief Process  Jump Halt User Status related errors
//@param [in]  error_code        The error code in the descriptor status word

static inline void hw_handle_jmp_halt_user_err(union hw_error_code
						   error_code __unused)
{
	DMSG(" Not implemented");
}

// @brief Process CCB related errors
//@param [in]  error_code        The error code in the descriptor status word

static inline void hw_handle_ccb_err(union hw_error_code hw_error_code __unused)
{
	DMSG(" Not implemented");
}

// @brief Process Job Ring related errors
//@param [in]  error_code        The error code in the descriptor status word

static inline void hw_handle_jr_err(union hw_error_code hw_error_code __unused)
{
	DMSG(" Not implemented");
}

// GLOBAL FUNCTIONS

int hw_reset_job_ring(struct sec_job_ring_t *job_ring)
{
	int ret = 0;
	struct jr_regs *regs = (struct jr_regs *)job_ring->register_base_addr;

	// First reset the job ring in hw
	ret = hw_shutdown_job_ring(job_ring);
	if (ret) {
		DMSG("Failed resetting job ring in hardware");
		return ret;
	}

	// In order to have the HW JR in a workable state
	//after a reset, I need to re-write the input
	//queue size, input start address, output queue
	//size and output start address

	// Write the JR input queue size to the HW register
	sec_out32(ptov(&regs->irs), SEC_JOB_RING_SIZE);

	// Write the JR output queue size to the HW register
	sec_out32(ptov(&regs->ors), SEC_JOB_RING_SIZE);

	// Write the JR input queue start address
	hw_set_input_ring_start_addr(regs,
				(phys_addr_t *)vtop(job_ring->input_ring));

	// Write the JR output queue start address
	hw_set_output_ring_start_addr(regs,
				(phys_addr_t *)vtop(job_ring->output_ring));

	return 0;
}

int hw_shutdown_job_ring(struct sec_job_ring_t *job_ring)
{
	struct jr_regs *regs = (struct jr_regs *)job_ring->register_base_addr;
	unsigned int timeout = SEC_TIMEOUT;
	uint32_t tmp = 0;

	DMSG("Resetting Job ring %p", (void *)job_ring);

	//
	//Mask interrupts since we are going to poll
	//for reset completion status
	//Also, at POR, interrupts are ENABLED on a JR, thus
	//this is the point where I can disable them without
	//changing the code logic too much

	jr_disable_irqs(job_ring->irq_fd);

	// initiate flush (required prior to reset)
	sec_out32(ptov(&regs->jrcr), JR_REG_JRCR_VAL_RESET);

	// dummy read
	tmp = sec_in32(ptov(&regs->jrcr));

	do {
		tmp = sec_in32(ptov(&regs->jrint));
	} while (((tmp & JRINT_ERR_HALT_MASK) == JRINT_ERR_HALT_INPROGRESS) &&
		 --timeout);

	if ((tmp & JRINT_ERR_HALT_MASK) != JRINT_ERR_HALT_COMPLETE ||
	    timeout == 0) {
		DMSG("Failed to flush hw job ring\n");
		DMSG("0x%x", tmp);
		DMSG("timeout %d", timeout);
		// unmask interrupts
		if (job_ring->jr_mode != SEC_NOTIFICATION_TYPE_POLL)
			jr_enable_irqs(job_ring->irq_fd);
		return -1;
	}

	// Initiate reset
	timeout = SEC_TIMEOUT;
	sec_out32(ptov(&regs->jrcr), JR_REG_JRCR_VAL_RESET);

	do {
		tmp = sec_in32(ptov(&regs->jrcr));
	} while ((tmp & JR_REG_JRCR_VAL_RESET) && --timeout);

	if (timeout == 0) {
		DMSG("Failed to reset hw job ring %p", (void *)job_ring);
		// unmask interrupts
		if (job_ring->jr_mode != SEC_NOTIFICATION_TYPE_POLL)
			jr_enable_irqs(job_ring->irq_fd);
		return -1;
	}
	// unmask interrupts
	if (job_ring->jr_mode != SEC_NOTIFICATION_TYPE_POLL)
		jr_enable_irqs(job_ring->irq_fd);
	return 0;
}

void hw_handle_job_ring_error(struct sec_job_ring_t *job_ring __unused,
			      uint32_t error_code)
{
	union hw_error_code hw_err_code;

	hw_err_code.error = error_code;

	switch (hw_err_code.error_desc.value.ssrc) {
	case SEC_HW_ERR_SSRC_NO_SRC:
		DMSG("No Status Source ");
		break;
	case SEC_HW_ERR_SSRC_CCB_ERR:
		DMSG("CCB Status Source");
		hw_handle_ccb_err(hw_err_code);
		break;
	case SEC_HW_ERR_SSRC_JMP_HALT_U:
		DMSG("Jump Halt User Status Source");
		hw_handle_jmp_halt_user_err(hw_err_code);
		break;
	case SEC_HW_ERR_SSRC_DECO:
		DMSG("DECO Status Source");
		hw_handle_deco_err(hw_err_code);
		break;
	case SEC_HW_ERR_SSRC_JR:
		DMSG("Job Ring Status Source");
		hw_handle_jr_err(hw_err_code);
		break;
	case SEC_HW_ERR_SSRC_JMP_HALT_COND:
		DMSG("Jump Halt Condition Codes");
		hw_handle_jmp_halt_cond_err(hw_err_code);
		break;
	default:
		DMSG("Unknown SSRC");
		break;
	}
}

int hw_job_ring_error(struct sec_job_ring_t *job_ring)
{
	uint32_t jrint_error_code;
	struct jr_regs *regs = (struct jr_regs *)job_ring->register_base_addr;

	if (JR_REG_JRINT_JRE_EXTRACT(sec_in32(ptov(&regs->jrint))) == 0)
		return 0;

	jrint_error_code = JR_REG_JRINT_ERR_TYPE_EXTRACT(
				sec_in32(ptov(&regs->jrint)));
	switch (jrint_error_code) {
	case JRINT_ERR_WRITE_STATUS:
		DMSG("Error writing status to Output Ring ");
		break;
	case JRINT_ERR_BAD_INPUT_BASE:
		DMSG("Bad Input Ring Base (not on a 4-byte boundary) ");
		break;
	case JRINT_ERR_BAD_OUTPUT_BASE:
		DMSG("Bad Output Ring Base (not on a 4-byte boundary) ");
		break;
	case JRINT_ERR_WRITE_2_IRBA:
		DMSG("Invalid write to Input Ring Base Address Register ");
	case JRINT_ERR_WRITE_2_ORBA:
		DMSG("Invalid write to Output Ring Base Address Register ");
	case JRINT_ERR_RES_B4_HALT:
		DMSG("Job Ring released before Job Ring is halted");
		break;
	case JRINT_ERR_REM_TOO_MANY:
		DMSG("Removed too many jobs from job ring");
		break;
	case JRINT_ERR_ADD_TOO_MANY:
		DMSG("Added too many jobs on job ring");
		break;
	default:
		DMSG(" Unknown SEC JR Error :%d", jrint_error_code);
		break;
	}
	return jrint_error_code;
}

int hw_job_ring_set_coalescing_param(struct sec_job_ring_t *job_ring,
				     uint16_t irq_coalescing_timer,
				     uint8_t irq_coalescing_count)
{
	uint32_t reg_val = 0;
	struct jr_regs *regs = (struct jr_regs *)job_ring->register_base_addr;

	// Set descriptor count coalescing
	reg_val |= (irq_coalescing_count << JR_REG_JRCFG_LO_ICDCT_SHIFT);

	// Set coalescing timer value
	reg_val |= (irq_coalescing_timer << JR_REG_JRCFG_LO_ICTT_SHIFT);

	// Update parameters in HW
	sec_out32(ptov(&regs->jrcfg1), reg_val);

	DMSG("Set coalescing params on jr %p ", (void *)job_ring);
	DMSG("timer %d", irq_coalescing_timer);
	DMSG("desc count %d", irq_coalescing_timer);

	return 0;
}

int hw_job_ring_enable_coalescing(struct sec_job_ring_t *job_ring)
{
	uint32_t reg_val = 0;
	struct jr_regs *regs = (struct jr_regs *)job_ring->register_base_addr;

	// Get the current value of the register
	reg_val = sec_in32(ptov(&regs->jrcfg1));

	// Enable coalescing
	reg_val |= JR_REG_JRCFG_LO_ICEN_EN;

	// Write in hw
	sec_out32(ptov(&regs->jrcfg1), reg_val);

	DMSG("Enabled coalescing on jr %p", (void *)job_ring);

	return 0;
}

int hw_job_ring_disable_coalescing(struct sec_job_ring_t *job_ring)
{
	uint32_t reg_val = 0;
	struct jr_regs *regs = (struct jr_regs *)job_ring->register_base_addr;

	// Get the current value of the register
	reg_val = sec_in32(ptov(&regs->jrcfg1));

	// Disable coalescing
	reg_val &= ~JR_REG_JRCFG_LO_ICEN_EN;

	// Write in hw
	sec_out32(ptov(&regs->jrcfg1), reg_val);

	DMSG("Disabled coalescing on jr %p", (void *)job_ring);

	return 0;
}

void hw_flush_job_ring(struct sec_job_ring_t *job_ring,
		       uint32_t do_notify,
		       uint32_t error_code __maybe_unused,
		       uint32_t *notified_descs)
{
	int32_t jobs_no_to_discard = 0;
	int32_t discarded_descs_no = 0;
	int32_t number_of_jobs_available = 0;
	//phys_addr_t current_desc;

	DMSG("JR[%p]", (void *)job_ring);
	DMSG("pi[%d]", job_ring->pidx);
	DMSG("ci[%d]", job_ring->cidx);
	DMSG("error code %x", error_code);
	DMSG("Notify_desc = %d", do_notify);

	number_of_jobs_available = hw_get_no_finished_jobs(job_ring);

	// Discard all jobs
	jobs_no_to_discard = number_of_jobs_available;

	DMSG("JR[%p]", (void *)job_ring);
	DMSG("pi[%d]", job_ring->pidx);
	DMSG("ci[%d]", job_ring->cidx);
	DMSG("Discarding desc = %d\n", jobs_no_to_discard);

	while (jobs_no_to_discard > discarded_descs_no) {
		// Get completed descriptor
		// Since the memory is contigous, then P2V translation is a
		//mere addition to
		//the base descriptor physical address
		//current_desc = job_ring->output_ring[job_ring->cidx].desc;

		discarded_descs_no++;
		// Now increment the consumer index for the current job ring,
		//AFTER saving job in temporary location!
		//Increment the consumer index for the current job ring

		job_ring->cidx = SEC_CIRCULAR_COUNTER(job_ring->cidx,
						      SEC_JOB_RING_SIZE);

		hw_remove_entries(job_ring, 1);
	}

	if (do_notify == true) {
		if (notified_descs == NULL)
			return;
		*notified_descs = discarded_descs_no;
	}
}

// return >0 in case of success
// -1 in case of error from SEC block
// 0 in case job not yet processed by SEC
//  or  Descriptor returned is NULL after dequeue
int hw_poll_job_ring(struct sec_job_ring_t *job_ring,
		     int32_t limit)
{
	int32_t jobs_no_to_notify = 0;
	int32_t number_of_jobs_available = 0;
	int32_t notified_descs_no = 0;
	uint32_t error_descs_no = 0;
	uint32_t sec_error_code = 0;
	uint32_t do_driver_shutdown = false;
	user_callback usercall = NULL;
	struct job_descriptor *current_desc;
	void *oraddr;
	size_t orsize;
	phys_addr_t current_desc_addr;

	// check here if any JR error that cannot be written
	//in the output status word has occurred

	sec_error_code = hw_job_ring_error(job_ring);
	if (unlikely(sec_error_code)) {
		DMSG("Error during initial processing");
		return -1;
	}
	// Compute the number of notifications that need to be raised to UA
	//If limit < 0 -> notify all done jobs
	//If limit > total number of done jobs -> notify all done jobs
	//If limit = 0 -> error
	//If limit > 0 && limit < total number of done jobs -> notify a number
	//of done jobs equal with limit

	//compute the number of jobs available in the job ring based on the
	//producer and consumer index values.

	number_of_jobs_available = hw_get_no_finished_jobs(job_ring);
	jobs_no_to_notify = (limit < 0 || limit > number_of_jobs_available) ?
			    number_of_jobs_available : limit;
#if 0
	DMSG("JR");
	DMSG("pi %d", job_ring->pidx);
	DMSG("ci %d", job_ring->cidx);
	DMSG("Jobs submitted %d", number_of_jobs_available);
	DMSG("Jobs to notify %d", jobs_no_to_notify);
#endif

	// Virtual address and size of output_ring for cache invalidate
	oraddr = phys_to_virt((paddr_t)job_ring->output_ring, MEM_AREA_TEE_RAM);
	orsize = ROUNDUP(SEC_DMA_MEM_OUTPUT_RING_SIZE, STACK_ALIGNMENT);

	while (jobs_no_to_notify > notified_descs_no) {
		// Invalidate the output ring in cache before we check it.
		cache_operation(TEE_CACHEINVALIDATE, oraddr, orsize);

		// Get job status here
		sec_error_code = job_ring->output_ring[job_ring->cidx].status;

		// Get completed descriptor
		current_desc_addr = (phys_addr_t)phys_to_virt(
			(paddr_t) sec_read_addr((phys_addr_t)
			&job_ring->output_ring[job_ring->cidx].desc),
			MEM_AREA_TEE_RAM);

		current_desc = (struct job_descriptor *)(current_desc_addr
							 - sizeof(user_callback)
							 - sizeof(void *));

		if (current_desc == 0) {
			DMSG("No descriptor returned from SEC");
			return 0;
		}

		// now increment the consumer index for the current job ring,
		//AFTER saving job in temporary location!
		job_ring->cidx = SEC_CIRCULAR_COUNTER(job_ring->cidx,
						      SEC_JOB_RING_SIZE);

		if (sec_error_code) {
			DMSG("desc at cidx %x ", job_ring->cidx);
			DMSG("generated error %x", sec_error_code);

			sec_handle_desc_error(job_ring,
					      sec_error_code,
					      &error_descs_no,
					      &do_driver_shutdown);

			hw_remove_entries(job_ring, 1);

			return -1;
		}

		// Signal that the job has been processed and the slot is free
		hw_remove_entries(job_ring, 1);
		notified_descs_no++;

		if (*(current_desc->callback)) {
			//DMSG("Calling descriptor callback\n");
			usercall = (user_callback) current_desc->callback;
			(*usercall)((uint32_t *)&current_desc->desc,
				sec_error_code, current_desc->arg, job_ring);
		}
	}

	return notified_descs_no;
}

void sec_handle_desc_error(struct sec_job_ring_t *job_ring,
			   uint32_t sec_error_code,
			   uint32_t *notified_descs __unused,
			   uint32_t *do_driver_shutdown __unused)
{
	// Analyze the SEC error on this job ring
	hw_handle_job_ring_error(job_ring, sec_error_code);
}

void flush_job_rings(void)
{
	struct sec_job_ring_t *job_ring = NULL;
	int i = 0;

	for (i = 0; i < g_job_rings_no; i++) {
		job_ring = &g_job_rings[i];
		// Producer index is frozen. If consumer index is not equal
		//with producer index, then we have descs to flush.

		while (job_ring->pidx != job_ring->cidx)
			hw_flush_job_ring(job_ring, false, 0, NULL);
	}
}

int shutdown_job_ring(struct sec_job_ring_t *job_ring)
{
	int ret = 0;

	ret = hw_shutdown_job_ring(job_ring);
	if (ret) {
		DMSG("Failed to shutdown hardware job ring");
		return ret;
	}

	if (job_ring->coalescing_en)
		hw_job_ring_disable_coalescing(job_ring);

	if (job_ring->jr_mode != SEC_NOTIFICATION_TYPE_POLL) {
		ret = jr_disable_irqs(job_ring->irq_fd);
		if (ret) {
			DMSG("Failed to disable irqs for job ring");
			return ret;
		}
	}

	return 0;
}

int jr_enable_irqs(uint32_t irq_id __unused)
{
	return 0;
}

int jr_disable_irqs(uint32_t irq_id __unused)
{
	return 0;
}
