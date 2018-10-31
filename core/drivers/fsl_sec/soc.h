/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2015, 2016 Freescale Semiconductor, Inc.
 * Copyright (C) 2016, 2017 NXP
 *
 *  Rod Dorris <rod.dorris@nxp.com>
 */

#ifndef _SOC_H
#define _SOC_H

// pwr mgmt features supported in the soc-specific code:
//   value == 0x0  the soc code does not support this feature
//   value != 0x0  the soc code supports this feature
#define SOC_CORE_RELEASE 0x0
#define SOC_CORE_RESTART 0x0
#define SOC_CORE_OFF 0x0
#define SOC_CORE_STANDBY 0x1
#define SOC_CORE_PWR_DWN 0x1
#define SOC_CLUSTER_STANDBY 0x1
#define SOC_CLUSTER_PWR_DWN 0x1
#define SOC_SYSTEM_STANDBY 0x1
#define SOC_SYSTEM_PWR_DWN 0x1
#define SOC_SYSTEM_OFF 0x1
#define SOC_SYSTEM_RESET 0x1

// base addresses
#define GICD_BASE_ADDR 0x01401000
#define GICC_BASE_ADDR 0x01402000

// OCRAM
#define OCRAM_SIZE_IN_BYTES 0x20000
#define OCRAM_MID_ADDR 0x10010000

#define CONFIG_CHIP_SELECTS_PER_CTRL 1

//-----------------------------------------------------------------------------

// set this switch to 1 if you need to keep the debug block
// clocked during system power-down
#define DEBUG_ACTIVE 0
// set this switch to 1 if you need to keep the ocram 1&2
// clocked during system power-down
#define OCRAM_ACTIVE 1

// base addresses
#define CCI_400_BASE_ADDR 0x01180000

// retry count for cci400 status bit
#define CCI400_PEND_CNT 0x800

#define IPPDEXPCR_PFE_MAC1 0x80000000
#define IPPDEXPCR_PFE_MAC2 0x40000000
#define IPPDEXPCR_PFE_PE 0x20000000
#define IPPDEXPCR_PFE_250M 0x10000000
#define IPPDEXPCR_I2C1 0x00080000
#define IPPDEXPCR_FLEXTIMER1 0x00020000
#define IPPDEXPCR_OCRAM1 0x00010000
#define IPPDEXPCR_GPIO1 0x00000040
#define IPPDEXPCR_PFE 0x00000020

#define IPPDEXPCR_PFE_MASK                                            \
	(IPPDEXPCR_PFE_MAC1 | IPPDEXPCR_PFE_MAC2 | IPPDEXPCR_PFE_PE | \
	 IPPDEXPCR_PFE_250M | IPPDEXPCR_PFE)

#define DEVDISR1_SEC 0x00000200
#define DEVDISR1_USB3 0x00004000
#define DEVDISR1_SATA 0x00008000
#define DEVDISR1_USB2 0x00040000
#define DEVDISR1_PFE 0x00080000
#define DEVDISR1_EDMA 0x00400000
#define DEVDISR1_ESDHC2 0x10000000
#define DEVDISR1_ESDHC1 0x20000000
#define DEVDISR1_PBL 0x80000000

#define DEVDISR1_VALUE                                                  \
	(DEVDISR1_PBL | DEVDISR1_ESDHC1 | DEVDISR1_ESDHC2 |             \
	 DEVDISR1_EDMA | DEVDISR1_PFE | DEVDISR1_USB2 | DEVDISR1_SATA | \
	 DEVDISR1_USB3 | DEVDISR1_SEC)

#define DEVDISR4_QSPI 0x08000000
#define DEVDISR4_DUART1 0x20000000

#define DEVDISR4_VALUE (DEVDISR4_QSPI | DEVDISR4_DUART1)

#define DEVDISR5_CCI400 0x00000001
#define DEVDISR5_I2C_1 0x00000002
#define DEVDISR5_I2C_2 0x00000004
#define DEVDISR5_SPI1 0x00000100
#define DEVDISR5_WDOG2 0x00000200
#define DEVDISR5_FLEXTIMER 0x00000400
#define DEVDISR5_WDOG1 0x00000800
#define DEVDISR5_SAI5 0x00040000
#define DEVDISR5_SAI4 0x00080000
#define DEVDISR5_SAI3 0x00100000
#define DEVDISR5_DBG 0x00200000
#define DEVDISR5_GPIO 0x00400000
#define DEVDISR5_OCRAM2 0x01000000
#define DEVDISR5_OCRAM1 0x02000000
#define DEVDISR5_SAI2 0x04000000
#define DEVDISR5_SAI1 0x08000000
#define DEVDISR5_DDR 0x80000000

#define DEVDISR5_BASE                                          \
	(DEVDISR5_SAI1 | DEVDISR5_SAI2 |                       \
	 DEVDISR5_GPIO | DEVDISR5_SAI3 |                       \
	 DEVDISR5_SAI4 | DEVDISR5_SAI5 | DEVDISR5_WDOG1 |      \
	 DEVDISR5_FLEXTIMER | DEVDISR5_WDOG2 | DEVDISR5_SPI1 | \
	 DEVDISR5_I2C_2 | DEVDISR5_I2C_1)

#if (OCRAM_ACTIVE && DEBUG_ACTIVE)
#define DEVDISR5_VALUE DEVDISR5_BASE
#elif (!OCRAM_ACTIVE && !DEBUG_ACTIVE)
#define DEVDISR5_VALUE \
	(DEVDISR5_BASE | DEVDISR5_OCRAM1 | DEVDISR5_OCRAM2 | DEVDISR5_DBG)
#elif !OCRAM_ACTIVE
#define DEVDISR5_VALUE \
	(DEVDISR5_BASE | DEVDISR5_OCRAM1 | DEVDISR5_OCRAM2)
#elif !DEBUG_ACTIVE
#define DEVDISR5_VALUE (DEVDISR5_BASE | DEVDISR5_DBG)
#endif

#define DEVDISR5_MEM DEVDISR5_DDR

// Note that the IPSTPCRn and IPSTPACKRn registers have the same bit
// definition as DEVDISRn.
// IPSTPCR0 to DEVDISR1, IPSTPCR3 to DEVDISR4, IPSTPCR4 to DEVDISR5

#define IPSTPCR0_VALUE DEVDISR1_VALUE
#define IPSTPCR3_VALUE DEVDISR4_VALUE
#define IPSTPCR4_VALUE DEVDISR5_VALUE

// 25mhz
#define COUNTER_FRQ_EL0 0x017D7840

//----------------------------------------------------------------------------

#endif // _SOC_H
