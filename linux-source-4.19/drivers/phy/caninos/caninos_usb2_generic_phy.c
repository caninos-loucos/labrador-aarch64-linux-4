/*
 * Caninos SoCs phy driver
 *
 * Copyright (c) 2015 Actions Semiconductor Co., ltd.
 * tangshaoqing <tangshaoqing@actions-semi.com>
 * 
 * Copyright (c) 2023  LSI-TEC
 * Ana Clara Forcelli <ana.forcelli@lsitec.org.br>
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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include "phy-caninos-usb.h"

static void setphy(struct caninos_usbphy *cphy, unsigned char reg_add, unsigned char value)
{
	volatile unsigned char addr_low;
	volatile unsigned char addr_high;
	volatile unsigned int vstate;

	addr_low =  reg_add & 0x0f;
	addr_high =  (reg_add >> 4) & 0x0f;

	vstate = value;
	vstate = vstate << 8;

	addr_low |= 0x10;
	writel(vstate | addr_low, cphy->regs);
	mb();

	addr_low &= 0x0f; 
	writel(vstate | addr_low, cphy->regs);
	mb();

	addr_low |= 0x10;
	writel(vstate | addr_low, cphy->regs);
	mb();

	addr_high |= 0x10;
	writel(vstate | addr_high, cphy->regs);
	mb();

	addr_high &= 0x0f; 
	writel(vstate | addr_high, cphy->regs);
	mb();

	addr_high |= 0x10;
	writel(vstate | addr_high, cphy->regs);  
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

static int s700_usb2phy_param_setup(struct caninos_usbphy *cphy, int is_device_mode)
{
	unsigned char val_u8, slew_rate;
	int value;
	slew_rate = dwc3_get_slewrate_config();

	if (is_device_mode) {
		//dev_info(cphy->dev, "%s device mode\n", __func__);

		val_u8 = (1<<7)|(1<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(cphy, 0xf4, val_u8);
		val_u8 = (0x6<<5)|(0<<4)|(1<<3)|(1<<2)|(3<<0);
		setphy(cphy, 0xe1, val_u8);

		val_u8 = (1<<7)|(0<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(cphy, 0xf4, val_u8);
		val_u8 = (1<<7)|(4<<4)|(1<<3)|(1<<2)|(0<<0);
		setphy(cphy, 0xe6, val_u8);

		val_u8 = (1<<7)|(1<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(cphy, 0xf4, val_u8);
		setphy(cphy, 0xe2, 0x2);
		setphy(cphy, 0xe2, 0x12);

		val_u8 = (1<<7)|(0<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(cphy, 0xf4, val_u8);
		val_u8 = (0xa<<4)|(0<<1)|(1<<0);
		setphy(cphy, 0xe7, val_u8);

		val_u8 = (1<<7)|(1<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(cphy, 0xf4, val_u8);
		setphy(cphy, 0xe0, 0x31);
		setphy(cphy, 0xe0, 0x35);

		val_u8 = (1<<7)|(0<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(cphy, 0xf4, val_u8);

		value = usb_current_calibrate(1);
		//dev_info(cphy->dev, "USB3 CURRENT value: 0x%x\n", value);

		val_u8 = (0xa<<4) | (value<<0);
		setphy(cphy, 0xe4, val_u8);
	} else {
		//dev_info(cphy->dev, "%s host mode\n", __func__);

		val_u8 = (1<<7)|(1<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(cphy, 0xf4, val_u8);
		val_u8 = (0x6<<5)|(0<<4)|(1<<3)|(1<<2)|(3<<0);
		setphy(cphy, 0xe1, val_u8);

		val_u8 = (1<<7)|(0<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(cphy, 0xf4, val_u8);
		val_u8 = (1<<7)|(4<<4)|(1<<3)|(1<<2)|(0<<0);
		setphy(cphy, 0xe6, val_u8);

		val_u8 = (1<<7)|(1<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(cphy, 0xf4, val_u8);
		setphy(cphy, 0xe2, 0x2);
		setphy(cphy, 0xe2, 0x16);

		val_u8 = (1<<7)|(0<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(cphy, 0xf4, val_u8);
		val_u8 = (0xa<<4)|(0<<1)|(1<<0);
		setphy(cphy, 0xe7, val_u8);

		val_u8 = (1<<7)|(1<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(cphy, 0xf4, val_u8);
		setphy(cphy, 0xe0, 0x31);
		setphy(cphy, 0xe0, 0x35);

		val_u8 = (1<<7)|(0<<5)|(1<<4)|(2<<2)|(3<<0);
		setphy(cphy, 0xf4, val_u8);

		value = usb_current_calibrate(0);
		//dev_info(cphy->dev, "USB3 CURRENT value: 0x%x\n", value);

		val_u8 = (0xa<<4) | (value<<0);
		setphy(cphy, 0xe4, val_u8);

		val_u8 = (1<<7)|(1<<6)|(1<<5)|(1<<4)|
			(1<<3)|(1<<2)|(0<<1)|(0<<0);
		setphy(cphy, 0xf0, val_u8);
	}

	return 0;
} 

static int caninos_k7_usb2phy_set_mode(struct phy *phy, enum phy_mode mode)
{
    struct caninos_usbphy *cphy = phy_to_cphy(&phy);

    switch(mode){
        case PHY_MODE_USB_DEVICE:
            s700_usb2phy_param_setup(cphy, 1);
        case PHY_MODE_USB_HOST:
            s700_usb2phy_param_setup(cphy, 0);
        default:
            s700_usb2phy_param_setup(cphy, 0);
    }
	return 0;
}


static int caninos_k7_usb2phy_init(struct phy *phy)
{
	return 0;
}

static int caninos_k7_usb2phy_exit(struct phy *phy)
{
	return 0;
}

static struct phy *caninos_usb2phy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct caninos_usbphy *drv;

	drv = dev_get_drvdata(dev);
	if (!drv)
		return ERR_PTR(-EINVAL);

	return drv->phy;
}


static const struct phy_ops caninos_usb2_phy_ops = {
	.init = caninos_k7_usb2phy_init,
	.exit = caninos_k7_usb2phy_exit,
	.set_mode = caninos_k7_usb2phy_set_mode,
	.owner = THIS_MODULE,
};

static int caninos_usb2phy_probe(struct platform_device *pdev)
{

	struct caninos_usbphy *cphy;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	void __iomem	*phy_base;
	struct phy_provider *phy_provider;	
	int ret = 0;

	cphy = devm_kzalloc(dev, sizeof(*cphy), GFP_KERNEL);
	if (!cphy)
		return -ENOMEM;

	cphy->dev = dev;

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap(dev, phy_mem->start, resource_size(phy_mem));
	if (!phy_base) {
		dev_err(dev, "ioremap failed\n");
		return -ENOMEM;
	}

	cphy->regs = phy_base;

	cphy->phy = devm_phy_create(dev, NULL, &caninos_usb2_phy_ops);
	
	if(IS_ERR(cphy->phy)) {
		dev_err(dev, "Failed to create usb2phy\n");
		return PTR_ERR(cphy->phy);
	}

	dev_set_drvdata(dev, cphy);

	phy_provider = devm_of_phy_provider_register(dev,
							caninos_usb2phy_xlate);
	if (IS_ERR(phy_provider)) {
		dev_err(dev, "Failed to register usb2 phy provider\n");
		return PTR_ERR(phy_provider);
	}

	dev_info(dev, "testing mode");

	//cphy->phy->ops->set_mode(cphy->phy, PHY_MODE_USB_HOST);
	caninos_k7_usb2phy_set_mode(cphy->phy, PHY_MODE_USB_HOST);

	dev_info(dev, "probe finished");
		
	return ret;	
}

static int caninos_usb2phy_remove(struct platform_device *pdev)
{
	struct caninos_usbphy *cphy = platform_get_drvdata(pdev);
	devm_phy_destroy(cphy->dev, cphy->phy);
	return 0;
}

static const struct of_device_id caninos_usbphy_dt_match[] = {
	{.compatible = "caninos,k7-usb2phy"},
	{},
};

MODULE_DEVICE_TABLE(of, caninos_usbphy_dt_match);

static struct platform_driver caninos_usb2phy_driver = {
	.probe		= caninos_usb2phy_probe,
	.remove		= caninos_usb2phy_remove,
	.driver		= {
		.name	= "caninos-usb2phy",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(caninos_usbphy_dt_match),
	},
};

module_platform_driver(caninos_usb2phy_driver);

MODULE_DESCRIPTION("Caninos USB 2.0 phy controller");
MODULE_AUTHOR("Ana Clara Forcelli <ana.forcelli@lsitec.org.br");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:caninos-usb2phy");
