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
#include <linux/gpio.h>

#define BANK_LABEL_LEN (16U)
#define GPIO_PER_BANK  (32U)

#define GPIOA(x) (  0U + (x))
#define GPIOB(x) ( 32U + (x))
#define GPIOC(x) ( 64U + (x))
#define GPIOD(x) ( 96U + (x))
#define GPIOE(x) (128U + (x))

#define GPIO_OUTEN(x) (((x) * 0xC) + 0x00)
#define GPIO_INEN(x)  (((x) * 0xC) + 0x04)
#define GPIO_DAT(x)   (((x) * 0xC) + 0x08)

#define MFP_CTL0      (0x40)
#define MFP_CTL1      (0x44)
#define MFP_CTL2      (0x48)
#define MFP_CTL3      (0x4C)
#define PAD_PULLCTL0  (0x60)
#define PAD_PULLCTL1  (0x64)
#define PAD_PULLCTL2  (0x68)
#define PAD_ST0       (0x6C)
#define PAD_ST1       (0x70)
#define PAD_CTL       (0x74)
#define PAD_DRV0      (0x80)
#define PAD_DRV1      (0x84)
#define PAD_DRV2      (0x88)

/* CTLR */
#define GPIO_CTLR_PENDING        (0x1 << 0)
#define GPIO_CTLR_ENABLE         (0x1 << 1)
#define GPIO_CTLR_SAMPLE_CLK     (0x1 << 2)
#define	GPIO_CTLR_SAMPLE_CLK_32K (0x0 << 2)
#define	GPIO_CTLR_SAMPLE_CLK_24M (0x1 << 2)

/* TYPE */
#define GPIO_INT_TYPE_MASK    (0x3)
#define GPIO_INT_TYPE_HIGH    (0x0)
#define GPIO_INT_TYPE_LOW     (0x1)
#define GPIO_INT_TYPE_RISING  (0x2)
#define GPIO_INT_TYPE_FALLING (0x3)

/* pending mask for share intc_ctlr */
#define GPIO_CTLR_PENDING_MASK (0x42108421)

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
	char label[BANK_LABEL_LEN];
	volatile u32 *inen;
	volatile u32 *outen;
	volatile u32 *dat;
	raw_spinlock_t lock;
	int addr, npins;
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
