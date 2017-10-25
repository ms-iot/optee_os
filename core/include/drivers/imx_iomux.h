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
#ifndef _IMX_IOMUX_H_
#define _IMX_IOMUX_H_

void imx_iomux_init(void);
void imx_iomux_configure_spi2(int32_t *bank, int32_t *pin);

#endif // _IMX_IOMUX_H_
