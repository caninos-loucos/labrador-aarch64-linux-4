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
 
#include "hdmi.h"

/* Output Devices ----------------------------------------------------------- */
#define DE_OUTPUT_CON_HDMI (0x0)

/* Output Paths ------------------------------------------------------------- */
#define DE_PATH0 (0x0)
#define DE_PATH1 (0x1)

/* Micro Layers ------------------------------------------------------------- */
#define DE_MICRO_LAYER0 (0x0)
#define DE_MICRO_LAYER1 (0x1)
#define DE_MICRO_LAYER2 (0x2)
#define DE_MICRO_LAYER3 (0x3)

/* Sub Layers --------------------------------------------------------------- */
#define DE_SUB_LAYER0 (0x0)
#define DE_SUB_LAYER1 (0x1)
#define DE_SUB_LAYER2 (0x2)
#define DE_SUB_LAYER3 (0x3)

/* Register Offsets --------------------------------------------------------- */
#define DE_IRQENABLE       (0x0000)
#define DE_IRQSTATUS       (0x0004)
#define DE_MMU_EN          (0x0008)
#define DE_MMU_BASE        (0x000c)
#define DE_QOS             (0x0014)
#define DE_MAX_OUTSTANDING (0x0018)
#define DE_PATH_BASE       (0x0100)
#define DE_ML_BASE         (0x0400)
#define DE_SCALER_BASE     (0x0C00)

/* DE_PATH_CTL -------------------------------------------------------------- */
#define DE_PATH_CTL(x) (DE_PATH_BASE + ((x) * 0x100) + 0x000)

#define DE_PATH_CTL_MLn_EN_LBIT    (0)
#define DE_PATH_CTL_MLn_EN_HBIT    (3)
#define DE_PATH_CTL_ILACE_BIT      (9)
#define DE_PATH_CTL_RGB_YUV_EN_BIT (13)

#define DE_PATH_CTL_MLn_EN_MASK \
	GENMASK(DE_PATH_CTL_MLn_EN_HBIT, DE_PATH_CTL_MLn_EN_LBIT)

#define DE_PATH_CTL_MLn_EN(n) (BIT(n) << DE_PATH_CTL_MLn_EN_LBIT)

/* DE_PATH_EN --------------------------------------------------------------- */
#define DE_PATH_EN(x) (DE_PATH_BASE + ((x) * 0x100) + 0x008)

#define DE_PATH_EN_ENABLE_BIT (0)

/* DE_PATH_FCR -------------------------------------------------------------- */
#define DE_PATH_FCR(x) (DE_PATH_BASE + ((x) * 0x100) + 0x004)

#define DE_PATH_FCR_GO_BIT (0)

/* DE_ML_CFG ---------------------------------------------------------------- */
#define DE_ML_CFG(x) (DE_ML_BASE + ((x) * 0x200) + 0x000)

#define DE_ML_CFG_SLn_EN_LBIT (0)
#define DE_ML_CFG_SLn_EN_HBIT (3)
#define DE_ML_CFG_ROT180_BIT  (20)

#define DE_ML_CFG_SLn_EN_MASK \
	GENMASK(DE_ML_CFG_SLn_EN_HBIT, DE_ML_CFG_SLn_EN_LBIT)

#define DE_ML_CFG_SLn_EN(n) (BIT(n) << DE_ML_CFG_SLn_EN_LBIT)

/* DE_OUTPUT_CON ------------------------------------------------------------ */
#define DE_OUTPUT_CON (0x1000)

#define DE_OUTPUT_CON_PATH0_DEVICE_LBIT (0)
#define DE_OUTPUT_CON_PATH0_DEVICE_HBIT (1)
#define DE_OUTPUT_CON_PATH1_DEVICE_LBIT (4)
#define DE_OUTPUT_CON_PATH1_DEVICE_HBIT (5)

#define DE_OUTPUT_CON_PATH0_DEVICE_MASK \
	GENMASK(DE_OUTPUT_CON_PATH0_DEVICE_HBIT, DE_OUTPUT_CON_PATH0_DEVICE_LBIT)

#define DE_OUTPUT_CON_PATH0_DEVICE(x) ((x) << DE_OUTPUT_CON_PATH0_DEVICE_LBIT)

#define DE_OUTPUT_CON_PATH1_DEVICE_MASK \
	GENMASK(DE_OUTPUT_CON_PATH1_DEVICE_HBIT, DE_OUTPUT_CON_PATH1_DEVICE_LBIT)

#define DE_OUTPUT_CON_PATH1_DEVICE(x) ((x) << DE_OUTPUT_CON_PATH1_DEVICE_LBIT)

/* DE_PATH_SIZE ------------------------------------------------------------- */
#define DE_PATH_SIZE(x) (DE_PATH_BASE + ((x) * 0x100) + 0x010)

#define DE_PATH_SIZE_WIDTH_LBIT  (0)
#define DE_PATH_SIZE_WIDTH_HBIT  (11)
#define DE_PATH_SIZE_HEIGHT_LBIT (16)
#define DE_PATH_SIZE_HEIGHT_HBIT (27)

#define DE_PATH_SIZE_WIDTH_MASK \
	GENMASK(DE_PATH_SIZE_WIDTH_HBIT, DE_PATH_SIZE_WIDTH_LBIT)

#define DE_PATH_SIZE_WIDTH(x) ((x) << DE_PATH_SIZE_WIDTH_LBIT)

#define DE_PATH_SIZE_HEIGHT_MASK \
	GENMASK(DE_PATH_SIZE_HEIGHT_HBIT, DE_PATH_SIZE_HEIGHT_LBIT)

#define DE_PATH_SIZE_HEIGHT(x) ((x) << DE_PATH_SIZE_HEIGHT_LBIT)

/* DE_PATH_BK --------------------------------------------------------------- */
#define DE_PATH_BK(x) (DE_PATH_BASE + ((x) * 0x100) + 0x00c)

/* DE_SL_FB ----------------------------------------------------------------- */
#define DE_SL_FB(x, y)  (DE_ML_BASE + ((x) * 0x200) + 0x02c + ((y) * 0x80))

/* DE_SL_STR ---------------------------------------------------------------- */
#define DE_SL_STR(x, y) (DE_ML_BASE + ((x) * 0x200) + 0x044 + ((y) * 0x80))

/* DE_SL_CFG ---------------------------------------------------------------- */
#define DE_SL_CFG(x, y) (DE_ML_BASE + ((x) * 0x200) + 0x020 + ((y) * 0x80))

#define DE_SL_CFG_FMT_LBIT          (0)
#define DE_SL_CFG_FMT_HBIT          (4)
#define DE_SL_CFG_GLOBAL_ALPHA_LBIT (8)
#define DE_SL_CFG_GLOBAL_ALPHA_HBIT (15)
#define DE_SL_CFG_DATA_MODE_BIT     (16)

#define DE_SL_CFG_FMT_MASK \
	GENMASK(DE_SL_CFG_FMT_HBIT, DE_SL_CFG_FMT_LBIT)

#define DE_SL_CFG_FMT(x) ((x) << DE_SL_CFG_FMT_LBIT)

#define DE_SL_CFG_GLOBAL_ALPHA_MASK \
	GENMASK(DE_SL_CFG_GLOBAL_ALPHA_HBIT, DE_SL_CFG_GLOBAL_ALPHA_LBIT)

#define DE_SL_CFG_GLOBAL_ALPHA(x) ((x) << DE_SL_CFG_GLOBAL_ALPHA_LBIT)

/* DE_SL_CROPSIZE ----------------------------------------------------------- */
#define DE_SL_CROPSIZE(x, y) (DE_ML_BASE + ((x) * 0x200) + 0x048 + ((y) * 0x80))

/* DE_ML_ISIZE -------------------------------------------------------------- */
#define DE_ML_ISIZE(x) (DE_ML_BASE + ((x) * 0x200) + 0x0004)

/* DE_SL_COOR --------------------------------------------------------------- */
#define DE_SL_COOR(x, y) (DE_ML_BASE + ((x) * 0x200) + 0x028 + ((y) * 0x80))

/* DE_PATH_COOR ------------------------------------------------------------- */
#define DE_PATH_COOR(m, n) (DE_PATH_BASE + ((m) * 0x100) + ((n) * 0x4) + 0x20)

/* DE_SCALER_OSZIE ---------------------------------------------------------- */
#define DE_SCALER_OSZIE(x) (DE_SCALER_BASE + ((x) * 0x80) + 0x0004)

/* -------------------------------------------------------------------------- */

#define DE_SCLCOEF_ZOOMIN (0)
#define DE_SCLCOEF_HALF_ZOOMOUT (1)
#define DE_SCLCOEF_SMALLER_ZOOMOUT (2)

#define DE_ML_CSC(x) (DE_ML_BASE + (x) * 0x200  +  0x0014)

#define DE_SCALER_HSR(x) (DE_SCALER_BASE + (x) * 0x80 + 0x0008)
#define DE_SCALER_VSR(x) (DE_SCALER_BASE + (x) * 0x80 + 0x000C)

#define DE_SCALER_SCOEF0(x) (DE_SCALER_BASE + (x) * 0x80 + 0x0020)
#define DE_SCALER_SCOEF1(x) (DE_SCALER_BASE + (x) * 0x80 + 0x0024)
#define DE_SCALER_SCOEF2(x) (DE_SCALER_BASE + (x) * 0x80 + 0x0028)
#define DE_SCALER_SCOEF3(x)	(DE_SCALER_BASE + (x) * 0x80 + 0x002C)
#define DE_SCALER_SCOEF4(x)	(DE_SCALER_BASE + (x) * 0x80 + 0x0030)
#define DE_SCALER_SCOEF5(x)	(DE_SCALER_BASE + (x) * 0x80 + 0x0034)
#define DE_SCALER_SCOEF6(x) (DE_SCALER_BASE + (x) * 0x80 + 0x0038)
#define DE_SCALER_SCOEF7(x)	(DE_SCALER_BASE + (x) * 0x80 + 0x003C)

#define RECOMMENDED_PRELINE_TIME (60)

enum caninos_blending_type
{
	CANINOS_BLENDING_NONE = 0,
	CANINOS_BLENDING_COVERAGE = 1,
};

struct caninos_gfx
{
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct drm_plane plane;
	struct drm_crtc crtc;
	struct device *dev;
	
	void __iomem *base;
	void __iomem *cvbs_base;
	void __iomem *dcu_base;
	
	struct clk *clk, *parent_clk;
	struct clk *tvout_clk, *cvbspll_clk;
	
	struct reset_control *cvbs_rst;
	struct reset_control *de_rst;
	
	struct caninos_hdmi *caninos_hdmi;
};

extern int caninos_gfx_pipe_init(struct drm_device *drm);
                                 
extern irqreturn_t caninos_gfx_irq_handler(int irq, void *data);

