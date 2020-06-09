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

struct owl_usbphy *usb3_sphy;
static bool usb3_usb3phy_type_is_r(void);


static inline u8 owl_phy_readb(void __iomem *base, u32 offset)
{
	return readb(base + offset);
}

static inline void owl_phy_writeb(void __iomem *base, u32 offset, u8 value)
{
	writeb(value, base + offset);
}

static inline u32 owl_phy_readl(void __iomem *base, u32 offset)
{
	return readl(base + offset);
}

static inline void owl_phy_writel(void __iomem *base, u32 offset, u32 value)
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

static int s700_usb3phy_param_setup(struct owl_usbphy *sphy, int is_device_mode)
{
	void __iomem *base = sphy->regs;

	if (is_device_mode) {
		dev_dbg(sphy->dev, "%s device mode\n", __func__);
	} else {
		dev_dbg(sphy->dev, "%s host mode\n", __func__);
		__s700_usb3phy_param_setup(base, 0);
	}

	return 0;
}

static int s700_usb3phy_set_lpfs_det_mode(struct owl_usbphy *sphy,
	int is_device_mode)
{
	void __iomem *base = sphy->regs;
	u32 val;

	if (is_device_mode) {
		dev_dbg(sphy->dev, "%s device mode\n", __func__);
	} else {
		dev_dbg(sphy->dev, "%s host mode\n", __func__);
		owl_phy_writel(base, USB3_ANA0F, (1<<14));

		val = owl_phy_readl(base, USB3_FLD0);
		val &= ~((1<<8) | (1<<15));
		owl_phy_writel(base, USB3_FLD0, val);

		val = owl_phy_readl(base, USB3_PAGE1_REG0B);
		val &= ~(1<<3);
		owl_phy_writel(base, USB3_PAGE1_REG0B, val);
	}

	return 0;
}

int owl_dwc3_usb3phy_param_setup(int is_device_mode)
{
	int ret = 0;

	if (!usb3_sphy)
		return 0;

	if (usb3_sphy->phy_type == USB3PHY_R)
		ret = s700_usb3phy_param_setup(usb3_sphy, is_device_mode);

	return ret;
}
EXPORT_SYMBOL_GPL(owl_dwc3_usb3phy_param_setup);

int owl_dwc3_usb3phy_set_lpfs_det_mode(int is_device_mode)
{
	int ret = 0;

	if (!usb3_sphy)
		return 0;

	if (usb3_sphy->phy_type == USB3PHY_R)
		ret = s700_usb3phy_set_lpfs_det_mode(usb3_sphy, is_device_mode);

	return ret;
}
EXPORT_SYMBOL_GPL(owl_dwc3_usb3phy_set_lpfs_det_mode);

static int s700_usb3phy_init(struct usb_phy *phy)
{
	struct owl_usbphy *sphy = phy_to_sphy(phy);
	void __iomem *base = sphy->regs;

	dev_dbg(phy->dev, "%s %d\n", __func__, __LINE__);

	owl_phy_writel(base, USB3_ANA02, 0x6046);
	owl_phy_writel(base, USB3_ANA0E, 0x2010);
	owl_phy_writel(base, USB3_ANA0F, 0x8000);
	owl_phy_writel(base, USB3_REV1, 0x0);
	owl_phy_writel(base, USB3_PAGE1_REG02, 0x0013);
	owl_phy_writel(base, USB3_PAGE1_REG06, 0x0004);
	owl_phy_writel(base, USB3_PAGE1_REG07, 0x22ed);
	owl_phy_writel(base, USB3_PAGE1_REG08, 0xf802);
	owl_phy_writel(base, USB3_PAGE1_REG09, 0x3080);
	owl_phy_writel(base, USB3_PAGE1_REG0B, 0x2030);
	owl_phy_writel(base, USB3_ANA0F, (1<<14));

	return 0;
}

#define     USB3_TX_DATA_PATH_CTRL   (0X5D)
#define     USB3_RX_DATA_PATH_CTRL1  (0X87)

static int s900_usb3phy_init(struct usb_phy *phy)
{
	struct owl_usbphy *sphy = phy_to_sphy(phy);
	void __iomem *base = sphy->regs;
	u8		reg;
	u32		offset;


	/* IO_OR_U8(USB3_TX_DATA_PATH_CTRL, 0x02); */
	offset = USB3_TX_DATA_PATH_CTRL;
	reg = owl_phy_readb(base, offset);
	reg |= 0x02;
	owl_phy_writeb(base, offset, reg);
	dev_dbg(phy->dev, "%s 0x%x:0x%x\n", __func__,
		offset, owl_phy_readb(base, offset));

	/* IO_OR_U8(USB3_RX_DATA_PATH_CTRL1, 0x20); */
	offset = USB3_RX_DATA_PATH_CTRL1;
	reg = owl_phy_readb(base, offset);
	reg |= 0x20;
	owl_phy_writeb(base, offset, reg);
	dev_dbg(phy->dev, "%s 0x%x:0x%x\n", __func__,
		offset, owl_phy_readb(base, offset));

	return 0;
}

static int owl_usb3phy_init(struct usb_phy *phy)
{
	struct owl_usbphy *sphy = phy_to_sphy(phy);

	if (sphy->phy_type == USB3PHY_C)
		s900_usb3phy_init(phy);
	else if (sphy->phy_type == USB3PHY_R)
		s700_usb3phy_init(phy);

	return 0;
}

static void owl_usb3phy_shutdown(struct usb_phy *phy)
{
}

static bool usb3_usb3phy_type_is_r(void)
{
	if (usb3_sphy->phy_type == USB3PHY_R)
		return true;
	else
		return false;
}



bool usb3_support_runtime_pm(void)
{
	if (usb3_usb3phy_type_is_r())
		return false;
	else
		return true;
}
EXPORT_SYMBOL_GPL(usb3_support_runtime_pm);

bool usb3_need_double_check_inactive(void)
{
	if (usb3_usb3phy_type_is_r())
		return true;
	else
		return false;
}
EXPORT_SYMBOL_GPL(usb3_need_double_check_inactive);

bool xhci_need_phy_config(void)
{
	if (usb3_usb3phy_type_is_r())
		return true;
	else
		return false;
}
EXPORT_SYMBOL_GPL(xhci_need_phy_config);

bool usb3_need_set_device_noattached(void)
{
	if (usb3_usb3phy_type_is_r())
		return true;
	else
		return false;
}
EXPORT_SYMBOL_GPL(usb3_need_set_device_noattached);

static int fix_vbus_reset;
bool usb_need_fix_vbus_reset(void)
{
       if (fix_vbus_reset == 1)
               return true;

       return false;
}
EXPORT_SYMBOL_GPL(usb_need_fix_vbus_reset);

static const struct of_device_id owl_usbphy_dt_match[];
static int owl_usb3phy_probe(struct platform_device *pdev)
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

        phy_type = of_get_property(node, "fix_vbus_reset", NULL);
        if (!phy_type)
                pr_info("%s no config fix_vbus_reset.\n", __func__);
        else {
                fix_vbus_reset = be32_to_cpup(phy_type);
                pr_info("%s fix_vbus_reset: %d\n", __func__, fix_vbus_reset);
        }

	sphy->regs		= phy_base;
	sphy->phy.dev		= sphy->dev;
	sphy->phy.label		= "owl-usb3phy";
	sphy->phy.init		= owl_usb3phy_init;
	sphy->phy.shutdown	= owl_usb3phy_shutdown;

	platform_set_drvdata(pdev, sphy);

	ret = usb_add_phy(&sphy->phy, USB_PHY_TYPE_USB3);
	if (ret)
		return ret;

	usb3_sphy = sphy;

	return ret;
}

static int owl_usb3phy_remove(struct platform_device *pdev)
{
	struct owl_usbphy *sphy = platform_get_drvdata(pdev);

	usb_remove_phy(&sphy->phy);

	return 0;
}

static const struct of_device_id owl_usbphy_dt_match[] = {
	{.compatible = "actions,s700-usb3phy"},
	{.compatible = "actions,s900-usb3phy"},
	{},
};
MODULE_DEVICE_TABLE(of, owl_usbphy_dt_match);

static struct platform_driver owl_usb3phy_driver = {
	.probe		= owl_usb3phy_probe,
	.remove		= owl_usb3phy_remove,
	.driver		= {
		.name	= "owl-usb3phy",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(owl_usbphy_dt_match),
	},
};

module_platform_driver(owl_usb3phy_driver);

MODULE_DESCRIPTION("Actions owl USB 3.0 phy controller");
MODULE_AUTHOR("tangshaoqing <tangshaoqing@actions-semi.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:owl-usb3phy");
