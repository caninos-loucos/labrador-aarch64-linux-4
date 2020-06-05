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

#ifndef __MFD_ATC260X_CORE_H__
#define __MFD_ATC260X_CORE_H__

#include <linux/kernel.h>
#include <linux/mfd/atc260x/atc260x.h>

struct atc260x_dev
{
	struct device *dev;
	struct regmap *regmap;
	u8 ic_type;	/* see ATC260X_ICTYPE_2603A ... */
};

#endif				/* __MFD_ATC260X_CORE_H__ */
