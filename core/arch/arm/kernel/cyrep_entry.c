
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

CyrepFwArgs optee_args __section(".data");
CyrepFwInfo nsfw_info __section(".data");

#define CYREP_CRITICAL_ERROR(...) \
	do { \
		EMSG(__VA_ARGS__); \
		panic("Cyrep critical error detected"); \
	} while(0)

/*
 * Copies the CyReP args locally for use later.
 * There is no tolerance for error in this function, any error will result in
 * a panic.
 */
void cyrep_entry(paddr_t cyrep_args_addr)
{
	CyrepFwArgs *cyrep_args;

	cyrep_args = (CyrepFwArgs *)(void *)cyrep_args_addr;
	assert(cyrep_args != NULL);
	assert(cyrep_args->FwInfo != NULL);
	IMSG("CyReP: Captured args cyrep_args_addr:0x%p fw_info:0x%p",
		(void *)cyrep_args, (void *)cyrep_args->FwInfo);

	memcpy(&optee_args, cyrep_args, sizeof(optee_args));
	optee_args.FwInfo = &nsfw_info;
	memcpy(&nsfw_info, cyrep_args->FwInfo, sizeof(nsfw_info));

	IMSG("CyReP: Non-sec firmware %s info: addr:0x%p size:%u", nsfw_info.FwName,
		nsfw_info.FwBase, (uint32_t)nsfw_info.FwSize);

	/* 
	 * Don't leave anything behind and clear away the memory region of the
	 * passed in cyrep args.
	 */
	Cyrep_SecureClearMemory(cyrep_args->FwInfo, sizeof(*cyrep_args->FwInfo));
	Cyrep_SecureClearMemory(cyrep_args, sizeof(*cyrep_args));
}

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
 */
paddr_t cyrep_measure_nsfw(void)
{
	paddr_t nsfw_cyrep_args_pa;
	CyrepFwArgs *nsfw_cyrep_args;
	void *fw_base;
	CyrepFwMeasurement *optee_measurement;
	CyrepFwInfo fw_info;

	/* [mlotfy] FIXME: See the issue description below
	vaddr_t fw_page_start_va;
	size_t fw_num_pages;
	*/

	IMSG("CyReP: Measuring Non-sec firmware %s info: addr:0x%p size:%u",
		nsfw_info.FwName, nsfw_info.FwBase, (uint32_t)nsfw_info.FwSize);

	if (!Cyrep_VerifyArgs(&optee_args)) {
		CYREP_CRITICAL_ERROR("Cyrep_VerifyArgs() failed");
	}

	optee_measurement = &optee_args.FwMeasurement;

	if (optee_measurement->CertChainCount == 0) {
		CYREP_CRITICAL_ERROR("OPTEE measurement certificate chain is empty");
	}

	if (nsfw_info.FwBase == NULL || nsfw_info.FwSize == 0) {
		EMSG("Passed-in non-sec firmware image info is invalid");
		CYREP_CRITICAL_ERROR("Passed-in non-sec firmware image info is invalid");
	}

	/*
	 * The FwBase passed in is a physical memory address. We have to convert it
	 * to OPTEE virtual address before Cyrep_* functions can consume it.
	 */

	if (!core_mmu_add_mapping(MEM_AREA_IO_NSEC, (paddr_t)nsfw_info.FwBase, nsfw_info.FwSize)) {
		CYREP_CRITICAL_ERROR("core_mmu_add_mapping failed to map nsec fw physical base");
	}
	
	fw_base = (void *)phys_to_virt((paddr_t)nsfw_info.FwBase, MEM_AREA_IO_NSEC);
	if (!fw_base) {
		CYREP_CRITICAL_ERROR("phys_to_virt(nsfw_info.FwBase) failed");
	}

	memcpy(&fw_info, &nsfw_info, sizeof(fw_info));
	fw_info.FwBase = fw_base;

	/*
	 * Place the non-sec firmware CyReP args before the base address of the firmware
	 * leaving 1K of space between the end of the args and the firmware base.
	 * OPTEE uses the first 4K of the SHMEM for some  bookkeeping.
	 */
	nsfw_cyrep_args_pa = CFG_SHMEM_START + 0x1000;
	nsfw_cyrep_args = phys_to_virt(nsfw_cyrep_args_pa, MEM_AREA_NSEC_SHM);
	if (nsfw_cyrep_args == NULL) {
		CYREP_CRITICAL_ERROR("phys_to_virt(nsfw_cyrep_args_pa) failed");
	}

#ifdef DEBUG
	/* Sanity check that VA and PA mapping is working 2-way as expected */
	if (virt_to_phys(nsfw_cyrep_args) != nsfw_cyrep_args_pa) {
		CYREP_CRITICAL_ERROR("VA and PA mismatch");
	}
#endif

	Cyrep_InitArgs(nsfw_cyrep_args);

	if (!Cyrep_MeasureL2plusFirmware(optee_measurement, &fw_info, "OPTEE",
		&nsfw_cyrep_args->FwMeasurement)) {
		CYREP_CRITICAL_ERROR("Cyrep_MeasureL2plusImage() failed");
	}

	if (!Cyrep_PostprocessArgs(nsfw_cyrep_args)) {
		CYREP_CRITICAL_ERROR("Cyrep_PostprocessArgs(nsfw_cyrep_args) failed");
	}

	/*
	 * Clear away our private key, we are done with it and no need to
	 * keep it around.
	 */
	Cyrep_SecureClearMemory(&optee_measurement->AliasKeyPriv,
		sizeof(optee_measurement->AliasKeyPriv));

	/*
	 * Flush caches because the non-sec firmware we are handing over to will be
	 * accessing the CyReP args directly from physical memory and it will very
	 * likely have caches disabled or invalidated very early.
	 */
	if (!cache_flush_range(nsfw_cyrep_args_pa, nsfw_cyrep_args, sizeof(*nsfw_cyrep_args))) {
		CYREP_CRITICAL_ERROR("cache_flush_range() failed");
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

	return nsfw_cyrep_args_pa;
}