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
#include "caninos_cvbs.h"
#include "caninos_cvbs_regs.h"

static inline void cvbs_write_reg(struct caninos_gfx *priv, u32 offset, u32 val)
{
    writel(val, priv->cvbs_base + offset);
}

inline u32 cvbs_read_reg(struct caninos_gfx *priv, u32 offset)
{
    return readl(priv->cvbs_base + offset);
}

static void configure_ntsc(struct caninos_gfx *priv)
{
    cvbs_write_reg(priv, CVBS_MSR, CVBS_MSR_CVBS_NTSC_M | CVBS_MSR_CVCKS);

    cvbs_write_reg(priv, CVBS_AL_SEPO, (cvbs_read_reg(priv, CVBS_AL_SEPO) &
        (~CVBS_AL_SEPO_ALEP_MASK)) | CVBS_AL_SEPO_ALEP(0x104));

    cvbs_write_reg(priv, CVBS_AL_SEPO, (cvbs_read_reg(priv, CVBS_AL_SEPO) &
        (~CVBS_AL_SEPO_ALSP_MASK)) | CVBS_AL_SEPO_ALSP(0x15));

    cvbs_write_reg(priv, CVBS_AL_SEPE, (cvbs_read_reg(priv, CVBS_AL_SEPE) &
        (~CVBS_AL_SEPE_ALEPEF_MASK)) | CVBS_AL_SEPE_ALEPEF(0x20b));

    cvbs_write_reg(priv, CVBS_AL_SEPE, (cvbs_read_reg(priv, CVBS_AL_SEPE) &
        (~CVBS_AL_SEPE_ALSPEF_MASK)) | CVBS_AL_SEPE_ALSPEF(0x11c));

    cvbs_write_reg(priv, CVBS_AD_SEP, (cvbs_read_reg(priv, CVBS_AD_SEP) &
        (~CVBS_AD_SEP_ADEP_MASK)) | CVBS_AD_SEP_ADEP(0x2cf));

    cvbs_write_reg(priv, CVBS_AD_SEP, (cvbs_read_reg(priv, CVBS_AD_SEP) &
        (~CVBS_AD_SEP_ADSP_MASK)) | CVBS_AD_SEP_ADSP(0x0));
}

static void configure_pal(struct caninos_gfx *priv)
{
    cvbs_write_reg(priv, CVBS_MSR, CVBS_MSR_CVBS_PAL_D | CVBS_MSR_CVCKS);

    cvbs_write_reg(priv, CVBS_AL_SEPO, (cvbs_read_reg(priv, CVBS_AL_SEPO) &
        (~CVBS_AL_SEPO_ALEP_MASK)) | CVBS_AL_SEPO_ALEP(0x136));

    cvbs_write_reg(priv, CVBS_AL_SEPO, (cvbs_read_reg(priv, CVBS_AL_SEPO) &
        (~CVBS_AL_SEPO_ALSP_MASK)) | CVBS_AL_SEPO_ALSP(0x17));

    cvbs_write_reg(priv, CVBS_AL_SEPE, (cvbs_read_reg(priv, CVBS_AL_SEPE) &
        (~CVBS_AL_SEPE_ALEPEF_MASK)) | CVBS_AL_SEPE_ALEPEF(0x26f));

    cvbs_write_reg(priv, CVBS_AL_SEPE, (cvbs_read_reg(priv, CVBS_AL_SEPE) &
        (~CVBS_AL_SEPE_ALSPEF_MASK)) | CVBS_AL_SEPE_ALSPEF(0x150));

    cvbs_write_reg(priv, CVBS_AD_SEP, (cvbs_read_reg(priv, CVBS_AD_SEP) &
        (~CVBS_AD_SEP_ADEP_MASK)) | CVBS_AD_SEP_ADEP(0x2cf));

    cvbs_write_reg(priv, CVBS_AD_SEP, (cvbs_read_reg(priv, CVBS_AD_SEP) &
        (~CVBS_AD_SEP_ADSP_MASK)) | CVBS_AD_SEP_ADSP(0x0));
}

static int cvbs_configure(struct caninos_gfx *priv, int mode)
{
    switch (mode)
    {
    case VID720x576I_25_PAL:
        configure_pal(priv);
        break;
        
    case VID720x480I_30_NTSC:
        configure_ntsc(priv);
        break;
        
    default:
        return -EINVAL;
    }
    return 0;
}

void cvbs_power_off(struct caninos_gfx *priv)
{
    clk_disable_unprepare(priv->tvout_clk);
    clk_disable_unprepare(priv->cvbspll_clk);
    reset_control_assert(priv->cvbs_rst);
}

void cvbs_power_on(struct caninos_gfx *priv)
{
    reset_control_assert(priv->cvbs_rst);
    
    clk_prepare_enable(priv->tvout_clk);
    mdelay(10);
    
    clk_set_rate(priv->cvbspll_clk, 432000000);
    clk_prepare_enable(priv->cvbspll_clk);
    mdelay(50);
    
    reset_control_deassert(priv->cvbs_rst);
}

static void cvbs_output_enable(struct caninos_gfx *priv)
{
    u32 val;
    
    val = cvbs_read_reg(priv, TVOUT_EN);
    val |= TVOUT_EN_CVBS_EN;
    cvbs_write_reg(priv, TVOUT_EN, val);
    
    val = cvbs_read_reg(priv, TVOUT_OCR);
    val |= TVOUT_OCR_DAC3;    /* hdac cvsb enable */
    val |= TVOUT_OCR_INREN;   /* cvbs internal 75ohm enable */
    val &= ~TVOUT_OCR_DACOUT; /* disable color bar */
    cvbs_write_reg(priv, TVOUT_OCR, val);
}

static void cvbs_output_disable(struct caninos_gfx *priv)
{
    int val;
    
    val = cvbs_read_reg(priv, TVOUT_OCR);
    val &= ~TVOUT_OCR_DAC3;  /* hdac cvsb disable */
    val &= ~TVOUT_OCR_INREN; /* cvbs internal 75ohm disable */
    cvbs_write_reg(priv, TVOUT_OCR, val);
    
    val = cvbs_read_reg(priv, TVOUT_EN);
    val &= ~TVOUT_EN_CVBS_EN;
    cvbs_write_reg(priv, TVOUT_EN, val);
}

static void cvbs_preline_config(struct caninos_gfx *priv, int preline)
{
    u32 val;
    
    preline = (preline <= 0 ? 1 : preline);
    preline = (preline > 16 ? 16 : preline);
    
    val = cvbs_read_reg(priv, TVOUT_PRL);
    val = REG_SET_VAL(val, preline - 1, 11, 8);
    cvbs_write_reg(priv, TVOUT_PRL, val);
}

int cvbs_display_enable(struct caninos_gfx *priv, int mode_id, int preline)
{
    cvbs_preline_config(priv, preline);
    cvbs_configure(priv, mode_id);
    cvbs_output_enable(priv);
    return 0;
}

int cvbs_display_disable(struct caninos_gfx *priv)
{
    cvbs_output_disable(priv);
    return 0;
}

