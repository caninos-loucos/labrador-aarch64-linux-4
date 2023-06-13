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
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <linux/of_device.h>

#include "phy-caninos-usb.h"


#define USB3_TX_DATA_PATH_CTRL   (0X5D)
#define USB3_RX_DATA_PATH_CTRL1  (0X87)

#define USB3_ANA00	(0x00)
#define USB3_ANA01	(0x04)
#define USB3_ANA02	(0x08)
#define USB3_ANA03	(0x0C)
#define USB3_ANA04	(0x10)
#define USB3_ANA05	(0x14)
#define USB3_ANA06	(0x18)
#define USB3_ANA07	(0x1C)
#define USB3_ANA08	(0x20)
#define USB3_ANA09	(0x24)
#define USB3_ANA0A	(0x28)
#define USB3_ANA0B	(0x2C)
#define USB3_ANA0C	(0x30)
#define USB3_ANA0D	(0x34)
#define USB3_ANA0E	(0x38)
#define USB3_ANA0F	(0x3C)

#define USB3_DMR		(0x40)
#define USB3_BACR		(0x44)
#define USB3_IER		(0x48)
#define USB3_BCSR		(0x4C)
#define USB3_BPR		(0x50)
#define USB3_BPNR2		(0x54)
#define USB3_BFNR		(0x58)
#define USB3_BRNR2		(0x5C)
#define USB3_BENR		(0x60)
#define USB3_REV0		(0x64)
#define USB3_REV1		(0x68)
#define USB3_REV2		(0x6C)
#define USB3_REV3		(0x70)
#define USB3_FLD0		(0x74)
#define USB3_FLD1		(0x78)
#define USB3_ANA1F		(0x7C)

#define USB3_PAGE1_REG00		(0x80)
#define USB3_PAGE1_REG01		(0x84)
#define USB3_PAGE1_REG02		(0x88)
#define USB3_PAGE1_REG03		(0x8C)
#define USB3_PAGE1_REG04		(0x90)
#define USB3_PAGE1_REG05		(0x94)
#define USB3_PAGE1_REG06		(0x98)
#define USB3_PAGE1_REG07		(0x9C)
#define USB3_PAGE1_REG08		(0xA0)
#define USB3_PAGE1_REG09		(0xA4)
#define USB3_PAGE1_REG0A		(0xA8)
#define USB3_PAGE1_REG0B		(0xAC)
#define USB3_PAGE1_REG0C		(0xB0)
#define USB3_PAGE1_REG0D		(0xB4)
#define USB3_PAGE1_REG0E		(0xB8)
#define USB3_PAGE1_REG0F		(0xBC)
#define USB3_PAGE1_REG10		(0xC0)


static inline u8 caninos_phy_readb(void __iomem *base, u32 offset)
{
	return readb(base + offset);
}

static inline void caninos_phy_writeb(void __iomem *base, u32 offset, u8 value)
{
	writeb(value, base + offset);
}

static inline u32 caninos_phy_readl(void __iomem *base, u32 offset)
{
	return readl(base + offset);
}

static inline void caninos_phy_writel(void __iomem *base, u32 offset, u32 value)
{
	writel(value, base + offset);
}

static void set_reg32_val(void __iomem *base , unsigned int mask,
				unsigned int val, unsigned int reg)
{
	unsigned int tmp;

	tmp = readl(base + reg);
	tmp &= ~(mask);
	val &= mask;
	tmp |= val;
	writel(tmp, base +  reg);
}

static void  __s700_usb3phy_rx_init(void __iomem *base)
{
	/* modified by lchen */
	/* for lowest OOBS_sensitivity */
	set_reg32_val(base, 0x1f<<1, 0x16<<1, USB3_ANA03);
	/* for lowest OOBS_sensitivity */
	writel(0x9555, base +	USB3_ANA04);

	/* for proper DC gain */
	writel(0x2d91, base + USB3_ANA08);
	/* for IB_TX and IB_OOBS */
	set_reg32_val(base, 0x7<<12, 0x3<<12, USB3_ANA09);
	/* for CDR_PI and RX_Z0	*/
	writel(0xFF68, base + USB3_ANA0D);
	/* for CDR and PI in 0x1B---[0xFF08--BAD] */
	writel(0xFF0C, base + USB3_REV2);
	/* for CP0 selected */
	set_reg32_val(base, 0x7<<2, 0x7<<2, USB3_REV3);

	/* for disable BER checker */
	set_reg32_val(base, 0x1<<0, 0x0<<0, USB3_FLD0);

	/* for 0x20 TX_DRV */
	writel(0xD4FF, base + USB3_PAGE1_REG00);
	/* TX_DRV */
	writel(0xAAFF, base + USB3_PAGE1_REG01);
	/* TX_DRV and BER ckecker sel */
	writel(0x0051, base + USB3_PAGE1_REG02);
	/* TX_DRV */
	writel(0xDB60, base + USB3_PAGE1_REG03);

	/* for force offset value */
	set_reg32_val(base, 0xf<<5, 0x6<<5, USB3_ANA0B);
	set_reg32_val(base, 0x1<<13, 0x0<<13, USB3_ANA0D);

	/* APHY Dbg Switch */
	/* for TX_TEST_EN=1 */
	set_reg32_val(base,  1<<0, 1<<0, USB3_ANA0D);
	mdelay(5);

}

static void __s700_usb3phy_analog_setting(void __iomem *base)
{
	writel(0x8000, base + USB3_ANA0F);
	writel(0x6046, base + USB3_ANA02);

	writel(0x0, base + USB3_REV1);
	writel(0x0013, base + USB3_PAGE1_REG02);
	writel(0x0004, base + USB3_PAGE1_REG06);
	writel(0x22ed, base + USB3_PAGE1_REG07);
	writel(0xf802, base + USB3_PAGE1_REG08);
	writel(0x3080, base + USB3_PAGE1_REG09);
	writel(0x2030, base + USB3_PAGE1_REG0B);
	mdelay(5);

	__s700_usb3phy_rx_init(base);

	/* bit4: [0=cmfb mode=usingRxDetect]  [1=opab  mode=Nomal Using] */
	set_reg32_val(base, ((1<<5)|(1<<4)), ((1<<5)|(0<<4)), USB3_ANA0E);
	/* Tx_SCE_VCM=RxDetec compareVoltage---20150721 for [samsung hard disk] */
	set_reg32_val(base, (3<<12), (2<<12), USB3_ANA0E);
	/* RxSel---time */
	set_reg32_val(base, (3<<2), (3<<2), USB3_ANA0C);
	set_reg32_val(base, (1<<15), (0<<15), USB3_FLD0);

	/* Rx DC gain---20150721 for [transcend udisk log:Cannot enable port 1.  Maybe the USB cable is bad?] */
	set_reg32_val(base, (3<<0), (3<<0), USB3_ANA08);
	mdelay(5);
}

static void ss_phy_check700a(void __iomem *base)
{
	/* 700A setting */
	writel(0x00004000, (base+0x00));
	writel(0x0000E109, (base+0x04));
	writel(0x00006A46, (base+0x08));

	writel(0x00009555, (base+0x10));

	writel(0x0000B623, (base+0x28));
	writel(0x0000A927, (base+0x2c));

	writel(0x00004000, (base+0x30));
	writel(0x0000F35D, (base+0x34));

	writel(0x00003804, (base+0x64));

	writel(0x0000203F, (base+0x74));

	writel(0x00000006, (base+0x98));

	writel(0x00000000, (base+0xb0));
	writel(0x000000FF, (base+0xb4));

	/*
	 * there is no need to sync from 700C,
	 * for 700D, it is 0x000082D0, it's OK.
	 */
	/* writel(0x000082C0,(0xe040ce04)); */
	mdelay(5);

}

static void __s700_usb3phy_param_setup(void __iomem *base, int is_device_mode)
{
	/*
	 * There is need to delay 5ms here, or some usb3.0 u-disks
	 * will enter compliance state when plugged in.
	 * There are many delay operations during usb3phy setup.
	 * Have not found which one is indispensable yet, but they
	 * really work a lot.
	 */
	mdelay(5);
	__s700_usb3phy_analog_setting(base);

	/* must bit3=0---20140925 */
	set_reg32_val(base, ((1<<5)|(1<<3)), ((1<<5)|(0<<3)), USB3_PAGE1_REG0B);
	/* must bit7=0---20140925 */
	set_reg32_val(base, (1<<7), (0<<7), USB3_PAGE1_REG0A);
	set_reg32_val(base, (0xF<<2), (0x0<<2), USB3_ANA0D);
	set_reg32_val(base, (1<<3), (0<<3), USB3_PAGE1_REG0B);
	set_reg32_val(base, ((1<<8)|(1<<10)), ((0<<8)|(0<<10)), USB3_FLD1);
	set_reg32_val(base, (1<<7), (0<<7), USB3_PAGE1_REG0A);

	/* enable SSC for reduce EMI----20150810 */
	set_reg32_val(base, (1<<13), (1<<13), USB3_ANA04);
	mdelay(5);

	ss_phy_check700a(base);
}

static int s700_usb3phy_param_setup(struct caninos_usbphy *cphy, int is_device_mode)
{
	void __iomem *base = cphy->regs;

	if (is_device_mode) {
		dev_info(cphy->dev, "%s device mode\n", __func__);
		__s700_usb3phy_param_setup(base, 1);
	} else {
		dev_info(cphy->dev, "%s host mode\n", __func__);
		__s700_usb3phy_param_setup(base, 0);
	}

	return 0;
}

static int s700_usb3phy_set_lpfs_det_mode(struct caninos_usbphy *cphy,
	int is_device_mode)
{
	void __iomem *base = cphy->regs;
	u32 val;

	if (is_device_mode) {
		dev_info(cphy->dev, "%s device mode\n", __func__);
	} else {
		dev_info(cphy->dev, "%s host mode\n", __func__);
		caninos_phy_writel(base, USB3_ANA0F, (1<<14));

		val = caninos_phy_readl(base, USB3_FLD0);
		val &= ~((1<<8) | (1<<15));
		caninos_phy_writel(base, USB3_FLD0, val);

		val = caninos_phy_readl(base, USB3_PAGE1_REG0B);
		val &= ~(1<<3);
		caninos_phy_writel(base, USB3_PAGE1_REG0B, val);
	}

	return 0;
}

static int caninos_k7_usb3phy_set_mode(struct phy *phy, enum phy_mode mode)
{
    struct caninos_usbphy *cphy = phy_to_cphy(&phy);

    switch(mode){
        case PHY_MODE_USB_DEVICE:
	        s700_usb3phy_set_lpfs_det_mode(cphy, 1);
            s700_usb3phy_param_setup(cphy, 1);
        case PHY_MODE_USB_HOST:
	        s700_usb3phy_set_lpfs_det_mode(cphy, 0);
            s700_usb3phy_param_setup(cphy, 0);
        default:
	        s700_usb3phy_set_lpfs_det_mode(cphy, 0);
            s700_usb3phy_param_setup(cphy, 0);
    }
	return 0;
}


static int caninos_k7_usb3phy_init(struct phy *phy)
{
	struct caninos_usbphy *cphy = phy_to_cphy(&phy);
	void __iomem *base = cphy->regs;

	caninos_phy_writel(base, USB3_ANA02, 0x6046);
	caninos_phy_writel(base, USB3_ANA0E, 0x2010);
	caninos_phy_writel(base, USB3_ANA0F, 0x8000);
	caninos_phy_writel(base, USB3_REV1, 0x0);
	caninos_phy_writel(base, USB3_PAGE1_REG02, 0x0013);
	caninos_phy_writel(base, USB3_PAGE1_REG06, 0x0004);
	caninos_phy_writel(base, USB3_PAGE1_REG07, 0x22ed);
	caninos_phy_writel(base, USB3_PAGE1_REG08, 0xf802);
	caninos_phy_writel(base, USB3_PAGE1_REG09, 0x3080);
	caninos_phy_writel(base, USB3_PAGE1_REG0B, 0x2030);
	caninos_phy_writel(base, USB3_ANA0F, (1<<14));

	return 0;
}

static int caninos_k7_usb3phy_exit(struct phy *phy)
{
    return 0;
}

static struct phy *caninos_usb3phy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct caninos_usbphy *drv;

	drv = dev_get_drvdata(dev);
	if (!drv)
		return ERR_PTR(-EINVAL);

	return drv->phy;
}


static const struct phy_ops caninos_usb3_phy_ops = {
	.init = caninos_k7_usb3phy_init,
	.exit = caninos_k7_usb3phy_exit,
	.set_mode = caninos_k7_usb3phy_set_mode,
	.owner = THIS_MODULE,
};

static int caninos_usb3phy_probe(struct platform_device *pdev)
{
	struct caninos_usbphy *cphy;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	struct phy_provider *phy_provider;
	void __iomem	*phy_base;
	
	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap_resource(dev, phy_mem);
	if (IS_ERR(phy_base)) {
		dev_err(dev, "ioremap failed\n");
		return PTR_ERR(phy_base);
	}

	cphy = devm_kzalloc(dev, sizeof(*cphy), GFP_KERNEL);
	if (!cphy)
		return -ENOMEM;

	cphy->dev = dev;
	cphy->regs = phy_base;

	cphy->phy = devm_phy_create(dev, NULL, &caninos_usb3_phy_ops);
	
	if(IS_ERR(cphy->phy)) {
		dev_err(dev, "Failed to create usb3phy\n");
		return PTR_ERR(cphy->phy);
	}

	dev_set_drvdata(dev, cphy);

	phy_provider = devm_of_phy_provider_register(dev,
							caninos_usb3phy_xlate);
	if (IS_ERR(phy_provider)) {
		dev_err(dev, "Failed to register phy provider\n");
		return PTR_ERR(phy_provider);
	}


	dev_info(dev, "testing init");

	cphy->phy->ops->init(cphy->phy);

	dev_info(dev, "probe finished");
		
	return 0;	
}

static int caninos_usb3phy_remove(struct platform_device *pdev)
{
	struct caninos_usbphy *cphy = platform_get_drvdata(pdev);
	devm_phy_destroy(cphy->dev, cphy->phy);
	return 0;
}

static const struct of_device_id caninos_usbphy_dt_match[] = {
	{.compatible = "caninos,k7-usb3phy"},
	{},
};
MODULE_DEVICE_TABLE(of, caninos_usbphy_dt_match);

static struct platform_driver caninos_usb3phy_driver = {
	.probe		= caninos_usb3phy_probe,
	.remove		= caninos_usb3phy_remove,
	.driver		= {
		.name	= "caninos-usb3phy",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(caninos_usbphy_dt_match),
	},
};

module_platform_driver(caninos_usb3phy_driver);

MODULE_DESCRIPTION("Caninos USB 3.0 phy controller");
MODULE_AUTHOR("Ana Clara Forcelli <ana.forcelli@lsitec.org.br");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:caninos-usb2phy");
