// SPDX-License-Identifier: GPL-2.0
/*
 * DRM/KMS driver for Caninos Labrador
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

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_device.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_drv.h>

#include "gfx_drv.h"

#define DRIVER_NAME "caninos-drm"
#define DRIVER_DESC "Caninos Labrador DRM/KMS driver"

#define DRIVER_FEATURES \
	DRIVER_GEM | DRIVER_MODESET | DRIVER_PRIME | DRIVER_ATOMIC

DEFINE_DRM_GEM_CMA_FOPS(caninos_fops);

static struct drm_driver caninos_gfx_driver = {
	.gem_free_object_unlocked  = drm_gem_cma_free_object,
	.gem_print_info            = drm_gem_cma_print_info,
	.gem_vm_ops                = &drm_gem_cma_vm_ops,
	.dumb_create               = drm_gem_cma_dumb_create,
	.prime_handle_to_fd        = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle        = drm_gem_prime_fd_to_handle,
	.gem_prime_export          = drm_gem_prime_export,
	.gem_prime_import          = drm_gem_prime_import,
	.gem_prime_get_sg_table    = drm_gem_cma_prime_get_sg_table,
	.gem_prime_import_sg_table = drm_gem_cma_prime_import_sg_table,
	.gem_prime_vmap            = drm_gem_cma_prime_vmap,
	.gem_prime_vunmap          = drm_gem_cma_prime_vunmap,
	.gem_prime_mmap            = drm_gem_cma_prime_mmap,
	.name                      = DRIVER_NAME,
	.desc                      = DRIVER_DESC,
	.date                      = "20230605",
	.major                     = 1,
	.minor                     = 1,
	.driver_features           = DRIVER_FEATURES,
	.fops                      = &caninos_fops,
};

static void __iomem *caninos_gfx_ioremap(struct device *dev, const char *name)
{
	struct platform_device *pdev = to_platform_device(dev);
	void __iomem *iobase = NULL;
	struct resource *res;
	
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	
	if (res) {
		iobase = devm_ioremap(dev, res->start, resource_size(res));
	}
	if (!iobase) {
		iobase = ERR_PTR(-EINVAL);
	}
	if (IS_ERR(iobase) && name) {
		dev_err(dev, "could not remap %s io memory\n", name);
	}
	return iobase;
}

static int caninos_gfx_get_resources(struct caninos_gfx *priv)
{
	priv->irq = platform_get_irq(to_platform_device(priv->dev), 0);
	
	if (priv->irq < 0) {
		return -EINVAL;
	}
	
	priv->base = caninos_gfx_ioremap(priv->dev, "de");
	
	if (IS_ERR(priv->base)) {
		return PTR_ERR(priv->base);
	}
	
	priv->cvbs_base = caninos_gfx_ioremap(priv->dev, "cvbs");
	
	if (IS_ERR(priv->cvbs_base)) {
		return PTR_ERR(priv->cvbs_base);
	}
	
	priv->dcu_base = caninos_gfx_ioremap(priv->dev, "dcu");
	
	if (IS_ERR(priv->dcu_base)) {
		return PTR_ERR(priv->dcu_base);
	}
	
	priv->tvout_clk = devm_clk_get(priv->dev, "tvout");
	
	if (IS_ERR(priv->tvout_clk)) {
		return -EINVAL;
	}
	
	priv->cvbspll_clk = devm_clk_get(priv->dev, "cvbs_pll");
	
	if (IS_ERR(priv->cvbspll_clk)) {
		return -EINVAL;
	}
	
	priv->clk = devm_clk_get(priv->dev, "clk");
	
	if (IS_ERR(priv->clk)) {
		return -EINVAL;
	}
	
	priv->parent_clk = devm_clk_get(priv->dev, "parent_clk");
	
	if (IS_ERR(priv->parent_clk)) {
		return -EINVAL;
	}
	
	priv->de_rst = devm_reset_control_get(priv->dev, "de");
	
	if (IS_ERR(priv->de_rst)) {
		return PTR_ERR(priv->de_rst);
	}
	
	priv->cvbs_rst = devm_reset_control_get(priv->dev, "tvout");
	
	if (IS_ERR(priv->cvbs_rst)) {
		return PTR_ERR(priv->cvbs_rst);
	}
	return 0;
}

static int caninos_gfx_get_hw_model(struct caninos_gfx *priv)
{
	const struct of_device_id *match;
	phys_addr_t value = 0;
	
	BUG_ON(!priv || !priv->dev);
	
	match = of_match_device(priv->dev->driver->of_match_table, priv->dev);
	
	if (match) {
		value = (phys_addr_t)(match->data);
	}
	
	switch (value) {
	case CANINOS_DE_HW_MODEL_K5:
	case CANINOS_DE_HW_MODEL_K7:
		priv->model = (enum caninos_de_hw_model)(value);
		break;
	default:
		priv->model = CANINOS_DE_HW_MODEL_INV;
		return -EINVAL;
	}
	return 0;
}

static int caninos_gfx_get_hdmi_phy(struct caninos_gfx *priv)
{
	struct platform_device *hdmi_pdev;
	struct device_node *hdmi_np, *np;
	
	BUG_ON(!priv || !priv->dev);
	np = priv->dev->of_node;
	BUG_ON(!np);
	
	priv->caninos_hdmi = NULL;
	
	hdmi_np = of_parse_phandle(np, "hdmi-phy", 0);
	
	if (!hdmi_np) {
		return -ENXIO;
	}
	
	hdmi_pdev = of_find_device_by_node(hdmi_np);
	
	if (hdmi_pdev) {
		priv->caninos_hdmi = platform_get_drvdata(hdmi_pdev);
	}
	of_node_put(hdmi_np);
	
	if (!priv->caninos_hdmi) {
		return -EPROBE_DEFER;
	}
	return 0;
}

static int caninos_gfx_probe(struct platform_device *pdev)
{
	struct caninos_gfx *priv;
	int ret;
	
	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "missing device OF node\n");
		return -ENODEV;
	}
	
	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	
	if (!priv) {
		dev_err(&pdev->dev, "unable to allocate private driver data\n");
		return -ENOMEM;
	}
	
	priv->dev = &pdev->dev;
	
	ret = caninos_gfx_get_hw_model(priv);
	
	if (ret) {
		dev_err(&pdev->dev, "unable to get hardware model\n");
		return ret;
	}
	
	ret = caninos_gfx_get_hdmi_phy(priv);
	
	if (ret) {
		if (ret == -EPROBE_DEFER) {
			dev_info(&pdev->dev, "hdmi-phy is not ready\n");
		}
		else {
			dev_err(&pdev->dev, "unable to get hdmi-phy node\n");
		}
		return ret;
	}
	
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	
	if (ret) {
		dev_err(&pdev->dev, "unable to set DMA mask\n");
		return ret;
	}
	
	priv->drm = drm_dev_alloc(&caninos_gfx_driver, &pdev->dev);
	
	if (IS_ERR(priv->drm)) {
		dev_err(&pdev->dev, "unable to allocate drm device\n");
		return PTR_ERR(priv->drm);
	}
	
	priv->drm->dev_private = priv;
	
	ret = caninos_gfx_get_resources(priv);
	
	if (ret) {
		goto err_free;
	}
	
	ret = caninos_gfx_pipe_init(priv->drm);
	
	if (ret) {
	    dev_err(&pdev->dev, "could not setup display pipe\n");
		goto err_free;
	}
	
	ret = devm_request_irq(&pdev->dev, priv->irq, caninos_gfx_irq_handler, 0,
	                       dev_name(&pdev->dev), priv->drm);
	
	if (ret) {
		dev_err(&pdev->dev, "failed to install IRQ handler\n");
		goto err_free;
	}
	
	ret = drm_dev_register(priv->drm, 0);
	
	if (ret) {
		goto err_unload;
	}
	
	drm_mode_config_reset(priv->drm);
	drm_fbdev_generic_setup(priv->drm, 32);
	return 0;
	
err_unload:
	drm_kms_helper_poll_fini(priv->drm);
	drm_mode_config_cleanup(priv->drm);
	priv->drm->dev_private = NULL;
	
err_free:
	drm_dev_put(priv->drm);
	return ret;
}

static int caninos_gfx_remove(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);
	drm_dev_unregister(drm);
	drm_kms_helper_poll_fini(drm);
	drm_mode_config_cleanup(drm);
	drm->dev_private = NULL;
	drm_dev_put(drm);
	return 0;
}

static const struct of_device_id caninos_gfx_match[] = {
	{ .compatible = "caninos,k7-drm", .data = (void*) CANINOS_DE_HW_MODEL_K7 },
	{ .compatible = "caninos,k5-drm", .data = (void*) CANINOS_DE_HW_MODEL_K5 },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, caninos_gfx_match);

static struct platform_driver caninos_gfx_platform_driver = {
    .probe = caninos_gfx_probe,
    .remove = caninos_gfx_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = caninos_gfx_match,
        .owner = THIS_MODULE,
    },
};

module_platform_driver(caninos_gfx_platform_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
