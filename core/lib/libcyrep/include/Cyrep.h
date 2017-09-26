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
 * Encapsulates the CyReP specific arguments that gets passed from a firmware
 * to another in a multistage bootloader/firmware boot.
 *
 * FwMeasurement: The measurements of the firmware that this args is passed to.
 * FwInfo: An optional pointer to a firmware info that can be consumed by the
 *         next firmware. This can be used when a firmware Ln loads the next 2
 *         firmwares Ln+1 and Ln+2, in which Ln+1 uses FwInfo to locate and
 *         measure Ln+2 before transferring control to it.
 *         When this arg is not used, it should be be a null pointer.
 */
typedef struct {
    CyrepFwMeasurement  FwMeasurement;
    CyrepFwInfo         *FwInfo;
} CyrepFwArgs;

bool Cyrep_MeasureL1Image(const uint8_t *Cdi, size_t CdiSize,
                          const CyrepFwInfo *FwInfo, const char *CertIssuerCommon,
                          CyrepFwMeasurement *MeasurementResult);

bool Cyrep_MeasureL2plusImage(const CyrepFwMeasurement *CurrentMeasurement,
                              const CyrepFwInfo *FwInfo,
                              const char *CertIssuerCommon,
                              CyrepFwMeasurement *MeasurementResult);

#ifdef __cplusplus
}
#endif
#endif // __CYREP_H__