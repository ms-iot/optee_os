/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * All rights reserved.
 * Copyright (c) 2016, Wind River Systems.
 * All rights reserved.
 * Copyright (c) 2017, Microsoft.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef PLAT_IMX_IMX_REGS_H
#define PLAT_IMX_IMX_REGS_H

#ifdef CFG_MX6
#define UART1_BASE			0x2020000
#define IOMUXC_BASE			0x020E0000
#define IOMUXC_SIZE			0x4000
#define IOMUXC_GPR_BASE			0x020E4000
#define SRC_BASE			0x020D8000
#define SRC_SIZE			0x4000
#define CCM_BASE			0x020C4000
#define CCM_SIZE			0x4000
#define ANATOP_BASE			0x020C8000
#define ANATOP_SIZE			0x1000
#define SNVS_BASE			0x020CC000
#define GPC_BASE			0x020DC000
#define GPC_SIZE			0x4000
#define WDOG_BASE			0x020BC000
#define SEMA4_BASE			0x02290000
#define SEMA4_SIZE			0x4000
#define MMDC_P0_BASE			0x021B0000
#define MMDC_P0_SIZE			0x4000
#define MMDC_P1_BASE			0x021B4000
#define MMDC_P1_SIZE			0x4000
#define TZASC_BASE			0x21D0000
#define TZASC2_BASE			0x21D4000
#define UART2_BASE			0x021E8000
#define UART3_BASE			0x021EC000
#define UART4_BASE			0x021F0000
#define UART5_BASE			0x021F4000
#define AIPS1_BASE			0x02000000
#define AIPS1_SIZE			0x100000
#define AIPS2_BASE			0x02100000
#define AIPS2_SIZE			0x100000
#define AIPS3_BASE			0x02200000
#define AIPS3_SIZE			0x100000

#define SCU_BASE			0x00A00000
#define PL310_BASE			0x00A02000
#define SRC_BASE			0x020D8000
#define SRC_SCR				0x000
#define SRC_GPR1			0x020
#define SRC_SCR_CPU_ENABLE_ALL		SHIFT_U32(0x7, 22)
#define SRC_SCR_CORE1_RST_OFFSET	14
#define SRC_SCR_CORE1_ENABLE_OFFSET	22
#define SRC_SCR_WARM_RESET_ENABLE       (1u << 0)
#define SRC_SCR_MASK_WDOG_RST           0x00000780
#define SRC_SCR_WDOG_NOTMASKED          (0xA << 7)
#define IRAM_BASE			0x00900000

#define OCOTP_BASE			0x021BC000

#define GIC_BASE			0x00A00000
#define GICD_OFFSET			0x1000
#define GPIO_BASE			0x0209C000

#if defined(CFG_MX6UL) || defined(CFG_MX6ULL)
#define GICC_OFFSET			0x2000
/* No CAAM on i.MX6ULL */
#define CAAM_BASE			0x02140000
#else
#define GICC_OFFSET			0x100
#if defined(CFG_CYREP)
#define CAAM_BASE			0x00100000
#else
#define CAAM_BASE			0x02100000
#endif
#endif

#if defined(CFG_MX6Q)
#define ATZ1_BASE_ADDR			AIPS1_BASE
#define ATZ2_BASE_ADDR			AIPS2_BASE

#define AIPS1_OFF_BASE_ADDR		(ATZ1_BASE_ADDR + 0x80000)
#define AIPS2_OFF_BASE_ADDR		(ATZ2_BASE_ADDR + 0x80000)

#define IP2APB_TZASC1_BASE_ADDR		(AIPS2_OFF_BASE_ADDR + 0x50000)
#endif

#define GIC_CPU_BASE			(GIC_BASE + GICC_OFFSET)
#define GIC_DIST_BASE			(GIC_BASE + GICD_OFFSET)

/* Central Security Unit register values */

/*
 * Grant R+W access:
 * - Just to TZ Supervisor execution mode, and
 * - Just to a single device
 */
#define CSU_TZ_SUPERVISOR		0x22

/*
 * Grant R+W access:
 * - To all execution modes, and
 * - To a single device
 */
#define CSU_ALL_MODES			0xFF

/*
 * Grant R+W access:
 * - To all execution modes, and
 * - To both devices sharing a single CSU_CSL register
 */
#define CSU_ACCESS_ALL			((CSU_ALL_MODES << 16) | (CSU_ALL_MODES << 0))

#define CSU_BASE			0x021C0000
#define CSU_CSL_START			0x0
#define CSU_CSL_END			0xA0
#define CSU_CSL5			0x14
#define CSU_CSL15			0x3C
#define CSU_CSL16			0x40
#define CSU_SETTING_LOCK		0x01000100

/* Used in suspend/resume and low power idle */
#define MX6Q_SRC_GPR1			0x20
#define MX6Q_SRC_GPR2			0x24
#define MX6Q_MMDC_MISC			0x18
#define MX6Q_MMDC_MAPSR			0x404
#define MX6Q_MMDC_MPDGCTRL0		0x83c
#define MX6Q_GPC_IMR1			0x08
#define MX6Q_GPC_IMR2			0x0c
#define MX6Q_GPC_IMR3			0x10
#define MX6Q_GPC_IMR4			0x14
#define MX6Q_CCM_CCR			0x0
#define MX6Q_ANATOP_CORE		0x140

#define IOMUXC_GPR9_OFFSET		0x24
#define IOMUXC_GPR10_OFFSET		0x28

#define IOMUXC_GPR10_OCRAM_TZ_ADDR_OFFSET	5
#define IOMUXC_GPR10_OCRAM_TZ_ADDR_MASK		GENMASK_32(10, 5)

#define IOMUXC_GPR10_OCRAM_TZ_EN_OFFSET		4
#define IOMUXC_GPR10_OCRAM_TZ_EN_MASK		GENMASK_32(4, 4)

#define IOMUXC_GPR10_OCRAM_TZ_EN_LOCK_OFFSET	20
#define IOMUXC_GPR10_OCRAM_TZ_EN_LOCK_MASK	GENMASK_32(20, 20)
#define IOMUXC_GPR10_OCRAM_TZ_ADDR_LOCK_OFFSET	21
#define IOMUXC_GPR10_OCRAM_TZ_ADDR_LOCK_MASK	GENMASK_32(26, 21)

#define IOMUXC_GPR10_OCRAM_TZ_ADDR_OFFSET_6UL	11
#define IOMUXC_GPR10_OCRAM_TZ_ADDR_MASK_6UL	GENMASK_32(15, 11)
#define IOMUXC_GPR10_OCRAM_TZ_EN_OFFSET_6UL	10
#define IOMUXC_GPR10_OCRAM_TZ_EN_MASK_6UL	GENMASK_32(10, 10)

#define IOMUXC_GPR10_OCRAM_TZ_EN_LOCK_OFFSET_6UL	26
#define IOMUXC_GPR10_OCRAM_TZ_EN_LOCK_MASK_6UL		GENMASK_32(26, 26)
#define IOMUXC_GPR10_OCRAM_TZ_ADDR_LOCK_OFFSET_6UL	(27)
#define IOMUXC_GPR10_OCRAM_TZ_ADDR_LOCK_MASK_6UL	GENMASK_32(31, 27)

/* CCM */

/* Clock enabled except in STOP mode */
#define IMX_CGR_CLK_ENABLED 0x3
#define IMX_CCM_CCGR1_ECSPI2_CLK_SHIFT 2
/* ECSPI2 clock enabled */
#define IMX_CCM_CCGR1_ECSPI2_CLK_ENABLED  \
    (IMX_CGR_CLK_ENABLED << IMX_CCM_CCGR1_ECSPI2_CLK_SHIFT) 

/* GPIO */
#define IMX_GPIO_PORTS              7
#define IMX_GPIO_PORT_GRANULARITY   0x4000
#define IMX_GPIO_REGISTER_BITS      32

/* ECSPI */
#define MXC_ECSPI1_BASE_ADDR        (ATZ1_BASE_ADDR + 0x08000)
#define MXC_ECSPI_BUS_COUNT         5
#define MXC_ECSPI_BUS_GRANULARITY   0x4000

#define MXC_CSPICON_POL		4
#define MXC_CSPICON_PHA		0
#define MXC_CSPICON_SSPOL	12

#define MXC_CSPICTRL_EN		        (1 << 0)
#define MXC_CSPICTRL_MODE	        (1 << 1)
#define MXC_CSPICTRL_XCH	        (1 << 2)
#define MXC_CSPICTRL_MODE_MASK      (0xf << 4)
#define MXC_CSPICTRL_CHIPSELECT(x)	(((x) & 0x3) << 12)
#define MXC_CSPICTRL_BITCOUNT(x)	(((x) & 0xfff) << 20)
#define MXC_CSPICTRL_PREDIV(x)	    (((x) & 0xF) << 12)
#define MXC_CSPICTRL_POSTDIV(x)	    (((x) & 0xF) << 8)
#define MXC_CSPICTRL_SELCHAN(x)	    (((x) & 0x3) << 18)
#define MXC_CSPICTRL_MAXBITS	    0xfff
#define MXC_CSPICTRL_TC		        (1 << 7)
#define MXC_CSPICTRL_RXOVF	        (1 << 6)
#define MXC_CSPIPERIOD_32KHZ	    (1 << 15)
#define MXC_MAX_SPI_BYTES	        32

#if defined(CFG_MX6UL) || defined(CFG_MX6ULL) || defined(CFG_MX6SX)
#define DRAM0_BASE			0x80000000
#else
#define DRAM0_BASE			0x10000000
#endif

#elif defined(CFG_MX7)

#define GIC_BASE		0x31000000
#define GIC_SIZE		0x8000
#define GICC_OFFSET		0x2000
#define GICD_OFFSET		0x1000

#if defined(CFG_CYREP)
#define CAAM_BASE		0x00100000
#else
#define CAAM_BASE		0x30900000
#endif
#define UART1_BASE		0x30860000
#define UART2_BASE		0x30890000
#define UART3_BASE		0x30880000

#define AIPS1_BASE		0x30000000
#define AIPS1_SIZE		0x400000
#define AIPS2_BASE		0x30400000
#define AIPS2_SIZE		0x400000
#define AIPS3_BASE		0x30800000
#define AIPS3_SIZE		0x400000

#define GPIO_BASE		0x30200000
#define GPIO_SIZE		0x70000

#define WDOG_BASE		0x30280000
#define LPSR_BASE		0x30270000
#define IOMUXC_BASE		0x30330000
#define IOMUXC_GPR_BASE		0x30340000
#define OCOTP_BASE		0x30350000
#define ANATOP_BASE		0x30360000
#define SNVS_BASE		0x30370000
#define CCM_BASE		0x30380000
#define SRC_BASE		0x30390000
#define GPC_BASE		0x303A0000
#define TZASC_BASE		0x30780000
#define DDRC_PHY_BASE		0x30790000
#define MMDC_P0_BASE		0x307A0000
#define DDRC_BASE		0x307A0000
#define IRAM_BASE		0x00900000
#define IRAM_S_BASE		0x00180000

/*
 * Grant R+W access:
 * - Just to TZ Supervisor execution mode, and
 * - Just to a single device
 */
#define CSU_TZ_SUPERVISOR		0x22

/*
 * Grant R+W access:
 * - To all execution modes, and
 * - To a single device
 */
#define CSU_ALL_MODES			0xFF

/*
 * Grant R+W access:
 * - To all execution modes, and
 * - To both devices sharing a single CSU_CSL register
 */
#define CSU_ACCESS_ALL			((CSU_ALL_MODES << 16) | (CSU_ALL_MODES << 0))

#define CSU_CSL_START		0x303E0000
#define CSU_CSL_END		0x303E0100
#define CSU_CSL_59		(0x303E0000 + 59 * 4)
#define CSU_CSL_28		(0x303E0000 + 28 * 4)
#define CSU_CSL_15		(0x303E0000 + 15 * 4)
#define CSU_CSL_12		(0x303E0000 + 12 * 4)
#define CSU_SETTING_LOCK	0x01000100

#define TRUSTZONE_OCRAM_START	0x180000

#define IOMUXC_GPR9_OFFSET				0x24
#define IOMUXC_GPR9_TZASC1_MUX_CONTROL_OFFSET		0

#define IOMUXC_GPR11_OFFSET				0x2C
#define IOMUXC_GPR11_OCRAM_S_TZ_ADDR_OFFSET		11
#define IOMUXC_GPR11_OCRAM_S_TZ_ADDR_MASK		GENMASK_32(13, 11)

#define IOMUXC_GPR11_OCRAM_S_TZ_EN_OFFSET		10
#define IOMUXC_GPR11_OCRAM_S_TZ_EN_MASK			GENMASK_32(10, 10)

#define IOMUXC_GPR11_OCRAM_S_TZ_EN_LOCK_OFFSET		26
#define IOMUXC_GPR11_OCRAM_S_TZ_EN_LOCK_MASK		GENMASK_32(26, 26)
#define IOMUXC_GPR11_OCRAM_S_TZ_ADDR_LOCK_OFFSET	GENMASK_32(29, 27)

/* SRC */
#define SRC_SCR_WARM_RESET_ENABLE       0
#define SRC_SCR_MASK_WDOG_RST           0x000F0000
#define SRC_SCR_WDOG_NOTMASKED          (0xA << 16)

#else
#error "CFG_MX6/7 not defined"
#endif

/* SNVS */
#define SNVS_HPLR			0x00000000
#define SNVS_HPCOMR			0x00000004
#define SNVS_HPCR			0x00000008
#define SNVS_HPSICR			0x0000000C
#define SNVS_HPSVCR			0x00000010
#define SNVS_HPSR			0x00000014
#define SNVS_HPSVSR			0x00000018
#define SNVS_HPHACIVR			0x0000001C
#define SNVS_HPHACR			0x00000020
#define SNVS_HPRTCMR			0x00000024
#define SNVS_HPRTCLR			0x00000028
#define SNVS_HPTAMR			0x0000002C
#define SNVS_HPTALR			0x00000030
#define SNVS_LPLR			0x00000034
#define SNVS_LPCR			0x00000038
#define SNVS_LPMKCR			0x0000003C
#define SNVS_LPSVCR			0x00000040
#define SNVS_LPTGFCR			0x00000044
#define SNVS_LPTDCR			0x00000048
#define SNVS_LPSR			0x0000004C
#define SNVS_LPSRTCMR			0x00000050
#define SNVS_LPSRTCLR			0x00000054
#define SNVS_LPTAR			0x00000058
#define SNVS_LPSMCMR			0x0000005C
#define SNVS_LPSMCLR			0x00000060
#define SNVS_LPPGDR			0x00000064
#define SNVS_LPGPR			0x00000068
#define SNVS_LPZMK			0x0000006C
#define SNVS_HPVIDR1			0x00000BF8
#define SNVS_HPVIDR2			0x00000BFC

#define SNVS_LPPGDR_INIT		0x41736166

#define SNVS_LPCR_DP_EN			(1u << 5)
#define SNVS_LPCR_TOP			(1u << 6)

#define SNVS_LPSR_PGD			(1u << 3)

/* Watchdog */

#define WDOG1_WCR			0x00000000
#define WDOG1_WSR			0x00000002
#define WDOG1_WRSR			0x00000004
#define WDOG1_WICR			0x00000006
#define WDOG1_WMCR			0x00000008

#define WDOG_WCR_WDE			(1u << 2)
#define WDOG_WCR_WDT			(1u << 3)
#define WDOG_WCR_SRS			(1u << 4)
#define WDOG_WCR_WDA			(1u << 5)

#define WDOG_WSR_FEED1			0x5555
#define WDOG_WSR_FEED2			0xAAAA

#define IOMUXC_GPR4_OFFSET	0x10
#define IOMUXC_GPR5_OFFSET	0x14
#define ARM_WFI_STAT_MASK(n)	BIT(n)

#define ARM_WFI_STAT_MASK_7D(n)	BIT(25 + ((n) & 1))

#define SRC_SCR				0x000
#define SRC_GPR1			0x020
#define SRC_GPR2			0x024
#define SRC_SCR_CORE1_RST_OFFSET	14
#define SRC_SCR_CORE1_ENABLE_OFFSET	22
#define SRC_SCR_CPU_ENABLE_ALL		SHIFT_U32(0x7, 22)

#define SRC_GPR1_MX7			0x074
#define SRC_A7RCR0			0x004
#define SRC_A7RCR1			0x008
#define SRC_A7RCR0_A7_CORE_RESET0_OFFSET	0
#define SRC_A7RCR1_A7_CORE1_ENABLE_OFFSET	1

#define WCR_OFF				0

#define OFFSET_DIGPROG			0x260
#define OFFSET_DIGPROG_IMX6SL		0x280
#define OFFSET_DIGPROG_IMX7D		0x800

/* GPC V2 */
#define GPC_PGC_C1			0x840
#define GPC_PGC_C1_PUPSCR		0x844

#define GPC_PGC_PCG_MASK		BIT(0)

#define GPC_CPU_PGC_SW_PUP_REQ		0xf0
#define GPC_PU_PGC_SW_PUP_REQ		0xf8
#define GPC_CPU_PGC_SW_PDN_REQ		0xfc
#define GPC_PU_PGC_SW_PDN_REQ		0x104
#define GPC_PGC_SW_PDN_PUP_REQ_CORE1_MASK BIT(1)
#endif
