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

//
// Byte value used to set memory to when clearing a memory region.
//
#define CYREP_CLEAR_MEM_VALUE	0x5A

//
// Macro to get the length of a string literal instead of calling strlen
//
#define STR_LITERAL_LEN(S)		  (sizeof(S) - 1)

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

static bool Cyrep_AppendCert(
    DERBuilderContext *DerContext,
    CyrepCertChain *CertChain
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
            CERT_TYPE,
            &CertChain->CertBuffer[certChainLen],
            &certBufferRemaining) != 0) {

        CYREP_INTERNAL_ERROR("DERtoPEM() failed\n");
        return false;
    }

    CertChain->CertBuffer[certChainLen + certBufferRemaining] = '\0';

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

    if (!Cyrep_AppendCert(&derCtx, CertChain)) {
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

    if (!Cyrep_AppendCert(&derCtx, CertChain)) {
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
    CertChain->PathLenRemaining = InitialPathLen;
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
    RIOT_X509_TBS_DATA	x509TBSData;
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

    if (!Cyrep_AppendCert(&cerCtx, CertChain)) {
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

