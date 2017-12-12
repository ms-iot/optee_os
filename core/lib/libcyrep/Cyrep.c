//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
#include <RiotTarget.h>
#include <RiotStatus.h>
#include <RiotSha256.h>
#include <RiotHmac.h>
#include <RiotKdf.h>
#include <RiotAes128.h>
#include <RiotKdf.h>
#include <RiotEcc.h>
#include <RiotDerEnc.h>
#include <RiotX509Bldr.h>
#include <RiotCrypt.h>
#include <CyrepCommon.h>

#ifdef CONFIG_CYREP_OPTEE_BUILD
#include <string_ext.h>
#include <util.h>
#endif

//
// Byte value used to set memory to when clearing a memory region.
//
#define CYREP_CLEAR_MEM_VALUE 0x5A

//
// Macro to get the length of a string literal instead of calling strlen
//
#define STR_LITERAL_LEN(S) (sizeof(S) - 1)

#define END_CERT_MARKER "-----END CERTIFICATE-----\n"

// The static data fields that make up the Alias Cert "to be signed" region
static const RIOT_X509_TBS_DATA x509AliasTBSData = { { 0 },
                                       "RIoT Device", "MSR_TEST", "US",
                                       "170101000000Z", "370101000000Z",
                                       "RIoT Fw", "MSR_TEST", "US" };

// The static data fields that make up the DeviceID Cert "to be signed" region
static const RIOT_X509_TBS_DATA x509DeviceTBSData = { { 0 },
                                       "Manufacturer", "MSR_TEST", "US",
                                       "170101000000Z", "370101000000Z",
                                       "RIoT Device", "MSR_TEST", "US" };

// The static data fields that make up the "root signer" Cert
static const RIOT_X509_TBS_DATA x509RootTBSData   = { { 0 },
                                       "Manufacturer", "MSR_TEST", "US",
                                       "170101000000Z", "370101000000Z",
                                       "Manufacturer", "MSR_TEST", "US" };

// The "root" signing key. This is intended for development purposes only.
// This key is used to sign the DeviceID certificate, the certificiate for
// this "root" key represents the "trusted" CA for the developer-mode DPS
// server(s). Again, this is for development purposes only and (obviously)
// provides no meaningful security whatsoever.
static const uint8_t TestEccKeyPub[sizeof(ecc_publickey)] = {
    0xeb, 0x9c, 0xfc, 0xc8, 0x49, 0x94, 0xd3, 0x50, 0xa7, 0x1f, 0x9d, 0xc5,
    0x09, 0x3d, 0xd2, 0xfe, 0xb9, 0x48, 0x97, 0xf4, 0x95, 0xa5, 0x5d, 0xec,
    0xc9, 0x0f, 0x52, 0xa1, 0x26, 0x5a, 0xab, 0x69, 0x00, 0x00, 0x00, 0x00,
    0x7d, 0xce, 0xb1, 0x62, 0x39, 0xf8, 0x3c, 0xd5, 0x9a, 0xad, 0x9e, 0x05,
    0xb1, 0x4f, 0x70, 0xa2, 0xfa, 0xd4, 0xfb, 0x04, 0xe5, 0x37, 0xd2, 0x63,
    0x9a, 0x46, 0x9e, 0xfd, 0xb0, 0x5b, 0x1e, 0xdf, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00 };

static const uint8_t TestEccKeyPriv[sizeof(ecc_privatekey)] = {
    0xe3, 0xe7, 0xc7, 0x13, 0x57, 0x3f, 0xd9, 0xc8, 0xb8, 0xe1, 0xea, 0xf4,
    0x53, 0xf1, 0x56, 0x15, 0x02, 0xf0, 0x71, 0xc0, 0x53, 0x49, 0xc8, 0xda,
    0xe6, 0x26, 0xa9, 0x0b, 0x17, 0x88, 0xe5, 0x70, 0x00, 0x00, 0x00, 0x00 };

static bool Cyrep_AppendPemGetIndexes(
    DERBuilderContext *DerContext,
    uint32_t Type,
    CyrepCertChain *CertChain,
    size_t *StartIndex,
    size_t *Length
    )
{
    size_t certChainLen;
    size_t certBufferRemaining;

    // Append certificate to certificate chain
    certChainLen =
        strnlen(CertChain->CertBuffer, CertChain->CertBufferSize - 1);

    certBufferRemaining = CertChain->CertBufferSize - certChainLen - 1;
    if (DERtoPEM(
            DerContext,
            Type,
            &CertChain->CertBuffer[certChainLen],
            &certBufferRemaining) != 0) {

        CYREP_INTERNAL_ERROR("DERtoPEM() failed\n");
        return false;
    }

    CertChain->CertBuffer[certChainLen + certBufferRemaining] = '\0';

    *StartIndex = certChainLen;
    *Length = certBufferRemaining;
    return true;
}

bool Cyrep_AppendPem(
    DERBuilderContext *DerContext,
    uint32_t Type,
    CyrepCertChain *CertChain)
{
    size_t dummy1, dummy2;
    return Cyrep_AppendPemGetIndexes(
        DerContext,
        Type,
        CertChain,
        &dummy1,
        &dummy2);
}

/*
 * Return the index of the next available slot in the certificate index,
 * or CYREP_CERT_INDEX_INVALID if there are no available slots.
 */
static size_t Cyrep_FindNextFreeIndex(const CyrepCertChain *CertChain)
{
    size_t index;
    for (index = 0; index < ARRAY_SIZE(CertChain->CertIndex); ++index) {
        if (CertChain->CertIndex[index].CertBufferStartIndex ==
            CYREP_CERT_INDEX_INVALID) {

            return index;
        }
    }

    return CYREP_CERT_INDEX_INVALID;
}

/*
 * Append a certificate to the certificate chain. Updates the index and
 * appends the PEM to the certificate buffer.
 */
static bool Cyrep_AppendCert(
    CyrepCertChain *CertChain,
    DERBuilderContext *DerContext,
    const char *IssuerName,
    const char *SubjectName
    )
{
    size_t index;
    size_t length;
    CyrepCertIndexEntry *entry;

    index = Cyrep_FindNextFreeIndex(CertChain);
    if (index == CYREP_CERT_INDEX_INVALID) {
        CYREP_INTERNAL_ERROR("Failed to find free cert index\n");
        return false;
    }

    entry = &CertChain->CertIndex[index];
    strlcpy(entry->Subject, SubjectName, ARRAY_SIZE(entry->Subject));

    if (!Cyrep_AppendPemGetIndexes(
            DerContext,
            CERT_TYPE,
            CertChain,
            &entry->CertBufferStartIndex,
            &length)) {

        return false;
    }

    entry->IssuerIndex =
        Cyrep_FindCertBySubjectName(CertChain, IssuerName);

    return true;
}

static bool Cyrep_DeriveCertSerialNum(
    const uint8_t *Source,
    size_t SourceSize,
    RIOT_X509_TBS_DATA *TbsData
    )
{
    uint8_t digest[RIOT_DIGEST_LENGTH];

    // Generate serial number
    if (RiotCrypt_Kdf(
            digest,
            sizeof(digest),
            Source,
            SourceSize,
            NULL,
            0,
            (const uint8_t *)RIOT_LABEL_SERIAL,
            STR_LITERAL_LEN(RIOT_LABEL_SERIAL),
            sizeof(digest)) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Kdf() failed\n");
        return false;
    }

    // Ensure that serial number is positive and does not have leading zeros
    digest[0] &= 0x7F;
    digest[0] |= 0x01;

    memcpy(TbsData->SerialNum, digest, sizeof(TbsData->SerialNum));

    return true;
}

static bool Cyrep_GenerateRootCert(
    const char *CertIssuer,
    const char *CertSubject,
    CyrepCertChain *CertChain
    )
{
    DERBuilderContext derCtx;
    RIOT_ECC_SIGNATURE tbsSig;
    RIOT_X509_TBS_DATA tbsData;
    RIOT_ECC_PUBLIC testPublicKey;
    RIOT_ECC_PRIVATE testPrivateKey;
    uint8_t derBuffer[DER_MAX_TBS];
    bool ret = false;

    memcpy(&tbsData, &x509RootTBSData, sizeof(tbsData));
    memcpy(&testPublicKey, TestEccKeyPub, sizeof(testPublicKey));
    memcpy(&testPrivateKey, TestEccKeyPriv, sizeof(testPrivateKey));

    if (!Cyrep_DeriveCertSerialNum(
            TestEccKeyPub,
            sizeof(TestEccKeyPub),
            &tbsData)) {

        CYREP_INTERNAL_ERROR("Failed to derive serial number\n");
        goto Exit;
    }

    DERInitContext(&derCtx, derBuffer, DER_MAX_TBS);
    tbsData.IssuerCommon = CertIssuer;
    tbsData.SubjectCommon = CertSubject;

    if (X509GetRootCertTBS(
            &derCtx,
            &tbsData,
            &testPublicKey,
            CertChain->PathLenRemaining) != 0) {

        CYREP_INTERNAL_ERROR("X509GetRootCertTBS() failed\n");
        goto Exit;
    }

    CertChain->PathLenRemaining -= 1;

    // Sign the certificate's TBS region
    if (RiotCrypt_Sign(
            &tbsSig,
            derCtx.Buffer,
            derCtx.Position,
            &testPrivateKey) != 0) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Sign() failed\n");
        goto Exit;
    }

    if (X509MakeRootCert(&derCtx, &tbsSig) != 0) {
        CYREP_INTERNAL_ERROR("X509MakeRootCert() failed\n");
        goto Exit;
    }

    if (!Cyrep_AppendCert(CertChain, &derCtx, CertIssuer, CertSubject)) {
        CYREP_INTERNAL_ERROR("Failed to append cert\n");
        goto Exit;
    }

    ret = true;

Exit:

    return ret;
}

static bool Cyrep_GenerateDeviceCert(
    const char *CertIssuer,
    const char *CertSubject,
    CyrepCertChain *CertChain
    )
{
    DERBuilderContext derCtx;
    RIOT_ECC_SIGNATURE tbsSig;
    RIOT_X509_TBS_DATA tbsData;
    RIOT_ECC_PUBLIC testPublicKey;
    RIOT_ECC_PRIVATE testPrivateKey;
    uint8_t derBuffer[DER_MAX_TBS];
    bool ret = false;

    memcpy(&tbsData, &x509DeviceTBSData, sizeof(tbsData));
    memcpy(&testPublicKey, TestEccKeyPub, sizeof(testPublicKey));
    memcpy(&testPrivateKey, TestEccKeyPriv, sizeof(testPrivateKey));

    if (!Cyrep_DeriveCertSerialNum(
            (const uint8_t *)&CertChain->DeviceIDPub,
            sizeof(RIOT_ECC_PUBLIC),
            &tbsData)) {

        CYREP_INTERNAL_ERROR("Failed to derive cert serial number\n");
        goto Exit;
    }

    DERInitContext(&derCtx, derBuffer, DER_MAX_TBS);
    tbsData.IssuerCommon = CertIssuer;
    tbsData.SubjectCommon = CertSubject;

    if (X509GetDeviceCertTBS(
            &derCtx,
            &tbsData,
            &CertChain->DeviceIDPub,
            &testPublicKey,
            CertChain->PathLenRemaining) != 0) {

        CYREP_INTERNAL_ERROR("X509GetDeviceCertTBS() failed\n");
        goto Exit;
    }

    CertChain->PathLenRemaining -= 1;

    // Sign the certificate's TBS region
    if (RiotCrypt_Sign(
            &tbsSig,
            derCtx.Buffer,
            derCtx.Position,
            &testPrivateKey) != 0) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Sign() failed\n");
        goto Exit;
    }

    if (X509MakeDeviceCert(&derCtx, &tbsSig) != 0) {
        CYREP_INTERNAL_ERROR("X509MakeDeviceCert() failed\n");
        goto Exit;
    }

    if (!Cyrep_AppendCert(CertChain, &derCtx, CertIssuer, CertSubject)) {
        CYREP_INTERNAL_ERROR("Failed to append cert\n");
        goto Exit;
    }

    ret = true;

Exit:

    return ret;
}

bool Cyrep_InitCertChain(
    uint32_t InitialPathLen,
    CyrepCertChain *CertChain,
    size_t CertChainSize
    )
{
    size_t i;
    CertChain->Magic = CYREP_CERT_CHAIN_MAGIC;
    CertChain->Version = CYREP_CERT_CHAIN_VERSION;
    CertChain->PathLenRemaining = InitialPathLen;

    for (i = 0; i < ARRAY_SIZE(CertChain->CertIndex); ++i) {
        CertChain->CertIndex[i].CertBufferStartIndex =
            CYREP_CERT_INDEX_INVALID;

        CertChain->CertIndex[i].IssuerIndex = CYREP_CERT_INDEX_INVALID;
        CertChain->CertIndex[i].Subject[0] = '\0';
    }

    CertChain->CertBufferSize =
        CertChainSize - offsetof(CyrepCertChain, CertBuffer);

    CertChain->CertBuffer[0] = '\0';

    return true;
}

bool Cyrep_AppendRootAndDeviceCerts(
    const uint8_t *Cdi,
    size_t CdiSize,
    const char *DeviceCertSubject,
    CyrepCertChain *CertChain,
    CyrepKeyPair *DeviceKeyPair
    )
{
    bool ret = false;
    uint8_t digest[RIOT_DIGEST_LENGTH];

    // Don't use CDI directly.
    if (RiotCrypt_Hash(digest, sizeof(digest), Cdi, CdiSize) !=
        RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Hash() failed\n");
        goto Exit;
    }

    // Derive DeviceID key pair from CDI.
    if (RiotCrypt_DeriveEccKey(
            &DeviceKeyPair->PublicKey,
            &DeviceKeyPair->PrivateKey,
            digest,
            sizeof(digest),
            (const uint8_t *)RIOT_LABEL_IDENTITY,
            STR_LITERAL_LEN(RIOT_LABEL_IDENTITY)) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_DeriveEccKey() failed\n");
        goto Exit;
    }

    // Copy the device public key into the cert chain
    memcpy(
        &CertChain->DeviceIDPub,
        &DeviceKeyPair->PublicKey,
        sizeof(RIOT_ECC_PUBLIC));

    if (!Cyrep_GenerateRootCert(
            "Contoso Ltd.",
            "Contoso Ltd.",
            CertChain)) {

        CYREP_INTERNAL_ERROR("Failed to generate root cert\n");
        goto Exit;
    }

    // Generate the device cert
    if (!Cyrep_GenerateDeviceCert(
            "Contoso Ltd.",
            DeviceCertSubject,
            CertChain)) {

        CYREP_INTERNAL_ERROR("Failed to generate device cert\n");
        goto Exit;
    }

    ret = true;

Exit:
    Cyrep_SecureClearMemory(digest, sizeof(digest));

    return ret;

}

bool Cyrep_MeasureImageAndAppendCert(
    const CyrepKeyPair *IssuerKeyPair,
    const void *SeedData,
    size_t SeedDataSize,
    const CyrepDigest *ImageDigest,
    const char *ImageSubjectName,
    const char *IssuerName,
    CyrepCertChain *CertChain,
    CyrepKeyPair *SubjectKeyPair
    )
{
    RIOT_ECC_SIGNATURE tbsSig;
    DERBuilderContext cerCtx;
    RIOT_X509_TBS_DATA x509TBSData;
    uint8_t digest[RIOT_DIGEST_LENGTH];
    uint8_t cerBuffer[DER_MAX_TBS];
    bool ret;
    ret = false;

    // Hash PrivateKey into a 256-bit key
    if (RiotCrypt_Hash(
            digest,
            sizeof(digest),
            (const uint8_t *)SeedData,
            SeedDataSize) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Hash() failed\n");
        goto Exit;
    }

    // Combine hashed PrivateKey and image digest, result in digest.
    if (RiotCrypt_Hash2(
            digest,
            sizeof(digest),
            digest,
            sizeof(digest),
            ImageDigest->digest,
            sizeof(ImageDigest->digest)) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Hash2() failed\n");
        goto Exit;
    }

    // Derive new Alias key pair from hashed PrivateKey and FWID.
    if (RiotCrypt_DeriveEccKey(
            &SubjectKeyPair->PublicKey,
            &SubjectKeyPair->PrivateKey,
            digest,
            sizeof(digest),
            (const uint8_t *)RIOT_LABEL_ALIAS,
            STR_LITERAL_LEN(RIOT_LABEL_ALIAS)) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_DeriveEccKey() failed\n");
        goto Exit;
    }

    memcpy(&x509TBSData, &x509AliasTBSData, sizeof(x509TBSData));
    x509TBSData.IssuerCommon = IssuerName;
    x509TBSData.SubjectCommon = ImageSubjectName;

    if (!Cyrep_DeriveCertSerialNum(
            (const uint8_t *)&SubjectKeyPair->PublicKey,
            sizeof(RIOT_ECC_PUBLIC),
            &x509TBSData)) {

        CYREP_INTERNAL_ERROR("Failed to derive cert serial number\n");
        goto Exit;
    }

    DERInitContext(&cerCtx, cerBuffer, DER_MAX_TBS);
    if (X509GetAliasCertTBS(
            &cerCtx,
            &x509TBSData,
            &SubjectKeyPair->PublicKey,
            (RIOT_ECC_PUBLIC *)&IssuerKeyPair->PublicKey, // discard const
            (uint8_t *)ImageDigest->digest, // discard const
            sizeof(ImageDigest->digest),
            CertChain->PathLenRemaining) != 0) {

        CYREP_INTERNAL_ERROR("X509GetAliasCertTBS() failed\n");
        goto Exit;
    }

    CertChain->PathLenRemaining -= 1;

    // Sign the Alias Key Certificate's TBS region.
    if (RiotCrypt_Sign(
            &tbsSig,
            cerCtx.Buffer,
            cerCtx.Position,
            &IssuerKeyPair->PrivateKey) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Sign() failed\n");
        goto Exit;
    }

    // Generate Alias Key Certificate by signing the TBS region.
    if (X509MakeAliasCert(&cerCtx, &tbsSig) != 0) {
        CYREP_INTERNAL_ERROR("X509MakeAliasCert() failed\n");
        goto Exit;
    }

    if (!Cyrep_AppendCert(CertChain, &cerCtx, IssuerName, ImageSubjectName)) {
        CYREP_INTERNAL_ERROR("Failed to append cert\n");
        goto Exit;
    }

    ret = true;

Exit:
    // Clean up potentially sensative data.
    Cyrep_SecureClearMemory(digest, sizeof(digest));

    return ret;
}

void Cyrep_CapturePrivateKey(
    CyrepKeyPair *Dest,
    CyrepKeyPair *Source
    )
{
    memcpy(
        Dest,
        Source,
        sizeof(CyrepKeyPair));

    Cyrep_SecureClearMemory(Source, sizeof(*Source));
}

bool Cyrep_ValidateCertChainVersion(
    const CyrepCertChain *CertChain
    )
{
    if (CertChain->Magic != CYREP_CERT_CHAIN_MAGIC) {
        CYREP_INTERNAL_ERROR("Cyrep cert chain: bad magic\n");
        return false;
    }

    if (CertChain->Version != CYREP_CERT_CHAIN_VERSION) {
        CYREP_INTERNAL_ERROR("Cyrep cert chain: bad version\n");
        return false;
    }

    return true;
}

#ifdef CONFIG_CYREP_OPTEE_BUILD
/**
 * strstr - Find the first substring in a %NUL terminated string
 * @s1: The string to be searched
 * @s2: The string to search for
 */
char * strstr(const char * s1,const char * s2);
char * strstr(const char * s1,const char * s2)
{
        int l1, l2;

        l2 = strlen(s2);
        if (!l2)
                return (char *) s1;
        l1 = strlen(s1);
        while (l1 >= l2) {
                l1--;
                if (!memcmp(s1,s2,l2))
                        return (char *) s1;
                s1++;
        }
        return NULL;
}
#endif

#ifdef CONFIG_CYREP_OPTEE_BUILD
/**
 * strncpy - Copy a length-limited, %NUL-terminated string
 * @dest: Where to copy the string to
 * @src: Where to copy the string from
 * @count: The maximum number of bytes to copy
 *
 * Note that unlike userspace strncpy, this does not %NUL-pad the buffer.
 * However, the result is not %NUL-terminated if the source exceeds
 * @count bytes.
 */
char * strncpy(char * dest,const char *src,size_t count);
char * strncpy(char * dest,const char *src,size_t count)
{
        char *tmp = dest;

        while (count-- && (*dest++ = *src++) != '\0')
                /* nothing */;

        return tmp;
}
#endif

size_t Cyrep_FindCertBySubjectName(
    const CyrepCertChain *CertChain,
    const char *SubjectName
    )
{
    size_t index;

    for (index = 0; index < ARRAY_SIZE(CertChain->CertIndex); ++index) {
        if (CertChain->CertIndex[index].CertBufferStartIndex ==
            CYREP_CERT_INDEX_INVALID) {

            break;
        }

        if (strcmp(CertChain->CertIndex[index].Subject,
               SubjectName) == 0) {

            return index;
        }
    }

    return CYREP_CERT_INDEX_INVALID;
}

size_t Cyrep_GetCertLength(
    const CyrepCertChain *CertChain,
    size_t CertIndex
    )
{
    const char *start;
    const char *marker;
    size_t bufferIndex;

    bufferIndex = CertChain->CertIndex[CertIndex].CertBufferStartIndex;
    start = &CertChain->CertBuffer[bufferIndex];

    marker = strstr(start, END_CERT_MARKER);
    if (!marker)
        return 0;

    return (marker - start) + sizeof(END_CERT_MARKER) - 1;
}

/*
 * Get the length of this cert plus all it's parent certs.
 */
size_t Cyrep_GetCertAndIssuersLength(
    const CyrepCertChain *CertChain,
    size_t CertIndex
    )
{
    size_t index = CertIndex;
    size_t length = 0;

    while (index != CYREP_CERT_INDEX_INVALID) {
        length += Cyrep_GetCertLength(CertChain, index);

        if (index == CertChain->CertIndex[index].IssuerIndex) {
            /* self-signed cert */
            break;
        }
        index = CertChain->CertIndex[index].IssuerIndex;
    }

    /* length does not include space for null terminator! */
    return length;
}

/*
 * Copy the cert at CertIndex and all of it's parent certs to a
 * null-terminated buffer
 */
bool Cyrep_GetCertAndIssuers(
    const CyrepCertChain *CertChain,
    size_t CertIndex,
    char *Buffer,
    size_t BufferSize
    )
{
    size_t index = CertIndex;
    size_t length = 0;
    size_t charsCopied = 0;
    const CyrepCertIndexEntry *entry;

    while (index != CYREP_CERT_INDEX_INVALID) {
        entry = &CertChain->CertIndex[index];
        length = Cyrep_GetCertLength(CertChain, index);
        if ((charsCopied + length + 1) > BufferSize) {
            return false;
        }

        strncpy(
            Buffer + charsCopied,
            &CertChain->CertBuffer[entry->CertBufferStartIndex],
            length);

        charsCopied += length;

        if (index == entry->IssuerIndex) {
            /* self-signed cert */
            break;
        }
        index = entry->IssuerIndex;
    }

    Buffer[charsCopied] = '\0';
    return true;
}

void Cyrep_SecureClearMemory(void *Mem, size_t Size)
{
    volatile char *vMem;

    if (Mem == NULL) return;

    vMem = (volatile char *)Mem;

    while (Size > 0) {
        // TODO: Use random values for clearing memory.
        *vMem = CYREP_CLEAR_MEM_VALUE;
        vMem++;
        Size--;
    }
}
