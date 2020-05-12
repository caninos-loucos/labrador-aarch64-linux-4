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

#define REG_MASK(start, end)	(((1 << ((start) - (end) + 1)) - 1) << (end))
#define REG_VAL(val, start, end) (((val) << (end)) & REG_MASK(start, end))
#define REG_GET_VAL(val, start, end) (((val) & REG_MASK(start, end)) >> (end))
#define REG_SET_VAL(orig, val, start, end) (((orig) & ~REG_MASK(start, end))\
						 | REG_VAL(val, start, end))

/****************************************************************************/
#define    TVOUT_EN						(0x0000)
#define     TVOUT_OCR						(0x0004)
#define     TVOUT_STA						(0x0008)
#define     TVOUT_CCR						(0x000C)
#define     TVOUT_BCR						(0x0010)
#define     TVOUT_CSCR						(0x0014)
#define     TVOUT_PRL						(0x0018)
#define     TVOUT_VFALD						(0x001C)
#define     CVBS_MSR						(0x0020)
#define     CVBS_AL_SEPO					(0x0024)
#define     CVBS_AL_SEPE					(0x0028)
#define     CVBS_AD_SEP						(0x002c)
#define     CVBS_HUECR						(0x0030)
#define     CVBS_SCPCR						(0x0034)
#define     CVBS_SCFCR						(0x0038)
#define     CVBS_CBACR						(0x003c)
#define     CVBS_SACR						(0x0040)

#define     TVOUT_DCR						(0x0070)
#define     TVOUT_DDCR						(0x0074)
#define     TVOUT_DCORCTL					(0x0078)
#define     TVOUT_DRCR						(0x007c)
/**********************************************************************/
/* TVOUT_EN */
/* bit[31:1]  Reserved */
#define TVOUT_EN_CVBS_EN					(0x1 << 0)

/* TVOUT_OCR */
/* bit[2:0],bit[6:5],bit[22:13],bit[31:29]  Reserved */
#define TVOUT_OCR_PI_IRQEN					(0x1 << 12)
#define TVOUT_OCR_PO_IRQEN					(0x1 << 11)
#define TVOUT_OCR_INACEN					(0x1 << 10)
#define TVOUT_OCR_PO_ADEN					(0x1 << 9)
#define TVOUT_OCR_PI_ADEN					(0x1 << 8)
#define TVOUT_OCR_INREN						(0x1 << 7)
#define TVOUT_OCR_DACOUT					(0x1 << 4)
#define TVOUT_OCR_DAC3						(0x1 << 3)

/* CVBS_MSR */
/* bit[31:7]  Reserved */
#define CVBS_MSR_SCEN					(0x1 << 6)
#define CVBS_MSR_APNS_MASK				(0x1 << 5)
#define CVBS_MSR_APNS_BT470				(0x0 << 5)
#define CVBS_MSR_APNS_BT656				(0x1 << 5)
#define CVBS_MSR_CVCKS					(0x1 << 4)
#define CVBS_MSR_CVBS_MASK				(0xF << 0)
#define CVBS_MSR_CVBS_NTSC_M				(0x0 << 0)
#define CVBS_MSR_CVBS_NTSC_J				(0x1 << 0)
#define CVBS_MSR_CVBS_PAL_NC				(0x2 << 0)
#define CVBS_MSR_CVBS_PAL_BGH				(0x3 << 0)
#define CVBS_MSR_CVBS_PAL_D				(0x4 << 0)
#define CVBS_MSR_CVBS_PAL_I				(0x5 << 0)
#define CVBS_MSR_CVBS_PAL_M				(0x6 << 0)
#define CVBS_MSR_CVBS_PAL_N				(0x7 << 0)


/* CVBS_AL_SEPO */
/* bit[31:26]  Reserved */
#define CVBS_AL_SEPO_ALEP_MASK				(0x3FF << 16)
#define CVBS_AL_SEPO_ALEP(x)				(((x) & 0x3FF) << 16)

/* bit[15:10]  Reserved */
#define CVBS_AL_SEPO_ALSP_MASK				(0x3FF << 0)
#define CVBS_AL_SEPO_ALSP(x)				(((x) & 0x3FF) << 0)

/* CVBS_AL_SEPE */
/* bit[31:26]  Reserved */
#define CVBS_AL_SEPE_ALEPEF_MASK			(0x3FF << 16)
#define CVBS_AL_SEPE_ALEPEF(x)				(((x) & 0x3FF) << 16)

/* bit[15:10]  Reserved */
#define CVBS_AL_SEPE_ALSPEF_MASK			(0x3FF << 0)
#define CVBS_AL_SEPE_ALSPEF(x)				(((x) & 0x3FF) << 0)

/* CVBS_AD_SEP */
/* bit[31:26]  Reserved */
#define CVBS_AD_SEP_ADEP_MASK				(0x3FF << 16)
#define CVBS_AD_SEP_ADEP(x)				(((x) & 0x3FF) << 16)

/* bit[15:10]  Reserved */
#define CVBS_AD_SEP_ADSP_MASK				(0x3FF << 0)
#define CVBS_AD_SEP_ADSP(x)				(((x) & 0x3FF) << 0)

