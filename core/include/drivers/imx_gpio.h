/*
 * Copyright (c) 2017 Microsoft
 */

#ifndef __IMX_GPIO_H__
#define __IMX_GPIO_H__

#include <gpio.h>

/**
 * Initialize GPIO.
 *
 * @param ops	On output, contains pointers to GPIO APIs.
 */
void mxc_gpio_init(struct gpio_ops *ops);

#endif	/* __IMX_GPIO_H__ */
