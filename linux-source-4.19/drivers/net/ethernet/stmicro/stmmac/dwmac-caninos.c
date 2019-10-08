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
};

/*ethernet mac base register*/
void __iomem *ppaddr;
struct owl_priv_data *gmac;

int phyaddr; /////MUST FIX!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1


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

#ifdef OWL_PHY_HAS_INTERRUPT
	phy_node = of_parse_phandle(pdev->dev.of_node, "phy-handle", 0);
	if (!phy_node) {
		pr_err("phy-handle of ethernet node can't be parsed\n");
		return -EINVAL;
	}
	gmac->phy_node = phy_node;

	str = of_get_property(phy_node, "compatible", NULL);
	pr_info("ethernet phy's compatible: %s\n", str);

	if (of_device_is_compatible(phy_node, "micrel_icplus,ksz8081_ip101g")) {
		pr_info("phy model is micrel_icplus,ksz8081_ip101g\n");
	} else if (of_device_is_compatible(phy_node, "SR8201G,sr8201g")) {
		pr_info("phy model is sr8201g\n");
	} else { /* ATC2605 or error */
		pr_info("compatible of %s: %s\n", phy_node->full_name, str);
	}

	phy_irq = irq_of_parse_and_map(phy_node, 0);
	pr_info("%s  phy_irq = %d\n", __func__, phy_irq);
	if (phy_irq < 0) {
		pr_err("No IRQ resource for phy\n");
		return -ENODEV;
	}
#endif

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

static void owl_gmac_exit(struct platform_device *pdev, void *priv)
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


int stmmac_pltfr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device *dev = &pdev->dev;
	void __iomem *addr = NULL;
	struct stmmac_priv *priv = NULL;
	struct plat_stmmacenet_data *plat_dat = NULL;
	const char *mac = NULL;
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res)
		return -ENODEV;

	addr = devm_ioremap_resource(dev, res);
	
	if (IS_ERR(addr))
		return PTR_ERR(addr);

	ppaddr = addr;
	
	if (pdev->dev.of_node) {
		//plat_dat = &owl_gmac_data;
		if (!plat_dat) {
			pr_err("%s: ERROR: no memory", __func__);
			return  -ENOMEM;
		}

		//ret = stmmac_probe_config_dt(pdev, plat_dat, &mac);
		if (ret) {
			pr_err("%s: main dt probe failed", __func__);
			return ret;
		}
	} else {
		plat_dat = pdev->dev.platform_data;
	}
	
	/* Custom initialisation (if needed)*/
	//if (plat_dat->init) {
	//	ret = plat_dat->init(pdev);
	//	if (unlikely(ret))
	//		return ret;
	//}

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	//priv = stmmac_dvr_probe(&(pdev->dev), plat_dat, addr);
	//if (!priv) {
	//	pr_err("%s: main driver probe failed", __func__);
	//	return -ENODEV;
	//}

	/* Get MAC address if available (DT) */
	if (mac)
		memcpy(priv->dev->dev_addr, mac, ETH_ALEN);

	/* Get the MAC information */
	priv->dev->irq = platform_get_irq_byname(pdev, "macirq");
	if (priv->dev->irq == -ENXIO) {
		pr_err("%s: ERROR: MAC IRQ configuration "
		       "information not found\n", __func__);
		return -ENXIO;
	}


	priv->wol_irq = platform_get_irq_byname(pdev, "eth_wake_irq");
	if (priv->wol_irq == -ENXIO)
		priv->wol_irq = priv->dev->irq;

	priv->lpi_irq = platform_get_irq_byname(pdev, "eth_lpi");

	platform_set_drvdata(pdev, priv->dev);
	
	pr_debug("STMMAC platform driver registration completed");

	return 0;
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
