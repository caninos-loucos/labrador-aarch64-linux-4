// SPDX-License-Identifier: GPL-2.0
/*
 * Caninos Labrador DRM/KMS driver
 * Copyright (c) 2018-2020 LSI-TEC - Caninos Loucos
 * Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
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

static int caninos_gfx_load(struct drm_device *drm, struct hdmi_ip_ops *hdmi_ip)
{
    struct platform_device *pdev = to_platform_device(drm->dev);
    struct caninos_gfx *priv;
    struct resource *res;
    int ret;
    
    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);

    if (!priv) {
        return -ENOMEM;
    }
    
    drm->dev_private = priv;
    priv->hdmi_ip = hdmi_ip;
    
    ret = dma_set_mask_and_coherent(drm->dev, DMA_BIT_MASK(32));
    
    if (ret)
    {
        dev_err(&pdev->dev, "failed to set DMA mask: %d\n", ret);
        return ret;
    }
    
    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "de");
    priv->base = devm_ioremap(drm->dev, res->start, resource_size(res));
    
    if (IS_ERR(priv->base)) {
        return PTR_ERR(priv->base);
    }
    
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cvbs");
	priv->cvbs_base = devm_ioremap(drm->dev, res->start, resource_size(res));
	
	if (IS_ERR(priv->cvbs_base)) {
		return PTR_ERR(priv->cvbs_base);
	}
	
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cmu");
	priv->cmu_base = devm_ioremap(drm->dev, res->start, resource_size(res));
	
	if (IS_ERR(priv->cmu_base)) {
		return PTR_ERR(priv->cmu_base);
	}
	
	priv->tvout_clk = devm_clk_get(drm->dev, "tvout");
	
	if (IS_ERR(priv->tvout_clk)) {
		return -EINVAL;
	}
	
	priv->cvbspll_clk = devm_clk_get(drm->dev, "cvbs_pll");
	
	if (IS_ERR(priv->cvbspll_clk)) {
		return -EINVAL;
	}
	
	priv->clk = devm_clk_get(drm->dev, "clk");
	
	if (IS_ERR(priv->clk)) {
		return -EINVAL;
	}
	
	priv->parent_clk = devm_clk_get(drm->dev, "parent_clk");
	
	if (IS_ERR(priv->parent_clk)) {
		return -EINVAL;
	}
	
	priv->de_rst = devm_reset_control_get(drm->dev, "de");
	
	if (IS_ERR(priv->de_rst)) {
		return PTR_ERR(priv->de_rst);
	}
	
	priv->cvbs_rst = devm_reset_control_get(drm->dev, "tvout");
	
	if (IS_ERR(priv->cvbs_rst)) {
		return PTR_ERR(priv->cvbs_rst);
	}
	
	ret = caninos_gfx_pipe_init(drm);
	
	if (ret < 0) {
	    dev_err(drm->dev, "Cannot setup display pipe\n");
		return ret;
	}
	
	ret = devm_request_irq(drm->dev, platform_get_irq(pdev, 0),
			       caninos_gfx_irq_handler, 0, "caninos gfx", drm);
	if (ret < 0) {
		dev_err(drm->dev, "Failed to install IRQ handler\n");
		return ret;
	}
	
	drm_mode_config_reset(drm);
	
	drm_fbdev_generic_setup(drm, 32);
	
	return 0;
}

static void caninos_gfx_unload(struct drm_device *drm)
{
	drm_kms_helper_poll_fini(drm);
	drm_mode_config_cleanup(drm);
	drm->dev_private = NULL;
}

DEFINE_DRM_GEM_CMA_FOPS(fops);

static struct drm_driver caninos_gfx_driver = {
    .driver_features = DRIVER_GEM | DRIVER_MODESET |
                       DRIVER_PRIME | DRIVER_ATOMIC,
    .gem_free_object_unlocked = drm_gem_cma_free_object,
    .gem_print_info = drm_gem_cma_print_info,
    .gem_vm_ops = &drm_gem_cma_vm_ops,
    .dumb_create = drm_gem_cma_dumb_create,
    .prime_handle_to_fd = drm_gem_prime_handle_to_fd,
    .prime_fd_to_handle = drm_gem_prime_fd_to_handle,
    .gem_prime_export = drm_gem_prime_export,
    .gem_prime_import = drm_gem_prime_import,
    .gem_prime_get_sg_table = drm_gem_cma_prime_get_sg_table,
    .gem_prime_import_sg_table = drm_gem_cma_prime_import_sg_table,
    .gem_prime_vmap = drm_gem_cma_prime_vmap,
    .gem_prime_vunmap = drm_gem_cma_prime_vunmap,
    .gem_prime_mmap = drm_gem_cma_prime_mmap,
    
    .fops = &fops,
    .name = "caninosdrm",
    .desc = "CANINOS DRM",
    .date = "20200121",
    .major = 1,
    .minor = 0,
};

static const struct of_device_id caninos_gfx_match[] = {
	{ .compatible = "caninos,k7-drm" },
	{ }
};

static int caninos_gfx_probe(struct platform_device *pdev)
{
	struct hdmi_ip_ops *hdmi_ip = NULL;
	struct platform_device *hdmi_pdev;
	struct device_node *hdmi_np;
	struct drm_device *drm;
	int ret;
	
	hdmi_np = of_parse_phandle(pdev->dev.of_node, "hdmi-phy", 0);
	
	if (!hdmi_np)
	{
		dev_err(&pdev->dev, "could not get hdmi-phy node\n");
		return -ENXIO;
	}
	
	hdmi_pdev = of_find_device_by_node(hdmi_np);
	
	if (hdmi_pdev) {
		hdmi_ip = platform_get_drvdata(hdmi_pdev);
	}
	
	of_node_put(hdmi_np);
	
	if (!hdmi_pdev || !hdmi_ip)
	{
		dev_err(&pdev->dev, "hdmi-phy is not ready\n");
		return -EPROBE_DEFER;
	}
	
	drm = drm_dev_alloc(&caninos_gfx_driver, &pdev->dev);
	
    if (IS_ERR(drm)) {
		return PTR_ERR(drm);
	}
	
	ret = caninos_gfx_load(drm, hdmi_ip);
	
	if (ret) {
		goto err_free;
	}

	ret = drm_dev_register(drm, 0);
	
	if (ret) {
		goto err_unload;
	}

	return 0;

err_unload:
	caninos_gfx_unload(drm);
err_free:
	drm_dev_put(drm);
	return ret;
}

static int caninos_gfx_remove(struct platform_device *pdev)
{
    struct drm_device *drm = platform_get_drvdata(pdev);
    
    drm_dev_unregister(drm);
    caninos_gfx_unload(drm);
    drm_dev_put(drm);

    return 0;
}

static struct platform_driver caninos_gfx_platform_driver = {
    .probe		= caninos_gfx_probe,
    .remove		= caninos_gfx_remove,
    .driver = {
        .name = "caninosdrm",
        .of_match_table = caninos_gfx_match,
    },
};

module_platform_driver(caninos_gfx_platform_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("CANINOS DRM/KMS driver");
MODULE_LICENSE("GPL");

