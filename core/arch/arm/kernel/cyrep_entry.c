
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
#include <RiotDerEnc.h>
#include <RiotX509Bldr.h>
#include <Cyrep.h>
#include <kernel/cyrep_entry.h>
#include <string.h>

CyrepFwMeasurement optee_measurement;
CyrepFwInfo nsfw_info;

/*
 * Copies the CyReP args locally for use later.
 * There is no tolerance for error in this function, any error will result in
 * a panic.
 */
void cyrep_entry(uint32_t cyrep_args_addr)
{
	CyrepFwArgs *cyrep_args;

	IMSG("CyReP: Captured args cyrep_args_addr=0x%08x", cyrep_args_addr);

	cyrep_args = (CyrepFwArgs *)(void *)cyrep_args_addr;
	if (cyrep_args == NULL) {
		panic("cyrep_entry: cyrep_args_addr is invalid");
	}

	memcpy(&optee_measurement, &cyrep_args->FwMeasurement, sizeof(optee_measurement));

	if (cyrep_args->FwInfo == NULL) {
		panic("cyrep_entry: cyrep_args->FwInfo is invalid");
	}

	memcpy(&nsfw_info, cyrep_args->FwInfo, sizeof(nsfw_info));

	if (optee_measurement.CertChainCount == 0) {
		panic("cyrep_entry: OPTEE measurement certificate chain is empty");
	}

	if (nsfw_info.FwBase == NULL || nsfw_info.FwSize == 0) {
		panic("cyrep_entry: Passed-in non-sec firmware image info is invalid");	
	}

	/* 
	 * Don't leave anything behind and clear away the memory region of the
	 * passed in cyrep args.
	 */
	memset(cyrep_args->FwInfo, 0x00, sizeof(*cyrep_args->FwInfo));
	memset(cyrep_args, 0x00, sizeof(*cyrep_args));

	IMSG("CyReP: Non-sec firmware %s info: addr:0x%p size:%u", nsfw_info.FwName,
		nsfw_info.FwBase, nsfw_info.FwSize);
}

static bool cache_flash_range(paddr_t pa, void* va, size_t len)
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
 */
uint32_t cyrep_measure_fw(void)
{
	paddr_t nsfw_cyrep_args_pa;
	CyrepFwArgs *nsfw_cyrep_args;
	vaddr_t fw_base_va;
	CyrepFwInfo fw_info_va;

	/* [mlotfy] FIXME: See the issue description below
	vaddr_t fw_page_start_va;
	size_t fw_num_pages;
	*/

	IMSG("CyReP: Measuring non-sec firmware %s", nsfw_info.FwName);

	/*
	 * Place the non-sec firmware CyReP args before the base address of the firmware
	 * leaving 1K of space between the end of the args and the firmware base.
	 * OPTEE uses the first 4K of the SHMEM for some  bookkeeping.
	 */
	nsfw_cyrep_args_pa = CFG_SHMEM_START + 0x1000;
	nsfw_cyrep_args = phys_to_virt(nsfw_cyrep_args_pa, MEM_AREA_NSEC_SHM);
	if (nsfw_cyrep_args == NULL) {
		panic("cyrep_measure_fw: phys_to_virt failed");
	}

	/* Sanity check that VA and PA mapping is working 2-way as expected */
	if (virt_to_phys(nsfw_cyrep_args) != nsfw_cyrep_args_pa) {
		panic("cyrep_measure_fw: VA and PA mismatch");
	}

	memset(nsfw_cyrep_args, 0x00, sizeof(*nsfw_cyrep_args));

	/*
	 * The FwBase passed in is a physical memory address. We have to convert it
	 * to OPTEE virtual address before Cyrep_* functions can consume it.
	 */
	assert(nsfw_info.FwBase != NULL);
	assert(nsfw_info.FwSize != 0);
	if (!core_mmu_add_mapping(MEM_AREA_IO_NSEC, (paddr_t)nsfw_info.FwBase, nsfw_info.FwSize)) {
		panic("cyrep_measure_fw: core_mmu_add_mapping failed to map nsec fw physical base");
	}
	
	fw_base_va = (vaddr_t)phys_to_virt((paddr_t)nsfw_info.FwBase, MEM_AREA_IO_NSEC);
	if (!fw_base_va) {
		panic("cyrep_measure_fw: phys_to_virt failed");
	}

	memcpy(&fw_info_va, &nsfw_info, sizeof(fw_info_va));
	fw_info_va.FwBase = (void *)fw_base_va;

	if (!Cyrep_MeasureL2plusImage(&optee_measurement, &fw_info_va, "OPTEE",
		&nsfw_cyrep_args->FwMeasurement)) {
		panic("cyrep_measure_fw: Cyrep_MeasureL2plusImage() failed.");
	}

	/*
	 * Clear away our private key, we are done with it and no need to
	 * keep it around.
	 */
	memset(&optee_measurement.AliasKeyPriv, 0x00,
		sizeof(optee_measurement.AliasKeyPriv));

	/*
	 * Flush caches because the non-sec firmware we are handing over to will be
	 * accessing the CyReP args directly from physical memory and it will very
	 * likely have caches disabled or invalidated very early.
	 */
	if (!cache_flash_range(nsfw_cyrep_args_pa, nsfw_cyrep_args, sizeof(*nsfw_cyrep_args))) {
		panic("cyrep_measure_fw: failed to flush caches");
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

	return (uint32_t)(void *)nsfw_cyrep_args_pa;
}