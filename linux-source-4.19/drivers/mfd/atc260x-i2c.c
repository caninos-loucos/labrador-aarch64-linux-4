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
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/mfd/atc260x/atc260x.h>
#include <linux/dma-mapping.h>
#include "atc260x-core.h"

static struct regmap_config atc2603c_i2c_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.cache_type = REGCACHE_NONE,
	.max_register = ATC2603C_CHIP_VER,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.val_format_endian = REGMAP_ENDIAN_BIG,
};

static struct regmap_config atc2609a_i2c_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.cache_type = REGCACHE_NONE,
	.max_register = ATC2609A_CHIP_VER,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.val_format_endian = REGMAP_ENDIAN_BIG,
};

static const struct mfd_cell sc_atc2603c_cells[] = {
	{
	.name = "atc2603c-audio",
	.of_compatible = "actions,atc2603c-audio",
	},
};

static int atc260x_i2c_probe(struct i2c_client *i2c,
                             const struct i2c_device_id *id)
{
	struct atc260x_dev *atc260x;
	struct regmap_config *p_regmap_cfg;
	int ret;
	
	ret = i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C);
	
	if (!ret)
	{
		dev_err(&i2c->dev, "i2c bus is not functional.\n");
		return -EFAULT;
	}
	
	atc260x = devm_kzalloc(&i2c->dev, sizeof(*atc260x), GFP_KERNEL);
	
	if (atc260x == NULL)
	{
		dev_err(&i2c->dev, "could not allocate memory.\n");
		return -ENOMEM;
	}
	
	atc260x->ic_type = id->driver_data;
	
	switch (atc260x->ic_type)
	{
	case ATC260X_ICTYPE_2603C:
		p_regmap_cfg = &atc2603c_i2c_regmap_config;
		break;
	case ATC260X_ICTYPE_2609A:
		p_regmap_cfg = &atc2609a_i2c_regmap_config;
		break;
	default:
		dev_err(&i2c->dev, "invalid ic type.\n");
		return -ENODEV;
	}
	
	atc260x->dev = &i2c->dev;
	
	dma_coerce_mask_and_coherent(atc260x->dev, DMA_BIT_MASK(32));
	
	atc260x->regmap = devm_regmap_init_i2c(i2c, p_regmap_cfg);
	
	if (IS_ERR(atc260x->regmap))
	{
		ret = PTR_ERR(atc260x->regmap);
		dev_err(atc260x->dev, "could not allocate register map: %d\n", ret);
		return ret;
	}
	
	i2c_set_clientdata(i2c, atc260x);
	
	ret = devm_mfd_add_devices(atc260x->dev, 0, sc_atc2603c_cells,
			      ARRAY_SIZE(sc_atc2603c_cells),
			      NULL, 0, NULL);
	
	if (ret)
	{
		dev_err(atc260x->dev, "failed to add children devices: %d\n", ret);
		return ret;
	}
	
	return 0;
}

static int atc260x_i2c_remove(struct i2c_client *i2c)
{
	return 0;
}

const struct i2c_device_id atc260x_i2c_id[] = {
	{ "atc2603c", ATC260X_ICTYPE_2603C },
	{ "atc2609a", ATC260X_ICTYPE_2609A },
    {}
};
MODULE_DEVICE_TABLE(i2c, atc260x_i2c_id);

static struct i2c_driver atc260x_i2c_driver = {
	.driver = {
		.name = "atc260x-i2c",
	},
	.id_table = atc260x_i2c_id,
	.probe = atc260x_i2c_probe,
	.remove = atc260x_i2c_remove,
};

module_i2c_driver(atc260x_i2c_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("ATC260x i2c device driver.");
MODULE_LICENSE("GPL");

