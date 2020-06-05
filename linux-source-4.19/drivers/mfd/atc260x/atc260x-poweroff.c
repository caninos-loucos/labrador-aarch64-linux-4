/*
 * Copyright (c) 2020 LSI-TEC - Caninos Loucos
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
#include <linux/pm.h>
#include <linux/of_device.h>
#include <asm/system_misc.h>
#include <linux/mfd/core.h>
#include <linux/mfd/atc260x/atc260x.h>

static struct atc260x_dev *pmic = NULL;

static void atc260x_poweroff(void)
{
	//
}

static void atc260x_restart(enum reboot_mode reboot_mode, const char *cmd)
{
	//
}

static int atc2603c_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	
	pmic = dev_get_drvdata(dev->parent);
	
	if (!pmic) {
		return -EINVAL;
	}
	
	pm_power_off = atc260x_poweroff;
	arm_pm_restart = atc260x_restart;
	
	return 0;
}

static int atc2603c_platform_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id atc2603c_poweroff_of_match[] = {
	{.compatible = "actions,atc2603c-poweroff",},
	{}
};
MODULE_DEVICE_TABLE(of, atc2603c_poweroff_of_match);

static struct platform_driver atc2603c_platform_driver = {
	.probe = atc2603c_platform_probe,
	.remove = atc2603c_platform_remove,
	.driver = {
		.name = "atc2603c-poweroff",
		.owner = THIS_MODULE,
		.of_match_table = atc2603c_poweroff_of_match,
	},
};

module_platform_driver(atc2603c_platform_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("ATC2603C poweroff driver");
MODULE_LICENSE("GPL");

