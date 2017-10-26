/*
 * Copyright (C) 2009
 * Guennadi Liakhovetski, DENX Software Engineering, <lg@denx.de>
 *
 * Copyright (C) 2011
 * Stefano Babic, DENX Software Engineering, <sbabic@denx.de>
 *
 * Copyright (C) 2017
 * Microsoft - adapted from u-boot\drivers\gpio\mxc_gpio.c
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <drivers/imx_gpio.h>
#include <mm/core_memprot.h>
#include <io.h>
#include <assert.h>

#define readl(va) 	            read32((vaddr_t)(va))
#define writel(value, va) 	    write32((value), (vaddr_t)(va))

/* GPIO registers */
struct gpio_regs {
	unsigned int gpio_dr;	/* data */
	unsigned int gpio_dir;	/* direction */
	unsigned int gpio_psr;	/* pad satus */
};

/* GPIO1 virtual address */
static vaddr_t gpio_port0_base;

register_phys_mem(MEM_AREA_IO_SEC, IMX_GPIO1_BASE_ADDR, CORE_MMU_DEVICE_SIZE);

static struct gpio_regs *mxc_gpio_get_reg_base(unsigned int gpio_pin)
{
    unsigned int port = (gpio_pin / IMX_GPIO_REGISTER_BITS);
    assert(port < IMX_GPIO_PORTS);
    return (struct gpio_regs *)(gpio_port0_base + (port * IMX_GPIO_PORT_GRANULARITY));
}

static unsigned int mxc_gpio_get_pin_index(unsigned int gpio_pin)
{
    return (gpio_pin & (IMX_GPIO_REGISTER_BITS - 1));
}

static void mxc_gpio_set_direction(unsigned int gpio_pin, enum gpio_dir direction)
{
    unsigned int regValue;
    unsigned int pinIndex = mxc_gpio_get_pin_index(gpio_pin);
    struct gpio_regs *regs = mxc_gpio_get_reg_base(gpio_pin);

    DMSG("Base address = %p, pinIndex = %u, direction = %u", 
        (void *)regs, pinIndex, (unsigned int)direction);

    regValue = readl(&regs->gpio_dir);

    switch (direction) {
        case GPIO_DIR_OUT:
            regValue |= 1 << pinIndex;
            break;
        case GPIO_DIR_IN:
            regValue &= ~(1 << pinIndex);
            break;
        default:
            assert(false);
            return;
    }
	
    writel(regValue, &regs->gpio_dir);
}

static void mxc_gpio_set_value(unsigned int gpio_pin, enum gpio_level value)
{
    unsigned int regValue;
    unsigned int pinIndex = mxc_gpio_get_pin_index(gpio_pin);
    struct gpio_regs *regs = mxc_gpio_get_reg_base(gpio_pin);

    DMSG("Base address = %p, pinIndex = %u, value = %u", 
        (void *)regs, pinIndex, (unsigned int)value);

    regValue = readl(&regs->gpio_dr);
    if (value == GPIO_LEVEL_HIGH)
        regValue |= 1 << pinIndex;
    else
        regValue &= ~(1 << pinIndex);

    writel(regValue, &regs->gpio_dr);
}

static enum gpio_level mxc_gpio_get_value(unsigned int gpio_pin)
{
    unsigned int val;
    unsigned int pinIndex = mxc_gpio_get_pin_index(gpio_pin);
    struct gpio_regs *regs = mxc_gpio_get_reg_base(gpio_pin);

    DMSG("Base address = %p, pinIndex = %u", (void *)regs, pinIndex);

    val = (readl(&regs->gpio_psr) >> pinIndex) & 0x01;

    if (val)
        return GPIO_LEVEL_HIGH;
    else
        return GPIO_LEVEL_LOW;
}

static const struct gpio_ops mxc_gpio_ops = {
	.get_direction = NULL,                      /* not implemented yet */
	.set_direction = mxc_gpio_set_direction,
	.get_value = mxc_gpio_get_value,
	.set_value = mxc_gpio_set_value,
	.get_interrupt = NULL,                      /* not implemented yet */
	.set_interrupt = NULL,                      /* not implemented yet */
};

void mxc_gpio_init(struct gpio_ops *ops)
{
    gpio_port0_base = (vaddr_t)phys_to_virt_io(IMX_GPIO1_BASE_ADDR);
    *ops = mxc_gpio_ops;
}
