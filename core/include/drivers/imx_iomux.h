/** @file
*
*  Header defining IMX6 Muxing and GPIO Routines
*  Imports the correct SOC specific header based on CPU_IMX6DQ, CPU_IMX6SDL or CPU_IMX6SX definition.
*
*  Copyright (c), Microsoft Corporation. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/
#ifndef IMX_IOMUX_H
#define IMX_IOMUX_H

/* Initialize the iomux driver */
void imx_iomux_init(void);

/* Configure iomux pads for a given SPI bus */
bool imx_iomux_configure_spi(uint8_t spiBus, int32_t *bank, int32_t *pin);

#endif // IMX_IOMUX_H
