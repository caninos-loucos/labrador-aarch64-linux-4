/*
 * Caninos DRM/KMS driver
 *
 * Copyright (c) 2020 LSI-TEC - Caninos Loucos
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
#include <linux/reset.h>
#include <linux/regmap.h>

#include <drm/drm_crtc_helper.h>
#include <drm/drm_device.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_vblank.h>

#include "caninos_gfx.h"

static struct caninos_gfx *
drm_pipe_to_caninos_gfx(struct drm_simple_display_pipe *pipe)
{
	return container_of(pipe, struct caninos_gfx, pipe);
}

static void caninos_gfx_pipe_enable(struct drm_simple_display_pipe *pipe,
			      struct drm_crtc_state *crtc_state,
			      struct drm_plane_state *plane_state)
{
	struct caninos_gfx *priv = drm_pipe_to_caninos_gfx(pipe);
	struct drm_crtc *crtc = &pipe->crtc;
	
	drm_crtc_vblank_on(crtc);
}

static void caninos_gfx_pipe_disable(struct drm_simple_display_pipe *pipe)
{
	struct caninos_gfx *priv = drm_pipe_to_caninos_gfx(pipe);
	struct drm_crtc *crtc = &pipe->crtc;

	drm_crtc_vblank_off(crtc);
}

static void caninos_gfx_path_set_go(struct caninos_gfx *priv, int path_id)
{
	u32 reg;
	reg = readl(priv->base + DE_PATH_FCR(path_id));
	reg |= BIT(DE_PATH_FCR_BIT);
	writel(reg, priv->base + DE_PATH_FCR(path_id));
}

static void caninos_gfx_set_video_fb(struct caninos_gfx *priv, u32 paddr, int video_id)
{
	writel(paddr, priv->base + DE_SL_FB(video_id, 0));
}

static void caninos_gfx_pipe_update(struct drm_simple_display_pipe *pipe,
				   struct drm_plane_state *plane_state)
{
	struct caninos_gfx *priv = drm_pipe_to_caninos_gfx(pipe);
	struct drm_crtc *crtc = &pipe->crtc;
	struct drm_framebuffer *fb = pipe->plane.state->fb;
	struct drm_pending_vblank_event *event;
	struct drm_gem_cma_object *gem;
	
	spin_lock_irq(&crtc->dev->event_lock);
	event = crtc->state->event;
	if (event) {
		crtc->state->event = NULL;

		if (drm_crtc_vblank_get(crtc) == 0)
			drm_crtc_arm_vblank_event(crtc, event);
		else
			drm_crtc_send_vblank_event(crtc, event);
	}
	spin_unlock_irq(&crtc->dev->event_lock);
	
	if (!fb)
		return;
	
	gem = drm_fb_cma_get_gem_obj(fb, 0);
	
	if (!gem)
		return;
	
	// set video framebuffer address
	caninos_gfx_set_video_fb(priv, gem->paddr, 0);
	
	caninos_gfx_path_set_go(priv, 0);
}

static int caninos_gfx_enable_vblank(struct drm_simple_display_pipe *pipe)
{
	struct caninos_gfx *priv = drm_pipe_to_caninos_gfx(pipe);
	
	// Clear pending irq
	writel(0x1, priv->base + DE_IRQSTATUS);
	
	// Enable path 0 vblank irq
	writel(0x1, priv->base + DE_IRQENABLE);
	
	return 0;
}

static void caninos_gfx_disable_vblank(struct drm_simple_display_pipe *pipe)
{
	struct caninos_gfx *priv = drm_pipe_to_caninos_gfx(pipe);
	
	// Disable path 0 vblank irq
	writel(0x0, priv->base + DE_IRQENABLE);
	
	// Clear pending irq
	writel(0x1, priv->base + DE_IRQSTATUS);
}

static struct drm_simple_display_pipe_funcs caninos_gfx_funcs = {
	.enable	= caninos_gfx_pipe_enable,
	.disable = caninos_gfx_pipe_disable,
	.update	= caninos_gfx_pipe_update,
	.prepare_fb	= drm_gem_fb_simple_display_pipe_prepare_fb,
	.enable_vblank = caninos_gfx_enable_vblank,
	.disable_vblank	= caninos_gfx_disable_vblank,
};

static const uint32_t caninos_gfx_formats[] = {
	DRM_FORMAT_XRGB8888,
};

int caninos_gfx_create_pipe(struct drm_device *drm)
{
	struct caninos_gfx *priv = drm->dev_private;

	return drm_simple_display_pipe_init(drm, &priv->pipe, &caninos_gfx_funcs,
					    caninos_gfx_formats,
					    ARRAY_SIZE(caninos_gfx_formats),
					    NULL,
					    &priv->connector);
}

