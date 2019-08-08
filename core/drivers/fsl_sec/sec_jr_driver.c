// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2017 NXP
 *
 *  Alexandru Porosanu <alexandru.porosanu@nxp.com>
 *  Ruchika Gupta <ruchika.gupta@nxp.com>
 */

#include <trace.h>
#include "sec_hw_specific.h"
#include "sec_jr_driver.h"
#include "malloc.h"
#include "string.h"
#include "jobdesc.h"

#define CAAM_TIMEOUT 200000 //ms
			    // Job rings used for communication with SEC HW
struct sec_job_ring_t g_job_rings[MAX_SEC_JOB_RINGS];

// The current state of SEC user space driver
volatile enum sec_driver_state_e g_driver_state = SEC_DRIVER_STATE_IDLE;

int g_job_rings_no;

void *init_job_ring(uint8_t jr_mode,
		    uint16_t irq_coalescing_timer,
		    uint8_t irq_coalescing_count,
		    void *reg_base_addr, uint32_t irq_id)
{
	struct sec_job_ring_t *job_ring = &g_job_rings[g_job_rings_no++];
	int ret = 0;
	unsigned long start, end;
	void *tmp;

	job_ring->register_base_addr = reg_base_addr;
	job_ring->jr_mode = jr_mode;
	job_ring->irq_fd = irq_id;

	// Allocate mem for input and output ring
	tmp = fsl_sec_memalign(64, SEC_DMA_MEM_INPUT_RING_SIZE);
	job_ring->input_ring = vtop(tmp);
	memset(tmp, 0, SEC_DMA_MEM_INPUT_RING_SIZE);
	start = (unsigned long) tmp;
	end = ROUNDUP(start + SEC_DMA_MEM_INPUT_RING_SIZE, STACK_ALIGNMENT);
	cache_operation(TEE_CACHEFLUSH, (void *) start, (size_t)(end - start));

	// Allocate memory for output ring
	tmp = fsl_sec_memalign(64, SEC_DMA_MEM_OUTPUT_RING_SIZE);
	job_ring->output_ring = (struct sec_outring_entry *)vtop(tmp);
	memset(tmp, 0, SEC_DMA_MEM_OUTPUT_RING_SIZE);
	start = (unsigned long) tmp;
	end = ROUNDUP(start + SEC_DMA_MEM_OUTPUT_RING_SIZE, STACK_ALIGNMENT);
	cache_operation(TEE_CACHEFLUSH, (void *) start, (size_t)(end - start));

	// Reset job ring in SEC hw and configure job ring registers
	ret = hw_reset_job_ring(job_ring);
	if (ret) {
		DMSG("Failed to reset hardware job ring\n");
		return NULL;
	}

	if (jr_mode == SEC_NOTIFICATION_TYPE_IRQ) {
		// Enble IRQ if driver work sin interrupt mode
		DMSG("IRQ generation enabled");
		ret = jr_enable_irqs(irq_id);
		if (ret) {
			DMSG("Failed to enable irqs for job ring\n");
			return NULL;
		}
	}
	if (irq_coalescing_timer || irq_coalescing_count) {
		hw_job_ring_set_coalescing_param(job_ring,
						 irq_coalescing_timer,
						 irq_coalescing_count);

		hw_job_ring_enable_coalescing(job_ring);
		job_ring->coalescing_en = 1;
	}

	job_ring->jr_state = SEC_JOB_RING_STATE_STARTED;

	return job_ring;
}

int sec_release(void)
{
	int i;

	// Validate driver state
	if (g_driver_state == SEC_DRIVER_STATE_RELEASE) {
		DMSG("Driver release is already in progress");
		return SEC_DRIVER_RELEASE_IN_PROGRESS;
	}
	// Update driver state
	g_driver_state = SEC_DRIVER_STATE_RELEASE;

	// If any descriptors in flight , poll and wait
	// until all descriptors are received and silently discarded.

	flush_job_rings();

	for (i = 0; i < g_job_rings_no; i++)
		shutdown_job_ring(&g_job_rings[i]);
	g_job_rings_no = 0;
	g_driver_state = SEC_DRIVER_STATE_IDLE;

	return SEC_SUCCESS;
}

int sec_jr_lib_init(void)
{
	// Validate driver state
	if (g_driver_state != SEC_DRIVER_STATE_IDLE) {
		DMSG("Driver already initialized\n");
		return 0;
	}

	memset(g_job_rings, 0, sizeof(g_job_rings));
	g_job_rings_no = 0;

	// Update driver state
	g_driver_state = SEC_DRIVER_STATE_STARTED;
	return 0;
}

int dequeue_jr(void *job_ring_handle, int32_t limit)
{
	int ret = 0;
	int notified_descs_no = 0;
	struct sec_job_ring_t *job_ring =
		(struct sec_job_ring_t *)job_ring_handle;
	unsigned long start_time;

	// Validate driver state
	if (g_driver_state != SEC_DRIVER_STATE_STARTED) {
		DMSG("Driver release is in progress or driver not init");
		return -1;
	}

	// Validate input arguments
	if (job_ring == NULL) {
		DMSG("job_ring_handle is NULL\n");
		return -1;
	}
	if (((limit == 0) || (limit > SEC_JOB_RING_SIZE))) {
		DMSG("Invalid limit parameter configuration");
		return -1;
	}

	//DMSG("JR Polling");
	//DMSG("limit[%d]", limit);

	// Poll job ring
	// If limit < 0 -> poll JR until no more notifications are available.
	// If limit > 0 -> poll JR until limit is reached.

	start_time = 0;

	while (notified_descs_no == 0) {
		// Run hw poll job ring
		notified_descs_no = hw_poll_job_ring(job_ring, limit);
		if (notified_descs_no < 0) {
			DMSG("Error polling SEC engine job ring");
			return notified_descs_no;
		}
		//DMSG("Jobs notified[%d]", notified_descs_no);

		start_time++;
		if (start_time == 400)
			break;

	}

	if (job_ring->jr_mode == SEC_NOTIFICATION_TYPE_IRQ) {

		// Always enable IRQ generation when in pure IRQ mode
		ret = jr_enable_irqs(job_ring->irq_fd);
		if (ret) {
			DMSG("Failed to enable irqs for job ring %p",
			     (void *) job_ring);
			return ret;
		}
	}
	return notified_descs_no;
}

int enq_jr_desc(void *job_ring_handle, struct job_descriptor *jobdescr)
{
	struct sec_job_ring_t *job_ring;
	unsigned long start, end;
	TEE_Result result = TEE_SUCCESS;

	job_ring = (struct sec_job_ring_t *)job_ring_handle;


	// Validate driver state
	if (g_driver_state != SEC_DRIVER_STATE_STARTED) {
		DMSG("Driver release in progress or driver not initialized");
		return -1;
	}

	// Check job ring state
	if (job_ring->jr_state != SEC_JOB_RING_STATE_STARTED) {
		DMSG("Job ring is currently resetting.");
		return -1;
	}

	if (SEC_JOB_RING_IS_FULL(job_ring->pidx, job_ring->cidx,
				 SEC_JOB_RING_SIZE, SEC_JOB_RING_SIZE)) {
		DMSG("Job ring is full\n");
		return -1;
	}

#if 0
	DMSG("Sending desc at virtual address %p", (void *)jobdescr->desc);
	DHEXDUMP(jobdescr->desc, desc_length(jobdescr->desc) * 4);
#endif

	// Set ptr in input ring to current descriptor
	out32((vaddr_t)phys_to_virt(
			(paddr_t)&job_ring->input_ring[job_ring->pidx],
			MEM_AREA_TEE_RAM),
		(phys_addr_t)vtop(jobdescr->desc));

	// Notify HW that a new job is enqueued

	start = (unsigned long) phys_to_virt((paddr_t)job_ring->input_ring,
					     MEM_AREA_TEE_RAM);
	end = ROUNDUP(start + SEC_JOB_RING_SIZE * 4, STACK_ALIGNMENT);
	result = cache_operation(TEE_CACHEFLUSH, (void *) start,
				 (size_t)(end - start));
	if (result != TEE_SUCCESS)
		DMSG("Cache flush input_ring failed!");

	start = (unsigned long)jobdescr->desc;
	end = ROUNDUP(start + desc_length(jobdescr->desc)*4, STACK_ALIGNMENT);
	result = cache_operation(TEE_CACHEFLUSH, (void *) start,
				 (size_t)(end - start));
	if (result != TEE_SUCCESS)
		DMSG("Cache flush desc failed!");

	start = (unsigned long) phys_to_virt((paddr_t)job_ring->output_ring,
					     MEM_AREA_TEE_RAM);
	end = ROUNDUP(start + SEC_DMA_MEM_OUTPUT_RING_SIZE, STACK_ALIGNMENT);
	result = cache_operation(TEE_CACHEINVALIDATE, (void *) start,
				 (size_t)(end - start));
	if (result != TEE_SUCCESS)
		DMSG("Invalidate output_ring failed!");

	hw_enqueue_desc_on_job_ring(
			(struct jr_regs *)job_ring->register_base_addr, 1);

	// increment the producer index for the current job ring
	job_ring->pidx = SEC_CIRCULAR_COUNTER(job_ring->pidx,
					      SEC_JOB_RING_SIZE);

	return 0;
}
