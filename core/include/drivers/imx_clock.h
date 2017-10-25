/*
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc.
 *
 * Copyright (C) 2017 Microsoft - adapted from:
 *
 * u-boot\arch\arm\cpu\armv7\mx6\clock.c
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef IMX_CLOCK_H
#define IMX_CLOCK_H

#include <stdbool.h>

unsigned int get_cspi_clk(void);

bool enable_cspi_clk(unsigned int spiBus);

#endif /* IMX_CLOCK_H */
