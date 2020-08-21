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

#include "caninos_thermal.h"

static long caninos_tsensdata_to_mcelsius(unsigned int tsens_data)
{
	long long temp1, temp2;
	
	temp1 = tsens_data + 10;
	
	temp2 = (temp1 * temp1);
	
	temp1 = (11513 * temp1) - 4471272;
	
	temp2 = (56534 * temp2) / 10000;
	
	return (long)((temp1 + temp2) / 10);
}

static int caninos_tmu_probe(struct platform_device *pdev)
{
	return 0;
}

static int caninos_tmu_remove(struct platform_device *pdev)
{
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

