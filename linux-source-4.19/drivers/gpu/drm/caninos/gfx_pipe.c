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

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>

#include "gfx_drv.h"

static int caninos_crtc_enable_vblank(struct drm_crtc *crtc)
{
    struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
    
    // Clear pending irq
    writel(0x1, pipe->base + DE_IRQSTATUS);

    // Enable path 0 vblank irq
    writel(0x1, pipe->base + DE_IRQENABLE);

    return 0;
}

static void caninos_crtc_disable_vblank(struct drm_crtc *crtc)
{
    struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
    
    // Disable path 0 vblank irq
    writel(0x0, pipe->base + DE_IRQENABLE);

    // Clear pending irq
    writel(0x1, pipe->base + DE_IRQSTATUS);
}

static int caninos_connector_get_modes(struct drm_connector *connector)
{
    struct drm_device *drm = connector->dev;
    int count, max_width, max_height;
    
    max_width = drm->mode_config.max_width;
    max_height = drm->mode_config.max_height;
    
    count = drm_add_modes_noedid(connector, max_width, max_height);
    
    if (count) {
        drm_set_preferred_mode(connector, max_width, max_height);
    }
    
    return count;
}

static void caninos_crtc_enable(struct drm_crtc *crtc, 
                                struct drm_crtc_state *old_state)
{
    //struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
    
    //struct drm_device *drm = crtc->dev;
    //dev_info(drm->dev, "crtc enable.\n");
    
    drm_crtc_vblank_on(crtc);
}

static void caninos_crtc_disable(struct drm_crtc *crtc,
                                 struct drm_crtc_state *old_state)
{
    //struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
    
    //struct drm_device *drm = crtc->dev;
    //dev_info(drm->dev, "crtc disable.\n");
    
    drm_crtc_vblank_off(crtc);
}

static enum drm_mode_status caninos_crtc_mode_valid(struct drm_crtc *crtc,
        const struct drm_display_mode *mode)
{
    int w = mode->hdisplay, h = mode->vdisplay;
    int vrefresh = drm_mode_vrefresh(mode);
    
    struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
    
    //struct drm_device *drm = crtc->dev;
    //dev_info(drm->dev, "is mode valid: %dx%d-%dHz\n", w, h, vrefresh);
    
    if (pipe->output_type == CANINOS_HDMI_OUTPUT)
    {
        if (w != 1920) {
            return MODE_BAD_HVALUE;
        }
        if (h != 1080) {
            return MODE_BAD_VVALUE;
        }
        if (vrefresh != 60) {
            return MODE_BAD;
        }
    }
    
    return MODE_OK;
}

static void caninos_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
    //struct drm_device *drm = crtc->dev;
    //dev_info(drm->dev, "set mode nofb\n");
}

irqreturn_t caninos_gfx_irq_handler(int irq, void *data)
{
    struct drm_device *drm = data;
    struct caninos_gfx *pipe = drm->dev_private;
    u32 val;

    val = readl(pipe->base + DE_IRQSTATUS);

    if (val & 0x1)
    {
        drm_crtc_handle_vblank(&pipe->crtc);
        writel(val, pipe->base + DE_IRQSTATUS);
        return IRQ_HANDLED;
    }

    return IRQ_NONE;
}

static void caninos_plane_atomic_update(struct drm_plane *plane,
                                        struct drm_plane_state *old_pstate)
{
    struct caninos_gfx *pipe = container_of(plane, struct caninos_gfx, plane);
    struct drm_crtc *crtc = &pipe->crtc;
    struct drm_framebuffer *fb = pipe->plane.state->fb;
    struct drm_pending_vblank_event *event;
    struct drm_gem_cma_object *gem;
    u32 val;

    spin_lock_irq(&crtc->dev->event_lock);
    
    event = crtc->state->event;

    if (event)
    {
        crtc->state->event = NULL;

        if (drm_crtc_vblank_get(crtc) == 0) {
            drm_crtc_arm_vblank_event(crtc, event);
        }
        else {
            drm_crtc_send_vblank_event(crtc, event);
        }
    }
    
    spin_unlock_irq(&crtc->dev->event_lock);
    
    if (!fb) {
        return;
    }

    gem = drm_fb_cma_get_gem_obj(fb, 0);
    
    if (!gem) {
        return;
    }
    
    // set video framebuffer address
    writel(gem->paddr, pipe->base + DE_SL_FB(0, 0));
    
    // path set go
    val = readl(pipe->base + DE_PATH_FCR(0));
    val |= BIT(DE_PATH_FCR_BIT);
    writel(val, pipe->base + DE_PATH_FCR(0));
}

static int caninos_crtc_check(struct drm_crtc *crtc,
                              struct drm_crtc_state *state)
{
    bool has_primary = state->plane_mask & drm_plane_mask(crtc->primary);
    
    /* We always want to have an active plane with an active CRTC */
    if (has_primary != state->enable) {
        return -EINVAL;
    }
    return drm_atomic_add_affected_planes(state->state, crtc);
}

static int caninos_plane_atomic_check(struct drm_plane *plane,
                                      struct drm_plane_state *plane_state)
{
    struct caninos_gfx *pipe = container_of(plane, struct caninos_gfx, plane);
    struct drm_crtc_state *crtc_state;
    int ret;
    
    crtc_state = drm_atomic_get_new_crtc_state(plane_state->state, &pipe->crtc);

    ret = drm_atomic_helper_check_plane_state(plane_state, crtc_state,
                                              DRM_PLANE_HELPER_NO_SCALING,
                                              DRM_PLANE_HELPER_NO_SCALING,
                                              false, true);
    return ret;
}


static const struct drm_mode_config_funcs caninos_mode_config_funcs = {
    .fb_create = drm_gem_fb_create,
    .atomic_check = drm_atomic_helper_check,
    .atomic_commit = drm_atomic_helper_commit,
};

static const struct drm_plane_helper_funcs caninos_plane_helper_funcs = {
    .prepare_fb = drm_gem_fb_prepare_fb,
    .atomic_check = caninos_plane_atomic_check,
    .atomic_update = caninos_plane_atomic_update,
};

static const struct drm_plane_funcs caninos_plane_funcs = {
    .update_plane = drm_atomic_helper_update_plane,
    .disable_plane = drm_atomic_helper_disable_plane,
    .destroy = drm_plane_cleanup,
    .reset = drm_atomic_helper_plane_reset,
    .atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static const struct drm_crtc_helper_funcs caninos_crtc_helper_funcs = {
    .mode_valid = caninos_crtc_mode_valid,
    .atomic_check = caninos_crtc_check,
    .atomic_enable = caninos_crtc_enable,
    .atomic_disable = caninos_crtc_disable,
    .mode_set_nofb = caninos_crtc_mode_set_nofb,
};

static const struct drm_crtc_funcs caninos_crtc_funcs = {
    .reset = drm_atomic_helper_crtc_reset,
    .destroy = drm_crtc_cleanup,
    .set_config = drm_atomic_helper_set_config,
    .page_flip = drm_atomic_helper_page_flip,
    .atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
    .enable_vblank = caninos_crtc_enable_vblank,
    .disable_vblank = caninos_crtc_disable_vblank,
};

static const struct drm_encoder_funcs caninos_encoder_funcs = {
    .destroy = drm_encoder_cleanup,
};

static const struct drm_connector_funcs caninos_connector_funcs = {
    .reset = drm_atomic_helper_connector_reset,
    .fill_modes = drm_helper_probe_single_connector_modes,
    .destroy = drm_connector_cleanup,
    .atomic_duplicate_state	= drm_atomic_helper_connector_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs caninos_connector_helper_funcs =
{
    .get_modes = caninos_connector_get_modes,
};

static const uint32_t caninos_formats[] = {
	DRM_FORMAT_XRGB8888,
};

int caninos_gfx_pipe_init(struct drm_device *drm,
                          enum caninos_output_type output_type)
{
    struct caninos_gfx *priv = drm->dev_private;
    
    struct drm_connector *connector = &priv->connector;
    struct drm_encoder *encoder = &priv->encoder;
	struct drm_plane *plane = &priv->plane;
	struct drm_crtc *crtc = &priv->crtc;
	int ret;
    
    priv->output_type = output_type;
    
    drm_mode_config_init(drm);
    
    if (output_type == CANINOS_HDMI_OUTPUT)
    {
        drm->mode_config.min_width = 640;
        drm->mode_config.min_height = 480;
        drm->mode_config.max_width = 1920;
        drm->mode_config.max_height = 1080;
    }
    else
    {
        drm->mode_config.min_width = 720;
        drm->mode_config.min_height = 480;
        drm->mode_config.max_width = 720;
        drm->mode_config.max_height = 576;
    }
    
    drm->mode_config.funcs = &caninos_mode_config_funcs;
    
    ret = drm_vblank_init(drm, 1);
    
    if (ret) {
        return ret;
    }
    
    if (output_type == CANINOS_HDMI_OUTPUT)
    {
        drm_connector_init(drm, connector, &caninos_connector_funcs,
                           DRM_MODE_CONNECTOR_HDMIA);
    }
    else
    {
        drm_connector_init(drm, connector, &caninos_connector_funcs,
                           DRM_MODE_CONNECTOR_Composite);
    }

    drm_connector_helper_add(connector, &caninos_connector_helper_funcs);
    
    drm_plane_helper_add(plane, &caninos_plane_helper_funcs);
    
    ret = drm_universal_plane_init(drm, &priv->plane, 0, &caninos_plane_funcs,
                                   caninos_formats, ARRAY_SIZE(caninos_formats),
                                   NULL, DRM_PLANE_TYPE_PRIMARY, NULL);
    
    if (ret) {
        return ret;
    }
    
    drm_crtc_helper_add(crtc, &caninos_crtc_helper_funcs);
    
    ret = drm_crtc_init_with_planes(drm, crtc, plane, NULL,
                                    &caninos_crtc_funcs, NULL);
    
    if (ret) {
        return ret;
    }
    
    encoder->possible_crtcs = drm_crtc_mask(crtc);
    
    ret = drm_encoder_init(drm, encoder, &caninos_encoder_funcs,
                           DRM_MODE_ENCODER_NONE, NULL);
    
    if (ret) {
        return ret;
    }
    
    return drm_connector_attach_encoder(connector, encoder);
}

//https://elixir.bootlin.com/linux/v4.19.120/source/drivers/gpu/drm/drm_simple_kms_helper.c#L254
//


