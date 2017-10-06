//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
#include <CyrepCommon.h>
#include <RiotDerEnc.h>
#include <RiotX509Bldr.h>
#include <Cyrep.h>

//
// Define 2 magic numbers that gets placed before and after the measurement
// to help in catching underwrite and overflow conditions by Cyrep measurement
// routines.
//
#define CYREP_MEASUREMENT_PREFIX_GUARD     0xBBEEEEFF
#define CYREP_MEASUREMENT_POSTFIX_GUARD    0xFFEEEEBB

//
// Byte value used to set memory to when clearing a memory region.
//
#define CYREP_CLEAR_MEM_VALUE   0x5A

//
// Key derivation labels used by both RIoT Devices and External Infrastructure
//
#define RIOT_LABEL_IDENTITY     "Identity"
#define RIOT_LABEL_ALIAS        "Alias"
#define RIOT_LABEL_PROTECTOR    "Encrypt"
#define RIOT_LABEL_INTEGRITY    "HMAC"
#define RIOT_LABEL_AIK          "AikProtector"
#define RIOT_LABEL_SK           "Sealing"
#define RIOT_LABEL_MK           "Migration"
#define RIOT_LABEL_AK           "Attestation"

//
// Macro for label sizes instead of calling strlen()
//
#define STR_LABEL_LEN(S)        (sizeof(S) - 1)

// The static data fields that make up the x509 "to be signed" region
const RIOT_X509_TBS_DATA x509TBSDataTemplate = { { 0x0A, 0x0B, 0x0C, 0x0D, 0x0E },
                                   "RIoT Core", "MSR_TEST", "US",
                                   "170101000000Z", "370101000000Z",
                                   "RIoT Device", "MSR_TEST", "US" };

static bool VerifyFwMeasurementGuards(const CyrepFwMeasurement *Measurement)
{
    assert(Measurement != NULL);
    if (Measurement->PrefixGuard != CYREP_MEASUREMENT_PREFIX_GUARD) {
        CYREP_INTERNAL_ERROR("Measurement->PrefixGuard mismatch. "
                             "(Expected:%x, Actual:%x)\n",
                             CYREP_MEASUREMENT_PREFIX_GUARD,
                             Measurement->PrefixGuard);
        return false;
    }

    if (Measurement->PostfixGuard != CYREP_MEASUREMENT_POSTFIX_GUARD) {
        CYREP_INTERNAL_ERROR("Measurement->PostfixGuard mismatch. "
                             "(Expected:%x, Actual:%x)\n",
                             CYREP_MEASUREMENT_POSTFIX_GUARD,
                             Measurement->PostfixGuard);
        return false;
    }

    return true;
}

static void FwMeasurementInit(CyrepFwMeasurement *Measurement)
{
    memset(Measurement, 0x00, sizeof(*Measurement));
    Measurement->PrefixGuard = CYREP_MEASUREMENT_PREFIX_GUARD;
    Measurement->PostfixGuard = CYREP_MEASUREMENT_POSTFIX_GUARD;
}

bool Cyrep_MeasureL1Firmware(const uint8_t *Cdi, size_t CdiSize,
                             const CyrepFwInfo *FwInfo, const char *CertIssuerCommon,
                             CyrepFwMeasurement *MeasurementResult)
{
    bool ret;
    uint8_t cerBuffer[DER_MAX_TBS];
    uint8_t cDigest[RIOT_DIGEST_LENGTH];
    RIOT_ECC_PRIVATE deviceIDPriv;
    RIOT_ECC_SIGNATURE tbsSig;
    DERBuilderContext cerCtx;
    uint8_t FWID[RIOT_DIGEST_LENGTH];
    RIOT_X509_TBS_DATA x509TBSData;
    size_t certLength;

    ret = false;

    assert(Cdi != NULL);
    assert(FwInfo != NULL);
    assert(CertIssuerCommon != NULL);
    assert(MeasurementResult != NULL);
    assert(CdiSize > 0);

    if ((Cdi == NULL) || (CdiSize == 0) || (FwInfo == NULL) || (CertIssuerCommon == NULL) ||
        (MeasurementResult == NULL)) {
        goto Exit;
    }

    FwMeasurementInit(MeasurementResult);

    // Don't use CDI directly.
    if (RiotCrypt_Hash(cDigest, sizeof(cDigest), Cdi, CdiSize) != RIOT_SUCCESS) {
        CYREP_INTERNAL_ERROR("RiotCrypt_Hash() failed\n");
        goto Exit;
    }

    // Derive DeviceID key pair from CDI.
    if (RiotCrypt_DeriveEccKey(&MeasurementResult->DeviceIDPub,
                               &deviceIDPriv,
                               cDigest, sizeof(cDigest),
                               (const uint8_t *)RIOT_LABEL_IDENTITY,
                               STR_LABEL_LEN(RIOT_LABEL_IDENTITY)) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_DeriveEccKey() failed\n");
        goto Exit;
    }

    if ((FwInfo->FwBase == NULL) || (FwInfo->FwSize == 0) ||
        (FwInfo->FwName[0] == '\0')) {

        CYREP_INTERNAL_ERROR("Invalid FwInfo name, address or size\n");
        goto Exit;
    }

    // Measure FW, i.e., calculate FWID.
    if (RiotCrypt_Hash(FWID, sizeof(FWID), FwInfo->FwBase, FwInfo->FwSize) !=
        RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Hash() failed\n");
        goto Exit;
    }

    // Combine CDI and FWID, result in cDigest.
    if (RiotCrypt_Hash2(cDigest, sizeof(cDigest),
                        cDigest, sizeof(cDigest),
                        FWID, sizeof(FWID)) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Hash2() failed\n");
        goto Exit;
    }

    // Derive Alias key pair from CDI and FWID.
    if (RiotCrypt_DeriveEccKey(&MeasurementResult->AliasKeyPub,
                               &MeasurementResult->AliasKeyPriv,
                               cDigest, sizeof(cDigest),
                               (const uint8_t *)RIOT_LABEL_ALIAS,
                               STR_LABEL_LEN(RIOT_LABEL_ALIAS)) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_DeriveEccKey() failed\n");
        goto Exit;
    }

    // Build the TBS (to be signed) region of Alias Key Certificate
    DERInitContext(&cerCtx, cerBuffer, DER_MAX_TBS);
    memcpy(&x509TBSData, &x509TBSDataTemplate, sizeof(x509TBSData));
    x509TBSData.IssuerCommon = CertIssuerCommon;
    x509TBSData.SubjectCommon = FwInfo->FwName;
    if (X509GetAliasCertTBS(&cerCtx, &x509TBSData,
                            &MeasurementResult->AliasKeyPub,
                            &MeasurementResult->DeviceIDPub,
                            FWID, sizeof(FWID)) != 0) {

        CYREP_INTERNAL_ERROR("X509GetAliasCertTBS() failed\n");
        goto Exit;
    }

    // Sign the Alias Key Certificate's TBS region.
    if (RiotCrypt_Sign(&tbsSig, cerCtx.Buffer, cerCtx.Position,
                       &deviceIDPriv) != 0) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Sign() failed\n");
        goto Exit;
    }

    // Generate Alias Key Certificate by signing the TBS region.
    if (X509MakeAliasCert(&cerCtx, &tbsSig) != 0) {
        CYREP_INTERNAL_ERROR("X509MakeAliasCert() failed\n");
        goto Exit;
    }

    // Append the Alias Key Certificate to the certificate chain
    certLength = sizeof(MeasurementResult->CertChain[0]) - 1;
    if (DERtoPEM(&cerCtx, CERT_TYPE, MeasurementResult->CertChain[0],
                  &certLength) != 0) {

        CYREP_INTERNAL_ERROR("DERtoPEM() failed\n");
        goto Exit;
    }

    MeasurementResult->CertChain[0][certLength] = '\0';
    MeasurementResult->CertChainCount = 1;

    if (!VerifyFwMeasurementGuards(MeasurementResult)) {
        CYREP_INTERNAL_ERROR("!!MeasurementResult underwrite/overflow detected!!\n");
        goto Exit;
    }

    ret = true;

Exit:
    // Clean up potentially sensative data.
    Cyrep_SecureClearMemory(cDigest, sizeof(cDigest));
    Cyrep_SecureClearMemory(&deviceIDPriv, sizeof(deviceIDPriv));
    return ret;
}

bool Cyrep_MeasureL2plusFirmware(const CyrepFwMeasurement *CurrentMeasurement,
                                 const CyrepFwInfo *FwInfo,
                                 const char *CertIssuerCommon,
                                 CyrepFwMeasurement *MeasurementResult)
{
    bool ret;
    uint8_t cerBuffer[DER_MAX_TBS];
    uint8_t cDigest[RIOT_DIGEST_LENGTH];
    RIOT_ECC_SIGNATURE tbsSig;
    DERBuilderContext cerCtx;
    uint8_t FWID[RIOT_DIGEST_LENGTH];
    RIOT_X509_TBS_DATA  x509TBSData;
    uint32_t certLength;
    size_t certIndex;

    ret = false;

    assert(CurrentMeasurement != NULL);
    assert(FwInfo != NULL);
    assert(CertIssuerCommon != NULL);
    assert(MeasurementResult != NULL);
    assert(CurrentMeasurement != MeasurementResult);

    if ((CurrentMeasurement == NULL) || (FwInfo == NULL) ||
        (CertIssuerCommon == NULL) || (MeasurementResult == NULL) ||
        (CurrentMeasurement == MeasurementResult)) {
        goto Exit;
    }

    if (!VerifyFwMeasurementGuards(CurrentMeasurement)) {
        CYREP_INTERNAL_ERROR("!!CurrentMeasurement underwrite/overflow detected!!\n");
        goto Exit;
    }

    // Copy the current FW measurement as a template to the new measurement
    // We will reuse the same DeviceIDPub and append to the certificate chain.
    memcpy(MeasurementResult, CurrentMeasurement, sizeof(*CurrentMeasurement));

    // Hash AliasKeyPriv into a 256-bit key
    if (RiotCrypt_Hash(cDigest, sizeof(cDigest), &CurrentMeasurement->AliasKeyPriv,
                       sizeof(CurrentMeasurement->AliasKeyPriv)) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Hash() failed\n");
        goto Exit;
    }

    if ((FwInfo->FwBase == NULL) || (FwInfo->FwSize == 0) ||
        (FwInfo->FwName[0] == '\0')) {

        CYREP_INTERNAL_ERROR("Invalid FwInfo name, address or size\n");
        goto Exit;
    }

    // Measure FW, i.e., calculate FWID.
    if (RiotCrypt_Hash(FWID, sizeof(FWID), FwInfo->FwBase, FwInfo->FwSize) !=
        RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Hash() failed\n");
        goto Exit;
    }

    // Combine hashed AliasKeyPriv and FWID, result in cDigest.
    if (RiotCrypt_Hash2(cDigest, sizeof(cDigest), cDigest, sizeof(cDigest),
                        FWID, sizeof(FWID)) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Hash2() failed\n");
        goto Exit;
    }

    // Derive new Alias key pair from hashed AliasKeyPriv and FWID.
    if (RiotCrypt_DeriveEccKey(&MeasurementResult->AliasKeyPub,
                               &MeasurementResult->AliasKeyPriv,
                               cDigest, sizeof(cDigest),
                               (const uint8_t *)RIOT_LABEL_ALIAS,
                               STR_LABEL_LEN(RIOT_LABEL_ALIAS)) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_DeriveEccKey() failed\n");
        goto Exit;
    }

    // Build the TBS (to be signed) region of Alias Key Certificate.
    DERInitContext(&cerCtx, cerBuffer, DER_MAX_TBS);
    memcpy(&x509TBSData, &x509TBSDataTemplate, sizeof(x509TBSData));
    x509TBSData.IssuerCommon = CertIssuerCommon;
    x509TBSData.SubjectCommon = FwInfo->FwName;
    if (X509GetAliasCertTBS(&cerCtx, &x509TBSData,
                            &MeasurementResult->AliasKeyPub,
                            &MeasurementResult->DeviceIDPub,
                            FWID, sizeof(FWID)) != 0) {

        CYREP_INTERNAL_ERROR("X509GetAliasCertTBS() failed\n");
        goto Exit;
    }

    // Sign the Alias Key Certificate's TBS region.
    if (RiotCrypt_Sign(&tbsSig, cerCtx.Buffer, cerCtx.Position,
                       &CurrentMeasurement->AliasKeyPriv) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Sign() failed\n");
        goto Exit;
    }

    // Generate Alias Key Certificate by signing the TBS region.
    if (X509MakeAliasCert(&cerCtx, &tbsSig) != 0) {
        CYREP_INTERNAL_ERROR("X509MakeAliasCert() failed\n");
        goto Exit;
    }

    // Append the Alias Key Certificate to the certificate chain.
    certIndex = MeasurementResult->CertChainCount;

    // This can't be the first certificate, and there should be a space for it.
    if (certIndex == 0 || certIndex >= CYREP_CERT_CHAIN_MAX) {
        CYREP_INTERNAL_ERROR("CertChain is either empty or full\n");
        goto Exit;
    }

    certLength = sizeof(MeasurementResult->CertChain[0]) - 1;
    if (DERtoPEM(&cerCtx, CERT_TYPE, MeasurementResult->CertChain[certIndex],
                 &certLength) != 0) {

        CYREP_INTERNAL_ERROR("DERtoPEM() failed\n");
        goto Exit;
    }

    MeasurementResult->CertChain[certIndex][certLength] = '\0';
    MeasurementResult->CertChainCount++;

    if (!VerifyFwMeasurementGuards(MeasurementResult)) {
        CYREP_INTERNAL_ERROR("!!MeasurementResult underwrite/overflow detected!!\n");
        goto Exit;
    }

    ret = true;

Exit:
    // Clean up potentially sensative data.
    Cyrep_SecureClearMemory(cDigest, sizeof(cDigest));
    return ret;
}

void Cyrep_ArgsInit(CyrepFwArgs *Args) {
    assert(Args != NULL);
    memset(Args, 0x0, sizeof(*Args));
}

bool Cyrep_ArgsVerify(const CyrepFwArgs *Args)
{
    uint8_t digest[RIOT_DIGEST_LENGTH];
    size_t actual_args_size;

    assert(Args != NULL);
    if (!VerifyFwMeasurementGuards(&Args->Fields.FwMeasurement)) {
        CYREP_INTERNAL_ERROR("VerifyArgsGuard(0x%p) failed\n", Args);
        return false;
    }

    actual_args_size = sizeof(Args->Fields) - sizeof(Args->Fields.Digest);

    if (RiotCrypt_Hash(digest, sizeof(digest), &Args->Fields.FwMeasurement,
                       actual_args_size) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("RiotCrypt_Hash(Args->FwMeasurement) failed\n");
        return false;
    }

    if (memcmp(digest, Args->Fields.Digest, sizeof(digest)) != 0) {
        CYREP_INTERNAL_ERROR("Verify FW measurement hash failed\n");
        return false;
    }

    return true;
}

bool Cyrep_ArgsPostprocess(CyrepFwArgs *Args)
{
    size_t actual_args_size;

    assert(Args != NULL);
    if (!VerifyFwMeasurementGuards(&Args->Fields.FwMeasurement)) {
        CYREP_INTERNAL_ERROR("VerifyArgsGuard(0x%p) failed\n", Args);
        return false;
    }

    actual_args_size = sizeof(Args->Fields) - sizeof(Args->Fields.Digest);

    if (RiotCrypt_Hash(Args->Fields.Digest, sizeof(Args->Fields.Digest),
                       &Args->Fields, actual_args_size) != RIOT_SUCCESS) {

        CYREP_INTERNAL_ERROR("Failed to generate args digest\n");
        return false;
    }

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