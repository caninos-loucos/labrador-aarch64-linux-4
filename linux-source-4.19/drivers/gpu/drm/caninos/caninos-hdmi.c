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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/dma-mapping.h>

#include "hdmi.h"
#include "ip-sx00.h"

#define PACKET_PERIOD 1

static int hdmi_gen_spd_infoframe(struct hdmi_ip *ip)
{
	static uint8_t pkt[32];
	static char spdname[8] = "Vienna";
	static char spddesc[16] = "DTV SetTop Box";
	
	unsigned int checksum = 0;
	unsigned int i;
	
	/* clear buffer */
	for (i = 0; i < 32; i++)
		pkt[i] = 0;
		
	/* header */
	pkt[0] = 0x80 | 0x03;	/* HB0: Packet Type = 0x83 */
	pkt[1] = 1;		/* HB1: version = 1 */
	pkt[2] = 0x1f & 25;	/* HB2: len = 25 */
	pkt[3] = 0x00;		/* PB0: checksum = 0 */

	/*
	 * data
	 */

	/* Vendor Name, 8 bytes */
	memcpy(&pkt[4], spdname, 8);

	/* Product Description, 16 bytes */
	memcpy(&pkt[12], spddesc, 16);

	/* Source Device Information, Digital STB */
	pkt[28] = 0x1;

	/* checksum */
	for (i = 0; i < 31; i++)
		checksum += pkt[i];
	pkt[3] = (~checksum + 1)  & 0xff;

	/* set to RAM Packet */
	ip->ops.packet_generate(ip, PACKET_SPD_SLOT, pkt);
	ip->ops.packet_send(ip, PACKET_SPD_SLOT, PACKET_PERIOD);

	return 0;
}

static int hdmi_gen_avi_infoframe(struct hdmi_ip *ip)
{
	static uint8_t pkt[32];
	uint8_t AR = 2;
	uint32_t checksum = 0, i = 0;

	/* clear buffer */
	for (i = 0; i < 32; i++)
		pkt[i] = 0;

	/* header */
	pkt[0] = 0x80 | 0x02;	/* header = 0x82 */
	pkt[1] = 2;		/* version = 2 */
	pkt[2] = 0x1f & 13;	/* len = 13 */
	pkt[3] = 0x00;		/* checksum = 0 */

	/*
	 * data
	 */

	/*
	 * PB1--Y1:Y0 = colorformat; R3:R1 is invalid;
	 * no bar info and scan info
	 */
	pkt[4] = (ip->settings.pixel_encoding << 5) | (0 << 4) | (0 << 2) | (0);

	/* 0--Normal YCC601 or YCC709; 1--xvYCC601; 2--xvYCC709 */
	if (ip->settings.color_xvycc == 0) {
		/*
		 * PB2--Colorimetry: SMPTE 170M | ITU601;
		 * Picture aspect Ratio; same as picture aspect ratio
		 */
		pkt[5] = (0x1<<6) | (AR << 4) | (0x8);

		/* PB3--No known non-uniform scaling */
		pkt[6] = 0x0;
	} else if (ip->settings.color_xvycc == 1) {
		/*
		 * PB2--Colorimetry: SMPTE 170M | ITU601;
		 * Picture aspect Ratio; same as picture aspect ratio
		 */
		pkt[5] = (0x3 << 6) | (AR << 4) | (0x8);

		/* PB3--xvYCC601;No known non-uniform scaling */
		pkt[6] = 0x0;

	} else {
		/*
		 * PB2--Colorimetry: SMPTE 170M | ITU601;
		 * Picture aspect Ratio; same as picture aspect ratio
		 */
		pkt[5] = (0x3 << 6) | (AR << 4) | (0x8);

		/* PB3--xvYCC709;No known non-uniform scaling */
		pkt[6] = 0x1 << 4;
	}

	/* PB4--Video Id */
	
		pkt[7] = ip->vid;

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
	for (i = 0; i < 31; i++)
		checksum += pkt[i];
	pkt[3] = (~checksum + 1) & 0xff;

	/* set to RAM Packet */
	ip->ops.packet_generate(ip, PACKET_AVI_SLOT, pkt);
	ip->ops.packet_send(ip, PACKET_AVI_SLOT, PACKET_PERIOD);

	return 0;
}

static int hdmi_gen_vs_infoframe(struct hdmi_ip *ip)
{
	static uint8_t pkt[32];
	uint32_t checksum = 0;
	int i;

	/* clear buffer */
	for (i = 0; i < 32; i++)
		pkt[i] = 0;

	/* header */
	pkt[0] = 0x81;	/* header */
	pkt[1] = 0x1;	/* Version */
	pkt[2] = 0x6;	/* length */
	pkt[3] = 0x00;	/* checksum */

	/*
	 * data
	 */

	/* PB1--PB3:24bit IEEE Registration Identifier */
	pkt[4] = 0x03;
	pkt[5] = 0x0c;
	pkt[6] = 0x00;


	if (ip->settings.mode_3d != 0)
	{
		pkt[7] = 0x2 << 5;	/* 3D format */

		switch (ip->settings.mode_3d) {
		case HDMI_3D_FRAME:
			pkt[8] = 0x0 << 4;
			pkt[9] = 0x0;
			break;

		case HDMI_3D_LR_HALF:
			pkt[8] = 0x8 << 4;
			pkt[9] = 0x1 << 4;
			break;

		case HDMI_3D_TB_HALF:
			pkt[8] = 0x6 << 4;
			pkt[9] = 0x0;
			break;

		default:
			break;
		}
	} else {
		/* not (3D and 4kx2k@24/25/30/24SMPTE) format, stop vs packet */
		ip->ops.packet_send(ip, PACKET_VS_SLOT, 0);
		return 0;
	}

	for (i = 0; i < 31; i++)
		checksum += pkt[i];
	pkt[3] = (~checksum + 1) & 0xff;

	/* set to RAM Packet */
	ip->ops.packet_generate(ip, PACKET_VS_SLOT, pkt);
	ip->ops.packet_send(ip, PACKET_VS_SLOT, PACKET_PERIOD);

	return 0;
}

static int hdmi_packet_gen_infoframe(struct hdmi_ip *ip)
{
	hdmi_gen_spd_infoframe(ip);

	if (hdmi_gen_avi_infoframe(ip))
		return -1;

	/* hdmi_gen_gbd_infoframe(ip); */
	hdmi_gen_vs_infoframe(ip);

	return 0;
}

static inline void hdmi_ip_writel(struct hdmi_ip *ip, const uint16_t idx, uint32_t val) {
	writel(val, ip->base + idx);
}

static inline uint32_t hdmi_ip_readl(struct hdmi_ip *ip, const uint16_t idx) {
	return readl(ip->base + idx);
}

#define REG_MASK(start, end) (((1 << ((start) - (end) + 1)) - 1) << (end))
#define REG_VAL(val, start, end) (((val) << (end)) & REG_MASK(start, end))
#define REG_GET_VAL(val, start, end) (((val) & REG_MASK(start, end)) >> (end))
#define REG_SET_VAL(orig, val, start, end) (((orig) & ~REG_MASK(start, end))\
						 | REG_VAL(val, start, end))

static int ip_packet_send(struct hdmi_ip *ip, uint32_t no, int period)
{
	uint32_t val;
	
	if (no > PACKET_MAX || no < 0) {
		return -1;
	}
	
	if (period > 0xf || period < 0) {
		return -1;
	}
	
	val = hdmi_ip_readl(ip, HDMI_RPCR);
	val &= (~(1 << no));
	hdmi_ip_writel(ip, HDMI_RPCR,  val);
	
	val = hdmi_ip_readl(ip, HDMI_RPCR);
	val &= (~(0xf << (no * 4 + 8)));
	hdmi_ip_writel(ip, HDMI_RPCR, val);
	
	/* enable and set period */
	if (period)
	{
		val = hdmi_ip_readl(ip, HDMI_RPCR);
		val |= (period << (no * 4 + 8));
		hdmi_ip_writel(ip, HDMI_RPCR,  val);
		
		val = hdmi_ip_readl(ip, HDMI_RPCR);
		val |= (0x1 << no);
		hdmi_ip_writel(ip, HDMI_RPCR,  val);
	}
	
	return 0;
}

static int ip_packet_generate(struct hdmi_ip *ip, uint32_t no, uint8_t *pkt)
{
	uint32_t addr = 126 + no * 14;
	uint32_t reg[9], val;
	uint8_t tpkt[36];
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
		
		for (j = 0; j < 4; j++)
		{
			reg[i] |= (tpkt[i * 4 + j]) << (j * 8);
		}
	}

	hdmi_ip_writel(ip, HDMI_OPCR, (1 << 8) | (addr & 0xff));
	hdmi_ip_writel(ip, HDMI_ORP6PH,  reg[0]);
	hdmi_ip_writel(ip, HDMI_ORSP6W0, reg[1]);
	hdmi_ip_writel(ip, HDMI_ORSP6W1, reg[2]);
	hdmi_ip_writel(ip, HDMI_ORSP6W2, reg[3]);
	hdmi_ip_writel(ip, HDMI_ORSP6W3, reg[4]);
	hdmi_ip_writel(ip, HDMI_ORSP6W4, reg[5]);
	hdmi_ip_writel(ip, HDMI_ORSP6W5, reg[6]);
	hdmi_ip_writel(ip, HDMI_ORSP6W6, reg[7]);
	hdmi_ip_writel(ip, HDMI_ORSP6W7, reg[8]);
	
	val = hdmi_ip_readl(ip, HDMI_OPCR);
	val |= (0x1 << 31);
	hdmi_ip_writel(ip, HDMI_OPCR, val);
	
	i = 100;
	
	while (i--)
	{
		val = hdmi_ip_readl(ip, HDMI_OPCR);
		val = val >> 31;
		
		if (val == 0) {
			break;
		}
		
		udelay(1);
	}
	
	return 0;
}

static bool ip_is_video_enabled(struct hdmi_ip *ip)
{
	return (hdmi_ip_readl(ip, HDMI_CR) & 0x01) != 0;
}

static void ip_video_disable(struct hdmi_ip *ip)
{
	uint32_t val;
	
	val = hdmi_ip_readl(ip, HDMI_TX_2);
	val = REG_SET_VAL(val, 0x0, 11, 8);
	val = REG_SET_VAL(val, 0x0, 17, 17);
	hdmi_ip_writel(ip, HDMI_TX_2, val);
	
	val = hdmi_ip_readl(ip, HDMI_CR);
	val = REG_SET_VAL(val, 0, 0, 0);
	hdmi_ip_writel(ip, HDMI_CR, val);
	
	val = readl(ip->cmu_base + ip->hwdiff->pll_reg);
	val &= ~(0x1 << ip->hwdiff->pll_24m_en);
	val &= ~(0x1 << ip->hwdiff->pll_en);
	writel(val, ip->cmu_base + ip->hwdiff->pll_reg);
	
	/* reset TVOUTPLL */
	writel(0, ip->cmu_base + ip->hwdiff->pll_reg);
	
	/* reset TVOUTPLL_DEBUG0 & TVOUTPLL_DEBUG1 */
	writel(0x0, ip->cmu_base + ip->hwdiff->pll_debug0_reg);
	writel(0x2614a, ip->cmu_base + ip->hwdiff->pll_debug1_reg);
	
	/* TMDS Encoder */
	val = hdmi_ip_readl(ip, TMDS_EODR0);
	val = REG_SET_VAL(val, 0, 31, 31);
	hdmi_ip_writel(ip, TMDS_EODR0, val);
	
	/* txpll_pu */
	val = hdmi_ip_readl(ip, HDMI_TX_1);
	val = REG_SET_VAL(val, 0, 23, 23);
	hdmi_ip_writel(ip, HDMI_TX_1, val);
	
	/* internal TMDS LDO */
	val = hdmi_ip_readl(ip, HDMI_TX_2);
	val = REG_SET_VAL(val, 0, 27, 27); /* LDO_TMDS power off */
	hdmi_ip_writel(ip, HDMI_TX_2, val);
}

static int ip_update_reg_values(struct hdmi_ip *ip)
{
	ip->pll_val = 0;
	
	/* bit31 = 0, debug mode disable, default value if it is not set */
	ip->pll_debug0_val = 0;
	ip->pll_debug1_val = 0;
	
	ip->tx_1 = 0;
	ip->tx_2 = 0;
	
	switch (ip->vid)
	{
	case VID640x480P_60_4VS3:
		ip->pll_val = 0x00000008;	/* 25.2MHz */
		ip->tx_1 = 0x819c2984;
		ip->tx_2 = 0x18f80f39;
		break;
	
	case VID720x576P_50_4VS3:
	case VID720x480P_60_4VS3:
		ip->pll_val = 0x00010008;	/* 27MHz */
		ip->tx_1 = 0x819c2984;
		ip->tx_2 = 0x18f80f39;
		break;

	case VID1280x720P_60_16VS9:
	case VID1280x720P_50_16VS9:
		ip->pll_val = 0x00040008;	/* 74.25MHz */
		ip->tx_1 = 0x81982984;
		ip->tx_2 = 0x18f80f39;
		break;

	case VID1920x1080P_60_16VS9:
	case VID1920x1080P_50_16VS9:
		ip->pll_val = 0x00060008;	/* 148.5MHz */
		ip->tx_1 = 0x81942988;
		ip->tx_2 = 0x18fe0f39;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static void __ip_tmds_ldo_enable(struct hdmi_ip *ip)
{
	uint32_t val;
	
	/* do not enable HDMI lane util video enable */
	val = ip->tx_2 & (~((0xf << 8) | (1 << 17)));
	hdmi_ip_writel(ip, HDMI_TX_2, val);
}

static void __ip_phy_enable(struct hdmi_ip *ip)
{
	uint32_t val;
	
	/* TMDS Encoder */
	val = hdmi_ip_readl(ip, TMDS_EODR0);
	val = REG_SET_VAL(val, 1, 31, 31);
	hdmi_ip_writel(ip, TMDS_EODR0, val);
	
	hdmi_ip_writel(ip, HDMI_TX_1, ip->tx_1);
}

static void __ip_pll_enable(struct hdmi_ip *ip)
{
	uint32_t val;
	
	/* 24M enable */
	val = readl(ip->cmu_base + ip->hwdiff->pll_reg);
	val |= (1 << ip->hwdiff->pll_24m_en);
	writel(val, ip->cmu_base + ip->hwdiff->pll_reg);
	mdelay(1);
	
	/* set PLL, only bit18:16 of pll_val is used */
	val = readl(ip->cmu_base + ip->hwdiff->pll_reg);
	val &= ~(0x7 << 16);
	val |= (ip->pll_val & (0x7 << 16));
	writel(val, ip->cmu_base + ip->hwdiff->pll_reg);
	mdelay(1);
	
	/* set debug PLL */
	writel(ip->pll_debug0_val, ip->cmu_base + ip->hwdiff->pll_debug0_reg);
	writel(ip->pll_debug1_val, ip->cmu_base + ip->hwdiff->pll_debug1_reg);
	
	/* enable PLL */
	val = readl(ip->cmu_base + ip->hwdiff->pll_reg);
	val |= (1 << ip->hwdiff->pll_en);
	writel(val, ip->cmu_base + ip->hwdiff->pll_reg);
	mdelay(1);
	
	val = hdmi_ip_readl(ip, CEC_DDC_HPD);
	
	/* 0 to 1, start calibration */
	val = REG_SET_VAL(val, 0, 20, 20);
	hdmi_ip_writel(ip, CEC_DDC_HPD, val);
	
	udelay(10);
	
	val = REG_SET_VAL(val, 1, 20, 20);
	hdmi_ip_writel(ip, CEC_DDC_HPD, val);
	
	while (1) {
		val = hdmi_ip_readl(ip, CEC_DDC_HPD);
		if ((val >> 24) & 0x1)
			break;
	}
}

static void __ip_video_timing_config(struct hdmi_ip *ip)
{
	bool vsync_pol, hsync_pol, interlace, repeat;
	uint32_t val;
	const struct videomode *mode = &ip->mode;
	
	vsync_pol = ((mode->sync & DSS_SYNC_VERT_HIGH_ACT) == 0);
	hsync_pol = ((mode->sync & DSS_SYNC_HOR_HIGH_ACT) == 0);
	
	interlace = ip->interlace;
	repeat = ip->repeat;
	
	val = hdmi_ip_readl(ip, HDMI_SCHCR);
	val = REG_SET_VAL(val, hsync_pol, 1, 1);
	val = REG_SET_VAL(val, vsync_pol, 2, 2);
	hdmi_ip_writel(ip, HDMI_SCHCR, val);

	val = hdmi_ip_readl(ip, HDMI_VICTL);
	val = REG_SET_VAL(val, interlace, 28, 28);
	val = REG_SET_VAL(val, repeat, 29, 29);
	hdmi_ip_writel(ip, HDMI_VICTL, val);
}

static void __ip_video_format_config(struct hdmi_ip *ip)
{
	uint32_t val, val_hp, val_vp;
	bool interlace;
	const struct videomode *mode = &ip->mode;

	val_hp = mode->xres + mode->hbp + mode->hfp + mode->hsw;
	val_vp = mode->yres + mode->vbp + mode->vfp + mode->vsw;
	interlace = ip->interlace;
	
	val = hdmi_ip_readl(ip, HDMI_VICTL);

	val = REG_SET_VAL(val, val_hp - 1, ip->hwdiff->hp_end, ip->hwdiff->hp_start);

	if (interlace == false)
		val = REG_SET_VAL(val, val_vp - 1, ip->hwdiff->vp_end, ip->hwdiff->vp_start);
	else
		val = REG_SET_VAL(val, val_vp * 2, ip->hwdiff->vp_end, ip->hwdiff->vp_start);
	
	hdmi_ip_writel(ip, HDMI_VICTL, val);
}

static int ip_video_enable(struct hdmi_ip *ip)
{
	uint32_t val, mode;
	int preline, ret;
	
	ret = ip_update_reg_values(ip);
	
	if (ret < 0) {
		return ret;
	}
	
	__ip_tmds_ldo_enable(ip);
	
	udelay(500);
	
	__ip_phy_enable(ip);
	__ip_pll_enable(ip);
	
	mdelay(10);
	
	__ip_video_timing_config(ip);
	__ip_video_format_config(ip);
	
	if (ip->interlace == 0)
	{
		val = 0;
		hdmi_ip_writel(ip, HDMI_VIVSYNC, val);
		
		val = hdmi_ip_readl(ip, HDMI_VIVHSYNC);
		
		if (ip->vstart != 1)
		{
			val = REG_SET_VAL(val, ip->mode.hsw - 1, 8, 0);
			val = REG_SET_VAL(val, ip->vstart - 2, 23, 12);
			val = REG_SET_VAL(val, ip->vstart + ip->mode.vsw - 2, 27, 24);
		}
		else
		{
			val = REG_SET_VAL(val, ip->mode.hsw - 1, 8, 0);
			val = REG_SET_VAL(val, ip->mode.yres + ip->mode.vbp + ip->mode.vfp + ip->mode.vsw - 1, 23, 12);
			val = REG_SET_VAL(val, ip->mode.vsw - 1, 27, 24);
		}
		
		hdmi_ip_writel(ip, HDMI_VIVHSYNC, val);
		
		/*
		 * VIALSEOF = (yres + vbp + vsp - 1) | ((vbp + vfp - 1) << 12)
		 */
		val = hdmi_ip_readl(ip, HDMI_VIALSEOF);
		
		val = REG_SET_VAL(val, ip->vstart - 1 + ip->mode.vsw + ip->mode.vbp + ip->mode.yres - 1, 23, 12);
		val = REG_SET_VAL(val, ip->vstart - 1 + ip->mode.vsw + ip->mode.vbp - 1, 10, 0);
		
		hdmi_ip_writel(ip, HDMI_VIALSEOF, val);
		
		val = 0;
		hdmi_ip_writel(ip, HDMI_VIALSEEF, val);
		
		/*
		 * VIADLSE = (xres + hbp + hsp - 1) | ((hbp + hsw - 1) << 12)
		 */
		val = hdmi_ip_readl(ip, HDMI_VIADLSE);
		val = REG_SET_VAL(val, ip->mode.hbp +  ip->mode.hsw - 1, 11, 0);
		val = REG_SET_VAL(val, ip->mode.xres + ip->mode.hbp + ip->mode.hsw - 1, 28, 16);
		
		hdmi_ip_writel(ip, HDMI_VIADLSE, val);
	}
	else
	{
		val = 0;
		hdmi_ip_writel(ip, HDMI_VIVSYNC, val);
		
		/*
		 * VIVHSYNC =
		 * (hsw -1 ) | ((yres + vsw + vfp + vbp - 1 ) << 12)
		 *  | (vfp -1 << 24)
		 */
		val = hdmi_ip_readl(ip, HDMI_VIVHSYNC);
		val = REG_SET_VAL(val, ip->mode.hsw - 1, 8, 0);
		val = REG_SET_VAL(val, (ip->mode.yres + ip->mode.vbp + ip->mode.vfp + ip->mode.vsw) * 2, 22, 12);
		val = REG_SET_VAL(val, ip->mode.vfp * 2, 22, 12);
		hdmi_ip_writel(ip, HDMI_VIVHSYNC, val);

		/*
		 * VIALSEOF = (yres + vbp + vfp - 1) | ((vbp + vfp - 1) << 12)
		 */
		val = hdmi_ip_readl(ip, HDMI_VIALSEOF);
		val = REG_SET_VAL(val, ip->mode.vbp + ip->mode.vfp  - 1, 22, 12);
		val = REG_SET_VAL(val, (ip->mode.yres + ip->mode.vbp + ip->mode.vfp) * 2, 10, 0);
		hdmi_ip_writel(ip, HDMI_VIALSEOF, val);

		val = 0;
		hdmi_ip_writel(ip, HDMI_VIALSEEF, val);

		/*
		 * VIADLSE = (xres + hbp + hsp - 1) | ((hbp + hsw - 1) << 12)
		 */
		val = hdmi_ip_readl(ip, HDMI_VIADLSE);
		val = REG_SET_VAL(val, ip->mode.hbp + ip->mode.hsw - 1, 27, 16);
		val = REG_SET_VAL(val, ip->mode.xres + ip->mode.hbp + ip->mode.hsw - 1, 11, 0);
		
		hdmi_ip_writel(ip, HDMI_VIADLSE, val);
	}
	
	switch (ip->vid)
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

	hdmi_ip_writel(ip, HDMI_DIPCCR, val);
	
	val = hdmi_ip_readl(ip, HDMI_ICR);
	
	if (ip->settings.hdmi_src == VITD)
	{
		val = REG_SET_VAL(val, 0x01, 24, 24);
		val = REG_SET_VAL(val, ip->settings.vitd_color, 23, 0);
	}
	else {
		val = REG_SET_VAL(val, 0x00, 24, 24);
	}
	
	hdmi_ip_writel(ip, HDMI_ICR, val);
	
	val = hdmi_ip_readl(ip, HDMI_SCHCR);
	val = REG_SET_VAL(val, ip->settings.pixel_encoding, 5, 4);
	hdmi_ip_writel(ip, HDMI_SCHCR, val);
	
	preline = ip->settings.prelines;
	preline = (preline <= 0 ? 1 : preline);
	preline = (preline > 16 ? 16 : preline);

	val = hdmi_ip_readl(ip, HDMI_SCHCR);
	val = REG_SET_VAL(val, preline - 1, 23, 20);
	hdmi_ip_writel(ip, HDMI_SCHCR, val);
	
	val = hdmi_ip_readl(ip, HDMI_SCHCR);
	val = REG_SET_VAL(val, ip->settings.deep_color, 17, 16);
	hdmi_ip_writel(ip, HDMI_SCHCR, val);
	
	val = hdmi_ip_readl(ip, HDMI_SCHCR);
	
	val = REG_SET_VAL(val, ip->settings.hdmi_mode,
		ip->hwdiff->mode_end, ip->hwdiff->mode_start);
	
	hdmi_ip_writel(ip, HDMI_SCHCR, val);
	
	
	val = hdmi_ip_readl(ip, HDMI_SCHCR);
	val = REG_SET_VAL(val, ip->settings.bit_invert, 28, 28);
	val = REG_SET_VAL(val, ip->settings.channel_invert, 29, 29);
	
	hdmi_ip_writel(ip, HDMI_SCHCR, val);
	
	mode = ip->settings.deep_color;

	val = hdmi_ip_readl(ip, HDMI_GCPCR);

	val = REG_SET_VAL(val, mode, 7, 4);
	val = REG_SET_VAL(val, 1, 31, 31);

	if (mode > HDMI_PACKETMODE24BITPERPIXEL) {
		val = REG_SET_VAL(val, 1, 30, 30);
	}
	else {
		val = REG_SET_VAL(val, 0, 30, 30);
	}

	/* clear specify avmute flag in gcp packet */
	val = REG_SET_VAL(val, 1, 1, 1);

	hdmi_ip_writel(ip, HDMI_GCPCR, val);
	
	val = hdmi_ip_readl(ip, HDMI_SCHCR);

	if (ip->settings.mode_3d == HDMI_3D_FRAME) {
		val = REG_SET_VAL(val, 1, 8, 8);
	}
	else {
		val = REG_SET_VAL(val, 0, 8, 8);
	}
	
	hdmi_ip_writel(ip, HDMI_SCHCR, val);
	
	hdmi_packet_gen_infoframe(ip);
	
	val = hdmi_ip_readl(ip, HDMI_CR);
	val = REG_SET_VAL(val, 1, 0, 0);
	hdmi_ip_writel(ip, HDMI_CR, val);
	
	val = hdmi_ip_readl(ip, HDMI_TX_2);
	val = REG_SET_VAL(val, (ip->tx_2 >> 8) & 0xf, 11, 8);
	val = REG_SET_VAL(val, (ip->tx_2 >> 17) & 0x1, 17, 17);
	hdmi_ip_writel(ip, HDMI_TX_2, val);
	
	return 0;
}

static bool ip_cable_status(struct hdmi_ip *ip)
{
	bool status = (hdmi_ip_readl(ip, CEC_DDC_HPD) & (0x3 << 14));
	status = status || (hdmi_ip_readl(ip, CEC_DDC_HPD) & (0x3 << 12));
	status = status || (hdmi_ip_readl(ip, CEC_DDC_HPD) & (0x3 << 10));
	status = status || (hdmi_ip_readl(ip, CEC_DDC_HPD) & (0x3 << 8));
	status = status && (hdmi_ip_readl(ip, HDMI_CR) & (1 << 29));
	return status;
}

static bool ip_hpd_is_pending(struct hdmi_ip *ip)
{
	return (hdmi_ip_readl(ip, HDMI_CR) & (1 << 30)) != 0;
}

static void ip_hpd_clear_pending(struct hdmi_ip *ip)
{
	u32 val = hdmi_ip_readl(ip, HDMI_CR);
	val = REG_SET_VAL(val, 0x01, 30, 30); /* clear pending bit */
	hdmi_ip_writel(ip, HDMI_CR, val);
}

static void ip_hpd_enable(struct hdmi_ip *ip)
{
	u32 val = hdmi_ip_readl(ip, HDMI_CR);
	val = REG_SET_VAL(val, 0x0f, 27, 24); /* hotplug debounce */
	val = REG_SET_VAL(val, 0x01, 31, 31); /* enable hotplug interrupt */
	val = REG_SET_VAL(val, 0x01, 28, 28); /* enable hotplug function */
	val = REG_SET_VAL(val, 0x00, 30, 30); /* not clear pending bit */
	hdmi_ip_writel(ip, HDMI_CR, val);
}

static void ip_hpd_disable(struct hdmi_ip *ip)
{
	u32 val = hdmi_ip_readl(ip, HDMI_CR);
	val = REG_SET_VAL(val, 0x00, 31, 31); /* disable hotplug interrupt */
	val = REG_SET_VAL(val, 0x00, 28, 28); /* enable hotplug function */
	val = REG_SET_VAL(val, 0x01, 30, 30); /* clear pending bit */
	hdmi_ip_writel(ip, HDMI_CR, val);
}

static void ip_power_off(struct hdmi_ip *ip)
{
	reset_control_assert(ip->hdmi_rst);
	clk_disable_unprepare(ip->hdmi_dev_clk);
}

static void ip_exit(struct hdmi_ip *ip)
{
	ip_power_off(ip);
}

static int ip_power_on(struct hdmi_ip *ip)
{
	int ret = 0;
	
	if (!ip_is_video_enabled(ip)) {
		reset_control_assert(ip->hdmi_rst);
	}
	
	clk_prepare_enable(ip->hdmi_dev_clk);
	mdelay(1);
	
	if (!ip_is_video_enabled(ip))
	{
		reset_control_deassert(ip->hdmi_rst);
		mdelay(1);
	}
	
	return ret;
}

static int ip_init(struct hdmi_ip *ip)
{
	ip->settings.hdmi_src = DE;
	ip->settings.vitd_color = 0xff0000;
	ip->settings.pixel_encoding = RGB444;
	ip->settings.color_xvycc = 0;
	ip->settings.deep_color = color_mode_24bit;
	ip->settings.hdmi_mode = HDMI_HDMI;
	ip->settings.mode_3d = HDMI_2D;
	ip->settings.prelines = 0;
	ip->settings.channel_invert = 0;
	ip->settings.bit_invert = 0;
	return ip_power_on(ip);
}

static int ip_audio_enable(struct hdmi_ip *ip)
{
	u32 val = hdmi_ip_readl(ip, HDMI_ICR);
	val |= (1 << 25);
	hdmi_ip_writel(ip, HDMI_ICR, val);
	return 0;
}

static int ip_audio_disable(struct hdmi_ip *ip)
{
	u32 val = hdmi_ip_readl(ip, HDMI_ICR);
	val &= ~(1 << 25);
	hdmi_ip_writel(ip, HDMI_ICR, val);
	return 0;
}

static const struct hdmi_ip_hwdiff ip_hwdiff_k7 = {
	.hp_start = 16,
	.hp_end = 28,
	.vp_start = 4,
	.vp_end = 15,
	.mode_start = 0,
	.mode_end = 0,
	.pll_reg = 0x18,
	.pll_24m_en = 23,
	.pll_en = 3,
	.pll_debug0_reg = 0xF0,
	.pll_debug1_reg = 0xF4,
};

static int caninos_hdmi_ip_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct hdmi_ip *ip;
	int ret;
	
	if (!np) {
		dev_err(dev, "missing device OF node\n");
		return -EINVAL;
	}
	
	match = of_match_device(dev->driver->of_match_table, dev);
	
	if (!match || !match->data)
	{
		dev_err(dev, "could not get hardware specific data\n");
		return -EINVAL;
	}
	
	ip = devm_kzalloc(dev, sizeof(*ip), GFP_KERNEL);
	
	if (!ip) {
		dev_err(dev, "could not alloc ip data structure\n");
		return -ENOMEM;
	}
	
	ip->hwdiff = devm_kzalloc(dev, sizeof(struct hdmi_ip_hwdiff), GFP_KERNEL);
	
	if (!ip->hwdiff) {
		dev_err(dev, "could not alloc hwdiff data structure\n");
		return -ENOMEM;
	}
	
	memcpy(ip->hwdiff, match->data, sizeof(struct hdmi_ip_hwdiff));
	
	ip->dev = dev;
	ip->ops.init = ip_init;
	ip->ops.exit = ip_exit;
	ip->ops.hpd_enable = ip_hpd_enable;
	ip->ops.hpd_disable = ip_hpd_disable;
	ip->ops.hpd_is_pending = ip_hpd_is_pending;
	ip->ops.hpd_clear_pending = ip_hpd_clear_pending;
	ip->ops.cable_status = ip_cable_status;
	ip->ops.video_enable = ip_video_enable;
	ip->ops.video_disable = ip_video_disable;
	ip->ops.is_video_enabled = ip_is_video_enabled;
	ip->ops.packet_generate = ip_packet_generate;
	ip->ops.packet_send = ip_packet_send;
	ip->ops.audio_enable = ip_audio_enable;
	ip->ops.audio_disable = ip_audio_disable;
	
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cmu");
	
	if (res) {
		ip->cmu_base = devm_ioremap(dev, res->start, resource_size(res));
	}
	if (IS_ERR_OR_NULL(ip->cmu_base))
	{
		dev_err(dev, "could not map cmu registers\n");
		return (!ip->cmu_base) ? -ENODEV : PTR_ERR(ip->cmu_base);
	}
	
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hdmi");
	
	if (res) {
		ip->base = devm_ioremap(dev, res->start, resource_size(res));
	}
	if (IS_ERR_OR_NULL(ip->base))
	{
		dev_err(dev, "could not map hdmi registers\n");
		return (!ip->base) ? -ENODEV : PTR_ERR(ip->base);
	}
	
	ip->hdmi_dev_clk = devm_clk_get(dev, "hdmi");
	
	if (IS_ERR(ip->hdmi_dev_clk)) {
		dev_err(dev, "could not get hdmi clock\n");
		return -ENODEV;
	}
	
	ip->hdmi_rst = devm_reset_control_get(dev, "hdmi");
	
	if (IS_ERR(ip->hdmi_rst)) {
		dev_err(dev, "could not get hdmi reset control\n");
		return PTR_ERR(ip->hdmi_rst);
	}
	
	ret = ip->ops.init(ip);
	
	if (ret) {
		dev_err(dev, "hdmi ip init failed\n");
		return ret;
	}
	
	platform_set_drvdata(pdev, ip);
	
	dev_info(dev, "probe finished\n");
	return 0;
}

static int caninos_hdmi_ip_remove(struct platform_device *pdev)
{
	struct hdmi_ip *ip = platform_get_drvdata(pdev);
	if (ip && ip->ops.exit)
		ip->ops.exit(ip);
	return 0;
}

static const struct of_device_id caninos_hdmi_ip_match[] = {
	{ .compatible = "caninos,k7-hdmi", .data = (void*)&ip_hwdiff_k7 },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, caninos_hdmi_ip_match);

static struct platform_driver caninos_hdmi_ip_driver = {
	.probe = caninos_hdmi_ip_probe,
	.remove = caninos_hdmi_ip_remove,
	.driver = {
		.name = "caninos-hdmi",
		.of_match_table = caninos_hdmi_ip_match,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(caninos_hdmi_ip_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("Caninos HDMI Video Driver");
MODULE_LICENSE("GPL v2");
