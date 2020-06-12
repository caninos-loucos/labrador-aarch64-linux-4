// SPDX-License-Identifier: GPL-2.0
/*
 * Caninos Labrador DWC3 specific glue layer
 * Copyright (c) 2019-2020 LSI-TEC - Caninos Loucos
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/delay.h>

struct dwc3_caninos
{
	struct device *dev;
	struct reset_control *reset;
	struct clk *clk_usb3_480mpll0;
	struct clk *clk_usb3_480mphy0;
	struct clk *clk_usb3_5gphy;
	struct clk *clk_usb3_cce;
};

static int dwc3_caninos_probe(struct platform_device *pdev)
{
	struct dwc3_caninos	*simple;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int	ret;
	
	simple = devm_kzalloc(dev, sizeof(*simple), GFP_KERNEL);
	
	if (!simple) {
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, simple);
	simple->dev = dev;
	
	simple->reset = devm_reset_control_get(dev, "usb3");
	
	if (IS_ERR(simple->reset))
	{
		dev_err(dev, "failed to get device reset\n");
		return -EINVAL;
	}
	
	simple->clk_usb3_480mpll0 = devm_clk_get(dev, "usb3_480mpll0");
	
	if (IS_ERR(simple->clk_usb3_480mpll0))
	{
		dev_err(dev, "failed to get clk_usb3_480mpll0\n");
		return -EINVAL;
	}

	simple->clk_usb3_480mphy0 = devm_clk_get(dev, "usb3_480mphy0");
	
	if (IS_ERR(simple->clk_usb3_480mphy0))
	{
		dev_err(dev, "failed to get usb3_480mphy0\n");
		return -EINVAL;
	}

	simple->clk_usb3_5gphy = devm_clk_get(dev, "usb3_5gphy");
	
	if (IS_ERR(simple->clk_usb3_5gphy))
	{
		dev_err(dev, "failed to get usb3_5gphy\n");
		return -EINVAL;
	}

	simple->clk_usb3_cce = devm_clk_get(dev, "usb3_cce");
	
	if (IS_ERR(simple->clk_usb3_cce))
	{
		dev_err(dev, "failed to get usb3_cce\n");
		return -EINVAL;
	}
	
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	
	clk_prepare_enable(simple->clk_usb3_480mpll0);
	clk_prepare_enable(simple->clk_usb3_480mphy0);
	clk_prepare_enable(simple->clk_usb3_5gphy);
	clk_prepare_enable(simple->clk_usb3_cce);
	
	reset_control_assert(simple->reset);
	udelay(100);
	reset_control_deassert(simple->reset);
	
	ret = of_platform_populate(np, NULL, NULL, dev);
	
	if (ret)
	{
		clk_disable_unprepare(simple->clk_usb3_cce);
		clk_disable_unprepare(simple->clk_usb3_5gphy);
		clk_disable_unprepare(simple->clk_usb3_480mphy0);
		clk_disable_unprepare(simple->clk_usb3_480mpll0);
	
		reset_control_assert(simple->reset);
	
		pm_runtime_disable(dev);
		pm_runtime_put_noidle(dev);
		pm_runtime_set_suspended(dev);
	}
	
	return ret;
}

static int dwc3_caninos_remove(struct platform_device *pdev)
{
	struct dwc3_caninos	*simple = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	of_platform_depopulate(dev);
	
	clk_disable_unprepare(simple->clk_usb3_cce);
	clk_disable_unprepare(simple->clk_usb3_5gphy);
	clk_disable_unprepare(simple->clk_usb3_480mphy0);
	clk_disable_unprepare(simple->clk_usb3_480mpll0);
	
	reset_control_assert(simple->reset);
	
	pm_runtime_disable(dev);
	pm_runtime_put_noidle(dev);
	pm_runtime_set_suspended(dev);
	
	return 0;
}

static int __maybe_unused dwc3_caninos_runtime_suspend(struct device *dev)
{
	struct dwc3_caninos	*simple = dev_get_drvdata(dev);
	
	clk_disable_unprepare(simple->clk_usb3_cce);
	clk_disable_unprepare(simple->clk_usb3_5gphy);
	clk_disable_unprepare(simple->clk_usb3_480mphy0);
	clk_disable_unprepare(simple->clk_usb3_480mpll0);

	return 0;
}

static int __maybe_unused dwc3_caninos_runtime_resume(struct device *dev)
{
	struct dwc3_caninos	*simple = dev_get_drvdata(dev);
	
	clk_prepare_enable(simple->clk_usb3_480mpll0);
	clk_prepare_enable(simple->clk_usb3_480mphy0);
	clk_prepare_enable(simple->clk_usb3_5gphy);
	clk_prepare_enable(simple->clk_usb3_cce);
	
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	
	return 0;
}

static int __maybe_unused dwc3_caninos_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused dwc3_caninos_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops dwc3_caninos_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_caninos_suspend, dwc3_caninos_resume)
	SET_RUNTIME_PM_OPS(dwc3_caninos_runtime_suspend,
			dwc3_caninos_runtime_resume, NULL)
};

static const struct of_device_id of_dwc3_caninos_match[] = {
	{ .compatible = "caninos,k7-dwc3" },
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_dwc3_caninos_match);

static struct platform_driver dwc3_caninos_driver = {
	.probe		= dwc3_caninos_probe,
	.remove		= dwc3_caninos_remove,
	.driver		= {
		.name	= "dwc3-caninos",
		.of_match_table = of_dwc3_caninos_match,
		.pm	= &dwc3_caninos_dev_pm_ops,
	},
};

module_platform_driver(dwc3_caninos_driver);

MODULE_DESCRIPTION("DesignWare USB3 Caninos Glue Layer");
MODULE_AUTHOR("LSI-TEC - Caninos Loucos");
MODULE_LICENSE("GPL v2");

