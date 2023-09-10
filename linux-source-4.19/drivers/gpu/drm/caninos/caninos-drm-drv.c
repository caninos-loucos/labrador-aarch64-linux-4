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

#include "caninos-drm-priv.h"

#define DRIVER_NAME "caninos-drm"
#define DRIVER_DESC "Caninos Labrador DRM/KMS driver"

#define DRIVER_FEATURES \
	DRIVER_GEM | DRIVER_MODESET | DRIVER_PRIME | DRIVER_ATOMIC

DEFINE_DRM_GEM_CMA_FOPS(caninos_fops);

static struct drm_driver caninos_drm_driver = {
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
	.date                      = "20230908",
	.major                     = 1,
	.minor                     = 4,
	.driver_features           = DRIVER_FEATURES,
	.fops                      = &caninos_fops,
};

static int caninos_connector_get_modes(struct drm_connector *connector)
{
	struct drm_device *drm = connector->dev;
	int count, max_width, max_height;
	
	max_width = drm->mode_config.max_width;
	max_height = drm->mode_config.max_height;
	
	count = drm_add_modes_noedid(connector, max_width, max_height);
	
	if (count) {
		drm_set_preferred_mode(connector, 1920, 1080);
	}
	return count;
}

static void caninos_update(struct drm_simple_display_pipe *pipe,
                           struct drm_plane_state *plane_state)
{
	struct caninos_gfx *priv = container_of(pipe, struct caninos_gfx, pipe);
	struct drm_crtc *crtc = &pipe->crtc;
	struct drm_framebuffer *fb = pipe->plane.state->fb;
	struct drm_pending_vblank_event *event;
	struct drm_gem_cma_object *gem;
	
	spin_lock_irq(&crtc->dev->event_lock);
	event = crtc->state->event;
	
	if (event) {
		crtc->state->event = NULL;
		drm_crtc_send_vblank_event(crtc, event);
	}
	
	spin_unlock_irq(&crtc->dev->event_lock);
	
	if (!fb) {
		return;
	}
	
	gem = drm_fb_cma_get_gem_obj(fb, 0);
	
	if (gem) {
		caninos_vdc_set_fbaddr(priv->caninos_vdc, (u32)gem->paddr);
	}
}

static void caninos_enable(struct drm_simple_display_pipe *pipe,
                           struct drm_crtc_state *crtc_state,
                           struct drm_plane_state *plane_state)
{
	struct caninos_gfx *priv = container_of(pipe, struct caninos_gfx, pipe);
	struct drm_display_mode *drm_mode = &crtc_state->adjusted_mode;
	struct caninos_hdmi *caninos_hdmi = priv->caninos_hdmi;
	struct caninos_vdc *caninos_vdc = priv->caninos_vdc;
	struct caninos_vdc_mode vdc_mode;
	int width, height, vrefresh;
	
	width = drm_mode->hdisplay;
	height = drm_mode->vdisplay;
	vrefresh = drm_mode_vrefresh(drm_mode);
	
	if ((width == 640) && (height == 480) && (vrefresh == 60)) {
		caninos_hdmi_set_mode(caninos_hdmi, VID640x480P_60_4VS3);
	}
	else if ((width == 720) && (height == 480) && (vrefresh == 60)) {
		caninos_hdmi_set_mode(caninos_hdmi, VID720x480P_60_4VS3);
	}
	else if ((width == 720) && (height == 576) && (vrefresh == 50)) {
		caninos_hdmi_set_mode(caninos_hdmi, VID720x576P_50_4VS3);
	}
	else if ((width == 1280) && (height == 720) && (vrefresh == 60)) {
		caninos_hdmi_set_mode(caninos_hdmi, VID1280x720P_60_16VS9);
	}
	else if ((width == 1280) && (height == 720) && (vrefresh == 50)) {
		caninos_hdmi_set_mode(caninos_hdmi, VID1280x720P_50_16VS9);
	}
	else if ((width == 1920) && (height == 1080) && (vrefresh == 50)) {
		caninos_hdmi_set_mode(caninos_hdmi, VID1920x1080P_50_16VS9);
	}
	else if ((width == 1920) && (height == 1080) && (vrefresh == 60)) {
		caninos_hdmi_set_mode(caninos_hdmi, VID1920x1080P_60_16VS9);
	}
	else {
		return;
	}
	
	vdc_mode.width  = width;
	vdc_mode.height = height;
	vdc_mode.format = DRM_FORMAT_XRGB8888;
	
	caninos_hdmi_disable(caninos_hdmi);
	
	caninos_vdc_disable(caninos_vdc);
	caninos_vdc_set_mode(caninos_vdc, &vdc_mode);
	caninos_vdc_enable(caninos_vdc);
	
	caninos_hdmi_enable(caninos_hdmi);
}

static enum drm_mode_status
caninos_mode_valid(struct drm_crtc *crtc, const struct drm_display_mode *mode)
{
	int w = mode->hdisplay, h = mode->vdisplay;
	int vrefresh = drm_mode_vrefresh(mode);
	
	if ((w == 640) && (h == 480) && (vrefresh == 60)) {
		return MODE_OK;
	}
	if ((w == 720) && (h == 480) && (vrefresh == 60)) {
		return MODE_OK;
	}
	if ((w == 720) && (h == 576) && (vrefresh == 50)) {
		return MODE_OK;
	}
	if ((w == 1280) && (h == 720) && ((vrefresh == 60) || (vrefresh == 50))) {
		return MODE_OK;
	}
	if ((w == 1920) && (h == 1080) && ((vrefresh == 60) || (vrefresh == 50))) {
		return MODE_OK;
	}
	return MODE_BAD;
}

static struct drm_simple_display_pipe_funcs caninos_gfx_funcs = {
	.mode_valid = caninos_mode_valid,
	.enable     = caninos_enable,
	.update     = caninos_update,
	.prepare_fb = drm_gem_fb_simple_display_pipe_prepare_fb,
};

static const struct drm_connector_funcs caninos_connector_funcs = {
	.fill_modes             = drm_helper_probe_single_connector_modes,
	.destroy                = drm_connector_cleanup,
	.reset                  = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state   = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs caninos_connector_helper = {
	.get_modes = caninos_connector_get_modes,
};

static const struct drm_mode_config_funcs caninos_mode_config_funcs = {
	.fb_create     = drm_gem_fb_create,
	.atomic_check  = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static const uint32_t caninos_gfx_formats[] = {
	DRM_FORMAT_XRGB8888,
};

static int caninos_gfx_pipe_init(struct drm_device *drm)
{
	struct caninos_gfx *priv = drm->dev_private;
	int ret;
	
	drm_mode_config_init(drm);
	
	drm->mode_config.min_width = 480;
	drm->mode_config.min_height = 480;
	drm->mode_config.max_width = 1920;
	drm->mode_config.max_height = 1920;
	
	drm->mode_config.funcs = &caninos_mode_config_funcs;
	
	priv->connector.dpms = DRM_MODE_DPMS_OFF;
	priv->connector.polled = 0;
	
	drm_connector_helper_add(&priv->connector, &caninos_connector_helper);
	
	ret = drm_connector_init(drm, &priv->connector, &caninos_connector_funcs,
	                         DRM_MODE_CONNECTOR_HDMIA);
	
	if (ret < 0) {
		return ret;
	}
	
	return drm_simple_display_pipe_init(drm, &priv->pipe, &caninos_gfx_funcs,
	                                    caninos_gfx_formats,
	                                    ARRAY_SIZE(caninos_gfx_formats),
	                                    NULL, &priv->connector);
}

static inline void *caninos_get_drvdata_by_node(struct device_node *np)
{
	struct platform_device *pdev = of_find_device_by_node(np);
	return pdev ? platform_get_drvdata(pdev) : NULL;
}

static int caninos_drm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct caninos_gfx *priv;
	struct device_node *np;
	int ret;
	
	if (!dev->of_node) {
		dev_err(dev, "missing device of node\n");
		return -ENODEV;
	}
	
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	
	if (ret) {
		dev_err(dev, "unable to set dma mask\n");
		return ret;
	}
	
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	
	if (!priv) {
		dev_err(dev, "unable to allocate private data structure\n");
		return -ENOMEM;
	}
	
	priv->dev = dev;
	
	np = of_parse_phandle(dev->of_node, "hdmi-controller", 0);
	
	if (np) {
		priv->caninos_hdmi = caninos_get_drvdata_by_node(np);
		of_node_put(np);
	}
	if (!priv->caninos_hdmi) {
		dev_info(dev, "unable to get handle of hdmi controller\n");
		return -EPROBE_DEFER;
	}
	
	np = of_parse_phandle(dev->of_node, "display-controller", 0);
	
	if (np) {
		priv->caninos_vdc = caninos_get_drvdata_by_node(np);
		of_node_put(np);
	}
	if (!priv->caninos_vdc) {
		dev_info(dev, "unable to get handle of video display controller\n");
		return -EPROBE_DEFER;
	}
	
	priv->drm = drm_dev_alloc(&caninos_drm_driver, dev);
	
	if (IS_ERR(priv->drm)) {
		dev_err(dev, "unable to allocate drm device\n");
		return PTR_ERR(priv->drm);
	}
	
	priv->drm->dev_private = priv;
	
	ret = caninos_gfx_pipe_init(priv->drm);
	
	if (ret) {
	    dev_err(dev, "could not setup display pipe\n");
		goto err_free;
	}
	
	drm_mode_config_reset(priv->drm);
	
	drm_fbdev_generic_setup(priv->drm, 32);
	
	ret = drm_dev_register(priv->drm, 0);
	
	if (ret) {
		goto err_unload;
	}
	return 0;
	
err_unload:
	drm_kms_helper_poll_fini(priv->drm);
	drm_mode_config_cleanup(priv->drm);
	priv->drm->dev_private = NULL;
	
err_free:
	drm_dev_put(priv->drm);
	return ret;
}

static int caninos_drm_remove(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);
	drm_dev_unregister(drm);
	drm_kms_helper_poll_fini(drm);
	drm_mode_config_cleanup(drm);
	drm->dev_private = NULL;
	drm_dev_put(drm);
	return 0;
}

static const struct of_device_id caninos_drm_match[] = {
	{ .compatible = "caninos,drm" },
	{ /* sentinel */ }
};

struct platform_driver caninos_drm_plat_driver = {
	.probe = caninos_drm_probe,
	.remove = caninos_drm_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = of_match_ptr(caninos_drm_match),
		.owner = THIS_MODULE,
	},
};

static struct platform_driver * const drivers[] = {
	&caninos_hdmi_plat_driver,
#ifdef CONFIG_DRM_CANINOS_HDMI_AUDIO
	&caninos_hdmi_audio_plat_driver,
#endif
	&caninos_vdc_plat_driver,
	&caninos_drm_plat_driver,
};

static int __init caninos_module_init(void)
{
	return platform_register_drivers(drivers, ARRAY_SIZE(drivers));
}

static void __exit caninos_module_fini(void)
{
	platform_unregister_drivers(drivers, ARRAY_SIZE(drivers));
}

module_init(caninos_module_init);
module_exit(caninos_module_fini);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
