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

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>

#include "gfx_drv.h"

static int caninos_de_calculate_preline(struct videomode *mode)
{
    int preline, total;
    total = (mode->xres + mode->hfp + mode->hbp + mode->hsw) * mode->pixclock;
    
    preline = ((RECOMMENDED_PRELINE_TIME * 1000000) + (total / 2)) / total;
    preline -= mode->vfp;
    preline = (preline <= 0) ? 1 : preline;
    
    return preline;
}

static void caninos_de_reset(struct drm_crtc *crtc)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	
	reset_control_assert(pipe->de_rst);
	udelay(200);
	reset_control_deassert(pipe->de_rst);
	udelay(1000);
	
	writel(0x3f, pipe->base + DE_MAX_OUTSTANDING);
	writel(0x0f, pipe->base + DE_QOS);
	
	writel(0xf832, pipe->dcu_base + 0x0c);
	writel(0x100, pipe->dcu_base + 0x68);
	
	writel(0x80000000, pipe->dcu_base);
	udelay(1000);
	writel(0x80000004, pipe->dcu_base);
}

static void caninos_de_video_enable(struct drm_crtc *crtc, bool enable)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	u32 val;
	
	val = readl(pipe->base + DE_PATH_CTL(DE_PATH0));
	val &= ~DE_PATH_CTL_MLn_EN_MASK;
	val |= enable ? DE_PATH_CTL_MLn_EN(DE_MICRO_LAYER0) : 0x0;
	writel(val, pipe->base + DE_PATH_CTL(DE_PATH0));
	
	val = readl(pipe->base + DE_ML_CFG(DE_MICRO_LAYER0));
	val &= ~DE_ML_CFG_SLn_EN_MASK;
	val |= enable ? DE_ML_CFG_SLn_EN(DE_SUB_LAYER0) : 0x0;
	writel(val, pipe->base + DE_ML_CFG(DE_MICRO_LAYER0));
}

static void caninos_de_path_enable(struct drm_crtc *crtc, bool enable)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	u32 val;
	
	val = readl(pipe->base + DE_PATH_EN(DE_PATH0));
	val &= ~BIT(DE_PATH_EN_ENABLE_BIT);
	val |= enable ? BIT(DE_PATH_EN_ENABLE_BIT) : 0x0;
	writel(val, pipe->base + DE_PATH_EN(DE_PATH0));
}

static void caninos_de_path_set_go(struct drm_crtc *crtc)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	u32 val;
	
	val = readl(pipe->base + DE_PATH_FCR(DE_PATH0));
    val |= BIT(DE_PATH_FCR_GO_BIT);
    writel(val, pipe->base + DE_PATH_FCR(DE_PATH0));
}

static void caninos_de_path_set_out_con(struct drm_crtc *crtc, u32 device)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	u32 val;
	
	val = readl(pipe->base + DE_OUTPUT_CON);
	val &= ~DE_OUTPUT_CON_PATH0_DEVICE_MASK;
	val |= DE_OUTPUT_CON_PATH0_DEVICE(device);
	writel(val, pipe->base + DE_OUTPUT_CON);
}

static void caninos_de_path_set_size(struct drm_crtc *crtc, u32 w, u32 h)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	u32 val = DE_PATH_SIZE_WIDTH(w - 1U) | DE_PATH_SIZE_HEIGHT(h - 1U);
	writel(val, pipe->base + DE_PATH_SIZE(DE_PATH0));
}

static void caninos_de_path_ilace_enable(struct drm_crtc *crtc, bool enable)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	u32 val;
	
	val = readl(pipe->base + DE_PATH_CTL(DE_PATH0));
	val &= ~BIT(DE_PATH_CTL_ILACE_BIT);
	val |= enable ? BIT(DE_PATH_CTL_ILACE_BIT) : 0x0;
	writel(val, pipe->base + DE_PATH_CTL(DE_PATH0));
}

static void caninos_de_path_yuv_enable(struct drm_crtc *crtc, bool enable)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	u32 val;
	
	val = readl(pipe->base + DE_PATH_CTL(DE_PATH0));
	val &= ~BIT(DE_PATH_CTL_RGB_YUV_EN_BIT);
	val |= enable ? BIT(DE_PATH_CTL_RGB_YUV_EN_BIT) : 0x0;
	writel(val, pipe->base + DE_PATH_CTL(DE_PATH0));
}

static void caninos_de_path_set_bk_color(struct drm_crtc *crtc, u32 color)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	writel(color, pipe->base + DE_PATH_BK(DE_PATH0));
}

static void caninos_video_fb_addr_set(struct drm_crtc *crtc, u32 paddr)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	writel(paddr, pipe->base + DE_SL_FB(DE_MICRO_LAYER0, DE_SUB_LAYER0));
}

static void caninos_video_pitch_set(struct drm_crtc *crtc, u32 pitch)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	writel(pitch, pipe->base + DE_SL_STR(DE_MICRO_LAYER0, DE_SUB_LAYER0));
}

static u32 caninos_drm_color_mode_to_hw_mode(u32 color_mode)
{
	u32 hw_format = 0U;
	
	switch (color_mode)
	{
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
		hw_format = 0U;
		break;
		
	case DRM_FORMAT_ABGR8888: 
	case DRM_FORMAT_XBGR8888:
		hw_format = 1U;
		break;
		
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX8888:
		hw_format = 2U;
		break;
		
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRX8888:
		hw_format = 3U;
		break;
	}
	return hw_format;
}

static void caninos_video_format_set(struct drm_crtc *crtc, u32 color_mode)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	u32 hw_format, val;
	
	hw_format = caninos_drm_color_mode_to_hw_mode(color_mode);
	
	val = readl(pipe->base + DE_SL_CFG(DE_MICRO_LAYER0, DE_SUB_LAYER0));
	val &= ~DE_SL_CFG_FMT_MASK;
	val |= DE_SL_CFG_FMT(hw_format);
	writel(val, pipe->base + DE_SL_CFG(DE_MICRO_LAYER0, DE_SUB_LAYER0));
}

static void caninos_video_rotate_set(struct drm_crtc *crtc, bool rotate)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	u32 val;
	
	val = readl(pipe->base + DE_ML_CFG(DE_MICRO_LAYER0));
	val &= ~BIT(DE_ML_CFG_ROT180_BIT);
	val |= rotate ? BIT(DE_ML_CFG_ROT180_BIT) : 0x0;
	writel(val, pipe->base + DE_ML_CFG(DE_MICRO_LAYER0));
}

static void caninos_video_crop_set(struct drm_crtc *crtc,
                                   u32 slw, u32 slh, u32 mlw, u32 mlh)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	u32 val;
	
	val = DE_PATH_SIZE_WIDTH(slw - 1U) | DE_PATH_SIZE_HEIGHT(slh - 1U);
	writel(val, pipe->base + DE_SL_CROPSIZE(DE_MICRO_LAYER0, DE_SUB_LAYER0));
	
	val = DE_PATH_SIZE_WIDTH(mlw - 1U) | DE_PATH_SIZE_HEIGHT(mlh - 1U);
	writel(val, pipe->base + DE_ML_ISIZE(DE_MICRO_LAYER0));
}

static void caninos_video_display_set(struct drm_crtc *crtc,
                                      u32 slx, u32 sly, u32 mlx, u32 mly,
                                      u32 outw, u32 outh)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	u32 val;
	
	val = DE_PATH_SIZE_WIDTH(slx) | DE_PATH_SIZE_HEIGHT(sly);
	writel(val, pipe->base + DE_SL_COOR(DE_MICRO_LAYER0, DE_SUB_LAYER0));
	
	val = DE_PATH_SIZE_WIDTH(mlx) | DE_PATH_SIZE_HEIGHT(mly);
	writel(val, pipe->base + DE_PATH_COOR(DE_PATH0, DE_MICRO_LAYER0));
	
	val = DE_PATH_SIZE_WIDTH(outw - 1U) | DE_PATH_SIZE_HEIGHT(outh - 1U);
	writel(val, pipe->base + DE_SCALER_OSZIE(DE_PATH0));
}

static void caninos_video_alpha_set(struct drm_crtc *crtc,
                                    enum caninos_blending_type blending,
                                    u8 alpha)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	u32 val;
	
	if (blending == CANINOS_BLENDING_NONE) {
		alpha = 0xff;
	}
	
	val = readl(pipe->base + DE_SL_CFG(DE_MICRO_LAYER0, DE_SUB_LAYER0));
	
	val &= ~(DE_SL_CFG_GLOBAL_ALPHA_MASK | BIT(DE_SL_CFG_DATA_MODE_BIT));
	val |= DE_SL_CFG_GLOBAL_ALPHA(alpha);
	
	if (blending == CANINOS_BLENDING_COVERAGE) {
		val |= BIT(DE_SL_CFG_DATA_MODE_BIT);
	}
	
	writel(val, pipe->base + DE_SL_CFG(DE_MICRO_LAYER0, DE_SUB_LAYER0));
}

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
        drm_set_preferred_mode(connector, 1920, 1080);
    }
    
    return count;
}

static void caninos_crtc_enable(struct drm_crtc *crtc, 
                                struct drm_crtc_state *old_state)
{
	//struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	//dev_info(pipe->dev, "%s routine called\n", __FUNCTION__);
	
    drm_crtc_vblank_on(crtc);
}

static void caninos_crtc_disable(struct drm_crtc *crtc,
                                 struct drm_crtc_state *old_state)
{
	//struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	//dev_info(pipe->dev, "%s routine called\n", __FUNCTION__);
	
	drm_crtc_vblank_off(crtc);
}

static enum drm_mode_status caninos_crtc_mode_valid(struct drm_crtc *crtc,
        const struct drm_display_mode *mode)
{
    int w = mode->hdisplay, h = mode->vdisplay;
    int vrefresh = drm_mode_vrefresh(mode);
    
    //struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	//dev_info(pipe->dev, "%s routine called\n", __FUNCTION__);
    
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

static void caninos_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	struct drm_display_mode *drm_mode = &crtc->state->adjusted_mode;
	struct caninos_hdmi *caninos_hdmi = pipe->caninos_hdmi;
	int width, height, vrefresh;
	struct videomode mode;
	
	//dev_info(pipe->dev, "%s routine called\n", __FUNCTION__);
	
	width = drm_mode->hdisplay;
	height = drm_mode->vdisplay;
	vrefresh = drm_mode_vrefresh(drm_mode);
	
	if ((width == 640) && (height == 480) && (vrefresh == 60)) //ok
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
	
	caninos_de_reset(crtc);
	caninos_de_path_set_out_con(crtc, DE_OUTPUT_CON_HDMI);
	caninos_de_path_set_size(crtc, width, height);
	caninos_de_path_ilace_enable(crtc, caninos_hdmi->interlace);
	caninos_de_path_yuv_enable(crtc, false);
	caninos_de_path_set_bk_color(crtc, 0x0);
	
	caninos_video_pitch_set(crtc, width * 4);
	caninos_video_format_set(crtc, DRM_FORMAT_XRGB8888);
	caninos_video_rotate_set(crtc, false);
	caninos_video_crop_set(crtc, width, height, width, height);
	caninos_video_display_set(crtc, 0, 0, 0, 0, width, height);
	caninos_video_alpha_set(crtc, CANINOS_BLENDING_NONE, 0x0);
	
	caninos_de_video_enable(crtc, true);
	caninos_de_path_enable(crtc, true);
	
	caninos_hdmi->ops.video_enable(caninos_hdmi);
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
    
	caninos_video_fb_addr_set(crtc, gem->paddr);
	caninos_de_path_set_go(crtc);
}

static int caninos_crtc_check(struct drm_crtc *crtc,
                              struct drm_crtc_state *state)
{
    bool has_primary = state->plane_mask & drm_plane_mask(crtc->primary);
    
    //struct caninos_gfx *pipe = container_of(crtc, struct caninos_gfx, crtc);
	//dev_info(pipe->dev, "%s routine called\n", __FUNCTION__);
    
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

int caninos_gfx_pipe_init(struct drm_device *drm)
{
    struct caninos_gfx *priv = drm->dev_private;
    
    struct drm_connector *connector = &priv->connector;
    struct drm_encoder *encoder = &priv->encoder;
	struct drm_plane *plane = &priv->plane;
	struct drm_crtc *crtc = &priv->crtc;
	int ret;
    
    drm_mode_config_init(drm);
    
    drm->mode_config.min_width = 480;
    drm->mode_config.min_height = 480;
    drm->mode_config.max_width = 1920;
    drm->mode_config.max_height = 1920;
    
    drm->mode_config.funcs = &caninos_mode_config_funcs;
    
    ret = drm_vblank_init(drm, 1);
    
    if (ret) {
        return ret;
    }
    
	drm_connector_init(drm, connector, &caninos_connector_funcs,
	                   DRM_MODE_CONNECTOR_HDMIA);
	
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

