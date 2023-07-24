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

#define RECOMMENDED_PRELINE_TIME (60)

struct caninos_de_hw;

enum caninos_de_hw_model
{
	CANINOS_DE_HW_MODEL_INV = 0x0,
	CANINOS_DE_HW_MODEL_K5  = 0x55AA,
	CANINOS_DE_HW_MODEL_K7  = 0xAA55,
};

struct caninos_gfx
{
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct drm_plane plane;
	struct drm_crtc crtc;
	
	struct drm_device *drm;
	struct device *dev;
	
	enum caninos_de_hw_model model;
	struct caninos_de_hw *hw;
	
	void __iomem *base;
	void __iomem *cvbs_base;
	void __iomem *dcu_base;
	
	struct clk *clk, *parent_clk;
	struct clk *tvout_clk, *cvbspll_clk;
	struct reset_control *cvbs_rst;
	struct reset_control *de_rst;
	int irq;
	
	struct caninos_hdmi *caninos_hdmi;
};

extern int caninos_gfx_pipe_init(struct drm_device *drm);

extern struct caninos_de_hw *caninos_de_k7_init(struct caninos_gfx *parent);

extern struct caninos_de_hw *caninos_de_k5_init(struct caninos_gfx *parent);

