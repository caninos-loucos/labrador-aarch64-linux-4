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
 
#include "hdmi.h"

#define DE_IRQENABLE (0x0000)
#define DE_IRQSTATUS (0x0004)
#define DE_MMU_EN    (0x0008)
#define DE_MMU_BASE  (0x000c)
#define DE_ML_BASE   (0x0400)
#define DE_PATH_BASE (0x0100)

#define DE_ML_CFG(x) (DE_ML_BASE + (x) * 0x200 + 0x0000)

#define DE_SL_STR(x, y) (DE_ML_BASE + (x) * 0x200 + 0x044 + (y) * 0x80)
#define DE_SL_FB(x, y)  (DE_ML_BASE + (x) * 0x200 + 0x02c + (y) * 0x80)
#define DE_SL_CFG(x, y) (DE_ML_BASE + (x) * 0x200 + 0x020 + (y) * 0x80)

#define DE_PATH_SIZE(n) (DE_PATH_BASE + (n) * 0x100 + 0x010)
#define DE_PATH_FCR(n) (DE_PATH_BASE + (n) * 0x100 + 0x004)

#define DE_PATH_FCR_BIT (0)

#define DE_PATH_SIZE_WIDTH (0x1 << 12)
#define DE_PATH_SIZE_HEIGHT (0x1 << 12)
#define DE_PATH_SIZE_WIDTH_BEGIN_BIT (0)
#define DE_PATH_SIZE_WIDTH_END_BIT (11)
#define DE_PATH_SIZE_HEIGHT_BEGIN_BIT (16)
#define DE_PATH_SIZE_HEIGHT_END_BIT (27)

#define DE_SL_CFG_FMT_BEGIN_BIT (0)
#define DE_SL_CFG_FMT_END_BIT (4)
#define DE_ML_ROT180_BIT (20)

#define DE_ML_ISIZE(x) (DE_ML_BASE + (x) * 0x200 + 0x0004)

#define DE_SL_CROPSIZE(x, y) (DE_ML_BASE + (x) * 0x200 + 0x048 + (y) * 0x80)

struct caninos_gfx
{
    struct drm_connector connector;
    struct drm_encoder encoder;
    struct drm_plane plane;
    struct drm_crtc crtc;
    
    void __iomem *base;
    void __iomem *cvbs_base;
    void __iomem *cmu_base;
    
    struct clk *clk, *parent_clk;
    struct clk *tvout_clk, *cvbspll_clk;
    
    struct reset_control *cvbs_rst;
    struct reset_control *de_rst;
    
    struct hdmi_ip_ops *hdmi_ip;
};

extern int caninos_gfx_pipe_init(struct drm_device *drm);
                                 
extern irqreturn_t caninos_gfx_irq_handler(int irq, void *data);

