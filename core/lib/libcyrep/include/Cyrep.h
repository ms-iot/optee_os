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

#include "CyrepDef.h"
#include "CyrepCommon.h"
#include "RiotDerEnc.h"
#include "RiotX509Bldr.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Encapsulates CyReP measurements for a certain firmware.
 *
 * PrefixGuard: A magic number used for data integrity verification.
 * DeviceIDPub: Public key of the device identity.
 * AliasKeyPub:  Public key that identifies the firmware.
 * AliasKeyPriv: Private key that identifies the firmware.
 * CertChainCount: Number of certificates in the CertChain array.
 * CertChain: An array of PEM certificates whose number of valid entries is
 *            identified by CertChainCount.
 * PostfixGuard: A magic number used for data integrity verification.
 */
typedef struct {
    uint32_t            PrefixGuard;
    RIOT_ECC_PUBLIC     DeviceIDPub;
    RIOT_ECC_PUBLIC     AliasKeyPub;
    RIOT_ECC_PRIVATE    AliasKeyPriv;
    size_t              CertChainCount;
    char                CertChain[CYREP_CERT_CHAIN_MAX][DER_MAX_PEM];
    uint32_t            PostfixGuard;
} CyrepFwMeasurement;

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
 * Encapsulates the CyReP specific arguments that gets passed from a firmware
 * to another in a multistage bootloader/firmware boot.
 *
 * FwMeasurement: The measurements of the firmware that this args is passed to.
 * FwInfo: An optional firmware info that can be consumed by the
 *         next firmware. This can be used when a firmware Ln loads the next 2
 *         firmwares Ln+1 and Ln+2, in which Ln+1 uses FwInfo to locate and
 *         measure Ln+2 before transferring control to it.
 *         When this field is not used it should be all zeros.
 * Digest: A hash digest for both the measurement and the firmware info which is
 *         used for data integrity verification.
 *
 * The CyrepFwArgs should be treated as an obaque struct, accessing its fields
 * should be  done through Cyrep_ArgsGet() accessors. This decouples the
 * external consuming implementation from the struct layout.
 */
typedef struct {
    CyrepFwMeasurement  FwMeasurement;
    CyrepFwInfo         FwInfo;
    uint8_t             Digest[RIOT_DIGEST_LENGTH];
} CyrepFwArgs;

/* Accessor to get a Cyrep args measurement */
static inline CyrepFwMeasurement* Cyrep_ArgsGetFwMeasurement(CyrepFwArgs *Args) {
    assert(Args != NULL);
    return &Args->FwMeasurement;
}

/* Accessor to get a Cyrep args firmware info */
static inline CyrepFwInfo* Cyrep_ArgsGetFwInfo(CyrepFwArgs *Args) {
    assert(Args != NULL);
    return &Args->FwInfo;
}

/*
 * Initializes a Cyrep fw args to a good initial state.
 *
 * Args: Pointer to the Cyrep args to initialize.
 */
void Cyrep_ArgsInit(CyrepFwArgs *Args);

/*
 * Do necessary post-process on the Cyrep fw args.
 *
 * This function should be called on the Cyrep fw args after both the measurement
 * and the firmware info have been written.
 *
 * Args: Pointer to the Cyrep args to do post-processing on.
 */
bool Cyrep_ArgsPostprocess(CyrepFwArgs *Args);

/*
 * Verifies a Cyrep fw args integrity by performing necessary data integrity
 * checks.
 *
 * Args: Pointer to the Cyrep args to verify.
 */
bool Cyrep_ArgsVerify(const CyrepFwArgs *Args);

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