/*
 * dwmac-owl.c - actions DWMAC specific glue layer
 *
 * Copyright (C) 2015 ouyang
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/clk.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/of_net.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/reset.h>

#include "stmmac.h"
#include "stmmac_platform.h"

struct owl_priv_data {
	int interface;
	int clk_enabled;
	struct clk *tx_clk;
	struct clk *eth_clk;
	struct regulator *power_regulator;
	struct device_node *phy_node;
	int resetgpio;
};

/*ethernet mac base register*/
void __iomem *ppaddr;
struct owl_priv_data *gmac;
extern int phyaddr;

#define OWL_GMAC_GMII_RGMII_RATE	125000000
#define OWL_GMAC_MII_RATE		50000000

static void phy_power_enable(void)
{
	int ret = regulator_enable(gmac->power_regulator);
	mdelay(500);
	pr_info("ethernet phy_power_enable ret:%d\n", ret);
}

static void phy_power_disable(void)
{
	pr_info("ethernet phy_power_disable\n");
	regulator_disable(gmac->power_regulator);
}

void owl_gmac_suspend(struct device *device)
{
	void __iomem *addr = NULL;
	int ret;

	{
			addr = ioremap(0xe01680a4, 4);
			ret = readl(addr);
			writel(ret & 0xff7fffff, addr);
	}
	mdelay(10);

	phy_power_disable();
	if (gmac->resetgpio != 0) {
		gpio_direction_output(gmac->resetgpio, 0);
	}
}

void owl_gmac_resume(struct device *device)
{
	struct reset_control *rst;
	int ret;
	void __iomem *addr = NULL;

	phy_power_enable();
	mdelay(10);
	if (gmac->resetgpio != 0) {
			gpio_direction_output(gmac->resetgpio, 0);
			mdelay(30);
			gpio_direction_output(gmac->resetgpio, 1);
			mdelay(40);
	}
	if (gmac) {
	{
			void __iomem *addr = NULL;
			addr = ioremap(0xe01680a4, 4);
			ret = readl(addr);
			writel(ret | 0x800000, addr);
			udelay(10);
	}

	/* reset ethernet clk */
	rst = devm_reset_control_get(device, NULL);
	if (IS_ERR(rst)) {
			dev_err(device, "Couldn't get ethernet reset\n");
			return;
	}

	/* Reset the UART controller to clear all previous status. */
	reset_control_assert(rst);
	udelay(10);
	reset_control_deassert(rst);
	udelay(100);
	pr_info("resume owl gmac\n");

	if (gmac->interface == PHY_INTERFACE_MODE_RGMII) {
			/*set tx 1 level rx 2 level delay chain */
			addr = ioremap(0xe01680ac, 4);
			ret = readl(addr);
			ret &= 0xff807fff;
			ret |= 0x00198000;
			writel(ret, addr);

			addr = ioremap(0xe01b0080, 4);
			ret = readl(addr);
			ret |= 0x0CC0C000;
			writel(ret, addr);
		} else {
			/*rmii */
			addr = ioremap(0xe024c0a0, 4);
			writel(0x4, addr);
		}
	}
}

static int owl_gmac_init(struct platform_device *pdev, void *priv)
{
	int ret;
	int limit;
	int vendor_id;
	struct device_node *np = NULL;
	struct device_node *phy_node;
	struct device *dev = &pdev->dev;
	struct reset_control *rst;
	struct resource *res;
	const char *pm;
	const char *str;

	void __iomem *addr = NULL;

	vendor_id = 1;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -ENODEV;

	addr = devm_ioremap_resource(dev, res);

	if (IS_ERR(addr))
		return PTR_ERR(addr);

	gmac = devm_kzalloc(dev, sizeof(*gmac), GFP_KERNEL);
	if (!gmac)
		return -1;

	gmac->interface = of_get_phy_mode(dev->of_node);

	gmac->tx_clk = devm_clk_get(dev, "rmii_ref");
	if (IS_ERR(gmac->tx_clk)) {
		dev_err(dev, "could not get rmii ref clock\n");
		return -1;
	}

	gmac->eth_clk = devm_clk_get(dev, "ethernet");
	if (IS_ERR(gmac->eth_clk)) {
		pr_warn("%s: warning: cannot get ethernet clock\n", __func__);
		return -1;
	}

	np = of_find_compatible_node(NULL, NULL, "actions,s700-ethernet");
	if (NULL == np) {
		pr_err("No \"s700-ethernet\" node found in dts\n");
		return -1;
	}

	if (of_find_property(np, "phy-power-en", NULL)) {
			ret = of_property_read_string(np, "phy-power-en", &pm);
			if (ret < 0) {
				printk("can not read regulator for ethernet phy power!\n");
				return -1;
			}
			gmac->power_regulator = regulator_get(NULL, pm);
			if (IS_ERR(gmac->power_regulator)) {
				gmac->power_regulator = NULL;
				printk("%s:failed to get regulator!\n", __func__);
				return -1;
			}
			phy_power_enable();
			printk("get ethernet phy power regulator success.\n");
	}

	if (of_find_property(np, "phy-reset-gpios", NULL)) {
		gmac->resetgpio = of_get_named_gpio(np, "phy-reset-gpios", 0);
		if (gpio_is_valid(gmac->resetgpio)) {
			ret = gpio_request(gmac->resetgpio, "phy-reset-gpios");
			if (ret < 0) {
				pr_err("couldn't claim phy-reset-gpios pin\n");
				return -1;
			}
			gpio_direction_output(gmac->resetgpio, 1);
			mdelay(30);
			gpio_direction_output(gmac->resetgpio, 0);
			mdelay(30);
			gpio_direction_output(gmac->resetgpio, 1);
			mdelay(40);
			pr_info("phy reset\n");
		} else {
			pr_err("gpio for phy-reset-gpios invalid.\n");
		}
	}

	if (of_find_property(np, "phy-addr", NULL)) {
		ret = of_property_read_string(np, "phy-addr", &pm);
		if (ret < 0) {
			pr_warn("can not read phy addr\n");
			return -1;
		}
		phyaddr = simple_strtoul(pm, NULL, 0);

		pr_info("get phy addr success: %d\n", phyaddr);
	}



	clk_prepare(gmac->eth_clk);
	ret = clk_prepare_enable(gmac->eth_clk);
	clk_enable(gmac->eth_clk);
	udelay(100);

	/* reset ethernet clk */
	rst = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(rst)) {
		dev_err(&pdev->dev, "Couldn't get ethernet reset\n");
		return -1;
	}

	reset_control_assert(rst);
	udelay(10);
	reset_control_deassert(rst);
	udelay(100);

	/* Set GMAC interface port mode
	 *
	 * The GMAC TX clock lines are configured by setting the clock
	 * rate, which then uses the auto-reparenting feature of the
	 * clock driver, and enabling/disabling the clock.
	 */
	if (gmac->interface == PHY_INTERFACE_MODE_RGMII) {
		clk_set_rate(gmac->tx_clk, OWL_GMAC_GMII_RGMII_RATE);
		clk_prepare_enable(gmac->tx_clk);
		gmac->clk_enabled = 1;
	} else {
		clk_set_rate(gmac->tx_clk, OWL_GMAC_MII_RATE);
		clk_prepare_enable(gmac->tx_clk);
	}

	if (gmac->interface == PHY_INTERFACE_MODE_RGMII) {
			/*rmii ref clk 125M, enable ethernet pll, from internal cmu, 4 divisor, cmu ethernet pll */
			addr = ioremap(0xe01680b4, 4);
			writel(0x1, addr);
	} else {
			/*rmii ref clk 50M, enable ethernet pll, from internal cmu, 10 divisor, cmu ethernet pll */
			addr = ioremap(0xe01680b4, 4);
			writel(0x5, addr);
	}

	/* Set GMAC interface port mode
	 *
	 * The GMAC TX clock lines are configured by setting the clock
	 * rate, which then uses the auto-reparenting feature of the
	 * clock driver, and enabling/disabling the clock.
	 */

	if (gmac->interface == PHY_INTERFACE_MODE_RGMII) {
			/*set tx 1 level rx 2 level delay chain */
			addr = ioremap(0xe01680ac, 4);
			ret = readl(addr);
			ret &= 0xff807fff;
			ret |= 0x00198000;
			writel(ret, addr);

			addr = ioremap(0xe01b0080, 4);
			ret = readl(addr);
			ret |= 0x0CC0C000;
			writel(ret, addr);
	}

	/*0xe01680b4, cmu ethernet pll*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	if (!res)
		return -ENODEV;

	addr = devm_ioremap_resource(dev, res);
	pr_info("read rmii ref clk:0x%x-0x%x\n", readl(addr), readl(ppaddr));

	/*write GMII address register,  set mdc clock */
	writel(0x10, ppaddr + 0x10);

	if (gmac->interface == PHY_INTERFACE_MODE_RGMII) {
			/*rgmii , phy interface select register*/
			addr = ioremap(0xe024c0a0, 4);
			writel(0x1, addr);
	} else {
			/*rmii , phy interface select register*/
			addr = ioremap(0xe024c0a0, 4);
			writel(0x4, addr);
	}

	limit = 10;
	writel(readl(ppaddr + 0x1000) | 0x1, ppaddr + 0x1000);
	do {
		ret = readl(ppaddr + 0x1000);
		mdelay(100);
		limit--;
		if (limit < 0) {
			pr_err("software reset busy, ret:0x%x, limit:%d\n", ret, limit);
			return -EBUSY;
		}
	} while (ret & 0x1);
	pr_info("software reset finish, ret & 0x1:0x%x, limit:%d\n", ret & 0x1, limit);

	return 0;
}

static void owl_gmac_exit(struct platform_device *pdev)
{
	if (gmac->clk_enabled) {
		clk_disable(gmac->tx_clk);
		gmac->clk_enabled = 0;
	}
	clk_unprepare(gmac->tx_clk);
	clk_disable(gmac->eth_clk);
	clk_unprepare(gmac->eth_clk);

	if (gmac->resetgpio != 0) {
		gpio_direction_output(gmac->resetgpio, 0);
		mdelay(30);
	}
	regulator_disable(gmac->power_regulator);
}

/* of_data specifying hardware features and callbacks.
hardware features were copied from drivers.*/
const struct plat_stmmacenet_data owl_gmac_data = {
	.has_gmac = 1,
	.tx_coe = 0,
	.init = owl_gmac_init,
	.exit = owl_gmac_exit,
};

static const struct of_device_id owl_dwmac_match[] = {
	{.compatible = "actions,s700-ethernet", .data = &owl_gmac_data},
	{}
};


MODULE_DEVICE_TABLE(of, owl_dwmac_match);

static struct platform_driver owl_dwmac_driver = {
	.probe = stmmac_pltfr_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		   .name = "asoc-ethernet",
		   .pm = &stmmac_pltfr_pm_ops,
		   .of_match_table = owl_dwmac_match,
		   },
};

module_platform_driver(owl_dwmac_driver);

MODULE_AUTHOR("Chen-Yu Tsai <wens@csie.org>");
MODULE_DESCRIPTION("actions owl DWMAC specific glue layer");
MODULE_LICENSE("GPL");
