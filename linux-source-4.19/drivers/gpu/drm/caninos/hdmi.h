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
 
#ifndef _HDMI_H_
#define _HDMI_H_

#include <linux/reset.h>
#include <linux/clk.h>
#include <sound/dmaengine_pcm.h>
#include <sound/soc.h>

#include "caninos-drm.h"

enum SRC_SEL {
	VITD = 0,
	DE,
	SRC_MAX
};

enum hdmi_core_hdmi_dvi {
	HDMI_DVI = 0,
	HDMI_HDMI = 1,
};

enum hdmi_core_packet_mode {
	HDMI_PACKETMODERESERVEDVALUE = 0,
	HDMI_PACKETMODE24BITPERPIXEL = 4,
	HDMI_PACKETMODE30BITPERPIXEL = 5,
	HDMI_PACKETMODE36BITPERPIXEL = 6,
	HDMI_PACKETMODE48BITPERPIXEL = 7
};

enum hdmi_packing_mode {
	HDMI_PACK_10b_RGB_YUV444 = 0,
	HDMI_PACK_24b_RGB_YUV444_YUV422 = 1,
	HDMI_PACK_20b_YUV422 = 2,
	HDMI_PACK_ALREADYPACKED = 7
};

enum hdmi_pixel_format {
	RGB444 = 0,
	YUV444 = 2
};

enum hdmi_deep_color {
	color_mode_24bit = 0,
	color_mode_30bit = 1,
	color_mode_36bit = 2,
	color_mode_48bit = 3
};

enum hdmi_3d_mode {
	HDMI_2D,
	HDMI_3D_LR_HALF,
	HDMI_3D_TB_HALF,
	HDMI_3D_FRAME,
};

/* horizontal and vertical sync active high */
#define DSS_SYNC_HOR_HIGH_ACT  (1 << 0)
#define DSS_SYNC_VERT_HIGH_ACT (1 << 1)

struct videomode
{
	int xres; /* visible resolution */
	int yres;
	int refresh; /* vertical refresh rate in hz */
	/*
	 * Timing: All values in pixclocks, except pixclock
	 */
	int pixclock; /* pixel clock in ps (pico seconds) */
	int hfp; /* horizontal front porch */
	int hbp; /* horizontal back porch */
	int vfp; /* vertical front porch */
	int vbp; /* vertical back porch */
	int hsw; /* horizontal synchronization pulse width */
	int vsw; /* vertical synchronization pulse width */
	int sync; /* see DSS_SYNC_* */
};

enum hdmi_packet_type {
	PACKET_AVI_SLOT   = 0,
	PACKET_AUDIO_SLOT = 1,
	PACKET_SPD_SLOT   = 2,
	PACKET_GBD_SLOT   = 3,
	PACKET_VS_SLOT    = 4,
	PACKET_HFVS_SLOT  = 5,
	PACKET_MAX,
};

struct caninos_hdmi_ops
{
	int  (*init)(struct caninos_hdmi *ip);
	void (*exit)(struct caninos_hdmi *ip);
	
	void (*hpd_enable)(struct caninos_hdmi *ip);
	void (*hpd_disable)(struct caninos_hdmi *ip);
	bool (*hpd_is_pending)(struct caninos_hdmi *ip);
	void (*hpd_clear_pending)(struct caninos_hdmi *ip);
	
	bool (*cable_status)(struct caninos_hdmi *ip);
	
	int  (*video_enable)(struct caninos_hdmi *ip);
	void (*video_disable)(struct caninos_hdmi *ip);
	bool (*is_video_enabled)(struct caninos_hdmi *ip);
	
	void (*audio_enable)(struct caninos_hdmi *ip);
	void (*audio_disable)(struct caninos_hdmi *ip);
	void (*set_audio_interface)(struct caninos_hdmi *ip);
	
	int  (*packet_generate)(struct caninos_hdmi *ip, uint32_t no, uint8_t *pkt);
	int  (*packet_send)(struct caninos_hdmi *ip, uint32_t no, int period);
};

struct caninos_hdmi_settings
{
	int hdmi_src;
	int vitd_color;
	int pixel_encoding;
	int color_xvycc;
	int deep_color;
	int hdmi_mode;
	int mode_3d;
	int prelines;
	int channel_invert;
	int bit_invert;
};

enum caninos_hdmi_model {
	HDMI_MODEL_K5 = 1,
	HDMI_MODEL_K7,
};

struct caninos_hdmi_hwdiff
{
	enum caninos_hdmi_model model;
	
	int hp_start;
	int hp_end;
	int vp_start;
	int vp_end;
	int mode_start;
	int mode_end;
	
	uint32_t pll_reg;
	int pll_24m_en;
	int pll_en;
	
	uint32_t pll_debug0_reg;
	uint32_t pll_debug1_reg;
};

struct caninos_hdmi
{
	/* register address */
	void __iomem *base;
	void __iomem *cmu_base;
	
	struct device *dev;
	struct reset_control *hdmi_rst;
	struct clk *hdmi_dev_clk;
	
	struct caninos_hdmi_settings settings;
	int vid; /* video mode */
	struct videomode mode;
	
	bool interlace;
	int vstart; /* vsync start line */
	bool repeat;
	
	/* used for registers setting */
	uint32_t pll_val;
	uint32_t pll_debug0_val;
	uint32_t pll_debug1_val;
	uint32_t tx_1;
	uint32_t tx_2;
	uint32_t phyctrl_1;
	uint32_t phyctrl_2;
	
	/* ip functions */
	struct caninos_hdmi_ops ops;
	
	/* used for hardware specific configurations */
	struct caninos_hdmi_hwdiff *hwdiff;
};

static inline void caninos_hdmi_writel(struct caninos_hdmi *hdmi, const uint16_t idx, uint32_t val) {
	writel(val, hdmi->base + idx);
}

static inline uint32_t caninos_hdmi_readl(struct caninos_hdmi *hdmi, const uint16_t idx) {
	return readl(hdmi->base + idx);
}

#endif

