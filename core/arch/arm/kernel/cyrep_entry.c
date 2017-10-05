
/** @file
*
*  Copyright (c) Microsoft Corporation. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/
#include <arm.h>
#include <assert.h>
#include <compiler.h>
#include <kernel/panic.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <inttypes.h>
#include <stdio.h>
#include <trace.h>
#include <Cyrep.h>
#include <kernel/cyrep_entry.h>
#include <string.h>

paddr_t cyrep_args_pa __section(".data");

static bool cache_flush_range(paddr_t pa, void *va, size_t len)
{
	if (cache_op_inner(DCACHE_AREA_CLEAN, va, len) != TEE_SUCCESS) {
		return false;
	}

	if (cache_op_outer(DCACHE_AREA_CLEAN_INV, pa, len) != TEE_SUCCESS) {
		return false;
	}

	if (cache_op_inner(DCACHE_AREA_CLEAN_INV, va, len) != TEE_SUCCESS) {
		return false;
	}

	return true;
}

/*
 * Measures the non-secure firmware which OPTEE will jump to and returns
 * the physical address to the cyrep args to be passed along with the bootargs
 * to the next non-sec firmware.
 * NOTE: This function executes after the MMU has been enabled. Any address
 * captured during cyrep_entry should be re-transalted to its virt address
 * before usage to avoid translation faults.
 * NOTE: This function should be called once per u-boot lifetime.
 */
paddr_t cyrep_measure_nsfw(void)
{
	CyrepFwArgs *cyrep_args;
	CyrepFwInfo nsfw_info;
	CyrepFwMeasurement optee_measurement;
	paddr_t nsfw_cyrep_args_pa = 0;
	void *fw_base;

	/* [mlotfy] FIXME: See the issue description below
	vaddr_t fw_page_start_va;
	size_t fw_num_pages;
	*/

	if (cyrep_args_pa == 0) {
		EMSG("AliasKeyPrivate hasn't been captured or it was "
			"consumed and got cleared-out");

		goto exit;
	}

	if (!core_mmu_add_mapping(MEM_AREA_RAM_NSEC, cyrep_args_pa, sizeof(CyrepFwArgs))) {
		EMSG("core_mmu_add_mapping failed to map cyrep_args");
		goto exit;
	}

	cyrep_args = (void *)phys_to_virt(cyrep_args_pa, MEM_AREA_RAM_NSEC);
	if (cyrep_args == NULL) {
		EMSG("phys_to_virt(cyrep_args_pa) failed");
		goto exit;
	}

	if (!Cyrep_ArgsVerify(cyrep_args)) {
		EMSG("Cyrep_VerifyArgs() failed");
		goto exit;
	}

	memmove(&optee_measurement, Cyrep_ArgsGetFwMeasurement(cyrep_args),
		sizeof(optee_measurement));

	memmove(&nsfw_info, Cyrep_ArgsGetFwInfo(cyrep_args), sizeof(nsfw_info));

	IMSG("CyReP: Measuring Non-sec firmware %s info: addr:0x%p size:%u",
		nsfw_info.FwName, nsfw_info.FwBase, (uint32_t)nsfw_info.FwSize);

	if (nsfw_info.FwBase == NULL || nsfw_info.FwSize == 0) {
		EMSG("Passed-in non-sec firmware image info is invalid");
		goto exit;
	}

	/*
	 * The FwBase passed in is a physical memory address. We have to convert it
	 * to OPTEE virtual address before Cyrep_* functions can consume it.
	 */

	if (!core_mmu_add_mapping(MEM_AREA_RAM_NSEC, (paddr_t)nsfw_info.FwBase, nsfw_info.FwSize)) {
		EMSG("core_mmu_add_mapping failed to map nsec fw physical base");
		goto exit;
	}
	
	fw_base = (void *)phys_to_virt((paddr_t)nsfw_info.FwBase, MEM_AREA_RAM_NSEC);
	if (fw_base == NULL) {
		EMSG("phys_to_virt(nsfw_info.FwBase) failed");
		goto exit;
	}

	nsfw_info.FwBase = fw_base;

	Cyrep_ArgsInit(cyrep_args);

	if (!Cyrep_MeasureL2plusFirmware(&optee_measurement, &nsfw_info, "OPTEE",
		Cyrep_ArgsGetFwMeasurement(cyrep_args))) {

		EMSG("Cyrep_MeasureL2plusImage() failed");
		goto exit;
	}

	if (!Cyrep_ArgsPostprocess(cyrep_args)) {
		EMSG("Cyrep_PostprocessArgs(cyrep_args) failed");
		goto exit;
	}

	/*
	 * Clear away our private key, we are done with it and no need to
	 * keep it around.
	 */
	Cyrep_SecureClearMemory(&optee_measurement.AliasKeyPriv,
		sizeof(optee_measurement.AliasKeyPriv));

	/*
	 * Flush caches because the non-sec firmware we are handing over to will be
	 * accessing the CyReP args directly from physical memory and it will very
	 * likely have caches disabled or invalidated very early.
	 */
	if (!cache_flush_range(cyrep_args_pa, cyrep_args, sizeof(*cyrep_args))) {
		EMSG("cache_flush_range() failed");
		goto exit;
	}

	/*
	 * [mlotfy] FIXME: unmap the mapped area before returning.
	 * The code below doesn't calculate page boundaries and size correctly
	 * leading to a panic in core_mmu_unmap_pages.
	 */
	/*
	fw_page_start_va = fw_base_va & ~SMALL_PAGE_MASK;
	fw_num_pages = ROUNDUP((fw_base_va - fw_page_start_va) + nsfw_info->FwSize,
							SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE;

	core_mmu_unmap_pages(fw_page_start_va, fw_num_pages);
	*/

	nsfw_cyrep_args_pa = cyrep_args_pa;

exit:
	/* Clear the args pointer so that this function can't be called again */
	cyrep_args_pa = 0;

	return nsfw_cyrep_args_pa;
}