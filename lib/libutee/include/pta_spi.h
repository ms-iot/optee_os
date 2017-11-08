/*
 * Copyright (C) Microsoft. All rights reserved.
 */

#ifndef __PTA_SPI_H
#define __PTA_SPI_H

/* PTA UUID: {72de577e-cf4d-430b-9994-7047af0f8452} */
#define PTA_SPI_UUID { 0x72de577e, 0xcf4d, 0x430b, { \
                       0x99, 0x94, 0x70, 0x47, 0xaf, 0x0f, 0x84, 0x52 } }

/* Command IDs supported by the SPI PTA */
enum PTA_SPI_COMMANDS
{
    PTA_SPI_COMMAND_INITIALIZE,
    PTA_SPI_COMMAND_TRANSFER_DATA,
};

/* PTA_SPI_COMMAND_TRANSFER_DATA flags */
enum PTA_SPI_TRANSFER_FLAGS
{
    /* Call spi_ops.start() before this data transfer */
    PTA_SPI_TRANSFER_FLAG_START = 0x1,

    /* Call spi_ops.end() after this data transfer */
    PTA_SPI_TRANSFER_FLAG_END   = 0x2,

    /* All supported PTA_SPI_COMMAND_TRANSFER_DATA flags */
    PTA_SPI_TRANSFER_ALL_FLAGS  = (PTA_SPI_TRANSFER_FLAG_START | \
        PTA_SPI_TRANSFER_FLAG_END)
};

/* SPI mode */
enum PTA_SPI_MODE
{
    PTA_SPI_MODE_0,
    PTA_SPI_MODE_1,
    PTA_SPI_MODE_2,
    PTA_SPI_MODE_3
};

#endif /* __PTA_SPI_H */
