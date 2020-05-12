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
#include <linux/reset.h>
#include <linux/delay.h>

#include "caninos_gfx.h"

#define REG_MASK(start, end)	(((1 << ((start) - (end) + 1)) - 1) << (end))
#define REG_VAL(val, start, end) (((val) << (end)) & REG_MASK(start, end))
#define REG_GET_VAL(val, start, end) (((val) & REG_MASK(start, end)) >> (end))
#define REG_SET_VAL(orig, val, start, end) (((orig) & ~REG_MASK(start, end))\
						 | REG_VAL(val, start, end))


#define RECOMMENDED_PRELINE_TIME (60)

static inline void de_write_reg(struct caninos_gfx *priv, u32 val, u32 offset)
{
    writel(val, priv->base + offset);
}

inline u32 de_read_reg(struct caninos_gfx *priv, u32 offset)
{
    return readl(priv->base + offset);
}

int de_calculate_preline(int mode_id)
{
    const struct owl_videomode *mode = &video_modes[mode_id];
    int preline_num;
    
    preline_num = mode->xres + mode->hfp + mode->hbp + mode->hsw;
    preline_num *= mode->pixclock;
    
    if (preline_num != 0)
    {
        preline_num = RECOMMENDED_PRELINE_TIME * 1000000 + preline_num / 2;
        preline_num /= preline_num;
    }
	
    preline_num -= mode->vfp;
    preline_num = (preline_num <= 0 ? 1 : preline_num);
    
    return preline_num;
}

void de_path_set_go(struct caninos_gfx *priv, int path_id)
{
    u32 val;
    val = de_read_reg(priv, DE_PATH_FCR(path_id));
    val = REG_SET_VAL(val, 1, DE_PATH_FCR_BIT, DE_PATH_FCR_BIT);
    de_write_reg(priv, val, DE_PATH_FCR(path_id));
}

void de_set_video_fb(struct caninos_gfx *priv, u32 paddr,
                     int path_id, int video_id)
{
	de_write_reg(priv, paddr, DE_SL_FB(video_id, path_id));
}

void de_power_on(struct caninos_gfx *priv)
{
    uint32_t val;
    int ret = 0, tmp, i;
    
    // reset assert

    void *dmm_reg1 = ioremap(0xe029000c, 4);
    void *dmm_reg2 = ioremap(0xe0290068, 4);
    void *dmm_reg3 = ioremap(0xe0290000, 4);
    void *dmm_reg4 = ioremap(0xe0290008, 4);

    clk_set_parent(priv->clk, priv->parent_clk);
    clk_set_rate(priv->clk, 300000000);
    clk_prepare_enable(priv->clk);
    
    mdelay(1);
    
    // reset deassert

    de_write_reg(priv, 0x3f, DE_MAX_OUTSTANDING);
    de_write_reg(priv, 0x0f, DE_QOS);

    writel(0xf832, dmm_reg1);
    writel(0xf801, dmm_reg4);
    writel(0x500, dmm_reg2);

    writel(0x80000000, dmm_reg3);
    mdelay(1);
    writel(0x80000004, dmm_reg3);

    iounmap(dmm_reg1);
    iounmap(dmm_reg2);
    iounmap(dmm_reg3);
    iounmap(dmm_reg4);
}

void de_power_off(struct caninos_gfx *priv)
{
    clk_disable_unprepare(priv->clk);
}

// The Display SubSystem (DSS)

void caninos_dss_power_on(struct caninos_gfx *priv)
{
    //hdmi_power_on(priv);
    //cvbs_power_on(priv);
    //de_power_on(priv);
}

void caninos_dss_power_off(struct caninos_gfx *priv)
{
    //hdmi_power_off(priv);
    //cvbs_power_off(priv);
    //de_power_off(priv);
}

void caninos_dss_display_enable(struct caninos_gfx *priv, int display, int video_mode)
{
    //
}

void caninos_dss_display_disable(struct caninos_gfx *priv, int display)
{
    //
}


