#include <CyrepCommon.h>
#include <RiotDerEnc.h>
#include <RiotX509Bldr.h>
#include <Cyrep.h>

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
// Macro for label sizes (skip strlen()).
//
#define lblSize(a)          	(sizeof(a) - 1)

// The static data fields that make up the x509 "to be signed" region
const RIOT_X509_TBS_DATA x509TBSDataTemplate = { { 0x0A, 0x0B, 0x0C, 0x0D, 0x0E },
                                   "RIoT Core", "MSR_TEST", "US",
                                   "170101000000Z", "370101000000Z",
                                   "RIoT Device", "MSR_TEST", "US" };

void Cyrep_MeasureL1Image(const uint8_t *Cid, size_t CdiSize, const void *ImageBase,
						size_t ImageSize, const char *CertIssuerCommon,
						const char *CertSubjectCommon, CyrepFwMeasurement *MeasurementResult)
{
    uint8_t             cerBuffer[DER_MAX_TBS];
    uint8_t             cDigest[RIOT_DIGEST_LENGTH];
    RIOT_ECC_PRIVATE    deviceIDPriv;
    RIOT_ECC_SIGNATURE  tbsSig;
    DERBuilderContext   cerCtx;
    uint8_t             FWID[RIOT_DIGEST_LENGTH];
    RIOT_X509_TBS_DATA  x509TBSData;
    size_t              certLength;

    // Sanity checks for programming errors, catch early
    assert(Cid != NULL);
    assert(CdiSize > 0);
    assert(ImageBase != NULL);
    assert(ImageSize > 0);
    assert(CertIssuerCommon != NULL);
    assert(CertSubjectCommon != NULL);
    assert(MeasurementResult != NULL);

    memset(MeasurementResult, 0x00, sizeof(*MeasurementResult));

    // Don't use CDI directly
    RiotCrypt_Hash(cDigest, sizeof(cDigest), Cid, CdiSize);

    // Derive DeviceID key pair from CDI
    RiotCrypt_DeriveEccKey(&MeasurementResult->DeviceIDPub,
                           &deviceIDPriv,
                           cDigest, sizeof(cDigest),
                           (const uint8_t *)RIOT_LABEL_IDENTITY,
                           lblSize(RIOT_LABEL_IDENTITY));

    // Measure FW, i.e., calculate FWID
    RiotCrypt_Hash(FWID, sizeof(FWID), ImageBase, ImageSize);

    // Combine CDI and FWID, result in cDigest
    RiotCrypt_Hash2(cDigest, sizeof(cDigest),
                    cDigest, sizeof(cDigest),
                    FWID,    sizeof(FWID));

    // Derive Alias key pair from CDI and FWID
    RiotCrypt_DeriveEccKey(&MeasurementResult->AliasKeyPub,
                           &MeasurementResult->AliasKeyPriv,
                           cDigest, sizeof(cDigest),
                           (const uint8_t *)RIOT_LABEL_ALIAS,
                           lblSize(RIOT_LABEL_ALIAS));

    // Clean up potentially sensative data
    memset(cDigest, 0x00, sizeof(cDigest));
    
    // Build the TBS (to be signed) region of Alias Key Certificate
    DERInitContext(&cerCtx, cerBuffer, DER_MAX_TBS);
    memcpy(&x509TBSData, &x509TBSDataTemplate, sizeof(x509TBSData));
    x509TBSData.IssuerCommon = CertIssuerCommon;
    x509TBSData.SubjectCommon = CertSubjectCommon;
    X509GetAliasCertTBS(&cerCtx, &x509TBSData,
                        &MeasurementResult->AliasKeyPub, &MeasurementResult->DeviceIDPub,
                        FWID, sizeof(FWID));

    // Sign the Alias Key Certificate's TBS region
    RiotCrypt_Sign(&tbsSig, cerCtx.Buffer, cerCtx.Position, &deviceIDPriv);
    memset(&deviceIDPriv, 0x00, sizeof(deviceIDPriv));

    // Generate Alias Key Certificate by signing the TBS region
    X509MakeAliasCert(&cerCtx, &tbsSig);

    // Append the Alias Key Certificate to the certiicate chain
    certLength = sizeof(*MeasurementResult->CertChain);
    DERtoPEM(&cerCtx, CERT_TYPE, MeasurementResult->CertChain[0], &certLength);
    MeasurementResult->CertChain[0][certLength - 1] = '\0';
    MeasurementResult->CertChainCount = 1;

    return;
}

void Cyrep_MeasureL2plusImage(const CyrepFwMeasurement *CurrentMeasurement,
            const void *ImageBase, size_t ImageSize, const char *CertIssuerCommon,
            const char *CertSubjectCommon, CyrepFwMeasurement *MeasurementResult)
{
    uint8_t             cerBuffer[DER_MAX_TBS];
    uint8_t             cDigest[RIOT_DIGEST_LENGTH];
    RIOT_ECC_PRIVATE    deviceIDPriv;
    RIOT_ECC_SIGNATURE  tbsSig;
    DERBuilderContext   cerCtx;
    uint8_t             FWID[RIOT_DIGEST_LENGTH];
    RIOT_X509_TBS_DATA  x509TBSData;
    uint32_t            certLength;
    size_t              certIndex;

    // Sanity checks for programming errors, catch early
    assert(CurrentMeasurement != NULL);
    assert(ImageBase != NULL);
    assert(ImageSize > 0);
    assert(CertIssuerCommon != NULL);
    assert(CertSubjectCommon != NULL);
    assert(MeasurementResult != NULL);

    // Copy the current FW measurement as a template to the new measurement
    // We will reuse the same DeviceIDPub and append to the certificate chain
    memcpy(MeasurementResult, CurrentMeasurement, sizeof(*CurrentMeasurement));

    // Hash AliasKeyPriv into a 256-bit key
    RiotCrypt_Hash(cDigest, sizeof(cDigest), &CurrentMeasurement->AliasKeyPriv,
                   sizeof(CurrentMeasurement->AliasKeyPriv));

    // Measure FW, i.e., calculate FWID
    RiotCrypt_Hash(FWID, sizeof(FWID), ImageBase, ImageSize);

    // Combine hashed AliasKeyPriv and FWID, result in cDigest
    RiotCrypt_Hash2(cDigest, sizeof(cDigest),
                    cDigest, sizeof(cDigest),
                    FWID,    sizeof(FWID));

    // Derive new Alias key pair from hashed AliasKeyPriv and FWID
    RiotCrypt_DeriveEccKey(&MeasurementResult->AliasKeyPub,
                           &MeasurementResult->AliasKeyPriv,
                           cDigest, sizeof(cDigest),
                           (const uint8_t *)RIOT_LABEL_ALIAS,
                           lblSize(RIOT_LABEL_ALIAS));

    // Clean up potentially sensative data
    memset(cDigest, 0x00, sizeof(cDigest));
    
    // Build the TBS (to be signed) region of Alias Key Certificate
    DERInitContext(&cerCtx, cerBuffer, DER_MAX_TBS);
    memcpy(&x509TBSData, &x509TBSDataTemplate, sizeof(x509TBSData));
    x509TBSData.IssuerCommon = CertIssuerCommon;
    x509TBSData.SubjectCommon = CertSubjectCommon;
    X509GetAliasCertTBS(&cerCtx, &x509TBSData,
                        &MeasurementResult->AliasKeyPub, &MeasurementResult->DeviceIDPub,
                        FWID, sizeof(FWID));

    // Sign the Alias Key Certificate's TBS region
    RiotCrypt_Sign(&tbsSig, cerCtx.Buffer, cerCtx.Position, &deviceIDPriv);
    memset(&deviceIDPriv, 0x00, sizeof(deviceIDPriv));

    // Generate Alias Key Certificate by signing the TBS region
    X509MakeAliasCert(&cerCtx, &tbsSig);

    // Append the Alias Key Certificate to the certiicate chain
    certIndex = MeasurementResult->CertChainCount;

    // This can't be the first certificate, and there should be a space for it
    // [mlotfy]: TODO: consider implementing proper handling for expected errors
    assert(certIndex + 1 < CYREP_CERT_CHAIN_MAX);
    assert(certIndex > 0);
    
    certLength = sizeof(*MeasurementResult->CertChain);
    DERtoPEM(&cerCtx, CERT_TYPE, MeasurementResult->CertChain[certIndex],
             &certLength);

    MeasurementResult->CertChain[certIndex][certLength - 1] = '\0';
    MeasurementResult->CertChainCount++;

    return;
}