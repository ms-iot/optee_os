/*
 * Copyright (c) 2016, Linaro Limited
 * Copyright (c) 2014, STMicroelectronics International N.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __DRIVERS_GIC_H
#define __DRIVERS_GIC_H
#include <types_ext.h>
#include <kernel/interrupt.h>

#define GIC_DIST_REG_SIZE	0x10000
#define GIC_CPU_REG_SIZE	0x10000

struct gic_per_cpu_state {
    /* CPU interface control register */
    uint32_t iccicr;
    /* Interrupt priority mask register */
    uint32_t iccpmr;
    /* binary point register */
    uint32_t iccbpr;
    /* banked per-processor enable-set registers */
    uint32_t icdiser;
    /* banked per-processor active-set reigsters */
    uint32_t icdabr;
    /* banked per-processor config registers */
    uint32_t icdicfr[2]; 
};

struct gic_saved_state {
    /* distributor control register */
    uint32_t icddcr;
    /* interrupt security registers */
    uint32_t icdisr[32];
    /* interrupt set-enable registers */
    uint32_t icdiser[32];
    /* active bit registers */
    uint32_t icdabr[32];
    /* interrupt priority registers */
    uint32_t icdipr[255];
    /* interrupt processor targets registers */
    uint32_t icdiptr[255];
    /* interrupt configuration registers */
    uint32_t icdicfr[64];
    
    struct gic_per_cpu_state per_cpu[8];
};

struct gic_data {
	vaddr_t gicc_base;
	vaddr_t gicd_base;
	size_t max_it;
	struct itr_chip chip;
    struct gic_saved_state saved_state;
};

/*
 * The two gic_init_* functions initializes the struct gic_data which is
 * then used by the other functions.
 */

void gic_init(struct gic_data *gd, paddr_t gicc_base, paddr_t gicd_base);
/* initial base address only */
void gic_init_base_addr(struct gic_data *gd, vaddr_t gicc_base,
			vaddr_t gicd_base);
/* initial cpu if only, mainly use for secondary cpu setup cpu interface */
void gic_cpu_init(struct gic_data *gd);

void gic_it_handle(struct gic_data *gd);

void gic_dump_state(struct gic_data *gd);

void gic_save_state(struct gic_data *gd);

void gic_restore_state(struct gic_data *gd);

#endif /*__DRIVERS_GIC_H*/
