/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Peng Fan <peng.fan@nxp.com>
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
#include <console.h>
#include <drivers/imx_uart.h>
#include <io.h>
#include <kernel/generic_boot.h>
#include <kernel/panic.h>
#include <kernel/pm_stubs.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <mm/tee_pager.h>
#include <platform_config.h>
#include <stdint.h>
#include <string.h>
#include <sm/optee_smc.h>
#include <sm/psci.h>
#include <tee/entry_std.h>
#include <tee/entry_fast.h>

struct gpio_regs {
    uint32_t gpio_dr;/* data */
    uint32_t gpio_dir;/* direction */
    uint32_t gpio_psr;/* pad satus */
};

extern void imx_resume (uint32_t r0);
extern uint8_t imx_resume_start;
extern uint8_t imx_resume_end;

static vaddr_t src_base(void)
{
	static void *va __data; /* in case it's used before .bss is cleared */

	if (cpu_mmu_enabled()) {
		if (!va)
			va = phys_to_virt(SRC_BASE, MEM_AREA_IO_SEC);
		return (vaddr_t)va;
	}
	return SRC_BASE;
}

/*
 * phys_base - physical base address of peripheral
 * mem_area - MEM_AREA_IO_SEC or MEM_AREA_IO_NSEC
 */
static vaddr_t periph_base(uint32_t phys_base, uint32_t mem_area)
{

	if (cpu_mmu_enabled()) {
	    return (vaddr_t)phys_to_virt(phys_base, mem_area);
	}
	return phys_base;
}

int psci_cpu_on(uint32_t core_idx, uint32_t entry,
		uint32_t context_id __attribute__((unused)))
{
	uint32_t val;
	vaddr_t va = src_base();

	if ((core_idx == 0) || (core_idx >= CFG_TEE_CORE_NB_CORE))
		return PSCI_RET_INVALID_PARAMETERS;

	/* set secondary cores' NS entry addresses */
	ns_entry_addrs[core_idx] = entry;

	/* boot secondary cores from OP-TEE load address */
	write32((uint32_t)CFG_TEE_LOAD_ADDR, va + SRC_GPR1 + core_idx * 8);

	/* release secondary core */
	val = read32(va + SRC_SCR);
	val |=  BIT32(SRC_SCR_CORE1_ENABLE_OFFSET + (core_idx - 1));
	val |=  BIT32(SRC_SCR_CORE1_RST_OFFSET + (core_idx - 1));
	write32(val, va + SRC_SCR);

	return PSCI_RET_SUCCESS;
}

int psci_cpu_suspend (
    uint32_t power_state,
    uintptr_t entry __unused,
    uint32_t context_id __unused
    )
{
    vaddr_t gpio_base;
    vaddr_t ocram_base;
    struct gpio_regs* gpio;
    uint32_t gpio_dir;
    uint32_t gpio_dr;
    uint32_t resume_fn_len;

    //uint32_t dr;

    DMSG("Hello from psci_cpu_suspend (power_state = %d)\n", power_state);  

    DMSG(
        "Length of imx_resume = %d\n",
        &imx_resume_end - &imx_resume_start);

    gpio_base = periph_base(GPIO_BASE, MEM_AREA_IO_NSEC);
    ocram_base = periph_base(OCRAM_BASE, MEM_AREA_RAM_SEC);

    DMSG(
        "gpio_base = 0x%x, ocram_base = 0x%x, imx_resume = 0x%x, "
        "&imx_resume_start = 0x%x",
        (uint32_t)gpio_base,
        (uint32_t)ocram_base,
        (uint32_t)imx_resume,
        (uint32_t)&imx_resume_start);
    
    DMSG("Setting LED pin to output in ON state\n");
    gpio = (struct gpio_regs*)gpio_base;
    gpio_dir = read32((vaddr_t)&gpio->gpio_dir);
    gpio_dir |= (1 << 2);
    write32(gpio_dir, (vaddr_t)&gpio->gpio_dir);

    gpio_dr = read32((vaddr_t)&gpio->gpio_dr);
    gpio_dr |= (1 << 2);
    write32(gpio_dr, (vaddr_t)&gpio->gpio_dr);

    DMSG("Successfully configured LED\n");

    // copy imx_resume to ocram
    resume_fn_len = &imx_resume_end - &imx_resume_start;
    memcpy(
        (void*)ocram_base,
        &imx_resume_start,
        resume_fn_len);
  
    cache_maintenance_l1(DCACHE_AREA_CLEAN, (void*)ocram_base, resume_fn_len);
    cache_maintenance_l1(ICACHE_AREA_INVALIDATE, (void*)ocram_base, resume_fn_len);

    DMSG("Successfully copied memory to OCRAM and flushed cache, jumping to OCRAM\n");

    ((void (*)(uint32_t r0))ocram_base)((uint32_t)gpio_base);

    //dr = read32(gpio_base + 0);
    //dr &= ~(1 << 2);
    //write32(dr, gpio_base + 0);

    //imx_resume((uint32_t)gpio_base);

    DMSG("Successfully turned off LED\n");
    
    // need to copy resume code into OCRAM
    // First prove that we can run LED blink code from OCRAM
    // Then program the resume address into GPR1 and execute WFI

    return PSCI_RET_SUCCESS;
}

