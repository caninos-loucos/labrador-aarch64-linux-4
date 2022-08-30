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

#ifndef __MFD_ATC260X_H__
#define __MFD_ATC260X_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>

#include <linux/mfd/atc260x/regs_map_atc2603a.h>
#include <linux/mfd/atc260x/regs_map_atc2603c.h>
#include <linux/mfd/atc260x/regs_map_atc2609a.h>

enum {
	ATC260X_ICTYPE_2603A = 0,
	ATC260X_ICTYPE_2603C,
	ATC260X_ICTYPE_2609A,
	ATC260X_ICTYPE_CNT
};

enum {
	ATC260X_ICVER_A = 0,
	ATC260X_ICVER_B,
	ATC260X_ICVER_C,
	ATC260X_ICVER_D,
	ATC260X_ICVER_E,
	ATC260X_ICVER_F,
	ATC260X_ICVER_G,
	ATC260X_ICVER_H,
};

struct atc260x_dev;

extern int atc260x_reg_read(struct atc260x_dev *atc260x, uint reg);
extern int atc260x_reg_write(struct atc260x_dev *atc260x, uint reg, u16 val);

#endif /* __MFD_ATC260X_H__ */
