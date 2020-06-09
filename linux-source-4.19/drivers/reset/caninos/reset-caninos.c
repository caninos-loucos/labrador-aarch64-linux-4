/*
 * Caninos Reset
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

#define S700_DEVRST0 0x00
#define S700_DEVRST1 0x04

struct caninos_rcu_reset_reg_data
{
	u32 assert_offset;
	u32 assert_clear_mask;
	u32 assert_set_mask;
	
	u32 deassert_offset;
	u32 deassert_clear_mask;
	u32 deassert_set_mask;
};

static struct caninos_rcu_reset_reg_data k7_reg_data[] = {
	[RST_UART0] = {
		.assert_offset       = S700_DEVRST1,
		.assert_clear_mask   = BIT(8),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST1,
		.deassert_clear_mask = BIT(8),
		.deassert_set_mask   = BIT(8),
	},
	[RST_UART1] = {
		.assert_offset       = S700_DEVRST1,
		.assert_clear_mask   = BIT(9),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST1,
		.deassert_clear_mask = BIT(9),
		.deassert_set_mask   = BIT(9),
	},
	[RST_UART2] = {
		.assert_offset       = S700_DEVRST1,
		.assert_clear_mask   = BIT(10),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST1,
		.deassert_clear_mask = BIT(10),
		.deassert_set_mask   = BIT(10),
	},
	[RST_UART3] = {
		.assert_offset       = S700_DEVRST1,
		.assert_clear_mask   = BIT(11),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST1,
		.deassert_clear_mask = BIT(11),
		.deassert_set_mask   = BIT(11),
	},
	[RST_UART4] = {
		.assert_offset       = S700_DEVRST1,
		.assert_clear_mask   = BIT(12),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST1,
		.deassert_clear_mask = BIT(12),
		.deassert_set_mask   = BIT(12),
	},
	[RST_UART5] = {
		.assert_offset       = S700_DEVRST1,
		.assert_clear_mask   = BIT(13),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST1,
		.deassert_clear_mask = BIT(13),
		.deassert_set_mask   = BIT(13),
	},
	[RST_UART6] = {
		.assert_offset       = S700_DEVRST1,
		.assert_clear_mask   = BIT(14),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST1,
		.deassert_clear_mask = BIT(14),
		.deassert_set_mask   = BIT(14),
	},
	[RST_SDC0] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(22),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(22),
		.deassert_set_mask   = BIT(22),
	},
	[RST_SDC1] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(23),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(23),
		.deassert_set_mask   = BIT(23),
	},
	[RST_SDC2] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(24),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(24),
		.deassert_set_mask   = BIT(24),
	},
	[RST_HDCP2] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(6),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(6),
		.deassert_set_mask   = BIT(6),
	},
	[RST_USBH0] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(26),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(26),
		.deassert_set_mask   = BIT(26),
	},
	[RST_USBH1] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(27),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(27),
		.deassert_set_mask   = BIT(27),
	},
	[RST_PCM1] = {
		.assert_offset       = S700_DEVRST1,
		.assert_clear_mask   = BIT(31),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST1,
		.deassert_clear_mask = BIT(31),
		.deassert_set_mask   = BIT(31),
	},
	[RST_PCM0] = {
		.assert_offset       = S700_DEVRST1,
		.assert_clear_mask   = BIT(30),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST1,
		.deassert_clear_mask = BIT(30),
		.deassert_set_mask   = BIT(30),
	},
	[RST_AUDIO] = {
		.assert_offset       = S700_DEVRST1,
		.assert_clear_mask   = BIT(29),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST1,
		.deassert_clear_mask = BIT(29),
		.deassert_set_mask   = BIT(29),
	},
	[RST_ETHERNET] = {
		.assert_offset       = S700_DEVRST1,
		.assert_clear_mask   = BIT(23),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST1,
		.deassert_clear_mask = BIT(23),
		.deassert_set_mask   = BIT(23),
	},
	[RST_VDE] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(10),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(10),
		.deassert_set_mask   = BIT(10),
	},
	[RST_VCE] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(11),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(11),
		.deassert_set_mask   = BIT(11),
	},
	[RST_GPU3D] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(8),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(8),
		.deassert_set_mask   = BIT(8),
	},
	[RST_TVOUT] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(3),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(3),
		.deassert_set_mask   = BIT(3),
	},
	[RST_HDMI] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(5),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(5),
		.deassert_set_mask   = BIT(5),
	},
	[RST_DE] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(0),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(0),
		.deassert_set_mask   = BIT(0),
	},
	[RST_USB3] = {
		.assert_offset       = S700_DEVRST0,
		.assert_clear_mask   = BIT(25),
		.assert_set_mask     = 0,
		.deassert_offset     = S700_DEVRST0,
		.deassert_clear_mask = BIT(25),
		.deassert_set_mask   = BIT(25),
	},
};

struct caninos_rcu_reset_priv
{
	struct reset_controller_dev rcdev;
	struct device *dev;
	void __iomem *cmu_base;
	const struct caninos_rcu_reset_reg_data *data;
	spinlock_t lock;
};

static struct caninos_rcu_reset_priv *to_caninos_rcu_reset_priv(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct caninos_rcu_reset_priv, rcdev);
}

static int caninos_rcu_reset_status(struct reset_controller_dev *rcdev, unsigned long id)
{
	struct caninos_rcu_reset_priv *priv = to_caninos_rcu_reset_priv(rcdev);
	unsigned long flags;
	int ret;
	u32 val;
	
	spin_lock_irqsave(&priv->lock, flags);
	
	val = readl(priv->cmu_base + priv->data[id].assert_offset);
	
	val &= ~(priv->data[id].assert_clear_mask);
	
	if (val == priv->data[id].assert_set_mask) {
		ret = 1;
	}
	else
	{
		val = readl(priv->cmu_base + priv->data[id].deassert_offset);
	
		val &= ~(priv->data[id].deassert_clear_mask);
	
		if (val == priv->data[id].deassert_set_mask) {
			ret = 0;
		}
		else {
			ret = -EINVAL;
		}
	}
	
	spin_unlock_irqrestore(&priv->lock, flags);
	return ret;
}

static int caninos_rcu_reset_update(struct reset_controller_dev *rcdev, unsigned long id, bool assert)
{
	struct caninos_rcu_reset_priv *priv = to_caninos_rcu_reset_priv(rcdev);
	unsigned long flags;
	u32 val;
	
	dev_info(priv->dev, "reset id=%lu assert=%d", id, assert);
	
	spin_lock_irqsave(&priv->lock, flags);
	
	if (assert)
	{
		val = readl(priv->cmu_base + priv->data[id].assert_offset);
		
		val &= ~(priv->data[id].assert_clear_mask);
		val |= (priv->data[id].assert_set_mask);
		
		writel(val, priv->cmu_base + priv->data[id].assert_offset);
	}
	else
	{
		val = readl(priv->cmu_base + priv->data[id].deassert_offset);
		
		val &= ~(priv->data[id].deassert_clear_mask);
		val |= (priv->data[id].deassert_set_mask);
		
		writel(val, priv->cmu_base + priv->data[id].deassert_offset);
	}
	
	spin_unlock_irqrestore(&priv->lock, flags);
	
	if (assert) {
		udelay(10);
	}
	
	return 0;
}

static int caninos_rcu_reset_assert(struct reset_controller_dev *rcdev, unsigned long id)
{
	return caninos_rcu_reset_update(rcdev, id, true);
}

static int caninos_rcu_reset_deassert(struct reset_controller_dev *rcdev, unsigned long id)
{
	return caninos_rcu_reset_update(rcdev, id, false);
}

static int caninos_rcu_reset_reset(struct reset_controller_dev *rcdev, unsigned long id)
{
	int ret;
	
	ret = caninos_rcu_reset_assert(rcdev, id);
	
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
	{ .compatible = "caninos,k7-reset", .data = &k7_reg_data },
	{ },
};
MODULE_DEVICE_TABLE(of, caninos_rcu_reset_dt_ids);

static int caninos_rcu_reset_probe(struct platform_device *pdev)
{
	struct caninos_rcu_reset_priv *priv;
	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	struct resource *res;
	
	of_id = of_match_node(caninos_rcu_reset_dt_ids, dev->of_node);
	
	if (!of_id)
	{
		dev_err(dev, "could not match device type.\n");
		return -ENODEV;
	}
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res)
	{
		dev_err(dev, "could not get register base from DTS.\n");
		return -ENOMEM;
	}
	
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	
	if (IS_ERR(priv))
	{
		dev_err(dev, "could not allocate private memory.\n");
		return PTR_ERR(priv);
	}
	
	priv->dev = dev;
	
	spin_lock_init(&priv->lock);
	
	priv->rcdev.ops = &caninos_rcu_reset_ops;
	priv->rcdev.owner = THIS_MODULE;
	priv->rcdev.of_node = dev->of_node;
	priv->rcdev.nr_resets = NR_RESETS;
	priv->data = of_id->data;
	
	priv->cmu_base = devm_ioremap(dev, res->start, resource_size(res));
	
	if (IS_ERR(priv->cmu_base))
	{
		dev_err(dev, "could not map cmu-base registers.\n");
		return PTR_ERR(priv->cmu_base);
	}
	
	return devm_reset_controller_register(dev, &priv->rcdev);
}

static struct platform_driver caninos_rcu_reset_driver = {
	.probe = caninos_rcu_reset_probe,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table	= caninos_rcu_reset_dt_ids,
	},
};
module_platform_driver(caninos_rcu_reset_driver);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
