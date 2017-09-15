
#include <arm.h>
#include <assert.h>
#include <compiler.h>
#include <kernel/panic.h>
#include <inttypes.h>
#include <stdio.h>
#include <trace.h>
#include <RiotDerEnc.h>
#include <RiotX509Bldr.h>
#include <Cyrep.h>
#include <kernel/cyrep_entry.h>
#include <string.h>

CyrepFwArgs optee_cyrep_args;

/*
 * Copies the CyReP args locally for use later.
 * There is no tolerance for error in this function, any error will result in
 * a panic.
 */
void cyrep_entry(uint32_t cyrep_args_addr)
{
	CyrepFwArgs *cyrep_args;
	CyrepFwMeasurement *measurement;
	CyrepFwInfo *nsfw_info;

	IMSG("CyReP: Captured args cyrep_args_addr=0x%08x", cyrep_args_addr);

	cyrep_args = (CyrepFwArgs *)(void *)cyrep_args_addr;
	if (cyrep_args == NULL) {
		panic("cyrep_entry: cyrep_args_addr is invalid");
	}

	memcpy(&optee_cyrep_args, cyrep_args, sizeof(optee_cyrep_args));

	/* 
	 * Don't leave anything behind and clear away the memory region of the
	 * passed in cyrep args
	 */
	memset(cyrep_args, 0x00, sizeof(*cyrep_args));

	measurement = &optee_cyrep_args.FwMeasurement;
	if (measurement->CertChainCount == 0) {
		panic("cyrep_entry: OPTEE measurement certificate chain is empty");
	}

	IMSG("CyReP: OPTEE measurement certificate:");
	IMSG("%s", measurement->CertChain[measurement->CertChainCount - 1]);

	nsfw_info = &optee_cyrep_args.FwInfo;

	if (nsfw_info->FwBase == NULL || nsfw_info->FwSize == 0) {
		panic("cyrep_entry: Passed-in non-sec firmware image info is invalid");	
	}
	
	IMSG("CyReP: Non-sec firmware %s info: addr:0x%p size:%u", nsfw_info->FwName,
		nsfw_info->FwBase, nsfw_info->FwSize);
}

/*
 * Measures the non-secure firmware which OPTEE will jump to and returns
 * address to the cyrep args to be passed along with the bootargs
 */
uint32_t cyrep_measure_nsfw(void)
{
	CyrepFwMeasurement *optee_measurement;
	CyrepFwMeasurement *nsfw_measurement;
	CyrepFwInfo *nsfw_info;
	CyrepFwArgs *nsfw_cyrep_args;

	optee_measurement = &optee_cyrep_args.FwMeasurement;
	nsfw_info = &optee_cyrep_args.FwInfo;

	IMSG("CyReP: Measuring non-sec firmware %s", nsfw_info->FwName);

	/*
	 * Place the non-sec firmware CyReP args before the base address of the firmware
	 * leaving 1K of space between the end of the args and the firmware base.
	 */
	nsfw_cyrep_args = (CyrepFwArgs *)(void *)((uintptr_t)nsfw_info->FwBase - 
		sizeof(*nsfw_cyrep_args) - 1024);

	if ((uintptr_t)nsfw_cyrep_args > (uintptr_t)nsfw_info->FwBase) {
		panic("cyrep_measure_nsfw: nsfw_cyrep_args address calculation overflow");
	}

	memset(nsfw_cyrep_args, 0x00, sizeof(*nsfw_cyrep_args));

	nsfw_measurement = &nsfw_cyrep_args->FwMeasurement;
	/* Cyrep_MeasureL2plusImage(optee_measurement, nsfw_info->FwBase, nsfw_info->FwSize,
		"OPTEE", nsfw_info->FwName, nsfw_measurement);*/

	/*
	 * Clear away our private key, we are done with it and no need to
	 * keep it around.
	 */
	memset(&optee_measurement->AliasKeyPriv, 0x00,
		sizeof(optee_measurement->AliasKeyPriv));

	IMSG("CyReP: %s measurement certificate:", nsfw_info->FwName);
	IMSG("%s", nsfw_measurement->CertChain[nsfw_measurement->CertChainCount - 1]);

	return (uint32_t)(void *)nsfw_cyrep_args;
}