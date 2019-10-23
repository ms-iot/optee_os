/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) Microsoft Corporation.
 */

#ifndef __TEE_ATTESTATION_H
#define __TEE_ATTESTATION_H

#include <utee_defines.h>

/* 
 * Attestation:
 * Attestation of user TAs in OP-TEE is done via code measurement at load
 * time. If CFG_ATTESTATION_MEASURE is enabled the System PTA will request
 * a hash from the user TA store. Each time a new binary is loaded (ie 
 * shared libraries) for a given TA context the existing measurement is 
 * combined with the incoming measurement to form a new hash. The
 * measurement is dependent on the order in which binaries are loaded. A 
 * copy of the running measurement is frozen before the first session to
 * the TA is opened.
 * 
 * This static measurement is included in a x509 certificate signed by
 * OP-TEE, along with other information. The measurement can also be used
 * to create binary locked encrypted objects and derive additional keys.
 *
 * Run Time Binary Loading:
 * The system PTA offers the option to load binaries after a TA has started
 * running (dlopen etc). This poses an interesting challenge for attestation
 * since the original measurement will no longer be valid. While the frozen
 * measurement is always available, a dynamic measurement is also maintained
 * which includes all dynamically loaded components.
 * 
 * TODO: Flesh out this description
 * 
 * Issues:
 * 
 * - Inconsistent Measurements
 * Secstor and Ree FS TAs are not measured the same as Early TAs. Early TAs do
 * not include any headers when they are encoded into the OP-TEE binary and
 * are a simple hash over the (default compressed) .elf being loaded. The
 * other user TA measurements are based on the existing verification flow and
 * include parts of the TA headers.
 *
 * - Measurements too early
 * Ideally measurements should be taked during mapping of the TAs rather than
 * during initial binary loading. The much simpler measurement is being used
 * for version 1.
 * 
 */
#ifdef CFG_ATTESTATION_MEASURE

#define ATTESTATION_MEASUREMENT_SIZE TEE_SHA256_HASH_SIZE
#define ATTESTATION_MEASUREMENT_ALGO TEE_ALG_SHA256
/*
 * struct tee_attestation_data - user TA attestation measurements
 * @dynamic_measurement	A running hash of all binary components loaded,
 *			including runtime dynamic libraries
 * @static_measurement	A measurement of the TA as it existed when execution
 *			started (first session start)
 * @is_measured		True if the static measurement has been set
 * @ta_version		The TA version as defined in the shdr_bootstrap_ta
 *			bootstrap header
 * @signer_measurement	Hash of the signing key used to verify the TA
 */
struct tee_attestation_data {
	bool is_measured;
	uint8_t dynamic_measurement[ATTESTATION_MEASUREMENT_SIZE];
	uint8_t static_measurement[ATTESTATION_MEASUREMENT_SIZE];
	uint32_t ta_version;
	uint8_t signer_measurement[ATTESTATION_MEASUREMENT_SIZE];
};

#endif /* CFG_ATTESTATION_MEASURE */

#endif /* __TEE_ATTESTATION_H */