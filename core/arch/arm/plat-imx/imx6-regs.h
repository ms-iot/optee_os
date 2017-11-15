/*
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * All rights reserved.
 * Copyright (c) 2016, Wind River Systems.
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

#define SCU_BASE			0x00A00000
#define PL310_BASE			0x00A02000
#define SRC_BASE			0x020D8000
#define SRC_SCR				0x000
#define SRC_GPR1			0x020
#define SRC_SCR_CPU_ENABLE_ALL		SHIFT_U32(0x7, 22)
#define SRC_SCR_CORE1_RST_OFFSET	14
#define SRC_SCR_CORE1_ENABLE_OFFSET	22
#define GIC_BASE			0x00A00000
#define GICC_OFFSET			0x100
#define GICD_OFFSET			0x1000
#define GIC_CPU_BASE			(GIC_BASE + GICC_OFFSET)
#define GIC_DIST_BASE			(GIC_BASE + GICD_OFFSET)

#define UART1_BASE			0x02020000
#define UART2_BASE			0x021E8000
#define UART4_BASE			0x021F0000

#if defined(CFG_MX6Q) || defined(CFG_MX6D)
#define UART3_BASE			0x021EC000
#define UART5_BASE			0x021F4000
#endif

/* Central Security Unit register values */
#define CSU_BASE			0x021C0000
#define CSU_CSL_START			0x0
#define CSU_CSL_END			0xA0
#define CSU_CSL5			0x14
#define CSU_CSL16			0x40
#define CSU_ACCESS_ALL			0x00FF00FF
#define CSU_SETTING_LOCK		0x01000100

#define DRAM0_BASE			0x10000000
#define CAAM_BASE			0x00100000

#if defined(CFG_MX6Q)

#define AIPS1_ARB_BASE_ADDR         0x02000000
#define AIPS2_ARB_BASE_ADDR         0x02100000

#define ATZ1_BASE_ADDR              AIPS1_ARB_BASE_ADDR
#define ATZ2_BASE_ADDR              AIPS2_ARB_BASE_ADDR

#define AIPS1_OFF_BASE_ADDR         (ATZ1_BASE_ADDR + 0x80000)
#define AIPS2_OFF_BASE_ADDR         (ATZ2_BASE_ADDR + 0x80000)

#define IP2APB_TZASC1_BASE_ADDR     (AIPS2_OFF_BASE_ADDR + 0x50000)

#define IMX_IOMUXC_BASE             0x020E0000

#define CCM_BASE_ADDR               (AIPS1_OFF_BASE_ADDR + 0x44000)

#define IMX_GPIO1_BASE_ADDR         (AIPS1_OFF_BASE_ADDR + 0x1C000)
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

/* CCM */
#define IMX_CCM_CCR_BASE_ADDR       0x020C4000
#define IMX_CCM_CCGR1_BASE_ADDR     0x020C406C

#define IMX_CGR_CLK_ENABLED 0x3     /* Clock enabled except in STOP mode */
#define IMX_CCM_CCGR1_ECSPI2_CLK_SHIFT      2
#define IMX_CCM_CCGR1_ECSPI2_CLK_ENABLED    \
    (IMX_CGR_CLK_ENABLED << IMX_CCM_CCGR1_ECSPI2_CLK_SHIFT) /* ECSPI2 clock enabled */

#endif // #if defined(CFG_MX6Q)
