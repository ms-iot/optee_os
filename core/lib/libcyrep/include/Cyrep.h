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
#ifndef __CYREP_H__
#define __CYREP_H__

#include "CyrepCommon.h"
#include "RiotDerEnc.h"
#include "RiotX509Bldr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of PEM certificates chain a measurement can have. */
#define CYREP_CERT_CHAIN_MAX    4

/*
 * Encapsulates CyReP measurements for a certain firmware.
 *
 * DeviceIDPub: Public key of the device identity.
 * AliasKeyPub:  Public key that identifies the firmware.
 * AliasKeyPriv: Private key that identifies the firmware.
 * CertChainCount: Number of certificates in the CertChain array.
 * CertChain: An array of PEM certificates whose number of valid entries is
 *            identified by CertChainCount.
 */
typedef struct {
    RIOT_ECC_PUBLIC     DeviceIDPub;
    RIOT_ECC_PUBLIC     AliasKeyPub;
    RIOT_ECC_PRIVATE    AliasKeyPriv;
    size_t              CertChainCount;
    char                CertChain[CYREP_CERT_CHAIN_MAX][DER_MAX_PEM];
} CyrepFwMeasurement;

/* Firmware name size including the null character. */
#define CYREP_FW_NAME_MAX   8

/*
 * Encapsualtes firmware info used by CyReP during measurements.
 *
 * FwName Firmware name identifier.
 * FwBase Firmware physical base address.
 * FwSize: Firmware image size in bytes.
 */
typedef struct {
    char    FwName[CYREP_FW_NAME_MAX];
    void    *FwBase;
    size_t  FwSize;
} CyrepFwInfo;

/*
 * Define 2 magic numbers that should be placed before and after the measurement
 * structure inside the Cyrep args binary blob to help in catching overflows
 * by the measurement routines.
 */
#define CYREP_ARGS_PREFIX_GUARD    0xBBEEEEFF
#define CYREP_ARGS_POSTFIX_GUARD   0xFFEEEEBB

/*
 * Encapsulates the CyReP specific arguments that gets passed from a firmware
 * to another in a multistage bootloader/firmware boot.
 *
 * PrefixGuard: A magic number placed by Cyrep library at the beginning of the
 *              args binary blob to catch overflows.
 * FwMeasurement: The measurements of the firmware that this args is passed to.
 * PostfixGuard: A magic number placed by Cyrep library directly after the
 *             measurements structure blob to catch measurements overflows.
 * FwMeasurementHash: A SHA digest for the measurements used to verify
 *                    measurement integrity.
 * FwInfo: An optional pointer to a firmware info that can be consumed by the
 *         next firmware. This can be used when a firmware Ln loads the next 2
 *         firmwares Ln+1 and Ln+2, in which Ln+1 uses FwInfo to locate and
 *         measure Ln+2 before transferring control to it.
 *         When this arg is not used, it should be be a null pointer.
 * FwInfoHash: A SHA digest for the firmware info used to verify info integrity.
 */
typedef struct {
    uint32_t            PrefixGuard;
    CyrepFwMeasurement  FwMeasurement;
    uint32_t            PostfixGuard;
    uint8_t             FwMeasurementHash[RIOT_DIGEST_LENGTH];
    CyrepFwInfo         *FwInfo;
    uint8_t             FwInfoHash[RIOT_DIGEST_LENGTH];
} CyrepFwArgs;

/*
 * Initializes a Cyrep fw args.
 *
 * Args: Pointer to the Cyrep args to initialize.
 */
void Cyrep_InitArgs(CyrepFwArgs *Args);

/*
 * Do necessary post-process on the Cyrep fw args such as hashing the measurement
 * to use later for data integrity verification.
 * This function should be called on the Cyrep fw args after both the measurement
 * and the firmware info have been written.
 *
 * Args: Pointer to the Cyrep args to do post-processing on.
 */
bool Cyrep_PostprocessArgs(CyrepFwArgs *Args);

/*
 * Verifies a Cyrep fw args integrity by making sure that the magic numbers are
 * present as expected.
 *
 * Args: Pointer to the Cyrep args to verify.
 */
bool Cyrep_VerifyArgs(const CyrepFwArgs *Args);

/*
 * Measures an L1 firmware using a device secret.
 *
 * Cdi: Pointer to a device secret buffer.
 * CdiSize: Device secret buffer size in bytes.
 * FwInfo: Pointer to the L1 firmware info.
 * CertIssuerCommon: Name of the entity issuing the measurement certificate.
 * CyrepFwMeasurement: Pointer to a Cyrep measurement to which L1 firmware
 *                     measurement result is written to.
 */
bool Cyrep_MeasureL1Firmware(const uint8_t *Cdi, size_t CdiSize,
                             const CyrepFwInfo *FwInfo,
                             const char *CertIssuerCommon,
                             CyrepFwMeasurement *MeasurementResult);

/*
 * Measures an L2 or beyond firmware using a the previous firmware L(n-1)
 * measurements to the current firmware L(n).
 *
 * CurrentMeasurement: Pointer to the current firmware measurements.
 * FwInfo: Pointer to the L2+ firmware info.
 * CertIssuerCommon: Name of the entity issuing the measurement certificate.
 * CyrepFwMeasurement: Pointer to a Cyrep measurement to which L2+ firmware
 *                     measurement result is written to.
 */
bool Cyrep_MeasureL2plusFirmware(const CyrepFwMeasurement *CurrentMeasurement,
                                 const CyrepFwInfo *FwInfo,
                                 const char *CertIssuerCommon,
                                 CyrepFwMeasurement *MeasurementResult);

/*
 * Clear the memory in a secure way that avoids a possible compiler optimization
 * for elemenating memset calls for memory region that is not going to be touched.
 * NOTE: This function doesn't necessarly zero the memory region, but it clears
 * it by writing random values to it.
 *
 * Mem: Base address of the memory region to clear.
 * Size: Memory region size in bytes.
 */
void Cyrep_SecureClearMemory(void *Mem, size_t Size);

#ifdef __cplusplus
}
#endif
#endif // __CYREP_H__