/*
 * (C) Copyright 2001
 * Gerald Van Baren, Custom IDEAS, vanbaren@cideas.com.
 *
 * Copyright (C) 2017
 * Microsoft.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef IMX_SPI_H
#define IMX_SPI_H

#include <spi.h>
#include <gpio.h>

/* SPI mode flags */
#define	MXC_SPI_CPHA        0x01			/* clock phase */
#define	MXC_SPI_CPOL	    0x02			/* clock polarity */

enum MXC_SPI_MODE {
    MXC_SPI_MODE_0 = (0|0),			        /* (original MicroWire) */
    MXC_SPI_MODE_1 = (0|MXC_SPI_CPHA),
    MXC_SPI_MODE_2 = (MXC_SPI_CPOL|0),
    MXC_SPI_MODE_3 = (MXC_SPI_CPOL|MXC_SPI_CPHA),
};

#define	MXC_SPI_CS_HIGH	    0x04			/* CS active high */
#define	MXC_SPI_LSB_FIRST   0x08			/* per-word bits-on-wire */
#define	MXC_SPI_3WIRE	    0x10			/* SI/SO signals shared */
#define	MXC_SPI_LOOP	    0x20			/* loopback mode */
#define	MXC_SPI_SLAVE	    0x40			/* slave mode */
#define	MXC_SPI_PREAMBLE    0x80			/* Skip preamble bytes */

/*
 * All the standard SPI APIs receive as parameter a "struct spi_chip *" value.
 * Therefore, the spi_chip structure has to be wrapped by a larger structure
 * containing controller-specific data.
 */
struct mxc_spi_data {
    bool        configured;
    uint8_t     bus;
    uint8_t     channel;
    uint8_t     mode;
    uint8_t     ss_pol;
    uint32_t    maxHz;
    uint32_t  	ctrl_reg;
    uint32_t	cfg_reg;
    uint32_t    gpio;
    vaddr_t	    baseVA;

    /* GPIO helpers */
    struct gpio_ops gpioOps;

    /* Contains the address of standard SPI APIs */
    struct spi_chip	chip;
};

enum spi_result mxc_spi_init(
    struct mxc_spi_data *spiData, 
    uint8_t spiBus,
    uint8_t channel,
    uint8_t mode,
    uint32_t maxHz
    );

#endif /* IMX_SPI_H */
