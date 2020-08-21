// SPDX-License-Identifier: GPL-2.0
/*
 * Thermal Sensor Driver for Caninos Labrador
 * Copyright (c) 2018-2020 LSI-TEC - Caninos Loucos
 * Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
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

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include "caninos_thermal.h"

static int caninos_tsensdata_to_mcelsius(unsigned int tsens_data)
{
	long long temp1, temp2;
	
	temp1 = tsens_data + 10;
	
	temp2 = temp1 * temp1;
	
	temp1 = (11513 * temp1) - 4471272;
	
	temp2 = (56534 * temp2) / 10000;
	
	return (int)((temp1 + temp2) / 10);
}

static int caninos_bind(struct thermal_zone_device *thermal,
                        struct thermal_cooling_device *cdev)
{
	return 0;
}

static int caninos_unbind(struct thermal_zone_device *thermal,
                          struct thermal_cooling_device *cdev)
{
	return 0;
}

static int caninos_get_temp(struct thermal_zone_device *thermal, int *temp)
{
	return 0;
}

static int caninos_get_trend(struct thermal_zone_device *thermal, int trip,
                             enum thermal_trend *trend)
{
	return 0;
}

static int caninos_get_mode(struct thermal_zone_device *thermal,
                            enum thermal_device_mode *mode)
{
	return 0;
}

static int caninos_set_mode(struct thermal_zone_device *thermal,
                            enum thermal_device_mode mode)
{
	return 0;
}

static int caninos_get_trip_type(struct thermal_zone_device *thermal, int trip,
                                 enum thermal_trip_type *type)
{
	return 0;
}

static int caninos_get_trip_temp(struct thermal_zone_device *thermal, int trip,
                                 int *temp)
{
	return 0;
}

static int caninos_set_trip_temp(struct thermal_zone_device *thermal, int trip,
                                 int temp)
{
	return 0;
}

static int caninos_get_trip_hyst(struct thermal_zone_device *thermal, int trip,
                                 int *temp)
{
	return 0;
}

static int caninos_get_crit_temp(struct thermal_zone_device *thermal, int *temp)
{
	return 0;
}

static struct thermal_zone_device_ops caninos_thermal_ops = {
	.bind = caninos_bind,
	.unbind = caninos_unbind,
	.get_temp = caninos_get_temp,
	.get_trend = caninos_get_trend,
	.get_mode = caninos_get_mode,
	.set_mode = caninos_set_mode,
	.get_trip_type = caninos_get_trip_type,
	.get_trip_temp = caninos_get_trip_temp,
	.set_trip_temp = caninos_set_trip_temp,
	.get_trip_hyst = caninos_get_trip_hyst,
	.get_crit_temp = caninos_get_crit_temp,
};

static int caninos_tmu_probe(struct platform_device *pdev)
{
	struct caninos_tmu_data *data;
	
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	
	if (!data)
	{
		dev_err(&pdev->dev, "could not allocate driver structure\n");
		return -ENOMEM;
	}
	
	data->tmu_clk = devm_clk_get(&pdev->dev, "thermal_sensor");
	
	if (IS_ERR(data->tmu_clk))
	{
		dev_err(&pdev->dev, "could not get device clock\n");
		return PTR_ERR(data->tmu_clk);
	}
	
	data->base = of_iomap(pdev->dev.of_node, 0);
	
	if (IS_ERR(data->base))
	{
		dev_err(&pdev->dev, "could not map device registers\n");
		return PTR_ERR(data->base);
	}
	
	platform_set_drvdata(pdev, data);
	
	return 0;
}

static int caninos_tmu_remove(struct platform_device *pdev)
{
	struct caninos_tmu_data *data = platform_get_drvdata(pdev);
	
	if(data)
	{
		iounmap(data->base);
	}
	
	return 0;
}

static const struct of_device_id caninos_tmu_match[] = {
	{ .compatible = "caninos,k7-tmu" },
	{ }
};

MODULE_DEVICE_TABLE(of, caninos_tmu_match);

static struct platform_driver caninos_tmu_driver = {
	.probe = caninos_tmu_probe,
	.remove = caninos_tmu_remove,
	.driver = {
		.name = "caninos-tmu",
		.of_match_table = caninos_tmu_match,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(caninos_tmu_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("Caninos Labrador SBCs Thermal Sensor Driver");
MODULE_LICENSE("GPL v2");

