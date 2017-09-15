#ifndef __CYREP_H__
#define __CYREP_H__

#define CYREP_CERT_CHAIN_MAX 4

typedef struct {
	RIOT_ECC_PUBLIC     DeviceIDPub;
	RIOT_ECC_PUBLIC     AliasKeyPub;
	RIOT_ECC_PRIVATE    AliasKeyPriv;
	size_t				CertChainCount;
	char                CertChain[CYREP_CERT_CHAIN_MAX][DER_MAX_PEM];
} CyrepFwMeasurement;

// Firmware name size including the null character
#define CYREP_FW_NAME_MAX 8

typedef struct {
	char FwName[CYREP_FW_NAME_MAX];
	void *FwBase;
	size_t FwSize;
} CyrepFwInfo;

// CyReP args to L1+ image (i.e OPTEE, u-boot or UEFI)
typedef struct {
	CyrepFwMeasurement FwMeasurement;
	CyrepFwInfo FwInfo;
} CyrepFwArgs;
	
void Cyrep_MeasureL1Image(const uint8_t *Cid, size_t CdiSize, const void *ImageBase,
						size_t ImageSize, const char *CertIssuerCommon,
						const char *CertSubjectCommon, CyrepFwMeasurement *MeasurementResult);

void Cyrep_MeasureL2plusImage(const CyrepFwMeasurement *CurrentMeasurement,
            const void *ImageBase, size_t ImageSize, const char *CertIssuerCommon,
            const char *CertSubjectCommon, CyrepFwMeasurement *MeasurementResult);

#endif // __CYREP_H__