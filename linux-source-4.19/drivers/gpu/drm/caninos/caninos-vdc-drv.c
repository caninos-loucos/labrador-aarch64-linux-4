// SPDX-License-Identifier: GPL-2.0
/*
 * Video Display Controller Driver for Caninos Labrador
 *
 * Copyright (c) 2022-2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2018-2020 LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
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
 
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/dma-mapping.h>

#include "caninos-vdc-priv.h"

#define DE_MAX_WIDTH  (2048U)
#define DE_MAX_HEIGHT (2048U)
#define DE_DEF_WIDTH  (1080U)
#define DE_DEF_HEIGHT (1920U)
#define DE_DEF_FORMAT (DRM_FORMAT_XRGB8888)

int caninos_vdc_set_mode(struct caninos_vdc *priv,
                         struct caninos_vdc_mode *mode)
{
	unsigned long flags;
	
	if (!priv || !mode) {
		return -EINVAL;
	}
	
	spin_lock_irqsave(&priv->lock, flags);
	
	if (mode->width && mode->width <= DE_MAX_WIDTH) {
		priv->next.mode.width = mode->width;
	}
	if (mode->height && mode->height <= DE_MAX_HEIGHT) {
		priv->next.mode.height = mode->height;
	}
	if (mode->format)
	{
		switch (mode->format)
		{
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_XRGB8888:
		case DRM_FORMAT_RGBA8888:
		case DRM_FORMAT_RGBX8888:
		case DRM_FORMAT_ABGR8888:
		case DRM_FORMAT_XBGR8888:
		case DRM_FORMAT_BGRA8888:
		case DRM_FORMAT_BGRX8888:
			priv->next.mode.format = mode->format;
			break;
		default:
			break;
		}
	}
	
	spin_unlock_irqrestore(&priv->lock, flags);
	return 0;
}

int caninos_vdc_set_fbaddr(struct caninos_vdc *priv, u32 fbaddr)
{
	unsigned long flags;
	
	if (!priv) {
		return -EINVAL;
	}
	if (!fbaddr) {
		fbaddr = (u32)priv->mem_phys;
	}
	
	spin_lock_irqsave(&priv->lock, flags);
	smp_wmb();
	WRITE_ONCE(priv->next.fbaddr, fbaddr);
	spin_unlock_irqrestore(&priv->lock, flags);
	
	return 0;
}

int caninos_vdc_disable(struct caninos_vdc *priv)
{
	unsigned long flags;
	
	if (!priv) {
		return -EINVAL;
	}
	
	spin_lock_irqsave(&priv->lock, flags);
	
	if (de_is_enabled(priv))
	{
		de_disable_irqs(priv);
		de_disable(priv);
	}
	
	spin_unlock_irqrestore(&priv->lock, flags);
	return 0;
}

int caninos_vdc_enable(struct caninos_vdc *priv)
{
	unsigned long flags;
	
	if (!priv) {
		return -EINVAL;
	}
	
	spin_lock_irqsave(&priv->lock, flags);
	
	if (!de_is_enabled(priv))
	{
		priv->fbaddr = (u32)priv->mem_phys;
		priv->mode = priv->next.mode;
		
		de_set_size(priv, priv->mode.width, priv->mode.height);
		de_set_stride(priv, de_calc_stride(priv->mode.width, 32U));
		de_set_format(priv, priv->mode.format);
		de_set_framebuffer(priv, priv->fbaddr);
		de_enable(priv);
		de_set_go(priv);
		
		de_enable_irqs(priv);
	}
	
	spin_unlock_irqrestore(&priv->lock, flags);
	return 0;
}

static irqreturn_t caninos_vdc_irq(int irq, void *data)
{
	struct caninos_vdc *priv = data;
	unsigned long flags;
	u32 fbaddr;
	
	if (de_handle_irqs(priv))
	{
		spin_lock_irqsave(&priv->lock, flags);
		
		if (de_is_enabled(priv))
		{
			fbaddr = READ_ONCE(priv->next.fbaddr);
			smp_rmb();
			
			if (priv->fbaddr != fbaddr)
			{
				priv->fbaddr = fbaddr;
				de_set_framebuffer(priv, fbaddr);
			}
			de_set_go(priv);
		}
		
		spin_unlock_irqrestore(&priv->lock, flags);
	}
	
	return IRQ_HANDLED;
}

static inline int caninos_vdc_get_reserved_memory(struct caninos_vdc *priv)
{
	struct device *dev = priv->dev;
	struct device_node *mem_np;
	struct resource res;
	void *mem_virt;
	int ret;
	
	mem_np = of_parse_phandle(dev->of_node, "memory-region", 0);
	
	if (!mem_np) {
		dev_err(dev, "unable to parse memory-region phandle\n");
		return -EINVAL;
	}
	
	ret = of_address_to_resource(mem_np, 0, &res);
	of_node_put(mem_np);
	
	if (ret < 0) {
		dev_err(dev, "unable to get memory-region resource\n");
		return ret;
	}
	
	mem_virt = devm_memremap(dev, res.start, resource_size(&res), MEMREMAP_WC);
	
	if (!mem_virt) {
		dev_err(dev, "unable to map memory-region resource\n");
		return -ENOMEM;
	}
	
	priv->mem_virt = mem_virt;
	priv->mem_phys = res.start;
	priv->mem_size = resource_size(&res);
	return 0;
}

static int caninos_vdc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct caninos_vdc *priv;
	struct resource *res;
	u32 stride, total;
	int ret;
	
	if (!np) {
		dev_err(dev, "missing device of node\n");
		return -EINVAL;
	}
	
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	
	if (ret) {
		dev_err(dev, "unable to set dma mask\n");
		return ret;
	}
	
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	
	if (!priv) {
		dev_err(dev, "unable to alloc private data structure\n");
		return -ENOMEM;
	}
	
	priv->dev = dev;
	spin_lock_init(&priv->lock);
	
	/* get reserved memory */
	if ((ret = caninos_vdc_get_reserved_memory(priv)) < 0) {
		return ret;
	}
	
	/* set next fbaddr to the reserved memory region */
	priv->next.fbaddr = (u32)priv->mem_phys;
	
	/* set next mode to the default mode */
	priv->next.mode.width  = DE_DEF_WIDTH;
	priv->next.mode.height = DE_DEF_HEIGHT;
	priv->next.mode.format = DE_DEF_FORMAT;
	
	/* currently all supported color formats are 32bpp */
	stride = de_calc_stride(DE_MAX_WIDTH, 32U);
	
	/* ensure that the largest possible framebuffer fits in the resvd mem */
	total = stride * DE_MAX_HEIGHT;
	
	if (total > priv->mem_size) {
		dev_err(priv->dev, "insufficient reserved memory\n");
		return -ENOMEM;
	}
	
	match = of_match_device(dev->driver->of_match_table, dev);
	
	if (!match) {
		dev_err(dev, "unable to get hardware model\n");
		return -EINVAL;
	}
	
	priv->model = (enum de_hw_model)(match->data);
	
	switch (priv->model)
	{
	case DE_HW_K7:
		priv->ops = &de_k7_ops;
		break;
		
	case DE_HW_K5:
		priv->ops = &de_k5_ops;
		break;
		
	default:
		dev_err(dev, "invalid hardware model\n");
		return -EINVAL;
	}
	
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "de");
	
	if (!res) {
		dev_err(dev, "unable to get memory resource from dts\n");
		return -ENODEV;
	}
	
	priv->base = devm_ioremap(dev, res->start, resource_size(res));
	
	if (!priv->base) {
		dev_err(dev, "unable to map base registers\n");
		return -ENOMEM;
	}
	
	priv->rst = devm_reset_control_get(dev, "de");
	
	if (IS_ERR(priv->rst)) {
		dev_err(dev, "unable to get reset control\n");
		return PTR_ERR(priv->rst);
	}
	
	priv->irq = platform_get_irq(pdev, 0);
	
	if (priv->irq < 0) {
		dev_err(dev, "unable to get irq number\n");
		return priv->irq;
	}
	
	priv->clk = devm_clk_get(dev, "de");
	
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "unable to get main clock\n");
		return PTR_ERR(priv->clk);
	}
	
	/* controller initialization */
	ret = de_init(priv);
	
	if (ret < 0) {
		dev_err(dev, "display engine initialization failed\n");
		return ret;
	}
	
	/* register irq routine */
	ret = devm_request_irq(dev, priv->irq, caninos_vdc_irq, 0,
	                       dev_name(dev), priv);
	
	if (ret < 0) {
		dev_err(dev, "unable to request irq %d\n", priv->irq);
		de_fini(priv);
		return ret;
	}
	
	/* clear the reserved memory */
	memset(priv->mem_virt, 0x0, total);
	smp_mb();
	
	/* set initial state */
	ret = caninos_vdc_enable(priv);
	
	if (ret < 0) {
		dev_err(dev, "unable to set display engine initial state\n");
		de_fini(priv);
		return ret;
	}
	
	platform_set_drvdata(pdev, priv);
	dev_info(dev, "probe finished\n");
	return 0;
}

static int caninos_vdc_remove(struct platform_device *pdev)
{
	struct caninos_vdc *priv = platform_get_drvdata(pdev);
	
	if (priv) {
		de_fini(priv);
	}
	return 0;
}

static void caninos_vdc_shutdown(struct platform_device *pdev)
{
	struct caninos_vdc *priv = platform_get_drvdata(pdev);
	
	if (priv) {
		de_fini(priv);
	}
}

static const struct of_device_id caninos_vdc_match[] = {
	{ .compatible = "caninos,k7-vdc", .data = (void*)DE_HW_K7 },
	{ .compatible = "caninos,k5-vdc", .data = (void*)DE_HW_K5 },
	{ /* sentinel */ },
};

struct platform_driver caninos_vdc_plat_driver = {
	.probe = caninos_vdc_probe,
	.remove = caninos_vdc_remove,
	.shutdown = caninos_vdc_shutdown,
	.driver = {
		.name = "caninos-vdc",
		.of_match_table = of_match_ptr(caninos_vdc_match),
		.owner = THIS_MODULE,
	},
};

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("Caninos Video Display Controller Driver");
MODULE_LICENSE("GPL v2");
