/**
 * Actions OWL SoCs dwc3 driver
 *
 * Copyright (c) 2015 Actions Semiconductor Co., ltd.
 * tangshaoqing <tangshaoqing@actions-semi.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <asm/uaccess.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include "core.h"

#ifdef DEBUG_S700_USB3_CLK
void dwc3_owl_printk_power_usage_count(void);
void dwc3_owl_printk_clk_reg(void);

#define PWR_USB3	(1<<10)
#define ACK_USB3	(1<<10)

#define SPS_PG_BASE	0xE01B0100
#define SPS_PG_CTL	(SPS_PG_BASE+0x0000)
#define SPS_PG_ACK	(SPS_PG_BASE+0x0018)

#define USB3_CONTROLLER_CLOCK_ENABLE	(1 << 25)
#define USB3_MOD_RST	(1 << 25)

#define CMU_BASE	0xE0168000
#define CMU_DEVCLKEN0	(CMU_BASE+0x00a0)
#define CMU_DEVRST0	(CMU_BASE+0x00A8)
#define CMU_USBPLL	(CMU_BASE+0x00b0)

#define PLL_LDO_EN	(1<<31)
#define SOFT_VBUS_VALUE	(1<<7)
#define SOFT_VBUS_EN	(1<<6)
#endif

#ifdef DEBUG_S900_USB3_CLK
void dwc3_owl_printk_power_usage_count(void);
void dwc3_owl_printk_clk_reg(void);

#define USB3_MOD_RST           (1 << 14)

#define PWR_USB3	(1<<8)
#define ACK_USB3	(1<<8)
#define USB3_AVDD_EN	(1<<17)

#define ASSIST_PLL_EN		(1<<0)
#define USB3_MAC_DIV(n)   ((n)<<12)
#define USB3_MAC_DIV_MASK		(3<<12)

#define SPS_PG_BASE	0xE012e000
#define SPS_PG_CTL	(SPS_PG_BASE+0x0000)
#define SPS_PG_ACK	(SPS_PG_BASE+0x0004)
#define SPS_LDO_CTL	(SPS_PG_BASE+0x0014)

#define CMU_BASE	0xE0160000
#define CMU_USBPLL	(CMU_BASE+0x0080)
#define CMU_ASSISTPLL	(CMU_BASE+0x0084)
#define CMU_DEVRST1	(CMU_BASE+0x00AC)
#endif

#ifdef OWL_FPGA_USB_VERIFY
#define S700_SOFTVBUS	(1<<7)
#define S700_SOFTVBUSEN	(1<<6)
#define S700_TIMER_BASE	0xE024C000
#define S700_USB3_ECS	(S700_TIMER_BASE+0x0090)

#define S900_SOFTVBUS	(1<<25)
#define S900_SOFTVBUSEN	(1<<24)
#define S900_TIMER_BASE	0xE0228000
#define S900_USB3_ECS	(S900_TIMER_BASE+0x0090)
#endif

struct dwc3_owl_regs {
#ifdef DEBUG_S700_USB3_CLK
	void __iomem *sps_pg_ctl;
	void __iomem *sps_pg_ack;

	void __iomem *usbpll;
	void __iomem *cmu_devclken0;
	void __iomem *devrst;
#endif

#ifdef DEBUG_S900_USB3_CLK
	void __iomem *sps_pg_ctl;
	void __iomem *sps_pg_ack;
	void __iomem *sps_ldo_ctl;
	
	void __iomem *usbpll;
	void __iomem *cmu_assistpll;
	void __iomem *devrst;
#endif

#ifdef OWL_FPGA_USB_VERIFY
	void __iomem *usbecs;
#endif
};

#define USB3_BACKDOOR    0x0

enum {
	DWC3_S700 = 0,
	DWC3_S900 = 0x100
};

struct dwc3_owl_data {
	int ic_type;
};

struct dwc3_owl {
	struct platform_device	*dwc3;
	struct device		*dev;

	struct dwc3_owl_data  *data;

	struct clk * clk_usb3_480mpll0;
	struct clk * clk_usb3_480mphy0;
	struct clk * clk_usb3_5gphy;
	struct clk * clk_usb3_cce;

	struct clk * clk_usb3_mac;
	
	struct dwc3_owl_regs regs;

	void __iomem        *base;
};

struct dwc3_owl *_dwc3_owl = 0;

#ifdef OWL_FPGA_USB_VERIFY
static void dwc3_owl_set_softvbus_for_fpga_verify(struct dwc3_owl *owl)
{
	u32		reg;

	reg = readl(owl->regs.usbecs);

	 if (owl->data->ic_type == DWC3_S700)
		reg |= (S700_SOFTVBUS | S700_SOFTVBUSEN);
	else if (owl->data->ic_type == DWC3_S900)
		reg |= (S900_SOFTVBUS | S900_SOFTVBUSEN);

	writel(reg, owl->regs.usbecs);
}
#endif

static void dwc3_owl_clk_init(struct dwc3_owl *owl)
{
	struct device		*dev = owl->dev;

	pm_runtime_get_sync(dev);

	clk_prepare_enable(owl->clk_usb3_480mpll0);
	clk_prepare_enable(owl->clk_usb3_480mphy0);
	clk_prepare_enable(owl->clk_usb3_5gphy);
	clk_prepare_enable(owl->clk_usb3_cce);

	if (owl->data->ic_type == DWC3_S900) {
		clk_prepare_enable(owl->clk_usb3_mac);
		clk_set_rate(owl->clk_usb3_mac,
			clk_round_rate(owl->clk_usb3_mac, (500000000/4)));
	}

#ifdef OWL_FPGA_USB_VERIFY
	dwc3_owl_set_softvbus_for_fpga_verify(owl);
#endif
}

static void dwc3_owl_clk_exit(struct dwc3_owl *owl)
{
	struct device		*dev = owl->dev;

	if (owl->data->ic_type == DWC3_S900)
		clk_disable_unprepare(owl->clk_usb3_mac);

	clk_disable_unprepare(owl->clk_usb3_cce);
	clk_disable_unprepare(owl->clk_usb3_5gphy);
	clk_disable_unprepare(owl->clk_usb3_480mphy0);
	clk_disable_unprepare(owl->clk_usb3_480mpll0);

	pm_runtime_put_sync(dev);
}

void dwc3_owl_ss_mac_sample_edge_init(void)
{
	struct dwc3_owl *owl = _dwc3_owl;
	struct device       *dev = owl->dev;
	u32 val;

	if (owl->data->ic_type == DWC3_S700) {
		val = readl(owl->base + USB3_BACKDOOR);
		val &= ~((0x1<<22)|(0x1<<8));
		val |= (0x1<<9);
		val |= (0x1<<4);
		writel(val, owl->base + USB3_BACKDOOR);
		dev_dbg(dev, "\n USB3_BACKDOOR bit4 set, %x\n", readl(owl->base + USB3_BACKDOOR));
	}
}
EXPORT_SYMBOL_GPL(dwc3_owl_ss_mac_sample_edge_init);

void dwc3_owl_clock_init(void)
{
	if (_dwc3_owl)
		dwc3_owl_clk_init(_dwc3_owl);
}
EXPORT_SYMBOL_GPL(dwc3_owl_clock_init);

void dwc3_owl_clock_exit(void)
{
	if (_dwc3_owl)
		dwc3_owl_clk_exit(_dwc3_owl);
}
EXPORT_SYMBOL_GPL(dwc3_owl_clock_exit);

void dwc3_actions_clock_init(void)
{
	dwc3_owl_clock_init();
}
EXPORT_SYMBOL_GPL(dwc3_actions_clock_init);

void dwc3_actions_clock_exit(void)
{
	dwc3_owl_clock_exit();
}
EXPORT_SYMBOL_GPL(dwc3_actions_clock_exit);

static int __dwc3_owl_suspend(struct dwc3_owl *owl)
{
	dev_dbg(owl->dev, "%s\n", __func__);
	if (owl->data->ic_type == DWC3_S900)
		clk_disable_unprepare(owl->clk_usb3_mac);

	clk_disable_unprepare(owl->clk_usb3_cce);
	clk_disable_unprepare(owl->clk_usb3_5gphy);
	clk_disable_unprepare(owl->clk_usb3_480mphy0);
	clk_disable_unprepare(owl->clk_usb3_480mpll0);

	return 0;
}

static int __dwc3_owl_resume(struct dwc3_owl *owl)
{
	struct device       *dev = owl->dev;

	dev_dbg(dev, "%s\n", __func__);

	clk_prepare_enable(owl->clk_usb3_480mpll0);
	clk_prepare_enable(owl->clk_usb3_480mphy0);
	clk_prepare_enable(owl->clk_usb3_5gphy);
	clk_prepare_enable(owl->clk_usb3_cce);

	if (owl->data->ic_type == DWC3_S900) {
		clk_prepare_enable(owl->clk_usb3_mac);
		clk_set_rate(owl->clk_usb3_mac,
			clk_round_rate(owl->clk_usb3_mac, (500000000/4)));
	}

	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}

void dwc3_owl_plug_suspend(void)
{
	if (_dwc3_owl)
		__dwc3_owl_suspend(_dwc3_owl);
}
EXPORT_SYMBOL_GPL(dwc3_owl_plug_suspend);

void dwc3_owl_plug_resume(void)
{
	if (_dwc3_owl)
		__dwc3_owl_resume(_dwc3_owl);
}
EXPORT_SYMBOL_GPL(dwc3_owl_plug_resume);

#ifdef DEBUG_S700_USB3_CLK
static int dwc3_owl_debug_regs_remap(struct dwc3_owl	*owl)
{
	struct device       *dev = owl->dev;

	owl->regs.sps_pg_ctl = devm_ioremap_nocache(dev, SPS_PG_CTL, 4);
	owl->regs.sps_pg_ack = devm_ioremap_nocache(dev, SPS_PG_ACK, 4);

	owl->regs.usbpll = devm_ioremap_nocache(dev, CMU_USBPLL, 4);
	owl->regs.cmu_devclken0  = devm_ioremap_nocache(dev, CMU_DEVCLKEN0, 4);
	owl->regs.devrst = devm_ioremap_nocache(dev, CMU_DEVRST0, 4);

	if ((!owl->regs.sps_pg_ctl) || (!owl->regs.sps_pg_ack)
		|| (!owl->regs.usbpll) || (!owl->regs.cmu_devclken0) || (!owl->regs.devrst)) {
		dev_err(dev, "%s %d ioremap failed\n", __func__, __LINE__);
		return -ENOMEM;
	}

	return 0;
}

void dwc3_owl_printk_power_usage_count(void)
{
	struct device       *dev;

	if (!_dwc3_owl)
		return;

	dev = _dwc3_owl->dev;

	dev_dbg(dev, "dwc3 owl dev: disable_depth=%d usage_count=%d\n",
		dev->power.disable_depth, dev->power.usage_count.counter);
}
EXPORT_SYMBOL_GPL(dwc3_owl_printk_power_usage_count);

void dwc3_owl_printk_clk_reg(void)
{
	struct dwc3_owl	*owl = _dwc3_owl;

	if (!owl)
		return;

	dev_dbg(owl->dev, "owl->regs.sps_pg_ctl:0x%x\n", readl(owl->regs.sps_pg_ctl));
	dev_dbg(owl->dev, "owl->regs.sps_pg_ack:0x%x\n", readl(owl->regs.sps_pg_ack));

	dev_dbg(owl->dev, "owl->regs.usbpll:0x%x\n", readl(owl->regs.usbpll));
	dev_dbg(owl->dev, "owl->regs.cmu_devclken0:0x%x\n", readl(owl->regs.cmu_devclken0));
	dev_dbg(owl->dev, "owl->regs.devrst:0x%x\n", readl(owl->regs.devrst));
}
EXPORT_SYMBOL_GPL(dwc3_owl_printk_clk_reg);
#endif

#ifdef DEBUG_S900_USB3_CLK
static int dwc3_owl_debug_regs_remap(struct dwc3_owl	*owl)
{
	struct device       *dev = owl->dev;

	owl->regs.sps_pg_ctl = devm_ioremap_nocache(dev, SPS_PG_CTL, 4);
	owl->regs.sps_pg_ack = devm_ioremap_nocache(dev, SPS_PG_ACK, 4);
	owl->regs.sps_ldo_ctl = devm_ioremap_nocache(dev, SPS_LDO_CTL, 4);

	owl->regs.usbpll = devm_ioremap_nocache(dev, CMU_USBPLL, 4);
	owl->regs.cmu_assistpll  = devm_ioremap_nocache(dev, CMU_ASSISTPLL, 4);
	owl->regs.devrst = devm_ioremap_nocache(dev, CMU_DEVRST1, 4);

	if ((!owl->regs.sps_pg_ctl) || (!owl->regs.sps_pg_ack) || (!owl->regs.sps_ldo_ctl)
		|| (!owl->regs.usbpll) || (!owl->regs.cmu_assistpll) || (!owl->regs.devrst)) {
		dev_err(dev, "%s %d ioremap failed\n", __func__, __LINE__);
		return -ENOMEM;
	}

	return 0;
}

void dwc3_owl_printk_power_usage_count(void)
{
	struct device       *dev;

	if (!_dwc3_owl)
		return;

	dev = _dwc3_owl->dev;

	dev_dbg(dev, "dwc3 owl dev: disable_depth=%d usage_count=%d\n",
		dev->power.disable_depth, dev->power.usage_count.counter);
}
EXPORT_SYMBOL_GPL(dwc3_owl_printk_power_usage_count);

void dwc3_owl_printk_clk_reg(void)
{
	struct dwc3_owl	*owl = _dwc3_owl;

	if (!owl)
		return;

	dev_dbg(owl->dev, "owl->regs.sps_pg_ctl:0x%x\n", readl(owl->regs.sps_pg_ctl));
	dev_dbg(owl->dev, "owl->regs.sps_pg_ack:0x%x\n", readl(owl->regs.sps_pg_ack));
	dev_dbg(owl->dev, "owl->regs.sps_ldo_ctl:0x%x\n", readl(owl->regs.sps_ldo_ctl));

	dev_dbg(owl->dev, "owl->regs.usbpll:0x%x\n", readl(owl->regs.usbpll));
	dev_dbg(owl->dev, "owl->regs.cmu_assistpll:0x%x\n", readl(owl->regs.cmu_assistpll));
	dev_dbg(owl->dev, "owl->regs.devrst:0x%x\n", readl(owl->regs.devrst));
}
EXPORT_SYMBOL_GPL(dwc3_owl_printk_clk_reg);
#endif

extern void dwc3_mask_supper_speed(void);

static const struct of_device_id owl_dwc3_match[];
static u64 dwc3_dma_mask = DMA_BIT_MASK(32);
static int dwc3_owl_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
			of_match_device(owl_dwc3_match, &pdev->dev);
	struct device_node	*node = pdev->dev.of_node;
	struct device       *dev = &pdev->dev;
	struct dwc3_owl	*owl;
	struct resource		*res;
	void __iomem        *base;
	int			ret = -ENOMEM;

	if (!node) {
		dev_err(dev, "device node not found\n");
		return -EINVAL;
	}

	if (!of_id) {
		dev_err(dev, "no compatible OF match\n");
		return -EINVAL;
	}

	dev->dma_mask		= &dwc3_dma_mask;
	dev->coherent_dma_mask	= DMA_BIT_MASK(32);

	owl = devm_kzalloc(dev, sizeof(*owl), GFP_KERNEL);
	if (!owl) {
		dev_err(dev, "not enough memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, owl);
	owl->dwc3 = pdev;
	owl->dev	= &pdev->dev;
	owl->data	= (struct dwc3_owl_data  *)of_id->data;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "missing memory base resource\n");
		return -EINVAL;
	}

	res = devm_request_mem_region(dev, res->start,resource_size(res),
				      dev_name(dev));
	if (!res) {
		dev_err(dev, "can't request mem region\n");
		return -ENOMEM;
	}

	base = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (!base) {
		dev_err(dev, "ioremap failed\n");
		return -ENOMEM;
	}

	owl->base	= base;

#ifdef OWL_FPGA_USB_VERIFY
	if (owl->data->ic_type == DWC3_S700)
		owl->regs.usbecs = devm_ioremap_nocache(dev, S700_USB3_ECS, 4);
	else if (owl->data->ic_type == DWC3_S900)
		owl->regs.usbecs = devm_ioremap_nocache(dev, S900_USB3_ECS, 4);

	if (!owl->regs.usbecs) {
		dev_err(dev, "ioremap failed\n");
		return -ENOMEM;
	}
#endif

#if defined(DEBUG_S700_USB3_CLK)  || defined(DEBUG_S900_USB3_CLK)
	if (dwc3_owl_debug_regs_remap(owl))
		return -ENOMEM;
#endif

	/* "usb3_480mpll0", "usb3_480mphy0", "usb3_5gphy", "usb3_cce", "usb3_mac"; */
	owl->clk_usb3_480mpll0 = devm_clk_get(dev, "usb3_480mpll0");
	if (IS_ERR(owl->clk_usb3_480mpll0)) {
		dev_err(&pdev->dev, "unable to get clk_usb3_480mpll0\n");
		return -EINVAL;
	}

	owl->clk_usb3_480mphy0 = devm_clk_get(dev, "usb3_480mphy0");
	if (IS_ERR(owl->clk_usb3_480mphy0)) {
		dev_err(&pdev->dev, "unable to get usb3_480mphy0\n");
		return -EINVAL;
	}

	owl->clk_usb3_5gphy = devm_clk_get(dev, "usb3_5gphy");
	if (IS_ERR(owl->clk_usb3_5gphy)) {
		dev_err(&pdev->dev, "unable to get usb3_5gphy\n");
		return -EINVAL;
	}

	owl->clk_usb3_cce = devm_clk_get(dev, "usb3_cce");
	if (IS_ERR(owl->clk_usb3_cce)) {
		dev_err(&pdev->dev, "unable to get usb3_cce\n");
		return -EINVAL;
	}
	if (owl->data->ic_type == DWC3_S900) {
		owl->clk_usb3_mac = devm_clk_get(dev, "usb3_mac");
		if (IS_ERR(owl->clk_usb3_mac)) {
			dev_err(&pdev->dev, "unable to get usb3_mac\n");
			return -EINVAL;
		}
	}

	//if (owl->data->ic_type == DWC3_S700)
	//	dwc3_mask_supper_speed();

	pm_runtime_enable(owl->dev);
	dwc3_owl_clk_init(owl);
	
	ret = of_platform_populate(node, NULL, NULL, dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to create dwc3 core\n");
		goto err;
	}

	_dwc3_owl = owl;

	dev_dbg(dev, "%s %d success\n", __func__, __LINE__);

	return 0;

err:
	dwc3_owl_clk_exit(owl);
	pm_runtime_disable(owl->dev);

	return ret;
}

static int dwc3_owl_remove_core(struct device *dev, void *c)
{
	struct platform_device *pdev = to_platform_device(dev);

	platform_device_unregister(pdev);

	return 0;
}

static int dwc3_owl_remove(struct platform_device *pdev)
{
	struct dwc3_owl	*owl = platform_get_drvdata(pdev);

	_dwc3_owl = 0;

	dwc3_owl_clk_exit(owl);
	pm_runtime_disable(owl->dev);
	device_for_each_child(&pdev->dev, NULL, dwc3_owl_remove_core);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dwc3_owl_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);

	return 0;
}

static int dwc3_owl_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);

	return 0;
}

static const struct dev_pm_ops dwc3_owl_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_owl_suspend, dwc3_owl_resume)
};

#define DEV_PM_OPS	(&dwc3_owl_dev_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif /* CONFIG_PM_SLEEP */

static  struct dwc3_owl_data  dwc3_owl_s700_data = {
	.ic_type = DWC3_S700,
};

static  struct dwc3_owl_data  dwc3_owl_s900_data = {
	.ic_type = DWC3_S900,
};

static const struct of_device_id owl_dwc3_match[] = {
	{ .compatible = "actions,s700-dwc3", .data = &dwc3_owl_s700_data,},
	{ .compatible = "actions,s900-dwc3", .data = &dwc3_owl_s900_data,},
	{},
};
MODULE_DEVICE_TABLE(of, owl_dwc3_match);

static struct platform_driver dwc3_owl_driver = {
	.probe		= dwc3_owl_probe,
	.remove		= dwc3_owl_remove,
	.driver		= {
		.name	= "owl-dwc3",
		.of_match_table = of_match_ptr(owl_dwc3_match),
		.pm	= DEV_PM_OPS,
	},
};

module_platform_driver(dwc3_owl_driver);

MODULE_ALIAS("platform:owl-dwc3");
MODULE_AUTHOR("tangshaoqing <tangshaoqing@actions-semi.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DesignWare USB3 actions Glue Layer");
