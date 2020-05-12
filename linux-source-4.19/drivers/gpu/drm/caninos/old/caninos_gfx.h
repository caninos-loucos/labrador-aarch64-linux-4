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

#include <drm/drm_device.h>
#include <drm/drm_simple_kms_helper.h>

#include "caninos_de_regs.h"

struct caninos_gfx
{
    struct drm_simple_display_pipe pipe;
    struct drm_connector connector;
    struct drm_crtc crtc;
    
    void __iomem *base;
    void __iomem *cvbs_base;
    void __iomem *hdmi_base;
    void __iomem *cmu_base;
    
    struct clk *clk, *parent_clk;
    struct clk *tvout_clk, *cvbspll_clk;
    struct clk *hdmi_dev_clk;
    
    struct reset_control *cvbs_rst;
    struct reset_control *hdmi_rst;
    struct reset_control *de_rst;
};

#define VID720x576I_25_PAL     (0)
#define VID720x480I_30_NTSC    (1)
#define VID640x480P_60_4VS3    (2)
#define VID720x576P_50_4VS3    (3)
#define VID720x480P_60_4VS3    (4)
#define VID1280x720P_60_16VS9  (5)
#define VID1280x720P_50_16VS9  (6)
#define VID1920x1080P_60_16VS9 (7)
#define VID1920x1080P_50_16VS9 (8)
#define VIDEO_MODE_COUNT       (9)

#define VID_SYNC_NOSYNC    (0)
#define VID_SYNC_HOR_HIGH  (0x1 << 0)
#define VID_SYNC_VERT_HIGH (0x1 << 1)

#define VID_VMODE_NONINTERLACED (0)
#define VID_VMODE_INTERLACED    (1)

struct owl_videomode
{
    int xres; /* visible resolution */
    int yres;
    int refresh; /* vertical refresh rate in hz */
    int pixclock; /* pixel clock in ps (pico seconds) */
    int hfp; /* horizontal front porch (pixclocks) */
    int hbp; /* horizontal back porch (pixclocks) */
    int vfp; /* vertical front porch (pixclocks) */
    int vbp; /* vertical back porch (pixclocks) */
    int hsw; /* horizontal synchronization pulse width (pixclocks) */
    int vsw; /* vertical synchronization pulse width (pixclocks) */
    int sync; /* see VID_SYNC_* */
    int vmode; /* see VID_VMODE_* */
};

static const struct owl_videomode video_modes[VIDEO_MODE_COUNT] =
{
    [VID720x576I_25_PAL]     = {720, 576, 25, 13500, 16, 39, 64, 5, 64, 5,
                                VID_SYNC_HOR_HIGH | VID_SYNC_VERT_HIGH,
                                VID_VMODE_INTERLACED},
    
    [VID720x480I_30_NTSC]    = {720, 480, 30, 27000, 16, 30, 60, 62, 6, 9,
                                VID_SYNC_HOR_HIGH | VID_SYNC_VERT_HIGH,
                                VID_VMODE_INTERLACED},
    
    [VID640x480P_60_4VS3] = {},
    
    [VID720x576P_50_4VS3]    = {720, 576, 50, 37037, 12, 68, 5, 39, 64, 5,
                                VID_SYNC_NOSYNC,
                                VID_VMODE_NONINTERLACED},
    
    [VID720x480P_60_4VS3]    = {720, 480, 60, 37000, 16, 60, 9, 30, 62, 6,
                                VID_SYNC_NOSYNC,
                                VID_VMODE_NONINTERLACED},
    
    [VID1280x720P_60_16VS9]  = {1280, 720, 60, 13468, 110, 220, 5, 20, 40, 5,
                                VID_SYNC_HOR_HIGH | VID_SYNC_VERT_HIGH,
                                VID_VMODE_NONINTERLACED},
    
    [VID1280x720P_50_16VS9]  = {1280, 720, 50, 13468, 440, 220, 5, 20, 40, 5,
                                VID_SYNC_HOR_HIGH | VID_SYNC_VERT_HIGH,
                                VID_VMODE_NONINTERLACED},
    
    [VID1920x1080P_60_16VS9] = {1920, 1080, 60, 6734, 88, 148, 4, 36, 44, 5,
                                VID_SYNC_HOR_HIGH | VID_SYNC_VERT_HIGH,
                                VID_VMODE_NONINTERLACED},
                                
    [VID1920x1080P_50_16VS9] = {1920, 1080, 50, 6734, 528, 148, 4, 36, 44, 5,
                                VID_SYNC_HOR_HIGH | VID_SYNC_VERT_HIGH,
                                VID_VMODE_NONINTERLACED},
};

int caninos_gfx_create_pipe(struct drm_device *drm);
int caninos_gfx_create_hdmi_output(struct drm_device *drm);
int caninos_gfx_create_composite_output(struct drm_device *drm);
/*
#define DE_IRQENABLE (0x0000)
#define DE_IRQSTATUS (0x0004)
#define DE_ML_BASE   (0x0400)
#define DE_PATH_BASE (0x0100)

#define DE_SL_FB(x, y) (DE_ML_BASE + (x) * 0x200 + 0x02c + (y) * 0x80)

#define DE_PATH_FCR(n) (DE_PATH_BASE + (n) * 0x100 + 0x004)
#define DE_PATH_FCR_BIT (0)
*/
