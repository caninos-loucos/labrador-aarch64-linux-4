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

#include <drm/drm_simple_kms_helper.h>
#include "gfx_drv.h"
#include "gfx_de.h"
#include "de-k5.h"

typedef struct caninos_de_k5
{
	caninos_de_hw_t hw;
	struct caninos_gfx *parent;
	
} caninos_de_k5_t;

static inline caninos_de_k5_t *caninos_de_hw_to_k5(const caninos_de_hw_t *hw)
{
	return container_of(hw, caninos_de_k5_t, hw);
}

static void caninos_de_plane_fbaddr_raw(struct caninos_gfx *priv, u32 paddr)
{
	writel(paddr, priv->base + DE_PLANE_BA0(DSS_PLANE1));
}

static void caninos_de_plane_fmt_raw(struct caninos_gfx *priv, u32 color_mode)
{
	u32 val, alpha;
	
	val = readl(priv->base + DE_PLANE_CFG(DSS_PLANE1));
	val &= ~(DE_PLANE_CFG_FMT_MSK | DE_PLANE_CFG_FMT_BYTES_MSK);
	
	switch (color_mode)
	{
	case DRM_FORMAT_ARGB8888:
		val |= DE_PLANE_CFG_FMT(0x1);
		alpha = 0x0;
		break;
		
	case DRM_FORMAT_XRGB8888:
		val |= DE_PLANE_CFG_FMT(0x1);
		alpha = 0x1;
		break;
		
	case DRM_FORMAT_RGBA8888:
		val |= DE_PLANE_CFG_FMT(0x1) | DE_PLANE_CFG_FMT_BYTES(0xc);
		alpha = 0x0;
		break;
		
	case DRM_FORMAT_RGBX8888:
		val |= DE_PLANE_CFG_FMT(0x1) | DE_PLANE_CFG_FMT_BYTES(0xc);
		alpha = 0x1;
		break;
		
	case DRM_FORMAT_ABGR8888:
		val |= DE_PLANE_CFG_FMT(0x5);
		alpha = 0x0;
		break;
		
	case DRM_FORMAT_XBGR8888:
		val |= DE_PLANE_CFG_FMT(0x5);
		alpha = 0x1;
		break;
		
	case DRM_FORMAT_BGRA8888:
		val |= DE_PLANE_CFG_FMT(0x5) | DE_PLANE_CFG_FMT_BYTES(0xc);
		alpha = 0x0;
		break;
		
	case DRM_FORMAT_BGRX8888:
		val |= DE_PLANE_CFG_FMT(0x5) | DE_PLANE_CFG_FMT_BYTES(0xc);
		alpha = 0x1;
		break;
		
	default:
		return;
	}
	writel(val, priv->base + DE_PLANE_CFG(DSS_PLANE1));
	writel(0xFF, priv->base + DE_PLANE_ALPHA_CFG(DSS_PLANE1));
	writel(alpha, priv->base + DE_PLANE_ALPHA_EN(DSS_PLANE1));
}

static void caninos_de_plane_rotate_raw(struct caninos_gfx *priv, u32 rotation)
{
	u32 val;
	val = readl(priv->base + DE_PLANE_CFG(DSS_PLANE1));
	val &= ~DE_PLANE_CFG_FLIP_MSK;
	val |= DE_PLANE_CFG_FLIP(rotation);
	writel(val, priv->base + DE_PLANE_CFG(DSS_PLANE1));
}

static void caninos_de_plane_stride_raw(struct caninos_gfx *priv, u32 stride)
{
	writel((stride >> 3), priv->base + DE_PLANE_STR(DSS_PLANE1));
}

static void caninos_de_init_scaler_raw(struct caninos_gfx *priv,
                                       u32 factor, u32 hsr, u32 vsr)
{
	hsr &= 0xFFFF;
	vsr &= 0xFFFF;
	
	wmb();
	writel_relaxed((vsr << 16) | hsr, priv->base + DE_PLANE_SR(DSS_PLANE1));
	
	if (factor <= 1U) {
		writel_relaxed(0x00400000, priv->base + DE_PLANE_SCOEF0(DSS_PLANE1));
		writel_relaxed(0xFC3E07FF, priv->base + DE_PLANE_SCOEF1(DSS_PLANE1));
		writel_relaxed(0xFA3810FE, priv->base + DE_PLANE_SCOEF2(DSS_PLANE1));
		writel_relaxed(0xF9301BFC, priv->base + DE_PLANE_SCOEF3(DSS_PLANE1));
		writel_relaxed(0xFA2626FA, priv->base + DE_PLANE_SCOEF4(DSS_PLANE1));
		writel_relaxed(0xFC1B30F9, priv->base + DE_PLANE_SCOEF5(DSS_PLANE1));
		writel_relaxed(0xFE1038FA, priv->base + DE_PLANE_SCOEF6(DSS_PLANE1));
		writel_relaxed(0xFF073EFC, priv->base + DE_PLANE_SCOEF7(DSS_PLANE1));        
	}
	else if (factor <= 4U) {
		writel_relaxed(0x00400000, priv->base + DE_PLANE_SCOEF0(DSS_PLANE1));
		writel_relaxed(0x00380800, priv->base + DE_PLANE_SCOEF1(DSS_PLANE1));
		writel_relaxed(0x00301000, priv->base + DE_PLANE_SCOEF2(DSS_PLANE1));
		writel_relaxed(0x00281800, priv->base + DE_PLANE_SCOEF3(DSS_PLANE1));
		writel_relaxed(0x00202000, priv->base + DE_PLANE_SCOEF4(DSS_PLANE1));
		writel_relaxed(0x00182800, priv->base + DE_PLANE_SCOEF5(DSS_PLANE1));
		writel_relaxed(0x00103000, priv->base + DE_PLANE_SCOEF6(DSS_PLANE1));
		writel_relaxed(0x00083800, priv->base + DE_PLANE_SCOEF7(DSS_PLANE1));       
	}
	else {
		writel_relaxed(0x10201000, priv->base + DE_PLANE_SCOEF0(DSS_PLANE1));
		writel_relaxed(0x0E1E1202, priv->base + DE_PLANE_SCOEF1(DSS_PLANE1));
		writel_relaxed(0x0C1C1404, priv->base + DE_PLANE_SCOEF2(DSS_PLANE1));
		writel_relaxed(0x0A1A1606, priv->base + DE_PLANE_SCOEF3(DSS_PLANE1));
		writel_relaxed(0x08181808, priv->base + DE_PLANE_SCOEF4(DSS_PLANE1));
		writel_relaxed(0x06161A0A, priv->base + DE_PLANE_SCOEF5(DSS_PLANE1));
		writel_relaxed(0x04141C0C, priv->base + DE_PLANE_SCOEF6(DSS_PLANE1));
		writel_relaxed(0x02121E0E, priv->base + DE_PLANE_SCOEF7(DSS_PLANE1));
	}
}

static void caninos_de_plane_scaling_raw(struct caninos_gfx *priv,
                                         const caninos_de_ovl_t *ovl)
{
	u32 factor, hsr, vsr, val;
	
	factor = (ovl->src.w * ovl->src.h) / (ovl->dst.w * ovl->dst.h);
	
	hsr = (ovl->src.w * DE_SCALE_CONST_VALUE + ovl->dst.w - 1U) / ovl->dst.w;
	vsr = (ovl->src.h * DE_SCALE_CONST_VALUE + ovl->dst.h - 1U) / ovl->dst.h;
	
	caninos_de_init_scaler_raw(priv, factor, hsr, vsr);
	
	val = ((ovl->src.h - 1U) << 16) | (ovl->src.w - 1U);
	writel(val, priv->base + DE_PLANE_ISIZE(DSS_PLANE1));
	
	val = ((ovl->dst.h - 1U) << 16) | (ovl->dst.w - 1U);
	writel(val, priv->base + DE_PLANE_OSIZE(DSS_PLANE1));
	
	val = (ovl->dst.y << 16) | ovl->dst.x;
	writel(val, priv->base + DE_PLANE_COOR(DSS_PLANE1));
}

static void caninos_de_plane_set_enable_raw(struct caninos_gfx *priv, bool en)
{
	void __iomem *ctl_base1, *ctl_base2;
	u32 val1, val2;
	
	ctl_base1 = priv->base + DE_PATH_CTL(DSS_PATH1);
	ctl_base2 = priv->base + DE_PATH_CTL(DSS_PATH2);
	
	val1 = readl_relaxed(ctl_base1);
	val2 = readl_relaxed(ctl_base2);
	rmb();
	
	val1 &= ~DE_PATH_CTL_PLANE_EN_MSK;
	val2 &= ~DE_PATH_CTL_PLANE_EN_MSK;
	
	if (en) {
		val1 |= DE_PATH_CTL_PLANE_EN(DSS_PLANE1);
	}
	
	wmb();
	writel_relaxed(val1, ctl_base1);
	writel_relaxed(val2, ctl_base2);
}

static void caninos_de_path_set_go_raw(struct caninos_gfx *priv)
{
	u32 val;
	val = readl(priv->base + DE_PATH_FCR(DSS_PATH1));
	val |= DE_PATH_FCR_SET_GO;
	writel(val, priv->base + DE_PATH_FCR(DSS_PATH1));
}

static void caninos_de_path_set_sz_raw(struct caninos_gfx *priv, u32 w, u32 h)
{
	u32 val = (((h - 1U) & 0xFFF) << 16) | ((w - 1U) & 0xFFF);
	writel(val, priv->base + DE_PATH_SIZE(DSS_PATH1));
}

static void caninos_de_path_set_bk_color_raw(struct caninos_gfx *priv, u32 bk)
{
	writel(bk, priv->base + DE_PATH_BK(DSS_PATH1));
}

static void caninos_de_path_set_enable_raw(struct caninos_gfx *priv, bool en)
{
	u32 val1, val2;
	
	val1 = readl_relaxed(priv->base + DE_PATH_EN(DSS_PATH1));
	val2 = readl_relaxed(priv->base + DE_PATH_EN(DSS_PATH2));
	rmb();
	
	/* disable all paths */
	val1 &= ~DE_PATH_EN_ENABLE;
	val2 &= ~DE_PATH_EN_ENABLE;
	
	/* enable/disable only the first path */
	if (en) {
		val1 |= DE_PATH_EN_ENABLE;
	}
	
	wmb();
	writel_relaxed(val1, priv->base + DE_PATH_EN(DSS_PATH1));
	writel_relaxed(val2, priv->base + DE_PATH_EN(DSS_PATH2));
}

static int k5_de_power_on(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	u32 val1, val2;
	
	clk_prepare_enable(priv->clk);
	
	/* reset controller */
	reset_control_assert(de->parent->de_rst);
	usleep_range(200, 300);
	reset_control_deassert(de->parent->de_rst);
	usleep_range(1000, 1250);
	
	/* special outstanding (whatever it is)*/
	writel(0x1f0000, priv->base + DE_IF_CON);
	
	/* disable irqs and clear pending ones */
	writel(0x0, priv->base + DE_IRQENABLE);
	val1 = readl_relaxed(priv->base + DE_IRQSTATUS);
	mb();
	writel_relaxed(val1, priv->base + DE_IRQSTATUS);
	readl(priv->base + DE_IRQSTATUS); /* flush status */
	
	/* make sure all planes and paths are disabled */
	caninos_de_plane_set_enable_raw(priv, false);
	caninos_de_path_set_enable_raw(priv, false);
	
	/* path initial config */
	val1 = readl_relaxed(priv->base + DE_OUTPUT_CON);
	val2 = readl_relaxed(priv->base + DE_PATH_CTL(DSS_PATH1));
	rmb();
	
	/* set output to hdmi */
	val1 &= ~DE_OUTPUT_CON_PATH1_DEV_MSK;
	val1 |= DE_OUTPUT_CON_PATH1_DEV(DE_OUTPUT_CON_HDMI);
	
	/* disable interlace and YUV */
	val2 &= ~(DE_PATH_CTL_ILACE_EN | DE_PATH_CTL_YUV_EN);
	
	wmb();
	writel_relaxed(val1, priv->base + DE_OUTPUT_CON);
	writel_relaxed(val2, priv->base + DE_PATH_CTL(DSS_PATH1));
	
	return 0;
}

static void k5_de_power_off(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	
	caninos_de_path_set_enable_raw(priv, false);
	caninos_de_plane_set_enable_raw(priv, false);
	
	reset_control_assert(priv->de_rst);
	clk_disable_unprepare(priv->clk);
}

static int k5_de_reset(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	
	const caninos_de_ovl_t ovl = {
		.src = { .w = 2U, .h = 2U },
		.dst = { .x = 0U, .y = 0U, .w = 2U, .h = 2U }
	};
	
	/* default state */
	hw->plane.enabled    = false;
	hw->plane.fb_paddr   = 0x0;
	hw->plane.color_mode = DRM_FORMAT_XRGB8888;
	hw->plane.stride     = 8U;
	hw->plane.rotation   = 0U;
	hw->plane.ovl        = ovl;
	hw->path.enabled     = false;
	hw->path.bk_color    = 0x0;
	hw->path.width       = 0x2;
	hw->path.height      = 0x2;
	
	/* update device state */
	caninos_de_plane_set_enable_raw(priv, hw->plane.enabled);
	caninos_de_path_set_enable_raw(priv, hw->path.enabled);
	caninos_de_path_set_bk_color_raw(priv, hw->path.bk_color);
	caninos_de_path_set_sz_raw(priv, hw->path.width, hw->path.height);
	caninos_de_plane_fmt_raw(priv, hw->plane.color_mode);
	caninos_de_plane_stride_raw(priv, hw->plane.stride);
	caninos_de_plane_rotate_raw(priv, hw->plane.rotation);
	caninos_de_plane_scaling_raw(priv, &hw->plane.ovl);
	caninos_de_plane_fbaddr_raw(priv, (u32)hw->plane.fb_paddr);
	return 0;
}

static int k5_de_init(caninos_de_hw_t *hw)
{
	k5_de_reset(hw);
	return 0;
}

static void k5_de_fini(caninos_de_hw_t *hw)
{
	return;
}

static bool k5_de_path_check_config(caninos_de_hw_t *hw, u32 w, u32 h, u32 bk)
{
	return (w >= 2U) && (h >= 2U) && (w <= 4096U) && (h <= 4096U);
}

static bool k5_de_plane_check_rotation(caninos_de_hw_t *hw, u32 rotation)
{
	return (rotation <= 0x3);
}

static bool k5_de_plane_check_overlay(caninos_de_hw_t *hw,
                                      const caninos_de_ovl_t *ovl)
{
	bool valid = (ovl->dst.x >= 0U) && (ovl->dst.y <= 4095U);
	valid = valid && (ovl->dst.w >= 2U) && (ovl->dst.h >= 2U);
	valid = valid && (ovl->src.w >= 2U) && (ovl->src.h >= 2U);
	valid = valid && (ovl->dst.w <= 4096U) && (ovl->dst.h <= 4096U);
	valid = valid && (ovl->src.w <= 4096U) && (ovl->src.h <= 4096U);
	return valid;
}

static bool k5_de_plane_check_fb_addr(caninos_de_hw_t *hw, phys_addr_t paddr)
{
	return (paddr <= 0xFFFFFFFF) && (paddr > 0x0);
}

static bool k5_de_plane_check_format(caninos_de_hw_t *hw, u32 color_mode)
{
	switch (color_mode)
	{
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_XBGR8888:
		return true;
	default:
		return false;
	}
}

static bool k5_de_plane_check_stride(caninos_de_hw_t *hw, u32 stride)
{
	return !(stride % 8U) && (stride >= 8U);
}

static void k5_de_plane_rotate(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	caninos_de_plane_rotate_raw(priv, hw->plane.rotation);
}

static void k5_de_plane_format_set(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	caninos_de_plane_fmt_raw(priv, hw->plane.color_mode);
}

static void k5_de_plane_stride_set(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	caninos_de_plane_stride_raw(priv, hw->plane.stride);
}

static void k5_de_plane_overlay_set(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	caninos_de_plane_scaling_raw(priv, &hw->plane.ovl);
}

static void k5_de_plane_fb_addr_set(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	caninos_de_plane_fbaddr_raw(priv, (u32)hw->plane.fb_paddr);
}

static void k5_de_plane_enable(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	caninos_de_plane_set_enable_raw(priv, true);
}

static void k5_de_plane_disable(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	caninos_de_plane_set_enable_raw(priv, false);
}

static void k5_de_path_set_go(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	caninos_de_path_set_go_raw(priv);
}

static void k5_de_path_enable(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	caninos_de_path_set_sz_raw(priv, hw->path.width, hw->path.height);
	caninos_de_path_set_bk_color_raw(priv, hw->path.bk_color);
	caninos_de_path_set_enable_raw(priv, true);
}

static void k5_de_path_disable(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	struct caninos_gfx *priv = de->parent;
	caninos_de_path_set_enable_raw(priv, false);
}

static const struct caninos_de_ops k5_de_ops = {
	.init = k5_de_init,
	.fini = k5_de_fini,
	.reset = k5_de_reset,
	.power_on = k5_de_power_on,
	.power_off = k5_de_power_off,
	.path_disable = k5_de_path_disable,
	.path_enable = k5_de_path_enable,
	.path_set_go = k5_de_path_set_go,
	.path_check_config = k5_de_path_check_config,
	.plane_enable = k5_de_plane_enable,
	.plane_disable = k5_de_plane_disable,
	.plane_rotate = k5_de_plane_rotate,
	.plane_format_set = k5_de_plane_format_set,
	.plane_stride_set = k5_de_plane_stride_set,
	.plane_overlay_set = k5_de_plane_overlay_set,
	.plane_fb_addr_set = k5_de_plane_fb_addr_set,
	.plane_check_overlay = k5_de_plane_check_overlay,
	.plane_check_rotation = k5_de_plane_check_rotation,
	.plane_check_format = k5_de_plane_check_format,
	.plane_check_stride = k5_de_plane_check_stride,
	.plane_check_fb_addr = k5_de_plane_check_fb_addr,
};

caninos_de_hw_t *caninos_de_k5_init(struct caninos_gfx *parent)
{
	caninos_de_k5_t *de;
	caninos_de_hw_t *hw;
	int ret;
	
	if (!parent) {
		return ERR_PTR(-EINVAL);
	}
	
	de = kzalloc(sizeof(*de), GFP_KERNEL);
	
	if (!de) {
		return ERR_PTR(-ENOMEM);
	}
	
	de->parent = parent;
	hw = &de->hw;
	
	ret = caninos_de_init(hw, &k5_de_ops);
	
	if (ret) {
		kfree(de);
		return ERR_PTR(ret);
	}
	return hw;
}

void caninos_de_k5_fini(caninos_de_hw_t *hw)
{
	caninos_de_k5_t *de = caninos_de_hw_to_k5(hw);
	
	if (de) {
		caninos_de_fini(hw);
		kfree(de);
	}
}

