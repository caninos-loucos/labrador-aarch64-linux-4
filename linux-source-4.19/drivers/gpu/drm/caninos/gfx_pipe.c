// SPDX-License-Identifier: GPL-2.0
/*
 * DRM/KMS driver for Caninos Labrador
 *
 * Copyright (c) 2022-2023 ITEX - LSITEC - Caninos Loucos
 * Author: Ana Clara Forcelli <ana.forcelli@lsitec.org.br>
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

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>

#include "gfx_drv.h"
#include "gfx_de.h"

static int caninos_de_calculate_preline(struct videomode *mode)
{
	int preline, total;
	total = (mode->xres + mode->hfp + mode->hbp + mode->hsw) * mode->pixclock;
	
	preline = ((RECOMMENDED_PRELINE_TIME * 1000000) + (total / 2)) / total;
	preline -= mode->vfp;
	preline = (preline <= 0) ? 1 : preline;
	
	return preline;
}

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
	
	if (!gem) {
		return;
	}
	
	caninos_de_plane_fb_addr_set(priv->hw, gem->paddr);
	caninos_de_plane_enable(priv->hw);
	caninos_de_path_set_go(priv->hw);
}

static void caninos_enable(struct drm_simple_display_pipe *pipe,
                           struct drm_crtc_state *crtc_state,
                           struct drm_plane_state *plane_state)
{
	struct caninos_gfx *priv = container_of(pipe, struct caninos_gfx, pipe);
	struct drm_display_mode *drm_mode = &crtc_state->adjusted_mode;
	struct caninos_hdmi *caninos_hdmi = priv->caninos_hdmi;
	int width, height, vrefresh;
	struct videomode mode;
	
	width = drm_mode->hdisplay;
	height = drm_mode->vdisplay;
	vrefresh = drm_mode_vrefresh(drm_mode);
	
	if ((width == 640) && (height == 480) && (vrefresh == 60))
	{
		mode.xres = 640;
		mode.yres = 480;
		mode.refresh = 60;
    	mode.pixclock = 39721;
		mode.hfp = 16;
		mode.hsw = 96;
		mode.hbp = 48;
		mode.vfp = 10;
		mode.vsw = 2;
		mode.vbp = 33;
		mode.sync = 0;
		
		caninos_hdmi->settings.prelines = caninos_de_calculate_preline(&mode);
		caninos_hdmi->vid = VID640x480P_60_4VS3;
		caninos_hdmi->mode = mode;
		caninos_hdmi->interlace = false;
		caninos_hdmi->vstart = 1;
		caninos_hdmi->repeat = false;
	}
	else if ((width == 720) && (height == 480) && (vrefresh == 60))
	{
		mode.xres = 720;
		mode.yres = 480;
		mode.refresh = 60;
		
		mode.pixclock = 37000;
		mode.hfp = 16;
		mode.hbp = 60;
		mode.vfp = 9;
		mode.vbp = 30;
		mode.hsw = 62;
		mode.vsw = 6;
		mode.sync = 0;
		
		caninos_hdmi->settings.prelines = caninos_de_calculate_preline(&mode);
		caninos_hdmi->vid = VID720x480P_60_4VS3;
		caninos_hdmi->mode = mode;
		caninos_hdmi->interlace = false;
		caninos_hdmi->vstart = 7;
		caninos_hdmi->repeat = false;
	}
	else if ((width == 720) && (height == 576) && (vrefresh == 50))
	{
		mode.xres = 720;
		mode.yres = 576;
		mode.refresh = 50;
		
		mode.pixclock = 37037;
		mode.hfp = 12;
		mode.hbp = 68;
		mode.vfp = 5;
		mode.vbp = 39;
		mode.hsw = 64;
		mode.vsw = 5;
		mode.sync = 0;
		
		caninos_hdmi->settings.prelines = caninos_de_calculate_preline(&mode);
		caninos_hdmi->vid = VID720x576P_50_4VS3;
		caninos_hdmi->mode = mode;
		caninos_hdmi->interlace = false;
		caninos_hdmi->vstart = 1;
		caninos_hdmi->repeat = false;
	}
	else if ((width == 1280) && (height == 720) && (vrefresh == 60)) //ok
	{
		mode.xres = 1280;
		mode.yres = 720;
		mode.refresh = 60;
		mode.pixclock = 13468;
		mode.hfp = 110;
		mode.hsw = 40;
		mode.hbp = 220;
		mode.vfp = 5;
		mode.vsw = 5;
		mode.vbp = 20;
		mode.sync = DSS_SYNC_HOR_HIGH_ACT | DSS_SYNC_VERT_HIGH_ACT;
		
		caninos_hdmi->settings.prelines = caninos_de_calculate_preline(&mode);
		caninos_hdmi->vid = VID1280x720P_60_16VS9;
		caninos_hdmi->mode = mode;
		caninos_hdmi->interlace = false;
		caninos_hdmi->vstart = 1;
		caninos_hdmi->repeat = false;
	}
	else if ((width == 1280) && (height == 720) && (vrefresh == 50))
	{
		mode.xres = 1280;
		mode.yres = 720;
		mode.refresh = 50;
		
		mode.pixclock = 13468;
		mode.hfp = 440;
		mode.hbp = 220;
		mode.vfp = 5;
		mode.vbp = 20;
		mode.hsw = 40;
		mode.vsw = 5;
		mode.sync = DSS_SYNC_HOR_HIGH_ACT | DSS_SYNC_VERT_HIGH_ACT;
		
		caninos_hdmi->settings.prelines = caninos_de_calculate_preline(&mode);
		caninos_hdmi->vid = VID1280x720P_50_16VS9;
		caninos_hdmi->mode = mode;
		caninos_hdmi->interlace = false;
		caninos_hdmi->vstart = 1;
		caninos_hdmi->repeat = false;
	}
	else if ((width == 1920) && (height == 1080) && (vrefresh == 50))
	{
		mode.xres = 1920;
		mode.yres = 1080;
		mode.refresh = 50;
		
		mode.pixclock = 6734;
		mode.hfp = 528;
		mode.hbp = 148;
		mode.vfp = 4;
		mode.vbp = 36;
		mode.hsw = 44;
		mode.vsw = 5;
		mode.sync = DSS_SYNC_HOR_HIGH_ACT | DSS_SYNC_VERT_HIGH_ACT;
		
		caninos_hdmi->settings.prelines = caninos_de_calculate_preline(&mode);
		caninos_hdmi->vid = VID1920x1080P_50_16VS9;
		caninos_hdmi->mode = mode;
		caninos_hdmi->interlace = false;
		caninos_hdmi->vstart = 1;
		caninos_hdmi->repeat = false;
	}
	else //if ((width == 1920) && (height == 1080) && (vrefresh == 60) //ok 
	{
		mode.xres = 1920;
		mode.yres = 1080;
		mode.refresh = 60;
		mode.pixclock = 6734;
		mode.hfp = 88;
		mode.hsw = 44;
		mode.hbp = 148;
		mode.vfp = 4;
		mode.vsw = 5;
		mode.vbp = 36;
		mode.sync = DSS_SYNC_HOR_HIGH_ACT | DSS_SYNC_VERT_HIGH_ACT;
		
		caninos_hdmi->settings.prelines = caninos_de_calculate_preline(&mode);
		caninos_hdmi->vid = VID1920x1080P_60_16VS9;
		caninos_hdmi->mode = mode;
		caninos_hdmi->interlace = false;
		caninos_hdmi->vstart = 1;
		caninos_hdmi->repeat = false;
	}
	
	width = caninos_hdmi->mode.xres;
	height = caninos_hdmi->mode.yres;
	vrefresh = caninos_hdmi->mode.refresh;
	
	caninos_hdmi->ops.video_disable(caninos_hdmi);
	
	{
		caninos_de_ovl_t ovl = { 
			.src = { .w = width, .h = height },
			.dst = { .w = width, .h = height, .x = 0U, .y = 0U }
		};
		
		caninos_de_plane_disable(priv->hw);
		caninos_de_path_disable(priv->hw);
		
		caninos_de_plane_overlay_set(priv->hw, &ovl);
		caninos_de_plane_format_set(priv->hw, DRM_FORMAT_XRGB8888);
		caninos_de_plane_stride_set(priv->hw, width * 4);
		caninos_de_plane_rotate(priv->hw, 0x0);
		
		caninos_de_path_config(priv->hw, width, height, 0x0);
		caninos_de_path_enable(priv->hw);
		caninos_de_path_set_go(priv->hw);
	}
	
	caninos_hdmi->ops.video_enable(caninos_hdmi);
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

int caninos_gfx_pipe_init(struct drm_device *drm)
{
	struct caninos_gfx *priv = drm->dev_private;
	int ret;
	
	if (priv->model == CANINOS_DE_HW_MODEL_K7) {
		priv->hw = caninos_de_k7_init(priv);
	}
	else {
		priv->hw = caninos_de_k5_init(priv);
	}
	
	if (IS_ERR(priv->hw)) {
		return PTR_ERR(priv->hw);
	}
	
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

