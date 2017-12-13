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

#define CYREP_CERT_CHAIN_MAGIC 0x50525943 /* 'PRYC' */
#define CYREP_CERT_CHAIN_VERSION 1
#define CYREP_CERT_INDEX_INVALID ((size_t)-1)

typedef struct {
    size_t          CertBufferStartIndex;
    size_t          IssuerIndex;
    char            Subject[64];
} CyrepCertIndexEntry;

typedef struct {
    uint32_t            Magic;
    uint32_t            Version;
    RIOT_ECC_PUBLIC     DeviceIDPub;
    uint32_t            PathLenRemaining;
    CyrepCertIndexEntry CertIndex[8];
    size_t              CertBufferSize;
    char                CertBuffer[1];
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

bool Cyrep_AppendPem(
    DERBuilderContext *DerContext,
    uint32_t Type,
    CyrepCertChain *CertChain
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
 * Returns true if the magic number and version match this version
 * of the Cyrep library, false otherwise.
 */
bool Cyrep_ValidateCertChainVersion(
    const CyrepCertChain *CertChain
    );

/*
 * Returns the index of the cert with the given subject name, or
 * CYREP_CERT_INDEX_INVALID if not found.
 */
size_t Cyrep_FindCertBySubjectName(
    const CyrepCertChain *CertChain,
    const char *SubjectName
    );

/*
 * Returns the length of the PEM certificate string of the given cert.
 */
size_t Cyrep_GetCertLength(
    const CyrepCertChain *CertChain,
    size_t CertIndex
    );

/*
 * Gets the total length of the PEM certificate strings of the given cert
 * and all it's issuers.
 */
size_t Cyrep_GetCertAndIssuersLength(
    const CyrepCertChain *CertChain,
    size_t CertIndex
    );

/*
 * Copy the PEM certificate strings of the given cert and all it's issuers
 * up to the root to the supplied buffer.
 */
bool Cyrep_GetCertAndIssuers(
    const CyrepCertChain *CertChain,
    size_t CertIndex,
    char *Buffer,
    size_t BufferSize
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
