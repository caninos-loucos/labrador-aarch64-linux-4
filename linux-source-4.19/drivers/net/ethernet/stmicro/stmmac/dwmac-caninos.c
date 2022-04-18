// SPDX-License-Identifier: GPL-2.0
/*
 * Caninos Labrador DWMAC specific glue layer
 * Copyright (c) 2019-2020 LSI-TEC - Caninos Loucos
 * Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 * Igor Ruschi Andrade E Lima <igor.lima@lsitec.org.br>
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
#include <linux/of_gpio.h>

#include "stmmac_platform.h"

struct caninos_priv_data {
	int interface;
	int clk_enabled;
	struct clk *tx_clk;
	int powergpio;
	int resetgpio;
	struct pinctrl *pctl;
	struct pinctrl_state *rmii_state;
	struct pinctrl_state *rgmii_state;
};

#define CANINOS_GMAC_GMII_RGMII_RATE 125000000
#define CANINOS_GMAC_MII_RATE 50000000

static void caninos_gmac_free_gpios(void *priv)
{
	struct caninos_priv_data *gmac = priv;
	
	if (gpio_is_valid(gmac->powergpio))
	{
		gpio_direction_output(gmac->powergpio, 0);
		gpio_free(gmac->powergpio);
		gmac->powergpio = -EINVAL;
	}
	
	if (gpio_is_valid(gmac->resetgpio))
	{
		gpio_direction_output(gmac->resetgpio, 0);
		gpio_free(gmac->resetgpio);
		gmac->resetgpio = -EINVAL;
	}
}

static int caninos_gmac_probe_gpios(struct platform_device *pdev, void *priv)
{
	struct caninos_priv_data *gmac = priv;
	struct device *dev = &pdev->dev;
	int ret;

	gmac->powergpio = of_get_named_gpio(dev->of_node, "phy-power-gpio", 0);
	
	gmac->resetgpio = of_get_named_gpio(dev->of_node, "phy-reset-gpio", 0);
	
	if (gpio_is_valid(gmac->powergpio))
	{
		ret = gpio_request(gmac->powergpio, "phy-power-gpio");
			
		if (ret < 0)
		{
			dev_err(dev, "couldn't claim phy-power-gpio pin\n");
			gmac->powergpio = -EINVAL;
			goto erro_gpio_request;
		}
		
		gpio_direction_output(gmac->powergpio, 0);
	}
	
	if (gpio_is_valid(gmac->resetgpio))
	{
		ret = gpio_request(gmac->resetgpio, "phy-reset-gpio");
		
		if (ret < 0)
		{
			dev_err(dev, "couldn't claim phy-reset-gpio pin\n");
			gmac->resetgpio = -EINVAL;
			goto erro_gpio_request;
		}
		
		gpio_direction_output(gmac->resetgpio, 0);
	}
	
	return 0;
	
erro_gpio_request:
	caninos_gmac_free_gpios(gmac);
	return ret;
}

static int caninos_gmac_interface_config(void *priv, int mode)
{
	struct caninos_priv_data *gmac = priv;
	void __iomem *addr = NULL;
	
	if (mode == PHY_INTERFACE_MODE_RGMII) {
		pinctrl_select_state(gmac->pctl, gmac->rgmii_state);
	}
	else {
		pinctrl_select_state(gmac->pctl, gmac->rmii_state);
	}
	
	addr = ioremap_nocache(0xe024c0a0, 4);
	
	if (addr == NULL) {
		return -ENOMEM;
	}
	
	if (mode == PHY_INTERFACE_MODE_RGMII) {
		writel(0x1, addr);
	} else {
		writel(0x4, addr);
	}
	
	iounmap(addr);
	return 0;
}

static int caninos_gmac_init(struct platform_device *pdev, void *priv)
{
	struct caninos_priv_data *gmac = priv;
	
	if (gpio_is_valid(gmac->powergpio))
	{
		gpio_direction_output(gmac->powergpio, 0);
		mdelay(15);
	}
	
	if (gpio_is_valid(gmac->resetgpio)) {
		gpio_direction_output(gmac->resetgpio, 0);
	}
	
	if (gpio_is_valid(gmac->powergpio)) {
		gpio_direction_output(gmac->powergpio, 1);
	}
	
	if (gpio_is_valid(gmac->resetgpio))
	{
		mdelay(15);
		gpio_direction_output(gmac->resetgpio, 1);
		mdelay(160);
	}
	
	/* Set GMAC interface port mode
	 *
	 * The GMAC TX clock lines are configured by setting the clock
	 * rate, which then uses the auto-reparenting feature of the
	 * clock driver, and enabling/disabling the clock.
	 */
	if (gmac->interface == PHY_INTERFACE_MODE_RGMII)
	{
		clk_set_rate(gmac->tx_clk, CANINOS_GMAC_GMII_RGMII_RATE);
		clk_prepare_enable(gmac->tx_clk);
		gmac->clk_enabled = 1;
	}
	else {
		clk_set_rate(gmac->tx_clk, CANINOS_GMAC_MII_RATE);
		clk_prepare(gmac->tx_clk);
	}
	
	return caninos_gmac_interface_config(priv, gmac->interface);
}

static void caninos_gmac_exit(struct platform_device *pdev, void *priv)
{
	struct caninos_priv_data *gmac = priv;

	if (gmac->clk_enabled) {
		clk_disable(gmac->tx_clk);
		gmac->clk_enabled = 0;
	}
	clk_unprepare(gmac->tx_clk);
	
	caninos_gmac_free_gpios(gmac);
}

static void caninos_fix_speed(void *priv, unsigned int speed)
{
	struct caninos_priv_data *gmac = priv;

	/* only GMII mode requires us to reconfigure the clock lines */
	if (gmac->interface != PHY_INTERFACE_MODE_GMII)
		return;

	if (gmac->clk_enabled) {
		clk_disable(gmac->tx_clk);
		gmac->clk_enabled = 0;
	}
	clk_unprepare(gmac->tx_clk);

	if (speed == 1000)
	{
		clk_set_rate(gmac->tx_clk, CANINOS_GMAC_GMII_RGMII_RATE);
		clk_prepare_enable(gmac->tx_clk);
		gmac->clk_enabled = 1;
		
		caninos_gmac_interface_config(priv, PHY_INTERFACE_MODE_RGMII);
	}
	else
	{
		clk_set_rate(gmac->tx_clk, CANINOS_GMAC_MII_RATE);
		clk_prepare(gmac->tx_clk);
		
		caninos_gmac_interface_config(priv, PHY_INTERFACE_MODE_RMII);
	}
}

static int caninos_gmac_probe(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	struct caninos_priv_data *gmac;
	struct device *dev = &pdev->dev;
	int ret;
	
	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	
	if (ret) {
		return ret;
	}
	
	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	
	if (IS_ERR(plat_dat)) {
		return PTR_ERR(plat_dat);
	}
	
	gmac = devm_kzalloc(dev, sizeof(*gmac), GFP_KERNEL);
	
	if (!gmac)
	{
		ret = -ENOMEM;
		goto err_remove_config_dt;
	}
	
	gmac->interface = of_get_phy_mode(dev->of_node);
	
	gmac->tx_clk = devm_clk_get(dev, "rmii_ref");
	
	if (IS_ERR(gmac->tx_clk)) {
		dev_err(dev, "could not get tx clock\n");
		ret = PTR_ERR(gmac->tx_clk);
		goto err_remove_config_dt;
	}
	
	gmac->pctl = devm_pinctrl_get(dev);
	
	if (IS_ERR(gmac->pctl)) {
		dev_err(dev, "devm_pinctrl_get() failed\n");
		return PTR_ERR(gmac->pctl);
	}
	
	gmac->rmii_state = pinctrl_lookup_state(gmac->pctl, "rmii");
	
	if (IS_ERR(gmac->rmii_state)) {
		dev_err(dev, "could not get pinctrl rmii state\n");
		return PTR_ERR(gmac->rmii_state);
	}
	
	gmac->rgmii_state = pinctrl_lookup_state(gmac->pctl, "rgmii");
	
	if (IS_ERR(gmac->rgmii_state)) {
		dev_err(dev, "could not get pinctrl rgmii state\n");
		return PTR_ERR(gmac->rgmii_state);
	}
	
	ret = caninos_gmac_probe_gpios(pdev, gmac);
	
	if (ret) {
		goto err_remove_config_dt;
	}
	
	/* platform data specifying hardware features and callbacks. */
	plat_dat->tx_coe = 0;
	plat_dat->riwt_off = 1;
	plat_dat->clk_csr = 0x4;
	plat_dat->has_gmac = 1;
	plat_dat->bsp_priv = gmac;
	plat_dat->init = caninos_gmac_init;
	plat_dat->exit = caninos_gmac_exit;
	plat_dat->fix_mac_speed = caninos_fix_speed;

	ret = caninos_gmac_init(pdev, plat_dat->bsp_priv);
	
	if (ret) {
		goto err_remove_config_dt;
	}
	
	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	
	if (ret) {
		goto err_gmac_exit;
	}

	return 0;

err_gmac_exit:
	caninos_gmac_exit(pdev, plat_dat->bsp_priv);
err_remove_config_dt:
	stmmac_remove_config_dt(pdev, plat_dat);
	
	return ret;
}

static const struct of_device_id caninos_dwmac_match[] = {
	{.compatible = "caninos,k7-gmac" },
	{ }
};

MODULE_DEVICE_TABLE(of, caninos_dwmac_match);

static struct platform_driver caninos_dwmac_driver = {
	.probe = caninos_gmac_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		   .name = "caninos-dwmac",
		   .pm = &stmmac_pltfr_pm_ops,
		   .of_match_table = caninos_dwmac_match,
		   },
};

module_platform_driver(caninos_dwmac_driver);

MODULE_AUTHOR("LSI-TEC - Caninos Loucos");
MODULE_DESCRIPTION("Caninos Labrador DWMAC specific glue layer");
MODULE_LICENSE("GPL v2");

