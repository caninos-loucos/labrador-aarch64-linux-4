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
#include "caninos_hdmi_regs.h"

#define PLL_REG        (0x18)
#define PLL_24M_EN     (23)
#define PLL_EN         (3)
#define PLL_DEBUG0_REG (0xF0)
#define PLL_DEBUG1_REG (0xF4)

#define PACKET_PERIOD (1)

enum hdmi_packet_type
{
    PACKET_AVI_SLOT   = 0,
    PACKET_AUDIO_SLOT = 1,
    PACKET_SPD_SLOT   = 2,
    PACKET_GBD_SLOT   = 3,
    PACKET_VS_SLOT    = 4,
    PACKET_HFVS_SLOT  = 5,
    PACKET_MAX,
};

static inline void hdmi_write_reg(struct caninos_gfx *priv, u32 offset, u32 val)
{
    writel(val, priv->hdmi_base + offset);
}

static inline u32 hdmi_read_reg(struct caninos_gfx *priv, u32 offset)
{
    return readl(priv->hdmi_base + offset);
}

static inline bool hdmi_is_video_enabled(struct caninos_gfx *priv)
{
    return (hdmi_read_reg(priv, HDMI_CR) & 0x01) != 0;
}

struct hdmi_reg_values
{
    u32 pll_val;
    u32 pll_debug0_val;
    u32 pll_debug1_val;
    u32 tx_1, tx_2;
};

static int hdmi_update_reg_values(struct hdmi_reg_values *reg_values,
                                  int mode_id)
{
    reg_values->pll_val = 0;
    reg_values->pll_debug0_val = 0;
    reg_values->pll_debug1_val = 0;
    reg_values->tx_1 = 0;
    reg_values->tx_2 = 0;
    
    switch (mode_id)
    {
    case VID640x480P_60_4VS3:
        reg_values->pll_val = 0x00000008; /* 25.2MHz */
        reg_values->tx_1 = 0x819c2984;
        reg_values->tx_2 = 0x18f80f39;
        break;
        
    case VID720x576P_50_4VS3:
    case VID720x480P_60_4VS3:
        reg_values->pll_val = 0x00010008; /* 27MHz */
        reg_values->tx_1 = 0x819c2984;
        reg_values->tx_2 = 0x18f80f39;
        break;
        
    case VID1280x720P_60_16VS9:
    case VID1280x720P_50_16VS9:
        reg_values->pll_val = 0x00040008; /* 74.25MHz */
        reg_values->tx_1 = 0x81982984;
        reg_values->tx_2 = 0x18f80f39;
        break;
        
    case VID1920x1080P_60_16VS9:
    case VID1920x1080P_50_16VS9:
        reg_values->pll_val = 0x00060008; /* 148.5MHz */
        reg_values->tx_1 = 0x81942988;
        reg_values->tx_2 = 0x18fe0f39;
        break;
         
    default:
        return -EINVAL;
    }
    return 0;
}

static void hdmi_tmds_ldo_enable(struct caninos_gfx *priv,
                                 struct hdmi_reg_values *reg_values)
{
    u32 val;
    val = reg_values->tx_2 & (~((0xf << 8) | (1 << 17)));
    hdmi_write_reg(priv, HDMI_TX_2, val);
}

static void hdmi_phy_enable(struct caninos_gfx *priv,
                            struct hdmi_reg_values *reg_values)
{
    u32 val;
    val = hdmi_read_reg(priv, TMDS_EODR0);
    val = REG_SET_VAL(val, 1, 31, 31);
    hdmi_write_reg(priv, TMDS_EODR0, val);
    hdmi_write_reg(priv, HDMI_TX_1, reg_values->tx_1);
}

static void hdmi_pll_enable(struct caninos_gfx *priv,
                            struct hdmi_reg_values *reg_values)
{
    u32 val;
    
    /* 24M enable */
    val = readl(priv->cmu_base + PLL_REG);
    val |= (1 << PLL_24M_EN);
    writel(val, priv->cmu_base + PLL_REG);
    mdelay(1);
    
    /* set PLL, only bit18:16 of pll_val is used */
    val = readl(priv->cmu_base + PLL_REG);
    val &= ~(0x7 << 16);
    val |= (reg_values->pll_val & (0x7 << 16));
    writel(val, priv->cmu_base + PLL_REG);
    mdelay(1);
    
    /* set debug PLL */
    writel(reg_values->pll_debug0_val, priv->cmu_base + PLL_DEBUG0_REG);
    writel(reg_values->pll_debug1_val, priv->cmu_base + PLL_DEBUG1_REG);
    
    /* enable PLL */
    val = readl(priv->cmu_base + PLL_REG);
    val |= (1 << PLL_EN);
    writel(val, priv->cmu_base + PLL_REG);
    mdelay(1);
    
    /* TDMS clock calibration */
    val = hdmi_read_reg(priv, CEC_DDC_HPD);
    /* 0 to 1, start calibration */
    val = REG_SET_VAL(val, 0, 20, 20);
    hdmi_write_reg(priv, CEC_DDC_HPD, val);
    udelay(10);
    
    val = REG_SET_VAL(val, 1, 20, 20);
    hdmi_write_reg(priv, CEC_DDC_HPD, val);
    
    while (1)
    {
        val = hdmi_read_reg(priv, CEC_DDC_HPD);
        if ((val >> 24) & 0x1) {
            break;
        }
    }
}

static void hdmi_video_timing_config(struct caninos_gfx *priv, int mode_id)
{
    const struct owl_videomode *mode = &video_modes[mode_id];
    bool vsync_pol, hsync_pol;
    uint32_t val;
    
    vsync_pol = ((mode->sync & VID_SYNC_VERT_HIGH) == 0) ? 0x1 : 0x0;
    hsync_pol = ((mode->sync & VID_SYNC_HOR_HIGH) == 0) ? 0x1 : 0x0;
    
    val = hdmi_read_reg(priv, HDMI_SCHCR);
    val = REG_SET_VAL(val, hsync_pol, 1, 1);
    val = REG_SET_VAL(val, vsync_pol, 2, 2);
    hdmi_write_reg(priv, HDMI_SCHCR, val);
    
    val = hdmi_read_reg(priv, HDMI_VICTL);
    val = REG_SET_VAL(val, 0x0, 29, 29); /* no pixel repeat */
    hdmi_write_reg(priv, HDMI_VICTL, val);
}

static void hdmi_video_format_config(struct caninos_gfx *priv, int mode_id)
{
    const struct owl_videomode *mode = &video_modes[mode_id];
    u32 val, val_hp, val_vp;
    
    val_hp = mode->xres + mode->hbp + mode->hfp + mode->hsw;
    val_vp = mode->yres + mode->vbp + mode->vfp + mode->vsw;
    
    val = hdmi_read_reg(priv, HDMI_VICTL);
    val = REG_SET_VAL(val, val_hp - 1, 28, 16);
    val = REG_SET_VAL(val, val_vp - 1, 15, 4);
    hdmi_write_reg(priv, HDMI_VICTL, val);
}

static void hdmi_video_interface_config(struct caninos_gfx *priv, int mode_id)
{
    const struct owl_videomode *mode = &video_modes[mode_id];
    u32 val;
    
    hdmi_write_reg(priv, HDMI_VIVSYNC, 0x0);
    
    val = hdmi_read_reg(priv, HDMI_VIVHSYNC);
    val = REG_SET_VAL(val, mode->hsw - 1, 8, 0);
    val = REG_SET_VAL(val, mode->yres + mode->vbp + 
                      mode->vfp + mode->vsw - 1, 23, 12);
    val = REG_SET_VAL(val, mode->vsw - 1, 27, 24);
    hdmi_write_reg(priv, HDMI_VIVHSYNC, val);
    
    val = hdmi_read_reg(priv, HDMI_VIALSEOF);
    val = REG_SET_VAL(val, mode->vsw + mode->vbp + mode->yres - 1, 23, 12);
    val = REG_SET_VAL(val, mode->vsw + mode->vbp - 1, 10, 0);
    hdmi_write_reg(priv, HDMI_VIALSEOF, val);
    
    hdmi_write_reg(priv, HDMI_VIALSEEF, 0x0);
    
    val = hdmi_read_reg(priv, HDMI_VIADLSE);
    val = REG_SET_VAL(val, mode->hbp + mode->hsw - 1, 11, 0);
    val = REG_SET_VAL(val, mode->xres + mode->hbp + mode->hsw - 1, 28, 16);
    hdmi_write_reg(priv, HDMI_VIADLSE, val);
}

static void hdmi_video_interval_packet_config
    (struct caninos_gfx *priv, int mode_id)
{
    u32 val;
    
    switch (mode_id)
    {
    case VID640x480P_60_4VS3:
    case VID720x480P_60_4VS3:
    case VID720x576P_50_4VS3:
        val = 0x701;
        break;
        
    case VID1280x720P_60_16VS9:
    case VID1280x720P_50_16VS9:
    case VID1920x1080P_50_16VS9:
        val = 0x1107;
        break;
        
    case VID1920x1080P_60_16VS9:
        val = 0x1105;
        break;
        
    default:
        val = 0x1107;
        break;
    }
    
    hdmi_write_reg(priv, HDMI_DIPCCR, val);
}

static void hdmi_core_input_src_config(struct caninos_gfx *priv)
{
    u32 val = hdmi_read_reg(priv, HDMI_ICR);
    val = REG_SET_VAL(val, 0x00, 24, 24); /* sorce is DE */
    hdmi_write_reg(priv, HDMI_ICR, val);
}

static void hdmi_core_pixel_fomat_config(struct caninos_gfx *priv)
{
    u32 val;
    val = hdmi_read_reg(priv, HDMI_SCHCR);
    val = REG_SET_VAL(val, 0x0, 5, 4); /* RGB444 */
    hdmi_write_reg(priv, HDMI_SCHCR, val);
}

static void hdmi_core_preline_config(struct caninos_gfx *priv, int preline)
{
    u32 val;
    
    preline = (preline <= 0 ? 1 : preline);
    preline = (preline > 16 ? 16 : preline);
    
    val = hdmi_read_reg(priv, HDMI_SCHCR);
    val = REG_SET_VAL(val, preline - 1, 23, 20);
    hdmi_write_reg(priv, HDMI_SCHCR, val);
}

static void hdmi_core_deepcolor_mode_config(struct caninos_gfx *priv)
{
    u32 val;
    val = hdmi_read_reg(priv, HDMI_SCHCR);
    val = REG_SET_VAL(val, 0x0, 17, 16); /* 8 bits */
    hdmi_write_reg(priv, HDMI_SCHCR, val);
}

static void hdmi_core_mode_config(struct caninos_gfx *priv)
{
    u32 val = hdmi_read_reg(priv, HDMI_SCHCR);
    val = REG_SET_VAL(val, 0x1, 0, 0); /* hdmi mode */
    hdmi_write_reg(priv, HDMI_SCHCR, val);
}

static void hdmi_core_invert_config(struct caninos_gfx *priv)
{
    u32 val;
    val = hdmi_read_reg(priv, HDMI_SCHCR);
    val = REG_SET_VAL(val, 0x0, 28, 28); /* no bit invert */
    val = REG_SET_VAL(val, 0x0, 29, 29); /* no channel invert */
    hdmi_write_reg(priv, HDMI_SCHCR, val);
}

static void hdmi_core_colordepth_config(struct caninos_gfx *priv)
{
    u32 val;
    
    val = hdmi_read_reg(priv, HDMI_GCPCR);
    
    val = REG_SET_VAL(val, 0, 7, 4);
    val = REG_SET_VAL(val, 1, 31, 31);
    val = REG_SET_VAL(val, 0, 30, 30); /* 24 bits per pixel */
    
    /* clear specify avmute flag in gcp packet */
    val = REG_SET_VAL(val, 1, 1, 1);
    
    hdmi_write_reg(priv, HDMI_GCPCR, val);
}

static void hdmi_core_3d_mode_config(struct caninos_gfx *priv)
{
    u32 val = hdmi_read_reg(priv, HDMI_SCHCR);
    val = REG_SET_VAL(val, 0x0, 8, 8); /* disable 3D frame */
    hdmi_write_reg(priv, HDMI_SCHCR, val);
}

static void hdmi_video_start(struct caninos_gfx *priv,
                             struct hdmi_reg_values *reg_values)
{
    u32 val;
    val = hdmi_read_reg(priv, HDMI_CR);
    val = REG_SET_VAL(val, 1, 0, 0);
    hdmi_write_reg(priv, HDMI_CR, val);
    
    val = hdmi_read_reg(priv, HDMI_TX_2);
    val = REG_SET_VAL(val, (reg_values->tx_2 >> 8) & 0xf, 11, 8);
    val = REG_SET_VAL(val, (reg_values->tx_2 >> 17) & 0x1, 17, 17);
    hdmi_write_reg(priv, HDMI_TX_2, val);
}

static void hdmi_video_stop(struct caninos_gfx *priv)
{
    u32 val;
    
    val = hdmi_read_reg(priv, HDMI_TX_2);
    val = REG_SET_VAL(val, 0x0, 11, 8);
    val = REG_SET_VAL(val, 0x0, 17, 17);
    hdmi_write_reg(priv, HDMI_TX_2, val);
    
    val = hdmi_read_reg(priv, HDMI_CR);
    val = REG_SET_VAL(val, 0, 0, 0);
    hdmi_write_reg(priv, HDMI_CR, val);
}

static void hdmi_pll_disable(struct caninos_gfx *priv)
{
    u32 val;
    
    val = readl(priv->cmu_base + PLL_REG);
    val &= ~(1 << PLL_24M_EN);
    val &= ~(1 << PLL_EN);
    writel(val, priv->cmu_base + PLL_REG);
    
    /* reset TVOUTPLL */
    writel(0, priv->cmu_base + PLL_REG);
    
    /* reset TVOUTPLL_DEBUG0 & TVOUTPLL_DEBUG1 */
    writel(0x0, priv->cmu_base + PLL_DEBUG0_REG);
    writel(0x2614a, priv->cmu_base + PLL_DEBUG1_REG);
}

static void hdmi_phy_disable(struct caninos_gfx *priv)
{
    u32 val;
    
    /* TMDS Encoder */
    val = hdmi_read_reg(priv, TMDS_EODR0);
    val = REG_SET_VAL(val, 0, 31, 31);
    hdmi_write_reg(priv, TMDS_EODR0, val);
    
    /* txpll_pu */
    val = hdmi_read_reg(priv, HDMI_TX_1);
    val = REG_SET_VAL(val, 0, 23, 23);
    hdmi_write_reg(priv, HDMI_TX_1, val);
}

static void hdmi_tmds_ldo_disable(struct caninos_gfx *priv)
{
    u32 val;
    val = hdmi_read_reg(priv, HDMI_TX_2);
    val = REG_SET_VAL(val, 0, 27, 27); /* LDO_TMDS power off */
    hdmi_write_reg(priv, HDMI_TX_2, val);
}

static int hdmi_packet_generate(struct caninos_gfx *priv, u32 no, u8 *pkt)
{
    u32 addr = 126 + no * 14;
    u32 reg[9], val;
    u8 tpkt[36];
    int i, j;

    if (no >= PACKET_MAX) {
        return -1;
    }

    /* Packet Header */
    tpkt[0] = pkt[0];
    tpkt[1] = pkt[1];
    tpkt[2] = pkt[2];
    tpkt[3] = 0;

    /* Packet Word0 */
    tpkt[4] = pkt[3];
    tpkt[5] = pkt[4];
    tpkt[6] = pkt[5];
    tpkt[7] = pkt[6];

    /* Packet Word1 */
    tpkt[8] = pkt[7];
    tpkt[9] = pkt[8];
    tpkt[10] = pkt[9];
    tpkt[11] = 0;

    /* Packet Word2 */
    tpkt[12] = pkt[10];
    tpkt[13] = pkt[11];
    tpkt[14] = pkt[12];
    tpkt[15] = pkt[13];

    /* Packet Word3 */
    tpkt[16] = pkt[14];
    tpkt[17] = pkt[15];
    tpkt[18] = pkt[16];
    tpkt[19] = 0;

    /* Packet Word4 */
    tpkt[20] = pkt[17];
    tpkt[21] = pkt[18];
    tpkt[22] = pkt[19];
    tpkt[23] = pkt[20];

    /* Packet Word5 */
    tpkt[24] = pkt[21];
    tpkt[25] = pkt[22];
    tpkt[26] = pkt[23];
    tpkt[27] = 0;

    /* Packet Word6 */
    tpkt[28] = pkt[24];
    tpkt[29] = pkt[25];
    tpkt[30] = pkt[26];
    tpkt[31] = pkt[27];

    /* Packet Word7 */
    tpkt[32] = pkt[28];
    tpkt[33] = pkt[29];
    tpkt[34] = pkt[30];
    tpkt[35] = 0;
    
    for (i = 0; i < 9; i++)
    {
        reg[i] = 0;
        
        for (j = 0; j < 4; j++) {
            reg[i] |= (tpkt[i * 4 + j]) << (j * 8);
        }
    }
    
    hdmi_write_reg(priv, HDMI_OPCR, (1 << 8) | (addr & 0xff));
    hdmi_write_reg(priv, HDMI_ORP6PH, reg[0]);
    hdmi_write_reg(priv, HDMI_ORSP6W0, reg[1]);
    hdmi_write_reg(priv, HDMI_ORSP6W1, reg[2]);
    hdmi_write_reg(priv, HDMI_ORSP6W2, reg[3]);
    hdmi_write_reg(priv, HDMI_ORSP6W3, reg[4]);
    hdmi_write_reg(priv, HDMI_ORSP6W4, reg[5]);
    hdmi_write_reg(priv, HDMI_ORSP6W5, reg[6]);
    hdmi_write_reg(priv, HDMI_ORSP6W6, reg[7]);
    hdmi_write_reg(priv, HDMI_ORSP6W7, reg[8]);

    val = hdmi_read_reg(priv, HDMI_OPCR);
    val |= (0x1 << 31);
    hdmi_write_reg(priv, HDMI_OPCR, val);
    
    i = 100;
    
    while (i--)
    {
        val = hdmi_read_reg(priv, HDMI_OPCR);
        val = val >> 31;
        
        if (val == 0) {
            break;
        }
        udelay(1);
    }
    return 0;
}

static int hdmi_packet_send(struct caninos_gfx *priv, u32 no, int period)
{
    u32 val;
    
    if (no > PACKET_MAX || no < 0) {
        return -1;
    }
    if (period > 0xf || period < 0) {
        return -1;
    }
    
    val = hdmi_read_reg(priv, HDMI_RPCR);
    val &= (~(1 << no));
    hdmi_write_reg(priv, HDMI_RPCR, val);
    
    val = hdmi_read_reg(priv, HDMI_RPCR);
    val &= (~(0xf << (no * 4 + 8)));
    hdmi_write_reg(priv, HDMI_RPCR, val);
    
    /* enable and set period */
    if(period)
    {
        val = hdmi_read_reg(priv, HDMI_RPCR);
        val |= (period << (no * 4 + 8));
        hdmi_write_reg(priv, HDMI_RPCR,  val);
        
        val = hdmi_read_reg(priv, HDMI_RPCR);
        val |= (0x1 << no);
        hdmi_write_reg(priv, HDMI_RPCR,  val);
    }
    return 0;
}

static int hdmi_gen_spd_infoframe(struct caninos_gfx *priv)
{
    static u8 pkt[32];
    static char spdname[8] = "Caninos";
    static char spddesc[16] = "CaninosLabrador";
    unsigned int checksum = 0;
    unsigned int i;
    
    /* clear buffer */
    for (i = 0; i < 32; i++) {
        pkt[i] = 0;
    }
    
    /* header */
    pkt[0] = 0x80 | 0x03; /* HB0: Packet Type = 0x83 */
    pkt[1] = 1;           /* HB1: version = 1 */
    pkt[2] = 0x1f & 25;   /* HB2: len = 25 */
    pkt[3] = 0x00;        /* PB0: checksum = 0 */
    
    /* data */
    
    /* Vendor Name, 8 bytes */
    memcpy(&pkt[4], spdname, 8);
    
    /* Product Description, 16 bytes */
    memcpy(&pkt[12], spddesc, 16);
    
    /* Source Device Information, Digital STB */
    pkt[28] = 0x1;
    
    /* checksum */
    for (i = 0; i < 31; i++) {
        checksum += pkt[i];
    }
    pkt[3] = (~checksum + 1)  & 0xff;
    
    /* set to RAM Packet */
    hdmi_packet_generate(priv, PACKET_SPD_SLOT, pkt);
    hdmi_packet_send(priv, PACKET_SPD_SLOT, PACKET_PERIOD);
    return 0;
}

static int hdmi_gen_avi_infoframe(struct caninos_gfx *priv, int mode_id)
{
    static u8 pkt[32];
    u32 checksum = 0, i = 0;
    u8 AR, vid;
    
    switch (mode_id)
    {
    case VID640x480P_60_4VS3:
        AR = 0x01;
        vid = 0x01;
        break;
        
    case VID720x480P_60_4VS3:
        AR = 0x01;
        vid = 0x02;
        break;
        
    case VID720x576P_50_4VS3:
        AR = 0x01;
        vid = 0x17;
        break;
        
    case VID1280x720P_60_16VS9:
        AR = 0x02;
        vid = 0x04;
        break;
        
    case VID1280x720P_50_16VS9:
        AR = 0x02;
        vid = 0x19;
        break;
        
    case VID1920x1080P_50_16VS9:
        AR = 0x02;
        vid = 0x31;
        break;
        
    case VID1920x1080P_60_16VS9:
        AR = 0x02;
        vid = 0x16;
        break;
        
    default:
        return -EINVAL;
    }
    
    /* clear buffer */
    for (i = 0; i < 32; i++) {
        pkt[i] = 0;
    }
    
    /* header */
    pkt[0] = 0x80 | 0x02; /* header = 0x82 */
    pkt[1] = 2;           /* version = 2 */
    pkt[2] = 0x1f & 13;   /* len = 13 */
    pkt[3] = 0x00;        /* checksum = 0 */
    
    /* data */
    
    /*
     * PB1--Y1:Y0 = colorformat; R3:R1 is invalid;
     * no bar info and scan info
     */
    pkt[4] = 0x00; /* 0--Normal YCC601 or YCC709; */
    
    /*
     * PB2--Colorimetry: SMPTE 170M | ITU601;
     * Picture aspect Ratio; same as picture aspect ratio
     */
    pkt[5] = (0x1 << 6) | (AR << 4) | (0x8);
    
    /* PB3--No known non-uniform scaling */
    pkt[6] = 0x0;
    
    /* PB4--Video Id */
    pkt[7] = vid;
    
    /* PB5--Pixel repeat time */
    pkt[8] = 0;
    
    /* PB6--PB13: Bar Info, no bar info */
    pkt[9] = 0;
    pkt[10] = 0;
    pkt[11] = 0;
    pkt[12] = 0;
    pkt[13] = 0;
    pkt[14] = 0;
    pkt[15] = 0;
    pkt[16] = 0;
    
    /* checksum */
    for (i = 0; i < 31; i++) {
        checksum += pkt[i];
    }
    
    pkt[3] = (~checksum + 1) & 0xff;
    
    /* set to RAM Packet */
    hdmi_packet_generate(priv, PACKET_AVI_SLOT, pkt);
    hdmi_packet_send(priv, PACKET_AVI_SLOT, PACKET_PERIOD);
    return 0;
}

static int hdmi_gen_vs_infoframe(struct caninos_gfx *priv)
{
    static u8 pkt[32];
    int i;
    
    /* clear buffer */
    for (i = 0; i < 32; i++) {
        pkt[i] = 0;
    }
    
    /* header */
    pkt[0] = 0x81; /* header */
    pkt[1] = 0x1;  /* Version */
    pkt[2] = 0x6;  /* length */
    pkt[3] = 0x00; /* checksum */
    
    /* data */
    
    /* PB1--PB3:24bit IEEE Registration Identifier */
    pkt[4] = 0x03;
    pkt[5] = 0x0c;
    pkt[6] = 0x00;
    
    hdmi_packet_send(priv, PACKET_VS_SLOT, 0);
    return 0;
}

static int hdmi_packet_gen_infoframe(struct caninos_gfx *priv, int mode_id)
{
    hdmi_gen_spd_infoframe(priv);
    
    if (hdmi_gen_avi_infoframe(priv, mode_id)) {
        return -EINVAL;
    }
    
    hdmi_gen_vs_infoframe(priv);
    return 0;
}

int hdmi_display_enable(struct caninos_gfx *priv, int mode_id, int preline)
{
    struct hdmi_reg_values reg_values;
    int ret = 0;
    
    ret = hdmi_update_reg_values(&reg_values, mode_id);
    
    if (ret < 0) {
        return ret;
    }
    
    hdmi_tmds_ldo_enable(priv, &reg_values);
    udelay(500);
    
    hdmi_phy_enable(priv, &reg_values);
    
    hdmi_pll_enable(priv, &reg_values);
    mdelay(10);
    
    hdmi_video_timing_config(priv, mode_id);
    hdmi_video_format_config(priv, mode_id);
    hdmi_video_interface_config(priv, mode_id);
    hdmi_video_interval_packet_config(priv, mode_id);
    hdmi_core_input_src_config(priv);
    hdmi_core_pixel_fomat_config(priv);
    hdmi_core_preline_config(priv, preline);
    hdmi_core_deepcolor_mode_config(priv);
    hdmi_core_mode_config(priv);
    hdmi_core_invert_config(priv);
    hdmi_core_colordepth_config(priv);
    hdmi_core_3d_mode_config(priv);
    
    hdmi_packet_gen_infoframe(priv, mode_id);
    
    hdmi_video_start(priv, &reg_values);
    
    return 0;
}

void hdmi_display_disable(struct caninos_gfx *priv)
{
    hdmi_video_stop(priv);
    hdmi_pll_disable(priv);
    hdmi_phy_disable(priv);
    hdmi_tmds_ldo_disable(priv);
}

void hdmi_power_on(struct caninos_gfx *priv)
{
    clk_prepare_enable(priv->hdmi_dev_clk);
    reset_control_assert(priv->hdmi_rst);
    
    mdelay(1);
    
    reset_control_deassert(priv->hdmi_rst);
}

void hdmi_power_off(struct caninos_gfx *priv)
{
    reset_control_assert(priv->hdmi_rst);
    clk_disable_unprepare(priv->hdmi_dev_clk);
}

