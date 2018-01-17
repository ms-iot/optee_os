/*
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc.
 *
 * Copyright (C) 2017 Microsoft - adapted from:
 *
 * u-boot\arch\arm\include\asm\arch-mx6\crm_regs.h
 * u-boot\arch\arm\cpu\armv7\mx6\clock.c
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <drivers/imx_clock.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <io.h>

#define __raw_readl(pa) read32((vaddr_t)phys_to_virt_io_check_mmu((paddr_t)(pa)))

#ifdef CONFIG_SYS_MX6_HCLK
#define MXC_HCLK	CONFIG_SYS_MX6_HCLK
#else
#define MXC_HCLK	24000000
#endif

#define MXC_CCM_CSCDR2_ECSPI_CLK_PODF_MASK		(0x3F << 19)
#define MXC_CCM_CSCDR2_ECSPI_CLK_PODF_OFFSET    19

#define BM_ANADIG_PLL_528_DIV_SELECT            0x00000001
#define BM_ANADIG_PLL_ENET_DIV_SELECT           0x00000003
#define BM_ANADIG_USB1_PLL_480_CTRL_DIV_SELECT  0x00000003
#define BM_ANADIG_PLL_SYS_DIV_SELECT            0x0000007F

struct mxc_ccm_reg {
	unsigned int ccr;	/* 0x0000 */
	unsigned int ccdr;
	unsigned int csr;
	unsigned int ccsr;
	unsigned int cacrr;	/* 0x0010*/
	unsigned int cbcdr;
	unsigned int cbcmr;
	unsigned int cscmr1;
	unsigned int cscmr2;	/* 0x0020 */
	unsigned int cscdr1;
	unsigned int cs1cdr;
	unsigned int cs2cdr;
	unsigned int cdcdr;	/* 0x0030 */
	unsigned int chsccdr;
	unsigned int cscdr2;
	unsigned int cscdr3;
	unsigned int cscdr4;	/* 0x0040 */
	unsigned int resv0;
	unsigned int cdhipr;
	unsigned int cdcr;
	unsigned int ctor;	/* 0x0050 */
	unsigned int clpcr;
	unsigned int cisr;
	unsigned int cimr;
	unsigned int ccosr;	/* 0x0060 */
	unsigned int cgpr;
	unsigned int CCGR0;
	unsigned int CCGR1;
	unsigned int CCGR2;	/* 0x0070 */
	unsigned int CCGR3;
	unsigned int CCGR4;
	unsigned int CCGR5;
	unsigned int CCGR6;	/* 0x0080 */
	unsigned int CCGR7;
	unsigned int cmeor;
	unsigned int resv[0xfdd];
	unsigned int analog_pll_sys;			/* 0x4000 */
	unsigned int analog_pll_sys_set;
	unsigned int analog_pll_sys_clr;
	unsigned int analog_pll_sys_tog;
	unsigned int analog_usb1_pll_480_ctrl;		/* 0x4010 */
	unsigned int analog_usb1_pll_480_ctrl_set;
	unsigned int analog_usb1_pll_480_ctrl_clr;
	unsigned int analog_usb1_pll_480_ctrl_tog;
	unsigned int analog_reserved0[4];
	unsigned int analog_pll_528;			/* 0x4030 */
	unsigned int analog_pll_528_set;
	unsigned int analog_pll_528_clr;
	unsigned int analog_pll_528_tog;
	unsigned int analog_pll_528_ss;			/* 0x4040 */
	unsigned int analog_reserved1[3];
	unsigned int analog_pll_528_num;			/* 0x4050 */
	unsigned int analog_reserved2[3];
	unsigned int analog_pll_528_denom;		/* 0x4060 */
	unsigned int analog_reserved3[3];
	unsigned int analog_pll_audio;			/* 0x4070 */
	unsigned int analog_pll_audio_set;
	unsigned int analog_pll_audio_clr;
	unsigned int analog_pll_audio_tog;
	unsigned int analog_pll_audio_num;		/* 0x4080*/
	unsigned int analog_reserved4[3];
	unsigned int analog_pll_audio_denom;		/* 0x4090 */
	unsigned int analog_reserved5[3];
	unsigned int analog_pll_video;			/* 0x40a0 */
	unsigned int analog_pll_video_set;
	unsigned int analog_pll_video_clr;
	unsigned int analog_pll_video_tog;
	unsigned int analog_pll_video_num;		/* 0x40b0 */
	unsigned int analog_reserved6[3];
	unsigned int analog_pll_video_denom;		/* 0x40c0 */
	unsigned int analog_reserved7[7];
	unsigned int analog_pll_enet;			/* 0x40e0 */
	unsigned int analog_pll_enet_set;
	unsigned int analog_pll_enet_clr;
	unsigned int analog_pll_enet_tog;
	unsigned int analog_pfd_480;			/* 0x40f0 */
	unsigned int analog_pfd_480_set;
	unsigned int analog_pfd_480_clr;
	unsigned int analog_pfd_480_tog;
	unsigned int analog_pfd_528;			/* 0x4100 */
	unsigned int analog_pfd_528_set;
	unsigned int analog_pfd_528_clr;
	unsigned int analog_pfd_528_tog;
};

enum pll_clocks {
	PLL_SYS,	/* System PLL */
	PLL_BUS,	/* System Bus PLL*/
	PLL_USBOTG,	/* OTG USB PLL */
	PLL_ENET,	/* ENET PLL */
};

register_phys_mem(MEM_AREA_IO_SEC, CCM_BASE, CORE_MMU_DEVICE_SIZE);

struct mxc_ccm_reg *imx_ccm = (struct mxc_ccm_reg *)CCM_BASE;

static unsigned int decode_pll(enum pll_clocks pll, unsigned int infreq)
{
	unsigned int div;

	switch (pll) {
	case PLL_SYS:
		div = __raw_readl(&imx_ccm->analog_pll_sys);
		div &= BM_ANADIG_PLL_SYS_DIV_SELECT;

		return infreq * (div >> 1);
	case PLL_BUS:
		div = __raw_readl(&imx_ccm->analog_pll_528);
		div &= BM_ANADIG_PLL_528_DIV_SELECT;

		return infreq * (20 + (div << 1));
	case PLL_USBOTG:
		div = __raw_readl(&imx_ccm->analog_usb1_pll_480_ctrl);
		div &= BM_ANADIG_USB1_PLL_480_CTRL_DIV_SELECT;

		return infreq * (20 + (div << 1));
	case PLL_ENET:
		div = __raw_readl(&imx_ccm->analog_pll_enet);
		div &= BM_ANADIG_PLL_ENET_DIV_SELECT;
		if (div == 0) return 25000000;
		else return (div == 3 ?   125000000 : 25000000 * (div << 1));
	default:
		return 0;
	}
	/* NOTREACHED */
}

unsigned int get_cspi_clk(void)
{
	unsigned int reg, cspi_podf;

	reg = __raw_readl(&imx_ccm->cscdr2);
	reg &= MXC_CCM_CSCDR2_ECSPI_CLK_PODF_MASK;
	cspi_podf = reg >> MXC_CCM_CSCDR2_ECSPI_CLK_PODF_OFFSET;

	return decode_pll(PLL_USBOTG, MXC_HCLK) / (8 * (cspi_podf + 1));
}

bool enable_cspi_clk(unsigned int spiBus)
{
    uint32_t val;
    struct mxc_ccm_reg *ccm = (struct mxc_ccm_reg *)
	phys_to_virt(CCM_BASE, MEM_AREA_IO_SEC);

    /* Just ECSPI2 is currently supported */
    if (spiBus != 1) {
        EMSG("Unsupported spiBus = %u", spiBus);
        return false;
    }

    FMSG("Enabling ECSPI2 clock");
    val = read32((vaddr_t)&ccm->CCGR1);
    val |=  IMX_CCM_CCGR1_ECSPI2_CLK_ENABLED;
    write32(val, (vaddr_t)&ccm->CCGR1);

    return true;
}
