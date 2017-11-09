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
#ifndef __CYREP_COMMON_H__
#define __CYREP_COMMON_H__

#if defined(CYREP_DEBUG)
#define CYREP_INTERNAL_ERROR(...) \
    do { \
        CYREP_PLATFORM_TRACE_ERROR("Cyrep Internal Error: "__VA_ARGS__); \
    } while(0)
#else
#define CYREP_INTERNAL_ERROR(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CyrepDigest {
	uint8_t digest[RIOT_DIGEST_LENGTH];
} CyrepDigest;

typedef struct {
    RIOT_ECC_PUBLIC DeviceIDPub;
    uint32_t        PathLenRemaining;
    size_t          CertBufferSize;
    char            CertBuffer[1];
} CyrepCertChain;

typedef struct {
    RIOT_ECC_PUBLIC  PublicKey;
    RIOT_ECC_PRIVATE PrivateKey;
} CyrepKeyPair;

bool Cyrep_InitCertChain(
    uint32_t InitialPathLen,
    CyrepCertChain *CertChain,
    size_t CertChainSize
    );

bool Cyrep_AppendRootAndDeviceCerts(
    const uint8_t *Cdi,
    size_t CdiSize,
    const char *DeviceCertSubject,
    CyrepCertChain *CertChain,
    CyrepKeyPair *DeviceKeyPair
    );

bool Cyrep_MeasureImageAndAppendCert(
    const CyrepKeyPair *IssuerKeyPair,
    const void *SeedData,
    size_t SeedDataSize,
	const CyrepDigest *ImageDigest,
    const char *ImageSubjectName,
    const char *IssuerName,
    CyrepCertChain *CertChain,
    CyrepKeyPair *SubjectKeyPair
    );

/*
 * Copies private key from Source to Dest, and securely zeros out Source.
 */
void Cyrep_CapturePrivateKey(
    CyrepKeyPair *Dest,
    CyrepKeyPair *Source
    );

/*
 * Clear the memory in a secure way that avoids a possible compiler
 * optimization for eliminating memset calls for memory region that is
 * not going to be touched.
 *
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
#endif // __CYREP_COMMON_H__
