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
#include "hdmi-regs.h"

#define PACKET_PERIOD 1

#define REG_MASK(start, end) (((1 << ((start) - (end) + 1)) - 1) << (end))
#define REG_VAL(val, start, end) (((val) << (end)) & REG_MASK(start, end))
#define REG_GET_VAL(val, start, end) (((val) & REG_MASK(start, end)) >> (end))
#define REG_SET_VAL(orig, val, start, end) (((orig) & ~REG_MASK(start, end))\
						 | REG_VAL(val, start, end))

static int hdmi_gen_spd_infoframe(struct caninos_hdmi *hdmi)
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
	hdmi->ops.packet_generate(hdmi, PACKET_SPD_SLOT, pkt);
	hdmi->ops.packet_send(hdmi, PACKET_SPD_SLOT, PACKET_PERIOD);

	return 0;
}

static int hdmi_gen_avi_infoframe(struct caninos_hdmi *hdmi)
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
	pkt[4] = (hdmi->settings.pixel_encoding << 5) | (0 << 4) | (0 << 2) | (0);

	/* 0--Normal YCC601 or YCC709; 1--xvYCC601; 2--xvYCC709 */
	if (hdmi->settings.color_xvycc == 0) {
		/*
		 * PB2--Colorimetry: SMPTE 170M | ITU601;
		 * Picture aspect Ratio; same as picture aspect ratio
		 */
		pkt[5] = (0x1<<6) | (AR << 4) | (0x8);

		/* PB3--No known non-uniform scaling */
		pkt[6] = 0x0;
	} else if (hdmi->settings.color_xvycc == 1) {
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
	
		pkt[7] = hdmi->vid;

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
	hdmi->ops.packet_generate(hdmi, PACKET_AVI_SLOT, pkt);
	hdmi->ops.packet_send(hdmi, PACKET_AVI_SLOT, PACKET_PERIOD);

	return 0;
}

static int hdmi_gen_vs_infoframe(struct caninos_hdmi *hdmi)
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


	if (hdmi->settings.mode_3d != 0)
	{
		pkt[7] = 0x2 << 5;	/* 3D format */

		switch (hdmi->settings.mode_3d) {
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
		hdmi->ops.packet_send(hdmi, PACKET_VS_SLOT, 0);
		return 0;
	}

	for (i = 0; i < 31; i++)
		checksum += pkt[i];
	pkt[3] = (~checksum + 1) & 0xff;

	/* set to RAM Packet */
	hdmi->ops.packet_generate(hdmi, PACKET_VS_SLOT, pkt);
	hdmi->ops.packet_send(hdmi, PACKET_VS_SLOT, PACKET_PERIOD);

	return 0;
}

int hdmi_gen_audio_infoframe(struct caninos_hdmi *hdmi)
{
	static uint8_t pkt[32];
	uint32_t checksum = 0;
	int i;

	// clear buffer 
	memset(pkt, 0, 32);
	
	// header
	pkt[0] = 0x80 | 0x04;	// HB0: Packet Type = 0x84 
	pkt[1] = 0x01;			// HB1: Version = 1 
	pkt[2] = 0x0A;			// HB2: Length = 10
	pkt[3] = 0x00;			// PB0: checksum = 0 

	// data
	pkt[4] = 0x01;			// PB1 : CodingType[3:0] R ChannelCount[2:0]
							//		 CT = 000 
							//		 CC = 001 = 2 CHANNELS

	pkt[5] = 0;				// PB2 : R[3] SampleFreq[2:0] SampleSize[1:0]
							// 		 SF = 000 for LPCM and IEC61937 streams
							//		 SS = 000

	pkt[6] = 0; 			// PB3 : 0

	// copy from snd_pcm_substream->snd_pcm_runtime->hw maybe
	
	pkt[7] = 0x00;			// PB4 : Channel/SpeakerAllocation[7:0]
							// 		 00 for FRONT LEFT; FRONT RIGHT

	pkt[8] = 0;				// PB5 : DM_INH LevelShift[3:0] R[3]
							// DM = 0;
							// LS = 0000 = 0dB on signal


	/* count checksum */
	for (i = 0; i < 31; i++)
		checksum += pkt[i];

	pkt[3] = (unsigned char)((~checksum + 1) & 0xff);

	// set to RAM Packet 
	hdmi->ops.packet_generate(hdmi, PACKET_AUDIO_SLOT, pkt);
	hdmi->ops.packet_send(hdmi, PACKET_AUDIO_SLOT, PACKET_PERIOD);

	return 0;
}

static int hdmi_packet_gen_infoframe(struct caninos_hdmi *hdmi)
{
	hdmi_gen_spd_infoframe(hdmi);

	if (hdmi_gen_avi_infoframe(hdmi))
		return -1;

	/* hdmi_gen_gbd_infoframe(hdmi); */
	hdmi_gen_audio_infoframe(hdmi);
	hdmi_gen_vs_infoframe(hdmi);

	return 0;
}

static int caninos_packet_send(struct caninos_hdmi *hdmi, uint32_t no, int period)
{
	uint32_t val;
	
	if (no > PACKET_MAX || no < 0) {
		return -1;
	}
	
	if (period > 0xf || period < 0) {
		return -1;
	}
	
	val = caninos_hdmi_readl(hdmi, HDMI_RPCR);
	val &= (~(1 << no));
	caninos_hdmi_writel(hdmi, HDMI_RPCR,  val);
	
	val = caninos_hdmi_readl(hdmi, HDMI_RPCR);
	val &= (~(0xf << (no * 4 + 8)));
	caninos_hdmi_writel(hdmi, HDMI_RPCR, val);
	
	/* enable and set period */
	if (period)
	{
		val = caninos_hdmi_readl(hdmi, HDMI_RPCR);
		val |= (period << (no * 4 + 8));
		caninos_hdmi_writel(hdmi, HDMI_RPCR,  val);
		
		val = caninos_hdmi_readl(hdmi, HDMI_RPCR);
		val |= (0x1 << no);
		caninos_hdmi_writel(hdmi, HDMI_RPCR,  val);
	}
	
	return 0;
}

static int caninos_packet_generate(struct caninos_hdmi *hdmi, uint32_t no, uint8_t *pkt)
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

	caninos_hdmi_writel(hdmi, HDMI_OPCR, (1 << 8) | (addr & 0xff));
	caninos_hdmi_writel(hdmi, HDMI_ORP6PH,  reg[0]);
	caninos_hdmi_writel(hdmi, HDMI_ORSP6W0, reg[1]);
	caninos_hdmi_writel(hdmi, HDMI_ORSP6W1, reg[2]);
	caninos_hdmi_writel(hdmi, HDMI_ORSP6W2, reg[3]);
	caninos_hdmi_writel(hdmi, HDMI_ORSP6W3, reg[4]);
	caninos_hdmi_writel(hdmi, HDMI_ORSP6W4, reg[5]);
	caninos_hdmi_writel(hdmi, HDMI_ORSP6W5, reg[6]);
	caninos_hdmi_writel(hdmi, HDMI_ORSP6W6, reg[7]);
	caninos_hdmi_writel(hdmi, HDMI_ORSP6W7, reg[8]);
	
	val = caninos_hdmi_readl(hdmi, HDMI_OPCR);
	val |= (0x1 << 31);
	caninos_hdmi_writel(hdmi, HDMI_OPCR, val);
	
	i = 100;
	
	while (i--)
	{
		val = caninos_hdmi_readl(hdmi, HDMI_OPCR);
		val = val >> 31;
		
		if (val == 0) {
			break;
		}
		
		udelay(1);
	}
	
	return 0;
}

static bool caninos_is_video_enabled(struct caninos_hdmi *hdmi)
{
	return (caninos_hdmi_readl(hdmi, HDMI_CR) & 0x01) != 0;
}

static void caninos_video_disable(struct caninos_hdmi *hdmi)
{
	uint32_t val;
	
	val = caninos_hdmi_readl(hdmi, HDMI_TX_2);
	val = REG_SET_VAL(val, 0x0, 11, 8);
	val = REG_SET_VAL(val, 0x0, 17, 17);
	caninos_hdmi_writel(hdmi, HDMI_TX_2, val);
	
	val = caninos_hdmi_readl(hdmi, HDMI_CR);
	val = REG_SET_VAL(val, 0, 0, 0);
	caninos_hdmi_writel(hdmi, HDMI_CR, val);
	
	val = readl(hdmi->cmu_base + hdmi->hwdiff->pll_reg);
	val &= ~(0x1 << hdmi->hwdiff->pll_24m_en);
	val &= ~(0x1 << hdmi->hwdiff->pll_en);
	writel(val, hdmi->cmu_base + hdmi->hwdiff->pll_reg);
	
	/* reset TVOUTPLL */
	writel(0, hdmi->cmu_base + hdmi->hwdiff->pll_reg);
	
	/* reset TVOUTPLL_DEBUG0 & TVOUTPLL_DEBUG1 */
	if (hdmi->hwdiff->model == HDMI_MODEL_K7)
	{
		writel(0x0, hdmi->cmu_base + hdmi->hwdiff->pll_debug0_reg);
		writel(0x2614a, hdmi->cmu_base + hdmi->hwdiff->pll_debug1_reg);
	}
	
	/* TMDS Encoder */
	val = caninos_hdmi_readl(hdmi, TMDS_EODR0);
	val = REG_SET_VAL(val, 0, 31, 31);
	caninos_hdmi_writel(hdmi, TMDS_EODR0, val);
	
	/* txpll_pu */
	val = caninos_hdmi_readl(hdmi, HDMI_TX_1);
	val = REG_SET_VAL(val, 0, 23, 23);
	caninos_hdmi_writel(hdmi, HDMI_TX_1, val);
	
	/* internal TMDS LDO */
	val = caninos_hdmi_readl(hdmi, HDMI_TX_2);
	val = REG_SET_VAL(val, 0, 27, 27); /* LDO_TMDS power off */
	caninos_hdmi_writel(hdmi, HDMI_TX_2, val);
}

static int caninos_update_reg_values(struct caninos_hdmi *hdmi)
{
	hdmi->pll_val = 0;
	
	/* bit31 = 0, debug mode disable, default value if it is not set */
	hdmi->pll_debug0_val = 0;
	hdmi->pll_debug1_val = 0;
	
	hdmi->tx_1 = 0;
	hdmi->tx_2 = 0;
	
	switch (hdmi->vid)
	{
	case VID640x480P_60_4VS3:
		if (hdmi->hwdiff->model == HDMI_MODEL_K7)
		{
			hdmi->pll_val = 0x00000008;	/* 25.2MHz */
			hdmi->tx_1 = 0x819c2984;
			hdmi->tx_2 = 0x18f80f39;
		}
		else
		{
			hdmi->pll_val = 0x00000008;	/* 25.2MHz */
			hdmi->tx_1 = 0x819c2984;
			hdmi->tx_2 = 0x18f80f87;
		}
		break;
	
	case VID720x576P_50_4VS3:
	case VID720x480P_60_4VS3:
		if (hdmi->hwdiff->model == HDMI_MODEL_K7)
		{
			hdmi->pll_val = 0x00010008;	/* 27MHz */
			hdmi->tx_1 = 0x819c2984;
			hdmi->tx_2 = 0x18f80f39;
		}
		else
		{
			hdmi->pll_val = 0x00010008;	/* 27MHz */
			hdmi->tx_1 = 0x819c2984;
			hdmi->tx_2 = 0x18f80f87;
		}
		break;

	case VID1280x720P_60_16VS9:
	case VID1280x720P_50_16VS9:
		if (hdmi->hwdiff->model == HDMI_MODEL_K7)
		{
			hdmi->pll_val = 0x00040008;	/* 74.25MHz */
			hdmi->tx_1 = 0x81982984;
			hdmi->tx_2 = 0x18f80f39;
		}
		else
		{
			hdmi->pll_val = 0x00040008;	/* 74.25MHz */
			hdmi->tx_1 = 0x81942986;
			hdmi->tx_2 = 0x18f80f87;
		}
		break;

	case VID1920x1080P_60_16VS9:
	case VID1920x1080P_50_16VS9:
		if (hdmi->hwdiff->model == HDMI_MODEL_K7)
		{
			hdmi->pll_val = 0x00060008;	/* 148.5MHz */
			hdmi->tx_1 = 0x81942988;
			hdmi->tx_2 = 0x18fe0f39;
		}
		else
		{
			hdmi->pll_val = 0x00060008;	/* 148.5MHz */
			hdmi->tx_1 = 0x8190284f;
			hdmi->tx_2 = 0x18fa0f87;
		}
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static void __caninos_tmds_ldo_enable(struct caninos_hdmi *hdmi)
{
	uint32_t val;
	
	/* do not enable HDMI lane util video enable */
	val = hdmi->tx_2 & (~((0xf << 8) | (1 << 17)));
	caninos_hdmi_writel(hdmi, HDMI_TX_2, val);
}

static void __caninos_phy_enable(struct caninos_hdmi *hdmi)
{
	uint32_t val;
	
	/* TMDS Encoder */
	val = caninos_hdmi_readl(hdmi, TMDS_EODR0);
	val = REG_SET_VAL(val, 1, 31, 31);
	caninos_hdmi_writel(hdmi, TMDS_EODR0, val);
	
	caninos_hdmi_writel(hdmi, HDMI_TX_1, hdmi->tx_1);
}

static void __caninos_pll_enable(struct caninos_hdmi *hdmi)
{
	uint32_t val;
	
	/* 24M enable */
	val = readl(hdmi->cmu_base + hdmi->hwdiff->pll_reg);
	val |= (1 << hdmi->hwdiff->pll_24m_en);
	writel(val, hdmi->cmu_base + hdmi->hwdiff->pll_reg);
	mdelay(1);
	
	/* set PLL, only bit18:16 of pll_val is used */
	val = readl(hdmi->cmu_base + hdmi->hwdiff->pll_reg);
	val &= ~(0x7 << 16);
	val |= (hdmi->pll_val & (0x7 << 16));
	writel(val, hdmi->cmu_base + hdmi->hwdiff->pll_reg);
	mdelay(1);
	
	/* set debug PLL */
	writel(hdmi->pll_debug0_val, hdmi->cmu_base + hdmi->hwdiff->pll_debug0_reg);
	writel(hdmi->pll_debug1_val, hdmi->cmu_base + hdmi->hwdiff->pll_debug1_reg);
	
	/* enable PLL */
	val = readl(hdmi->cmu_base + hdmi->hwdiff->pll_reg);
	val |= (1 << hdmi->hwdiff->pll_en);
	writel(val, hdmi->cmu_base + hdmi->hwdiff->pll_reg);
	mdelay(1);
	
	if (hdmi->hwdiff->model == HDMI_MODEL_K7)
	{
		val = caninos_hdmi_readl(hdmi, CEC_DDC_HPD);
		
		/* 0 to 1, start calibration */
		val = REG_SET_VAL(val, 0, 20, 20);
		caninos_hdmi_writel(hdmi, CEC_DDC_HPD, val);
		
		udelay(10);
		
		val = REG_SET_VAL(val, 1, 20, 20);
		caninos_hdmi_writel(hdmi, CEC_DDC_HPD, val);
		
		while (1) {
			val = caninos_hdmi_readl(hdmi, CEC_DDC_HPD);
			if ((val >> 24) & 0x1)
				break;
		}
	}
}

static void __caninos_video_timing_config(struct caninos_hdmi *hdmi)
{
	bool vsync_pol, hsync_pol, interlace, repeat;
	uint32_t val;
	const struct videomode *mode = &hdmi->mode;
	
	vsync_pol = ((mode->sync & DSS_SYNC_VERT_HIGH_ACT) == 0);
	hsync_pol = ((mode->sync & DSS_SYNC_HOR_HIGH_ACT) == 0);
	
	interlace = hdmi->interlace;
	repeat = hdmi->repeat;
	
	val = caninos_hdmi_readl(hdmi, HDMI_SCHCR);
	val = REG_SET_VAL(val, hsync_pol, 1, 1);
	val = REG_SET_VAL(val, vsync_pol, 2, 2);
	caninos_hdmi_writel(hdmi, HDMI_SCHCR, val);

	val = caninos_hdmi_readl(hdmi, HDMI_VICTL);
	val = REG_SET_VAL(val, interlace, 28, 28);
	val = REG_SET_VAL(val, repeat, 29, 29);
	caninos_hdmi_writel(hdmi, HDMI_VICTL, val);
}

static void __caninos_video_format_config(struct caninos_hdmi *hdmi)
{
	uint32_t val, val_hp, val_vp;
	bool interlace;
	const struct videomode *mode = &hdmi->mode;

	val_hp = mode->xres + mode->hbp + mode->hfp + mode->hsw;
	val_vp = mode->yres + mode->vbp + mode->vfp + mode->vsw;
	interlace = hdmi->interlace;
	
	val = caninos_hdmi_readl(hdmi, HDMI_VICTL);

	val = REG_SET_VAL(val, val_hp - 1, hdmi->hwdiff->hp_end, hdmi->hwdiff->hp_start);

	if (interlace == false)
		val = REG_SET_VAL(val, val_vp - 1, hdmi->hwdiff->vp_end, hdmi->hwdiff->vp_start);
	else
		val = REG_SET_VAL(val, val_vp * 2, hdmi->hwdiff->vp_end, hdmi->hwdiff->vp_start);
	
	caninos_hdmi_writel(hdmi, HDMI_VICTL, val);
}

static int caninos_video_enable(struct caninos_hdmi *hdmi)
{
	uint32_t val, mode;
	int preline, ret;
	
	ret = caninos_update_reg_values(hdmi);
	
	if (ret < 0) {
		return ret;
	}
	
	__caninos_tmds_ldo_enable(hdmi);
	
	udelay(500);
	
	__caninos_phy_enable(hdmi);
	__caninos_pll_enable(hdmi);
	
	mdelay(10);
	
	__caninos_video_timing_config(hdmi);
	__caninos_video_format_config(hdmi);
	
	if (hdmi->interlace == 0)
	{
		val = 0;
		caninos_hdmi_writel(hdmi, HDMI_VIVSYNC, val);
		
		val = caninos_hdmi_readl(hdmi, HDMI_VIVHSYNC);
		
		if (hdmi->vstart != 1)
		{
			val = REG_SET_VAL(val, hdmi->mode.hsw - 1, 8, 0);
			val = REG_SET_VAL(val, hdmi->vstart - 2, 23, 12);
			val = REG_SET_VAL(val, hdmi->vstart + hdmi->mode.vsw - 2, 27, 24);
		}
		else
		{
			val = REG_SET_VAL(val, hdmi->mode.hsw - 1, 8, 0);
			val = REG_SET_VAL(val, hdmi->mode.yres + hdmi->mode.vbp + hdmi->mode.vfp + hdmi->mode.vsw - 1, 23, 12);
			val = REG_SET_VAL(val, hdmi->mode.vsw - 1, 27, 24);
		}
		
		caninos_hdmi_writel(hdmi, HDMI_VIVHSYNC, val);
		
		/*
		 * VIALSEOF = (yres + vbp + vsp - 1) | ((vbp + vfp - 1) << 12)
		 */
		val = caninos_hdmi_readl(hdmi, HDMI_VIALSEOF);
		
		val = REG_SET_VAL(val, hdmi->vstart - 1 + hdmi->mode.vsw + hdmi->mode.vbp + hdmi->mode.yres - 1, 23, 12);
		val = REG_SET_VAL(val, hdmi->vstart - 1 + hdmi->mode.vsw + hdmi->mode.vbp - 1, 10, 0);
		
		caninos_hdmi_writel(hdmi, HDMI_VIALSEOF, val);
		
		val = 0;
		caninos_hdmi_writel(hdmi, HDMI_VIALSEEF, val);
		
		/*
		 * VIADLSE = (xres + hbp + hsp - 1) | ((hbp + hsw - 1) << 12)
		 */
		val = caninos_hdmi_readl(hdmi, HDMI_VIADLSE);
		val = REG_SET_VAL(val, hdmi->mode.hbp +  hdmi->mode.hsw - 1, 11, 0);
		val = REG_SET_VAL(val, hdmi->mode.xres + hdmi->mode.hbp + hdmi->mode.hsw - 1, 28, 16);
		
		caninos_hdmi_writel(hdmi, HDMI_VIADLSE, val);
	}
	else
	{
		val = 0;
		caninos_hdmi_writel(hdmi, HDMI_VIVSYNC, val);
		
		/*
		 * VIVHSYNC =
		 * (hsw -1 ) | ((yres + vsw + vfp + vbp - 1 ) << 12)
		 *  | (vfp -1 << 24)
		 */
		val = caninos_hdmi_readl(hdmi, HDMI_VIVHSYNC);
		val = REG_SET_VAL(val, hdmi->mode.hsw - 1, 8, 0);
		val = REG_SET_VAL(val, (hdmi->mode.yres + hdmi->mode.vbp + hdmi->mode.vfp + hdmi->mode.vsw) * 2, 22, 12);
		val = REG_SET_VAL(val, hdmi->mode.vfp * 2, 22, 12);
		caninos_hdmi_writel(hdmi, HDMI_VIVHSYNC, val);

		/*
		 * VIALSEOF = (yres + vbp + vfp - 1) | ((vbp + vfp - 1) << 12)
		 */
		val = caninos_hdmi_readl(hdmi, HDMI_VIALSEOF);
		val = REG_SET_VAL(val, hdmi->mode.vbp + hdmi->mode.vfp  - 1, 22, 12);
		val = REG_SET_VAL(val, (hdmi->mode.yres + hdmi->mode.vbp + hdmi->mode.vfp) * 2, 10, 0);
		caninos_hdmi_writel(hdmi, HDMI_VIALSEOF, val);

		val = 0;
		caninos_hdmi_writel(hdmi, HDMI_VIALSEEF, val);

		/*
		 * VIADLSE = (xres + hbp + hsp - 1) | ((hbp + hsw - 1) << 12)
		 */
		val = caninos_hdmi_readl(hdmi, HDMI_VIADLSE);
		val = REG_SET_VAL(val, hdmi->mode.hbp + hdmi->mode.hsw - 1, 27, 16);
		val = REG_SET_VAL(val, hdmi->mode.xres + hdmi->mode.hbp + hdmi->mode.hsw - 1, 11, 0);
		
		caninos_hdmi_writel(hdmi, HDMI_VIADLSE, val);
	}
	
	switch (hdmi->vid)
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

	caninos_hdmi_writel(hdmi, HDMI_DIPCCR, val);
	
	val = caninos_hdmi_readl(hdmi, HDMI_ICR);
	
	if (hdmi->settings.hdmi_src == VITD)
	{
		val = REG_SET_VAL(val, 0x01, 24, 24);
		val = REG_SET_VAL(val, hdmi->settings.vitd_color, 23, 0);
	}
	else {
		val = REG_SET_VAL(val, 0x00, 24, 24);
	}
	
	caninos_hdmi_writel(hdmi, HDMI_ICR, val);
	
	val = caninos_hdmi_readl(hdmi, HDMI_SCHCR);
	val = REG_SET_VAL(val, hdmi->settings.pixel_encoding, 5, 4);
	caninos_hdmi_writel(hdmi, HDMI_SCHCR, val);
	
	preline = hdmi->settings.prelines;
	preline = (preline <= 0 ? 1 : preline);
	preline = (preline > 16 ? 16 : preline);

	val = caninos_hdmi_readl(hdmi, HDMI_SCHCR);
	val = REG_SET_VAL(val, preline - 1, 23, 20);
	caninos_hdmi_writel(hdmi, HDMI_SCHCR, val);
	
	val = caninos_hdmi_readl(hdmi, HDMI_SCHCR);
	val = REG_SET_VAL(val, hdmi->settings.deep_color, 17, 16);
	caninos_hdmi_writel(hdmi, HDMI_SCHCR, val);
	
	val = caninos_hdmi_readl(hdmi, HDMI_SCHCR);
	
	val = REG_SET_VAL(val, hdmi->settings.hdmi_mode,
		hdmi->hwdiff->mode_end, hdmi->hwdiff->mode_start);
	
	caninos_hdmi_writel(hdmi, HDMI_SCHCR, val);
	
	
	val = caninos_hdmi_readl(hdmi, HDMI_SCHCR);
	val = REG_SET_VAL(val, hdmi->settings.bit_invert, 28, 28);
	val = REG_SET_VAL(val, hdmi->settings.channel_invert, 29, 29);
	
	caninos_hdmi_writel(hdmi, HDMI_SCHCR, val);
	
	mode = hdmi->settings.deep_color;

	val = caninos_hdmi_readl(hdmi, HDMI_GCPCR);

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

	caninos_hdmi_writel(hdmi, HDMI_GCPCR, val);
	
	val = caninos_hdmi_readl(hdmi, HDMI_SCHCR);

	if (hdmi->settings.mode_3d == HDMI_3D_FRAME) {
		val = REG_SET_VAL(val, 1, 8, 8);
	}
	else {
		val = REG_SET_VAL(val, 0, 8, 8);
	}
	
	caninos_hdmi_writel(hdmi, HDMI_SCHCR, val);
	
	hdmi_packet_gen_infoframe(hdmi);
	
	val = caninos_hdmi_readl(hdmi, HDMI_CR);
	val = REG_SET_VAL(val, 1, 0, 0);
	caninos_hdmi_writel(hdmi, HDMI_CR, val);
	
	val = caninos_hdmi_readl(hdmi, HDMI_TX_2);
	val = REG_SET_VAL(val, (hdmi->tx_2 >> 8) & 0xf, 11, 8);
	val = REG_SET_VAL(val, (hdmi->tx_2 >> 17) & 0x1, 17, 17);
	caninos_hdmi_writel(hdmi, HDMI_TX_2, val);
	
	return 0;
}

static void caninos_audio_enable(struct caninos_hdmi *hdmi)
{
	u32 val = caninos_hdmi_readl(hdmi, HDMI_ICR);
	val |= BIT(25);
	caninos_hdmi_writel(hdmi, HDMI_ICR, val);
}

static void caninos_audio_disable(struct caninos_hdmi *hdmi)
{
	u32 val = caninos_hdmi_readl(hdmi, HDMI_ICR);
	val &= ~BIT(25);
	caninos_hdmi_writel(hdmi, HDMI_ICR, val);
}

static void caninos_set_audio_interface(struct caninos_hdmi *hdmi)
{
	u32 tmp03, tmp47, CRP_N = 0;
	u32 ASPCR = 0;
	u32 ACACR = 0;

	hdmi->ops.audio_disable(hdmi);

	caninos_hdmi_writel(hdmi, HDMI_ACRPCR, caninos_hdmi_readl(hdmi, HDMI_ACRPCR) | BIT(31));
	caninos_hdmi_readl(hdmi, HDMI_ACRPCR); // flush write buffer

	tmp03 = caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE0TO3);
	tmp03 &= ~(0xf << 24);

	tmp47 = caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE4TO7);
	tmp47 &= ~(0xf << 4);
	tmp47 |= 0xb;

	/* assume 48KHz samplerate */
	tmp03 |= 0x2 << 24;
	tmp47 |= 0xd << 4;
	CRP_N = 6144;

	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE0TO3, tmp03);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE4TO7, tmp47);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE8TO11, 0x0);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE12TO15, 0x0);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE16TO19, 0x0);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE20TO23, 0x0);

	// assume 2 channels: channels 1 and 2
	caninos_hdmi_writel(hdmi, HDMI_AICHSTASCN, 0x20001);

	//Sample size = 24b */	
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE4TO7, (caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE4TO7) & ~0xf));
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE4TO7, caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE4TO7) | 0xb);

	// Assume audio is in IEC-60958 format, 2 channels
	ASPCR = 0x11;
	ACACR = 0xfac688;

	/* enable Audio FIFO_FILL  disable wait cycle */
	caninos_hdmi_writel(hdmi, HDMI_CR, caninos_hdmi_readl(hdmi, HDMI_CR) | 0x50);

	caninos_hdmi_writel(hdmi, HDMI_ASPCR, ASPCR);
	caninos_hdmi_writel(hdmi, HDMI_ACACR, ACACR);
     
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE0TO3, caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE0TO3) & ~0x3);
	caninos_hdmi_writel(hdmi, HDMI_ASPCR, caninos_hdmi_readl(hdmi, HDMI_ASPCR) & ~(0xff << 23));


	caninos_hdmi_writel(hdmi, HDMI_ACRPCR, CRP_N | (0x1 << 31));

	hdmi_packet_gen_infoframe(hdmi);

    /* enable CRP */
	caninos_hdmi_writel(hdmi, HDMI_ACRPCR, caninos_hdmi_readl(hdmi, HDMI_ACRPCR) & ~(0x1 << 31));

	hdmi->ops.audio_enable(hdmi);
}

static bool caninos_cable_status(struct caninos_hdmi *hdmi)
{
	bool status = (caninos_hdmi_readl(hdmi, CEC_DDC_HPD) & (0x3 << 14));
	status = status || (caninos_hdmi_readl(hdmi, CEC_DDC_HPD) & (0x3 << 12));
	status = status || (caninos_hdmi_readl(hdmi, CEC_DDC_HPD) & (0x3 << 10));
	status = status || (caninos_hdmi_readl(hdmi, CEC_DDC_HPD) & (0x3 << 8));
	status = status && (caninos_hdmi_readl(hdmi, HDMI_CR) & (1 << 29));
	return status;
}

static bool caninos_hpd_is_pending(struct caninos_hdmi *hdmi)
{
	return (caninos_hdmi_readl(hdmi, HDMI_CR) & (1 << 30)) != 0;
}

static void caninos_hpd_clear_pending(struct caninos_hdmi *hdmi)
{
	u32 val = caninos_hdmi_readl(hdmi, HDMI_CR);
	val = REG_SET_VAL(val, 0x01, 30, 30); /* clear pending bit */
	caninos_hdmi_writel(hdmi, HDMI_CR, val);
}

static void caninos_hpd_enable(struct caninos_hdmi *hdmi)
{
	u32 val = caninos_hdmi_readl(hdmi, HDMI_CR);
	val = REG_SET_VAL(val, 0x0f, 27, 24); /* hotplug debounce */
	val = REG_SET_VAL(val, 0x01, 31, 31); /* enable hotplug interrupt */
	val = REG_SET_VAL(val, 0x01, 28, 28); /* enable hotplug function */
	val = REG_SET_VAL(val, 0x00, 30, 30); /* not clear pending bit */
	caninos_hdmi_writel(hdmi, HDMI_CR, val);
}

static void caninos_hpd_disable(struct caninos_hdmi *hdmi)
{
	u32 val = caninos_hdmi_readl(hdmi, HDMI_CR);
	val = REG_SET_VAL(val, 0x00, 31, 31); /* disable hotplug interrupt */
	val = REG_SET_VAL(val, 0x00, 28, 28); /* enable hotplug function */
	val = REG_SET_VAL(val, 0x01, 30, 30); /* clear pending bit */
	caninos_hdmi_writel(hdmi, HDMI_CR, val);
}

static void caninos_power_off(struct caninos_hdmi *hdmi)
{
	reset_control_assert(hdmi->hdmi_rst);
	clk_disable_unprepare(hdmi->hdmi_dev_clk);
}

static void caninos_exit(struct caninos_hdmi *hdmi)
{
	caninos_power_off(hdmi);
}

static int caninos_power_on(struct caninos_hdmi *hdmi)
{
	int ret = 0;
	
	if (!caninos_is_video_enabled(hdmi)) {
		reset_control_assert(hdmi->hdmi_rst);
	}
	
	clk_prepare_enable(hdmi->hdmi_dev_clk);
	mdelay(1);
	
	if (!caninos_is_video_enabled(hdmi))
	{
		reset_control_deassert(hdmi->hdmi_rst);
		mdelay(1);
	}
	
	return ret;
}

static int caninos_init(struct caninos_hdmi *hdmi)
{
	hdmi->settings.hdmi_src = DE;
	hdmi->settings.vitd_color = 0xff0000;
	hdmi->settings.pixel_encoding = RGB444;
	hdmi->settings.color_xvycc = 0;
	hdmi->settings.deep_color = color_mode_24bit;
	hdmi->settings.hdmi_mode = HDMI_HDMI;
	hdmi->settings.mode_3d = HDMI_2D;
	hdmi->settings.prelines = 0;
	hdmi->settings.channel_invert = 0;
	hdmi->settings.bit_invert = 0;
	return caninos_power_on(hdmi);
}
static const struct caninos_hdmi_hwdiff caninos_hwdiff_k7 = {
	.model = HDMI_MODEL_K7,
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

static const struct caninos_hdmi_hwdiff caninos_hwdiff_k5 = {	
	.model = HDMI_MODEL_K5,		
	.hp_start = 16,
	.hp_end = 28,
	.vp_start = 4,
	.vp_end = 15,
	.mode_start = 0,
	.mode_end = 0,
	.pll_reg = 0x18,
	.pll_24m_en = 23,
	.pll_en = 3,
	.pll_debug0_reg = 0xEC,
	.pll_debug1_reg = 0xF4,
};

static int caninos_hdmi_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct caninos_hdmi *hdmi;
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
	
	hdmi = devm_kzalloc(dev, sizeof(*hdmi), GFP_KERNEL);
	
	if (!hdmi) {
		dev_err(dev, "could not alloc hdmi data structure\n");
		return -ENOMEM;
	}
	
	hdmi->hwdiff = devm_kzalloc(dev, sizeof(struct caninos_hdmi_hwdiff), GFP_KERNEL);
	
	if (!hdmi->hwdiff) {
		dev_err(dev, "could not alloc hwdiff data structure\n");
		return -ENOMEM;
	}
	
	memcpy(hdmi->hwdiff, match->data, sizeof(struct caninos_hdmi_hwdiff));
	
	hdmi->dev = dev;
	hdmi->ops.init = caninos_init;
	hdmi->ops.exit = caninos_exit;
	hdmi->ops.hpd_enable = caninos_hpd_enable;
	hdmi->ops.hpd_disable = caninos_hpd_disable;
	hdmi->ops.hpd_is_pending = caninos_hpd_is_pending;
	hdmi->ops.hpd_clear_pending = caninos_hpd_clear_pending;
	hdmi->ops.cable_status = caninos_cable_status;
	hdmi->ops.video_enable = caninos_video_enable;
	hdmi->ops.video_disable = caninos_video_disable;
	hdmi->ops.is_video_enabled = caninos_is_video_enabled;
	hdmi->ops.audio_disable = caninos_audio_disable;
	hdmi->ops.audio_enable = caninos_audio_enable;
	hdmi->ops.set_audio_interface = caninos_set_audio_interface;
	hdmi->ops.packet_generate = caninos_packet_generate;
	hdmi->ops.packet_send = caninos_packet_send;
	
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cmu");
	
	if (res) {
		hdmi->cmu_base = devm_ioremap(dev, res->start, resource_size(res));
	}
	if (IS_ERR_OR_NULL(hdmi->cmu_base))
	{
		dev_err(dev, "could not map cmu registers\n");
		return (!hdmi->cmu_base) ? -ENODEV : PTR_ERR(hdmi->cmu_base);
	}
	
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hdmi");
	
	if (res) {
		hdmi->base = devm_ioremap(dev, res->start, resource_size(res));
	}
	if (IS_ERR_OR_NULL(hdmi->base))
	{
		dev_err(dev, "could not map hdmi registers\n");
		return (!hdmi->base) ? -ENODEV : PTR_ERR(hdmi->base);
	}
	
	hdmi->hdmi_dev_clk = devm_clk_get(dev, "hdmi");
	
	if (IS_ERR(hdmi->hdmi_dev_clk)) {
		dev_err(dev, "could not get hdmi clock\n");
		return -ENODEV;
	}
	
	hdmi->hdmi_rst = devm_reset_control_get(dev, "hdmi");
	
	if (IS_ERR(hdmi->hdmi_rst)) {
		dev_err(dev, "could not get hdmi reset control\n");
		return PTR_ERR(hdmi->hdmi_rst);
	}
	
	ret = hdmi->ops.init(hdmi);
	
	if (ret) {
		dev_err(dev, "init failed\n");
		return ret;
	}
	
	platform_set_drvdata(pdev, hdmi);
	
	dev_info(dev, "probe finished\n");
	return 0;
}

static int caninos_hdmi_remove(struct platform_device *pdev)
{
	struct caninos_hdmi *hdmi = platform_get_drvdata(pdev);
	if (hdmi && hdmi->ops.exit)
		hdmi->ops.exit(hdmi);
	return 0;
}

static const struct of_device_id caninos_hdmi_match[] = {
	{ .compatible = "caninos,k7-hdmi", .data = (void*)&caninos_hwdiff_k7 },
	{ .compatible = "caninos,k5-hdmi", .data = (void*)&caninos_hwdiff_k5 },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, caninos_hdmi_match);

static struct platform_driver caninos_hdmi_driver = {
	.probe = caninos_hdmi_probe,
	.remove = caninos_hdmi_remove,
	.driver = {
		.name = "caninos-hdmi",
		.of_match_table = caninos_hdmi_match,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(caninos_hdmi_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("Caninos HDMI Video Driver");
MODULE_LICENSE("GPL v2");
