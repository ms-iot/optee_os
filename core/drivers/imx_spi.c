/*
 * Copyright (C) 2008, Guennadi Liakhovetski <lg@denx.de>
 * Copyright (C) 2017, Microsoft.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <drivers/imx_clock.h>
#include <drivers/imx_gpio.h>
#include <drivers/imx_iomux.h>
#include <drivers/imx_spi.h>
#include <assert.h>
#include <io.h>
#include <string.h>
#include <utee_defines.h>

#define reg_read(va)            read32((vaddr_t)(va))
#define reg_write(va, value)    write32((value), (vaddr_t)(va))

struct cspi_regs {
	uint32_t rxdata;
	uint32_t txdata;
	uint32_t ctrl;
	uint32_t cfg;
	uint32_t intr;
	uint32_t dma;
	uint32_t stat;
	uint32_t period;
};

/* Start transfer */
static void mxc_spi_start(struct spi_chip *chip)
{
    struct mxc_spi_data *spiData = container_of(chip, struct mxc_spi_data, chip);

    if (!spiData->configured)
    {
        return;
    }

    spiData->gpioOps.set_value(spiData->gpio,
        spiData->ss_pol ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}

/* End transfer */
static void mxc_spi_end(struct spi_chip *chip)
{
    struct mxc_spi_data *spiData = container_of(chip, struct mxc_spi_data, chip);

    if (!spiData->configured) {
        return;
    }

    spiData->gpioOps.set_value(spiData->gpio, 
        spiData->ss_pol ? GPIO_LEVEL_LOW : GPIO_LEVEL_HIGH);
}

/* Configure registers based on spiData input */
static bool mxc_spi_cfg(struct mxc_spi_data *spiData)
{
	uint32_t clk_src;
	int reg_ctrl, reg_config;
	uint32_t ss_pol = 0, sclkpol = 0, sclkpha = 0, pre_div = 0, post_div = 0;
    bool success = true;
	struct cspi_regs *regs = (struct cspi_regs *)spiData->baseVA;

	/*
	 * Reset SPI and set all CSs to master mode, if toggling
	 * between slave and master mode we might see a glitch
	 * on the clock line
	 */
	reg_ctrl = MXC_CSPICTRL_MODE_MASK;
	reg_write(&regs->ctrl, reg_ctrl);
	reg_ctrl |=  MXC_CSPICTRL_EN;
	reg_write(&regs->ctrl, reg_ctrl);

    assert(spiData->maxHz > 0);
	clk_src = get_cspi_clk();

    if (clk_src > spiData->maxHz) {
		pre_div = (clk_src - 1) / spiData->maxHz;

        if (pre_div == 0) {
            post_div = 0;
        } else {
            post_div = ((sizeof(pre_div) * 8) - __builtin_clz(pre_div));
            if (post_div > 4) {
                post_div -= 4;
                if (post_div >= 16) {
                    EMSG("no divider for the freq: %u\n", spiData->maxHz);
                    success = false;
                    goto done;
                }
                pre_div >>= post_div;

            } else {
                post_div = 0;
            }
        }
	}

	DMSG("pre_div = %d, post_div=%d\n", pre_div, post_div);
	reg_ctrl = (reg_ctrl & ~MXC_CSPICTRL_SELCHAN(3)) |
		MXC_CSPICTRL_SELCHAN(spiData->channel);
	reg_ctrl = (reg_ctrl & ~MXC_CSPICTRL_PREDIV(0x0F)) |
		MXC_CSPICTRL_PREDIV(pre_div);
	reg_ctrl = (reg_ctrl & ~MXC_CSPICTRL_POSTDIV(0x0F)) |
		MXC_CSPICTRL_POSTDIV(post_div);

	/* We need to disable SPI before changing registers */
	reg_ctrl &= ~MXC_CSPICTRL_EN;

	if (spiData->mode & MXC_SPI_CS_HIGH) {
		ss_pol = 1;
    }

	if (spiData->mode & MXC_SPI_CPOL) {
		sclkpol = 1;
    }

	if (spiData->mode & MXC_SPI_CPHA) {
		sclkpha = 1;
    }

	reg_config = reg_read(&regs->cfg);

	reg_config = (reg_config & ~(1 << (spiData->channel + MXC_CSPICON_SSPOL))) |
		(ss_pol << (spiData->channel + MXC_CSPICON_SSPOL));
	reg_config = (reg_config & ~(1 << (spiData->channel + MXC_CSPICON_POL))) |
		(sclkpol << (spiData->channel + MXC_CSPICON_POL));
	reg_config = (reg_config & ~(1 << (spiData->channel + MXC_CSPICON_PHA))) |
		(sclkpha << (spiData->channel + MXC_CSPICON_PHA));

	reg_write(&regs->ctrl, reg_ctrl);
	reg_write(&regs->cfg, reg_config);

	/* Save config register and control register */
	spiData->ctrl_reg = reg_ctrl;
	spiData->cfg_reg = reg_config;

	/* Clear interrupt reg */
	reg_write(&regs->intr, 0);
	reg_write(&regs->stat, MXC_CSPICTRL_TC | MXC_CSPICTRL_RXOVF);

done:
    return success;
}

/* Transfer up to MXC_MAX_SPI_BYTES size */
static void mxc_spi_xchg_single(
    struct spi_chip *chip,
	const uint8_t *dout,
    uint8_t *din,
    uint32_t sizeBytes
    )
{
    const uint8_t *byteAddress;
    struct mxc_spi_data *spiData = container_of(chip, struct mxc_spi_data, chip);
    struct cspi_regs *regs = (struct cspi_regs *)spiData->baseVA;

    /* Always transfer full "words" (i.e., full uint32_t values) */
    uint32_t wordValue;
    const uint32_t sizeWords = sizeBytes / sizeof(wordValue);
    const uint32_t unalignedBytes = (sizeBytes % sizeof(wordValue));

    assert(sizeBytes <= MXC_MAX_SPI_BYTES);

    spiData->ctrl_reg = (spiData->ctrl_reg &
    ~MXC_CSPICTRL_BITCOUNT(MXC_CSPICTRL_MAXBITS)) |
    MXC_CSPICTRL_BITCOUNT((sizeBytes * 8) - 1);

    reg_write(&regs->ctrl, spiData->ctrl_reg | MXC_CSPICTRL_EN);
    reg_write(&regs->cfg, spiData->cfg_reg);

    /* Clear interrupt register */
    reg_write(&regs->stat, MXC_CSPICTRL_TC | MXC_CSPICTRL_RXOVF);

    /* Write into FIFO the unaligned bytes */
    if (unalignedBytes != 0) {
        wordValue = 0;

        for (uint32_t byteIndex = 0; byteIndex < unalignedBytes; byteIndex++) {
            wordValue = ((wordValue << 8) | *dout);
            dout++;
        }

        reg_write(&regs->txdata, wordValue);
    }

    /* Write into FIFO the remaining full words */
    for (uint32_t wordIndex = 0; wordIndex < sizeWords; wordIndex++) {
        wordValue = 0;

        for (uint32_t byteIndex = 0; byteIndex < sizeof(wordValue); byteIndex++) {
            wordValue = ((wordValue << 8) | *dout);
            dout++;
        }

        reg_write(&regs->txdata, wordValue);
    }

    /* FIFO is written, now start the transfer */
    reg_write(&regs->ctrl, spiData->ctrl_reg | MXC_CSPICTRL_EN | MXC_CSPICTRL_XCH);

    /* Wait until the TC (Transfer completed) bit is set */
    for (;;) {
        uint32_t currentStatus = reg_read(&regs->stat);
        if (currentStatus & MXC_CSPICTRL_TC) {
            break;
        }
    }

    /* Transfer completed, clear any pending request */
    reg_write(&regs->stat, MXC_CSPICTRL_TC | MXC_CSPICTRL_RXOVF);

    /* Read the unaligned bytes */
    if (unalignedBytes != 0) {
        wordValue = reg_read(&regs->rxdata);
        wordValue = TEE_U32_BSWAP(wordValue);

        byteAddress = (const uint8_t *)&wordValue;
        byteAddress += sizeof(wordValue) - unalignedBytes;
		
        for (uint32_t byteIndex = 0; byteIndex < unalignedBytes; byteIndex++) {
            *din = *byteAddress;
            din++;
            byteAddress++;
        }
    }

    /* Read the remaining full words */
    for (uint32_t wordIndex = 0; wordIndex < sizeWords; wordIndex++) {
        wordValue = reg_read(&regs->rxdata);
        wordValue = TEE_U32_BSWAP(wordValue);

        byteAddress = (const uint8_t *)&wordValue;

        for (uint32_t byteIndex = 0; byteIndex < sizeof(wordValue); byteIndex++)
        {
            *din = *byteAddress;
            din++;
            byteAddress++;
        }
    }
}

/* Transfer num_pkts bytes */
static enum spi_result mxc_spi_txrx8(
    struct spi_chip *chip,
    uint8_t *wdat,
	uint8_t *rdat,
    size_t num_pkts
    )
{
	size_t blk_size;
    struct mxc_spi_data *spiData = container_of(chip, struct mxc_spi_data, chip);

    if (!spiData->configured) {
        return SPI_ERR_CFG;
    }

	while (num_pkts > 0) {
		if (num_pkts < MXC_MAX_SPI_BYTES) {
			blk_size = num_pkts;
        }
		else {
			blk_size = MXC_MAX_SPI_BYTES;
        }

		mxc_spi_xchg_single(chip, wdat, rdat, blk_size);

		wdat += blk_size;
		rdat += blk_size;
		num_pkts -= blk_size;
	}

	return SPI_OK;
}

static void mxc_spi_claim_bus(struct mxc_spi_data *spiData)
{
    struct cspi_regs *regs = (struct cspi_regs *)spiData->baseVA;

    reg_write(&regs->rxdata, 1);
	reg_write(&regs->ctrl, spiData->ctrl_reg);
	reg_write(&regs->period, MXC_CSPIPERIOD_32KHZ);
	reg_write(&regs->intr, 0);
}

static void mxc_spi_configure(struct spi_chip *chip)
{
    int32_t gpioPort, gpioPin;
    bool success;
    struct mxc_spi_data *spiData = container_of(chip, struct mxc_spi_data, chip);

    mxc_gpio_init(&spiData->gpioOps);
    
    success = enable_cspi_clk(spiData->bus) &&
        imx_iomux_configure_spi(spiData->bus, &gpioPort, &gpioPin);

    if (!success) {
        goto done;
    }

    assert(gpioPort < IMX_GPIO_PORTS);
    assert(gpioPin < IMX_GPIO_REGISTER_BITS);
    spiData->gpio = (gpioPort * IMX_GPIO_REGISTER_BITS) + gpioPin;

    assert(spiData->bus < MXC_ECSPI_BUS_COUNT);
	spiData->baseVA = (vaddr_t)phys_to_virt_io(
        MXC_ECSPI1_BASE_ADDR + (spiData->bus * MXC_ECSPI_BUS_GRANULARITY));
	spiData->ss_pol = (spiData->mode & MXC_SPI_CS_HIGH) ? 1 : 0;

    spiData->gpioOps.set_direction(
        spiData->gpio,
        spiData->ss_pol ? GPIO_DIR_IN : GPIO_DIR_OUT);

	success = mxc_spi_cfg(spiData);

    if (!success) {
        goto done;
    }
    
    mxc_spi_claim_bus(spiData);

done:
    spiData->configured = success;
}

static const struct spi_ops mxc_spi_ops = {
	.configure = mxc_spi_configure,
	.start = mxc_spi_start,
	.txrx8 = mxc_spi_txrx8,
	.txrx16 = NULL, /* Not implemented yet */
	.end = mxc_spi_end,
};

enum spi_result mxc_spi_init(
    struct mxc_spi_data *spiData, 
    uint8_t spiBus,
    uint8_t channel,
    uint8_t mode,
    uint32_t maxHz
    )
{
    if (maxHz == 0) {
        EMSG("Incorrect maxHz = %u", maxHz);
        return SPI_ERR_CFG;
    }

    if (spiBus >= MXC_ECSPI_BUS_COUNT) {
        EMSG("Incorrect spiBus = %u", maxHz);
        return SPI_ERR_CFG;
    }

    memset(spiData, 0, sizeof(*spiData));

    spiData->bus = spiBus;
    spiData->channel = channel;
    spiData->mode = mode;
    spiData->maxHz = maxHz;
    spiData->chip.ops = &mxc_spi_ops;

    return SPI_OK;
}
