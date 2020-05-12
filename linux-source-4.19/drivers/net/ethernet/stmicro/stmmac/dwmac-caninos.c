/*
 * Copyright (c) 2019 LSI-TEC - Caninos Loucos
 * Author: Igor Ruschi Andrade E Lima <igor.lima@lsitec.org.br>
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

#include <linux/stmmac.h>
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
#include "stmmac_platform.h"


struct owl_priv_data {
	int interface;
	int clk_enabled;
	struct clk *tx_clk;
	struct clk *eth_clk;
	struct regulator *power_regulator;
	struct device_node *phy_node;
	int resetgpio;
	int powergpio;
};

/*ethernet mac base register*/
void __iomem *ppaddr;
struct owl_priv_data *gmac;

#define OWL_GMAC_GMII_RGMII_RATE	125000000
#define OWL_GMAC_MII_RATE		50000000

static int owl_gmac_init(struct platform_device *pdev, void *priv)
{
	int ret;
	int limit;
	int vendor_id;
	struct device_node *np = NULL;
	struct device *dev = &pdev->dev;
	void __iomem *addr = NULL;

	vendor_id = 1;

	gmac = devm_kzalloc(dev, sizeof(*gmac), GFP_KERNEL);
	if (!gmac)
		return -1;

	gmac->interface = of_get_phy_mode(dev->of_node);

	gmac->tx_clk = devm_clk_get(dev, "rmii_ref");
	if (IS_ERR(gmac->tx_clk)) {
		dev_err(dev, "could not get rmii ref clock\n");
		return -1;
	}

	np = of_find_compatible_node(NULL, NULL, "caninos,s700-ethernet");
	if (NULL == np) {
		pr_err("No \"s700-ethernet\" node found in dts\n");
		return -1;
	}

	if (of_find_property(np, "phy-power-gpio", NULL)) {
		gmac->powergpio = of_get_named_gpio(np, "phy-power-gpio", 0);
		if (gpio_is_valid(gmac->powergpio)) {
			ret = gpio_request(gmac->powergpio, "phy-power-gpio");
			if (ret < 0) {
				pr_err("couldn't claim phy-power-gpio pin\n");
				return -1;
			}
			gpio_direction_output(gmac->powergpio, 1);
			gpio_set_value(gmac->powergpio, 0);
			mdelay(10);
			gpio_set_value(gmac->powergpio, 1);
			mdelay(10);
			pr_info("phy power_up\n");
		} else {
			pr_err("gpio for phy-power-gpio invalid.\n");
		}
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
			mdelay(10);
			gpio_direction_output(gmac->resetgpio, 0);
			mdelay(15);
			gpio_direction_output(gmac->resetgpio, 1);
			mdelay(160);
			pr_info("phy reset\n");
		} else {
			pr_err("gpio for phy-reset-gpios invalid.\n");
		}
	}

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
		gmac->clk_enabled = 1;
	}

	/* Set GMAC interface port mode
	 *
	 * The GMAC TX clock lines are configured by setting the clock
	 * rate, which then uses the auto-reparenting feature of the
	 * clock driver, and enabling/disabling the clock.
	 */

	if (gmac->interface == PHY_INTERFACE_MODE_RGMII) {
			/*set tx 1 level rx 2 level delay chain, not used yet*/
			addr = ioremap(0xe01680ac, 4);
			ret = readl(addr);
			ret &= 0xff807fff;
			ret |= 0x00198000;
			writel(ret, addr);
			
			addr = ioremap(0xe01b0080, 4);
			ret = readl(addr);
			ret &= ~(0x3FF << 14);//mask
			ret |= (0x2 << 14);//not tested yet
			writel(ret, addr);
	}else{
		/*when RMII set only driver str */
			addr = ioremap(0xe01b0080, 4);
			ret = readl(addr);
			ret &= ~(0x3FF << 14);//mask
			ret |= (0x5b << 14);//this value can change signal integrity
			writel(ret, addr);
	}

	addr = ioremap(0xe024c0a0, 4); //this register is not listed on datasheet
	if (gmac->interface == PHY_INTERFACE_MODE_RGMII) {
			/*rgmii , phy interface select register*/
			writel(0x1, addr);
	} else {
			/*rmii , phy interface select register*/
			writel(0x4, addr);
	}

	limit = 10;
	addr = ppaddr + 0x1000;
	ret = readl(addr);

	do {
		writel( ret | 0x1, addr);
		ret = readl(addr);
		mdelay(10);
		limit--;
		if (limit < 0) {
			pr_err("software reset busy, ret:0x%x, limit:%d\n", ret, limit);
			return -EBUSY;
		}
	} while (ret & 0x1);

	pr_info("software reset finish, ret & 0x1:0x%x, limit:%d\n", ret & 0x1, limit);

	return 0;
}

static void owl_gmac_exit(struct platform_device *pdev, void *priv)
{
	if (gmac->clk_enabled) {
		clk_disable(gmac->tx_clk);
		gmac->clk_enabled = 0;
	}
	clk_unprepare(gmac->tx_clk);

	if (gmac->resetgpio != 0) {
		gpio_direction_output(gmac->resetgpio, 0);
        gpio_free(gmac->resetgpio);
		mdelay(30);
	}

    if (gmac->powergpio != 0) {
		gpio_direction_output(gmac->powergpio, 0);
        gpio_free(gmac->powergpio);
		mdelay(30);
	}
}

/* of_data specifying hardware features and callbacks.
hardware features were copied from drivers.*/
struct plat_stmmacenet_data owl_gmac_data = {
	.has_gmac = 1,
	.tx_coe = 0,
	.init = owl_gmac_init,
	.exit = owl_gmac_exit,
};

int stmmac_pltfr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct stmmac_resources *stmmac_res;
	struct stmmac_priv *priv = NULL;
	struct plat_stmmacenet_data *plat_dat = NULL;
	const char *mac = NULL;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct stmmac_priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "Failed to allocate priv structure\n");
		return -ENOMEM;
	}

	stmmac_res = devm_kzalloc(&pdev->dev, sizeof(struct stmmac_resources), GFP_KERNEL);
	if (!stmmac_res) {
		dev_err(&pdev->dev, "Failed to allocate structure stmmac_resouces\n");
		return -ENOMEM;
	}

	ret = stmmac_get_platform_resources(pdev, stmmac_res);
	if (ret){
		pr_err("error to get irq resource");
		return ret;
	}
	
	ppaddr = stmmac_res->addr;
	
	if (pdev->dev.of_node) {

		plat_dat = stmmac_probe_config_dt(pdev, &mac);//
		if (!plat_dat) {
			pr_err("%s: main dt probe failed", __func__);
			return ret;
		}

		plat_dat->init = owl_gmac_data.init;
		plat_dat->exit = owl_gmac_data.exit;
		plat_dat->has_gmac = owl_gmac_data.has_gmac;
		plat_dat->tx_coe = owl_gmac_data.tx_coe;
		if (!plat_dat) {
			pr_err("%s: ERROR: no memory", __func__);
			return  -ENOMEM;
		}

	} else {
		plat_dat = pdev->dev.platform_data;
	}
	
	/*  Custom initialisation (if needed)*/
	if (plat_dat->init) {
		ret = plat_dat->init(pdev, NULL);
		if (unlikely(ret)){
			pr_err("%s: fail to custom init phy device",__func__);
			return ret;
		}
	}
	
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	/* using the default divisor, 0x2 to 50MHZ clock is not working, 
	the mdio clock is higher than 2,5MHZ, so changing to 0x4 is getting around 1,7MHZ. 
	*/
	plat_dat->clk_csr = 0x4; 

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, stmmac_res);
	if (ret) {
		pr_err("%s: main driver probe failed", __func__);
		return -ENODEV;
	}

	pr_debug("STMMAC platform driver registration completed");

	return 0;
}

static const struct of_device_id owl_dwmac_match[] = {
	{.compatible = "caninos,s700-ethernet", .data = &owl_gmac_data},
	{}
};


MODULE_DEVICE_TABLE(of, owl_dwmac_match);

static struct platform_driver __refdata owl_dwmac_driver = {
	.probe = stmmac_pltfr_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		   .name = "asoc-ethernet",
		   .pm = &stmmac_pltfr_pm_ops,
		   .of_match_table = owl_dwmac_match,
		   },
};

module_platform_driver(owl_dwmac_driver);

MODULE_AUTHOR("Igor Ruschi <igor.lima@lsitec.org.br>");
MODULE_DESCRIPTION("Caninos DWMAC specific glue layer");
MODULE_LICENSE("GPL");
