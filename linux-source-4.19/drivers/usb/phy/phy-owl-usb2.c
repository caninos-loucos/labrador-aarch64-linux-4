/*
 * Actions OWL SoCs phy driver
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
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include "phy-owl-usb.h"

struct owl_usbphy *usb2_sphy;

static void setphy(struct owl_usbphy *sphy, unsigned char reg_add, unsigned char value)
{
	void __iomem *usb3_usb_vcon = sphy->regs;
	volatile unsigned char addr_low;
	volatile unsigned char addr_high;
	volatile unsigned int vstate;

	addr_low =  reg_add & 0x0f;
	addr_high =  (reg_add >> 4) & 0x0f;

	vstate = value;
	vstate = vstate << 8;

	addr_low |= 0x10;
	writel(vstate | addr_low, usb3_usb_vcon);
	mb();

	addr_low &= 0x0f; 
	writel(vstate | addr_low, usb3_usb_vcon);
	mb();

	addr_low |= 0x10;
	writel(vstate | addr_low, usb3_usb_vcon);
	mb();

	addr_high |= 0x10;
	writel(vstate | addr_high, usb3_usb_vcon);
	mb();

	addr_high &= 0x0f; 
	writel(vstate | addr_high, usb3_usb_vcon);
	mb();

	addr_high |= 0x10;
	writel(vstate | addr_high, usb3_usb_vcon);  
	mb();
	return;
}

static int dwc3_slew_rate = -1;
module_param(dwc3_slew_rate, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dwc3_slew_rate, "dwc3_slew_rate");

static int dwc3_tx_bias = -1;
module_param(dwc3_tx_bias, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dwc3_tx_bias, "dwc3_tx_bias");

static int usb_current_calibrate(int is_device_mode)
{
	if (is_device_mode) {
		return 0xc;
	} else {
		return 0x6;
	}
}
static int dwc3_get_slewrate_config(void)
{
	/* defaut:3, range:0~7; 0:min,7:max;usb high */
	
	return 3;
}

static int s700_usb2phy_param_setup(struct owl_usbphy *sphy, int is_device_mode)
{
	unsigned char val_u8, slew_rate;
	int value;



	slew_rate = dwc3_get_slewrate_config();

	if (is_device_mode) {
		dev_dbg(sphy->dev, "%s device mode\n", __func__);

		val_u8 = (1<<7)|(1<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(sphy, 0xf4, val_u8);
		val_u8 = (0x6<<5)|(0<<4)|(1<<3)|(1<<2)|(3<<0);
		setphy(sphy, 0xe1, val_u8);

		val_u8 = (1<<7)|(0<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(sphy, 0xf4, val_u8);
		val_u8 = (1<<7)|(4<<4)|(1<<3)|(1<<2)|(0<<0);
		setphy(sphy, 0xe6, val_u8);

		val_u8 = (1<<7)|(1<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(sphy, 0xf4, val_u8);
		setphy(sphy, 0xe2, 0x2);
		setphy(sphy, 0xe2, 0x12);

		val_u8 = (1<<7)|(0<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(sphy, 0xf4, val_u8);
		val_u8 = (0xa<<4)|(0<<1)|(1<<0);
		setphy(sphy, 0xe7, val_u8);

		val_u8 = (1<<7)|(1<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(sphy, 0xf4, val_u8);
		setphy(sphy, 0xe0, 0x31);
		setphy(sphy, 0xe0, 0x35);

		val_u8 = (1<<7)|(0<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(sphy, 0xf4, val_u8);

		value = usb_current_calibrate(1);
		dev_dbg(sphy->dev, "USB3 CURRENT value: 0x%x\n", value);

		val_u8 = (0xa<<4) | (value<<0);
		setphy(sphy, 0xe4, val_u8);
	} else {
		dev_dbg(sphy->dev, "%s host mode\n", __func__);

		val_u8 = (1<<7)|(1<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(sphy, 0xf4, val_u8);
		val_u8 = (0x6<<5)|(0<<4)|(1<<3)|(1<<2)|(3<<0);
		setphy(sphy, 0xe1, val_u8);

		val_u8 = (1<<7)|(0<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(sphy, 0xf4, val_u8);
		val_u8 = (1<<7)|(4<<4)|(1<<3)|(1<<2)|(0<<0);
		setphy(sphy, 0xe6, val_u8);

		val_u8 = (1<<7)|(1<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(sphy, 0xf4, val_u8);
		setphy(sphy, 0xe2, 0x2);
		setphy(sphy, 0xe2, 0x16);

		val_u8 = (1<<7)|(0<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(sphy, 0xf4, val_u8);
		val_u8 = (0xa<<4)|(0<<1)|(1<<0);
		setphy(sphy, 0xe7, val_u8);

		val_u8 = (1<<7)|(1<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(sphy, 0xf4, val_u8);
		setphy(sphy, 0xe0, 0x31);
		setphy(sphy, 0xe0, 0x35);

		val_u8 = (1<<7)|(0<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(sphy, 0xf4, val_u8);

		value = usb_current_calibrate(0);
		dev_dbg(sphy->dev, "USB3 CURRENT value: 0x%x\n", value);

		val_u8 = (0xa<<4) | (value<<0);
		setphy(sphy, 0xe4, val_u8);

		val_u8 = (1<<7)|(1<<6)|(1<<5)|(1<<4)|
			(1<<3)|(1<<2)|(0<<1)|(0<<0);
		setphy(sphy, 0xf0, val_u8);
	}

	return 0;
}

static int s900_usb2phy_param_setup(struct owl_usbphy *sphy, int is_device_mode)
{


	if (is_device_mode) {
		dev_dbg(sphy->dev, "%s device mode\n", __func__);
		
		setphy(sphy, 0xe7, 0x1b);
		setphy(sphy, 0xe7,0x1f);

		udelay(10);

		setphy(sphy, 0xe2,0x48);
		/* setphy(sphy, 0xe5, 0x00); */
		setphy(sphy, 0xe0, 0xa3);
		setphy(sphy, 0x87, 0x1f);
	} else {
		dev_dbg(sphy->dev, "%s host mode\n", __func__);
		
		setphy(sphy, 0xe7, 0x1b);
		setphy(sphy, 0xe7, 0x1f);

		udelay(10);

		setphy(sphy, 0xe2,0x46);
		/* setphy(sphy, 0xe5, 0x00); */
		setphy(sphy, 0xe0, 0xa3);
		setphy(sphy, 0x87, 0x1f);
	}
	
	return 0;
}

int owl_dwc3_usb2phy_param_setup(int is_device_mode)
{
	int ret = 0;

	if (!usb2_sphy)
		return 0;

	if (usb2_sphy->phy_type == USB2PHY_R)
		ret = s700_usb2phy_param_setup(usb2_sphy, is_device_mode);

	if (usb2_sphy->phy_type == USB2PHY_C)
		ret = s900_usb2phy_param_setup(usb2_sphy, is_device_mode);

	return ret;
}
EXPORT_SYMBOL_GPL(owl_dwc3_usb2phy_param_setup);


static int owl_usb2phy_init(struct usb_phy *phy)
{
	return 0;
}

static void owl_usb2phy_shutdown(struct usb_phy *phy)
{
}

static const struct of_device_id owl_usbphy_dt_match[];
static int owl_usb2phy_probe(struct platform_device *pdev)
{
	struct device_node *node;
	const int *phy_type;

	struct owl_usbphy *sphy;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	void __iomem	*phy_base;
	int ret = 0;
	

	node = pdev->dev.of_node;

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap_nocache(dev, phy_mem->start, resource_size(phy_mem));
	if (!phy_base) {
		dev_err(dev, "ioremap failed\n");
		return -ENOMEM;
	}

	sphy = devm_kzalloc(dev, sizeof(*sphy), GFP_KERNEL);
	if (!sphy)
		return -ENOMEM;


	sphy->dev = dev;

	phy_type = of_get_property(node, "phy_type", NULL);
	if (!phy_type)
		pr_info("%s not config usb3phy type.\n", __func__);
	else {
		sphy->phy_type = be32_to_cpup(phy_type);
		pr_info("%s phy_type: %d\n", __func__, sphy->phy_type);
	}

	sphy->regs		= phy_base;
	sphy->phy.dev		= sphy->dev;
	sphy->phy.label		= "owl-usb2phy";
	sphy->phy.init		= owl_usb2phy_init;
	sphy->phy.shutdown	= owl_usb2phy_shutdown;


	platform_set_drvdata(pdev, sphy);

	ATOMIC_INIT_NOTIFIER_HEAD(&sphy->phy.notifier);


	ret = usb_add_phy(&sphy->phy, USB_PHY_TYPE_USB2);
	if(ret) {
		return ret;
	}

	usb2_sphy = sphy;
	
	return ret;
	
}

static int owl_usb2phy_remove(struct platform_device *pdev)
{
	struct owl_usbphy *sphy = platform_get_drvdata(pdev);

	usb2_sphy = 0;
	usb_remove_phy(&sphy->phy);

	return 0;
}

static const struct of_device_id owl_usbphy_dt_match[] = {
	{.compatible = "actions,s700-usb2phy"},
	{.compatible = "actions,s900-usb2phy"},
	{},
};
MODULE_DEVICE_TABLE(of, owl_usbphy_dt_match);

static struct platform_driver owl_usb2phy_driver = {
	.probe		= owl_usb2phy_probe,
	.remove		= owl_usb2phy_remove,
	.driver		= {
		.name	= "owl-usb2phy",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(owl_usbphy_dt_match),
	},
};

module_platform_driver(owl_usb2phy_driver);

MODULE_DESCRIPTION("Actions owl USB 2.0 phy controller");
MODULE_AUTHOR("tangshaoqing <tangshaoqing@actions-semi.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:owl-usb2phy");
