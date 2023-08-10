// SPDX-License-Identifier: GPL-2.0
/*
 * Pinctrl/GPIO driver for Caninos Labrador
 *
 * Copyright (c) 2022-2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2018-2020 LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _PINCTRL_CANINOS_H_
#define _PINCTRL_CANINOS_H_

#include <linux/kernel.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/gpio/driver.h>

#define BANK_LABEL_LEN (16U)
#define GPIO_PER_BANK  (32U)

#define GPIO_REGBASE(x) ((x) * 0xc)

#define GPIOA(x) (  0U + (x))
#define GPIOB(x) ( 32U + (x))
#define GPIOC(x) ( 64U + (x))
#define GPIOD(x) ( 96U + (x))
#define GPIOE(x) (128U + (x))

#define GPIO_AOUTEN  0x00
#define GPIO_AINEN   0x04
#define GPIO_ADAT    0x08
#define GPIO_BOUTEN  0x0C
#define GPIO_BINEN   0x10
#define GPIO_BDAT    0x14
#define GPIO_COUTEN  0x18
#define GPIO_CINEN   0x1C
#define GPIO_CDAT    0x20
#define GPIO_DOUTEN  0x24
#define GPIO_DINEN   0x28
#define GPIO_DDAT    0x2C
#define GPIO_EOUTEN  0x30
#define GPIO_EINEN   0x34
#define GPIO_EDAT    0x38
#define MFP_CTL0     0x40
#define MFP_CTL1     0x44
#define MFP_CTL2     0x48
#define MFP_CTL3     0x4C
#define PAD_PULLCTL0 0x60
#define PAD_PULLCTL1 0x64
#define PAD_PULLCTL2 0x68
#define PAD_ST0      0x6C
#define PAD_ST1      0x70
#define PAD_CTL      0x74
#define PAD_DRV0     0x80
#define PAD_DRV1     0x84
#define PAD_DRV2     0x88
#define INTC_GPIOCTL 0x204
#define INTC_GPIOCTL_GPIOX_EN(x) (1 << 5*x)
#define INTC_GPIOCTL_GPIOX_PD(x) (1 << (5*x + 1))
#define INTC_GPIOCTL_GPIOX_CLK(x) (1 << (5*x + 2))
#define INTC_GPIOX_PD(x) 	(0x208 + 0x8*x)
#define INTC_GPIOX_MSK(x) 	(0x20c + 0x8*x)
#define INTC_GPIOX_TYPE0(x) (0x230 + 0x8*x)
#define INTC_GPIOX_TYPE1(x)	(0x234 + 0x8*x)

/* TYPE */
#define GPIO_INT_TYPE_MASK    (0x3)
#define GPIO_INT_TYPE_HIGH    (0x0)
#define GPIO_INT_TYPE_LOW     (0x1)
#define GPIO_INT_TYPE_RISING  (0x2)
#define GPIO_INT_TYPE_FALLING (0x3)

#define to_caninos_gpio_chip(x) \
	container_of(x, struct caninos_gpio_chip, gpio_chip)

#define to_caninos_pinctrl(x) \
	(struct caninos_pinctrl*) pinctrl_dev_get_drvdata(x)

struct caninos_pinctrl;

struct caninos_group {
	const char *name;
	const unsigned *pins;
	unsigned num_pins;
};

struct caninos_pmx_func {
	const char *name;
	const char * const *groups;
	unsigned num_groups;
};

struct caninos_gpio_chip
{
	struct caninos_pinctrl *pinctrl;
	struct gpio_chip gpio_chip;
	raw_spinlock_t *lock;
	char label[BANK_LABEL_LEN];
	int addr, npins;
	unsigned int irq;
	u32 mask;
};

struct caninos_pinctrl
{
	struct device *dev;
	spinlock_t lock;
	void __iomem *base;
	struct clk *clk;
	struct pinctrl_desc pctl_desc;
	struct pinctrl_dev *pctl_dev;
	struct caninos_gpio_chip *banks;
	int nbanks;
	
	const struct caninos_pmx_func *functions;
	int nfuncs;
	
	const struct caninos_group *groups;
	int ngroups;
};

struct caninos_pinctrl_hwdiff
{
	const struct caninos_pmx_func *functions;
	int nfuncs;
	
	const struct caninos_group *groups;
	int ngroups;
	
	const struct pinctrl_pin_desc *pins;
	int npins;
	
	int (*hwinit)(struct caninos_pinctrl *pctl);
};

extern const struct caninos_pinctrl_hwdiff k7_pinctrl_hw;
extern const struct caninos_pinctrl_hwdiff k5_pinctrl_hw;

#endif
