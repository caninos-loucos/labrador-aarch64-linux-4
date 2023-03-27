// SPDX-License-Identifier: GPL-2.0
/*
 * Caninos Reset driver
 *
 * Copyright (c) 2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2019 LSI-TEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2012 Actions Semi Inc.
 * Author: Actions Semi, Inc.
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
#include <linux/reset-controller.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <dt-bindings/reset/caninos-rst.h>

#define DRIVER_NAME "caninos-reset"
#define DRIVER_DESC "Caninos Labrador Reset Controller Driver"

#define DEVRST0 0x00
#define DEVRST1 0x04

struct caninos_rcu_reset_reg_data
{
	u32 offset;
	u32 deassert;
	u32 assert;
	u32 mask;
};

#define CANINOS_RST_REG_DATA(_off,_deassert) \
	{ .offset = _off, .deassert = _deassert, .assert = 0x0, .mask = _deassert }

static struct caninos_rcu_reset_reg_data k5_reg_data[] =
{
	[RST_UART0]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(5)),
	[RST_UART1]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(6)),
	[RST_UART2]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(7)),
	[RST_UART3]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(15)),
	[RST_UART4]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(16)),
	[RST_UART5]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(17)),
	[RST_UART6]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(4)),
	[RST_SDC0]     = CANINOS_RST_REG_DATA(DEVRST0, BIT(4)),
	[RST_SDC1]     = CANINOS_RST_REG_DATA(DEVRST0, BIT(5)),
	[RST_SDC2]     = CANINOS_RST_REG_DATA(DEVRST0, BIT(9)),
	[RST_HDCP2]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(3)),
	[RST_USBH0]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(0)),
	[RST_USBH1]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(22)),
	[RST_PCM1]     = CANINOS_RST_REG_DATA(DEVRST0, BIT(6)),
	[RST_PCM0]     = CANINOS_RST_REG_DATA(DEVRST0, BIT(18)),
	[RST_AUDIO]    = CANINOS_RST_REG_DATA(DEVRST0, BIT(17)),
	[RST_ETHERNET] = CANINOS_RST_REG_DATA(DEVRST1, BIT(20)),
	[RST_VDE]      = CANINOS_RST_REG_DATA(DEVRST0, BIT(19)),
	[RST_VCE]      = CANINOS_RST_REG_DATA(DEVRST0, BIT(20)),
	[RST_GPU3D]    = CANINOS_RST_REG_DATA(DEVRST0, BIT(22)),
	[RST_TVOUT]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(1)),
	[RST_HDMI]     = CANINOS_RST_REG_DATA(DEVRST1, BIT(2)),
	[RST_DE]       = CANINOS_RST_REG_DATA(DEVRST0, BIT(7)),
	[RST_USB3]     = CANINOS_RST_REG_DATA(DEVRST1, BIT(14)),
	[RST_TWI0]     = CANINOS_RST_REG_DATA(DEVRST1, BIT(12)),
	[RST_TWI1]     = CANINOS_RST_REG_DATA(DEVRST1, BIT(13)),
	[RST_TWI2]     = CANINOS_RST_REG_DATA(DEVRST1, BIT(18)),
	[RST_TWI3]     = CANINOS_RST_REG_DATA(DEVRST1, BIT(19)),
};

static struct caninos_rcu_reset_reg_data k7_reg_data[] = 
{
	[RST_UART0]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(8)),
	[RST_UART1]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(9)),
	[RST_UART2]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(10)),
	[RST_UART3]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(11)),
	[RST_UART4]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(12)),
	[RST_UART5]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(13)),
	[RST_UART6]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(14)),
	[RST_SDC0]     = CANINOS_RST_REG_DATA(DEVRST0, BIT(22)),
	[RST_SDC1]     = CANINOS_RST_REG_DATA(DEVRST0, BIT(23)),
	[RST_SDC2]     = CANINOS_RST_REG_DATA(DEVRST0, BIT(24)),
	[RST_HDCP2]    = CANINOS_RST_REG_DATA(DEVRST0, BIT(6)),
	[RST_USBH0]    = CANINOS_RST_REG_DATA(DEVRST0, BIT(26)),
	[RST_USBH1]    = CANINOS_RST_REG_DATA(DEVRST0, BIT(27)),
	[RST_PCM1]     = CANINOS_RST_REG_DATA(DEVRST1, BIT(31)),
	[RST_PCM0]     = CANINOS_RST_REG_DATA(DEVRST1, BIT(30)),
	[RST_AUDIO]    = CANINOS_RST_REG_DATA(DEVRST1, BIT(29)),
	[RST_ETHERNET] = CANINOS_RST_REG_DATA(DEVRST1, BIT(23)),
	[RST_VDE]      = CANINOS_RST_REG_DATA(DEVRST0, BIT(10)),
	[RST_VCE]      = CANINOS_RST_REG_DATA(DEVRST0, BIT(11)),
	[RST_GPU3D]    = CANINOS_RST_REG_DATA(DEVRST0, BIT(8)),
	[RST_TVOUT]    = CANINOS_RST_REG_DATA(DEVRST0, BIT(3)),
	[RST_HDMI]     = CANINOS_RST_REG_DATA(DEVRST0, BIT(5)),
	[RST_DE]       = CANINOS_RST_REG_DATA(DEVRST0, BIT(0)),
	[RST_USB3]     = CANINOS_RST_REG_DATA(DEVRST0, BIT(25)),
	[RST_TWI0]     = CANINOS_RST_REG_DATA(DEVRST1, BIT(0)),
	[RST_TWI1]     = CANINOS_RST_REG_DATA(DEVRST1, BIT(1)),
	[RST_TWI2]     = CANINOS_RST_REG_DATA(DEVRST1, BIT(2)),
	[RST_TWI3]     = CANINOS_RST_REG_DATA(DEVRST1, BIT(3)),
};

struct caninos_rcu_reset_priv
{
	struct reset_controller_dev rcdev;
	struct device *dev;
	void __iomem *cmu_base;
	const struct caninos_rcu_reset_reg_data *data;
	spinlock_t lock;
};

static struct caninos_rcu_reset_priv *to_caninos_rcu_reset_priv
	(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct caninos_rcu_reset_priv, rcdev);
}

static int caninos_rcu_reset_status(struct reset_controller_dev *rcdev,
                                    unsigned long id)
{
	struct caninos_rcu_reset_priv *priv = to_caninos_rcu_reset_priv(rcdev);
	u32 val, deassert_val, assert_val;
	unsigned long flags;
	
	spin_lock_irqsave(&priv->lock, flags);
	val = readl(priv->cmu_base + priv->data[id].offset);
	spin_unlock_irqrestore(&priv->lock, flags);
	
	val &= priv->data[id].mask;
	assert_val = priv->data[id].assert & priv->data[id].mask;
	deassert_val = priv->data[id].deassert & priv->data[id].mask;
	
	if (val == assert_val) {
		return 1;
	}
	else if (val == deassert_val) {
		return 0;
	}
	return -EINVAL;
}

static int caninos_rcu_reset_update(struct reset_controller_dev *rcdev,
                                    unsigned long id, bool assert)
{
	struct caninos_rcu_reset_priv *priv = to_caninos_rcu_reset_priv(rcdev);
	unsigned long flags;
	u32 val, new_val;
	
	spin_lock_irqsave(&priv->lock, flags);
	val = readl(priv->cmu_base + priv->data[id].offset);
	new_val = val & ~(priv->data[id].mask);
	
	if (assert) {
		new_val |= priv->data[id].assert & priv->data[id].mask;
	}
	else {
		new_val |= priv->data[id].deassert & priv->data[id].mask;
	}
	
	if (val != new_val) {
		writel(new_val, priv->cmu_base + priv->data[id].offset);
	}
	
	val = readl(priv->cmu_base + priv->data[id].offset);
	spin_unlock_irqrestore(&priv->lock, flags);
	
	if (val != new_val) {
		return -EINVAL;
	}
	return 0;
}

static int caninos_rcu_reset_assert(struct reset_controller_dev *rcdev,
                                    unsigned long id)
{
	return caninos_rcu_reset_update(rcdev, id, true);
}

static int caninos_rcu_reset_deassert(struct reset_controller_dev *rcdev,
                                      unsigned long id)
{
	return caninos_rcu_reset_update(rcdev, id, false);
}

static int caninos_rcu_reset_reset(struct reset_controller_dev *rcdev,
                                   unsigned long id)
{
	int ret = caninos_rcu_reset_assert(rcdev, id);
	
	if (ret) {
		return ret;
	}
	
	return caninos_rcu_reset_deassert(rcdev, id);
}

static const struct reset_control_ops caninos_rcu_reset_ops = {
	.assert   = caninos_rcu_reset_assert,
	.deassert = caninos_rcu_reset_deassert,
	.status   = caninos_rcu_reset_status,
	.reset    = caninos_rcu_reset_reset,
};

static const struct of_device_id caninos_rcu_reset_dt_ids[] = {
	{ .compatible = "caninos,k5-reset", .data = (void*) &k5_reg_data },
	{ .compatible = "caninos,k7-reset", .data = (void*) &k7_reg_data },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, caninos_rcu_reset_dt_ids);

static int caninos_rcu_reset_probe(struct platform_device *pdev)
{
	struct caninos_rcu_reset_priv *priv;
	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	struct resource *res;
	u32 aux;
	int ret;
	
	of_id = of_match_node(caninos_rcu_reset_dt_ids, dev->of_node);
	
	if (!of_id)
	{
		dev_err(dev, "could not match device type\n");
		return -ENODEV;
	}
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res)
	{
		dev_err(dev, "could not get register base from DTS\n");
		return -ENOMEM;
	}
	
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	
	if (IS_ERR(priv))
	{
		dev_err(dev, "could not allocate private memory\n");
		return PTR_ERR(priv);
	}
	
	priv->dev = dev;
	
	spin_lock_init(&priv->lock);
	
	priv->rcdev.ops = &caninos_rcu_reset_ops;
	priv->rcdev.owner = THIS_MODULE;
	priv->rcdev.of_node = dev->of_node;
	priv->rcdev.nr_resets = NR_RESETS;
	priv->data = of_id->data;
	
	if (!priv->data)
	{
		dev_err(dev, "invalid device specific data\n");
		return -EINVAL;
	}
	
	priv->cmu_base = devm_ioremap(dev, res->start, resource_size(res));
	
	if (IS_ERR(priv->cmu_base))
	{
		dev_err(dev, "could not map cmu-base registers\n");
		return PTR_ERR(priv->cmu_base);
	}
	
	/* Set ethernet gmac delays */
	
	if (priv->data == (void*) &k7_reg_data)
	{
		aux = readl(priv->cmu_base + DEVRST1);
		aux &= 0xff807fff;
		writel(aux, priv->cmu_base + DEVRST1);
	
		if (aux != readl(priv->cmu_base + DEVRST1))
		{
			dev_err(dev, "could not set ethernet gmac delays\n");
			return -EINVAL;
		}
	}
	
	ret = devm_reset_controller_register(dev, &priv->rcdev);
	
	if (!ret) {
		dev_info(dev, "probe finished\n");
	}
	else {
		dev_err(dev, "could not register reset controller\n");
	}
	return ret;
}

static struct platform_driver caninos_rcu_reset_driver = {
	.probe = caninos_rcu_reset_probe,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table	= caninos_rcu_reset_dt_ids,
	},
};
module_platform_driver(caninos_rcu_reset_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

