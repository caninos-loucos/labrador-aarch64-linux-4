/*
 * Copyright (c) 2019 LSI-TEC - Caninos Loucos
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/pm.h>
#include <linux/spi/spi.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/suspend.h>
#include <linux/mfd/core.h>
#include <linux/mfd/atc260x/atc260x.h>

#include "atc260x-core.h"

int atc260x_reg_read(struct atc260x_dev *atc260x, uint reg)
{
	uint data = -1;
	int ret;
	
	if (!atc260x) {
		return -EINVAL;
	}
	
	ret = regmap_read(atc260x->regmap, reg, &data);
	
	if (ret < 0) {
		return ret;
	}
	
	return data;
}
EXPORT_SYMBOL(atc260x_reg_read);

int atc260x_reg_write(struct atc260x_dev *atc260x, uint reg, u16 val)
{
	if (!atc260x) {
		return -EINVAL;
	}
	return regmap_write(atc260x->regmap, reg, val);
}
EXPORT_SYMBOL(atc260x_reg_write);

