// SPDX-License-Identifier: GPL-2.0
/*
 * Video Display Controller Driver for Caninos Labrador
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

#include "caninos-vdc-priv.h"

#define DE_SCALE_CONST_VALUE (8192U)

#define DE_OUTPUT_CON_HDMI (0x2)

#define DE_PATH1 (0x0)
#define DE_PATH2 (0x1)

#define DE_PLANE1 (0x0)
#define DE_PLANE2 (0x1)
#define DE_PLANE3 (0x2)
#define DE_PLANE4 (0x3)

#define DE_IRQENABLE        (0x0000)
#define DE_IRQSTATUS        (0x0004)
#define DE_IF_CON           (0x000c)
#define DE_MMU_EN           (0x0010)
#define DE_MMU_BASE         (0x0014)
#define DE_PATH_BASE        (0x0100)
#define DE_PATH_DITHER      (0x0150)
#define DE_PLANE_BASE       (0x0400)
#define DE_OUTPUT_CON       (0x1000)
#define DE_WB_CON           (0x1004)
#define DE_WB_ADDR          (0x1008)
#define DE_OUTPUT_STAT      (0x100c)
#define DE_PATH_CTL(x)      (DE_PATH_BASE  + (x) * 0x100 + 0x00)
#define DE_PATH_BK(x)       (DE_PATH_BASE  + (x) * 0x100 + 0x20)
#define DE_PATH_SIZE(x)     (DE_PATH_BASE  + (x) * 0x100 + 0x24)
#define DE_PLANE_CFG(y)     (DE_PLANE_BASE + (y) * 0x100 + 0x00)
#define DE_PLANE_ISIZE(y)   (DE_PLANE_BASE + (y) * 0x100 + 0x04)
#define DE_PLANE_OSIZE(y)   (DE_PLANE_BASE + (y) * 0x100 + 0x08)
#define DE_PLANE_SR(y)      (DE_PLANE_BASE + (y) * 0x100 + 0x0c)
#define DE_PLANE_SCOEF0(y)  (DE_PLANE_BASE + (y) * 0x100 + 0x10)
#define DE_PLANE_SCOEF1(y)  (DE_PLANE_BASE + (y) * 0x100 + 0x14)
#define DE_PLANE_SCOEF2(y)  (DE_PLANE_BASE + (y) * 0x100 + 0x18)
#define DE_PLANE_SCOEF3(y)  (DE_PLANE_BASE + (y) * 0x100 + 0x1c)
#define DE_PLANE_SCOEF4(y)  (DE_PLANE_BASE + (y) * 0x100 + 0x20)
#define DE_PLANE_SCOEF5(y)  (DE_PLANE_BASE + (y) * 0x100 + 0x24)
#define DE_PLANE_SCOEF6(y)  (DE_PLANE_BASE + (y) * 0x100 + 0x28)
#define DE_PLANE_SCOEF7(y)  (DE_PLANE_BASE + (y) * 0x100 + 0x2c)
#define DE_PLANE_FB0(y)     (DE_PLANE_BASE + (y) * 0x100 + 0x30)
#define DE_PLANE_ALPHA(y)   (DE_PLANE_BASE + (y) * 0x100 + 0x58)
#define DE_PLANE_BLEND(y)   (DE_PLANE_BASE + (y) * 0x100 + 0x64)
#define DE_PLANE_COOR(y)    (DE_PLANE_BASE + (y) * 0x100 + 0x54)
#define DE_PLANE_STR(y)     (DE_PLANE_BASE + (y) * 0x100 + 0x48)

#define DE_PATH_CTL_PLANES_MSK      (GENMASK(23, 20))
#define DE_PATH_CTL_PLANES_SET(n)   ((n << 20) & DE_PATH_CTL_PLANES_MSK)
#define DE_PATH_CTL_PLANE1          (BIT(0))
#define DE_PATH_CTL_PLANE2          (BIT(1))
#define DE_PATH_CTL_PLANE3          (BIT(2))
#define DE_PATH_CTL_PLANE4          (BIT(3))
#define DE_PATH_CTL_DITHER_EN       (BIT(8))
#define DE_PATH_CTL_GAMMA_EN        (BIT(9))
#define DE_PATH_CTL_3D_EN           (BIT(10))
#define DE_PATH_CTL_ILACE_EN        (BIT(11))
#define DE_PATH_CTL_YUV_EN          (BIT(16))
#define DE_PATH_CTL_PATH_ENABLE     (BIT(28))
#define DE_PATH_CTL_SET_GO          (BIT(29))
#define DE_PLANE_CFG_FLIP_MSK       (GENMASK(21, 20))
#define DE_PLANE_CFG_FLIP(n)        ((n << 20) & DE_PLANE_CFG_FLIP_MSK)
#define DE_PLANE_CFG_CRITICAL_MSK   (GENMASK(27, 26))
#define DE_PLANE_CFG_CRITICAL(n)    ((n << 26) & DE_PLANE_CFG_CRITICAL_MSK)
#define DE_PLANE_CFG_CRITICAL_OFF   (0x2)
#define DE_PLANE_CFG_FMT_MSK        (GENMASK(2, 0) | GENMASK(31, 28))
#define DE_PLANE_CFG_FMT(n)         ((n) & DE_PLANE_CFG_FMT_MSK)
#define DE_PLANE_CFG_FMT_RGB565     (0x0 | BIT(29))
#define DE_PLANE_CFG_FMT_ARGB8888   (0x1 | BIT(29))
#define DE_PLANE_CFG_FMT_RGBA8888   (0x1 | GENMASK(31, 29))
#define DE_PLANE_CFG_FMT_ABGR8888   (0x5 | BIT(29))
#define DE_PLANE_CFG_FMT_BGRA8888   (0x5 | GENMASK(31, 29))
#define DE_PLANE_BLEND_ALPHA_EN     (BIT(0))
#define DE_PLANE_BLEND_CKEY_EN      (BIT(1))
#define DE_PLANE_ALPHA_VAL_MSK      (GENMASK(7, 0))
#define DE_PLANE_ALPHA_VAL(x)       ((x) & DE_PLANE_ALPHA_VAL_MSK)
#define DE_OUTPUT_CON_PATH1_DEV_MSK (GENMASK(2, 0))
#define DE_OUTPUT_CON_PATH1_DEV(x)  ((x) & DE_OUTPUT_CON_PATH1_DEV_MSK)
#define DE_OUTPUT_CON_PATH2_DEV_MSK (GENMASK(6, 4))
#define DE_OUTPUT_CON_PATH2_DEV(x)  (((x) << 4) & DE_OUTPUT_CON_PATH2_DEV_MSK)
#define DE_IRQENABLE_LCD            (BIT(0))
#define DE_IRQENABLE_DSI            (BIT(2))
#define DE_IRQENABLE_CVBS           (BIT(3))
#define DE_IRQENABLE_HDMI           (BIT(4))
#define DE_IRQSTATUS_CVBS_PRE       (BIT(0))
#define DE_IRQSTATUS_DSI_PRE        (BIT(1))
#define DE_IRQSTATUS_HDMI_PRE       (BIT(2))
#define DE_IRQSTATUS_LCD_PRE        (BIT(3))
#define DE_IRQSTATUS_LCD2_PRE       (BIT(4))
#define DE_IRQSTATUS_CVBS_VB        (BIT(8))
#define DE_IRQSTATUS_DSI_VB         (BIT(9))
#define DE_IRQSTATUS_HDMI_VB        (BIT(10))
#define DE_IRQSTATUS_LCD_VB         (BIT(11))
#define DE_IRQSTATUS_LCD2_VB        (BIT(12))

static void k5_disable_irqs_raw(struct caninos_vdc *priv)
{
	u32 val;
	
	/* disable irqs */
	writel(0x0, priv->base + DE_IRQENABLE);
	
	/* clear pending irqs */
	val = readl(priv->base + DE_IRQSTATUS);
	writel(val, priv->base + DE_IRQSTATUS);
	
	/* flush status */
	readl(priv->base + DE_IRQSTATUS);
}

static void k5_enable_irqs_raw(struct caninos_vdc *priv)
{
	u32 val;
	
	/* clear pending irqs */
	val = readl(priv->base + DE_IRQSTATUS);
	writel(val, priv->base + DE_IRQSTATUS);
	
	/* flush status */
	readl(priv->base + DE_IRQSTATUS);
	
	/* enable HDMI irq */
	writel(DE_IRQENABLE_HDMI, priv->base + DE_IRQENABLE);
}

static bool k5_path_set_go_raw(struct caninos_vdc *priv, u32 path)
{
	u32 val;
	
	/* set go */
	val = readl(priv->base + DE_PATH_CTL(path));
	val |= DE_PATH_CTL_SET_GO;
	writel(val, priv->base + DE_PATH_CTL(path));
	
	/* check for fifo underflow */
	val = readl(priv->base + DE_OUTPUT_STAT);
	if (val) {
		writel(val, priv->base + DE_OUTPUT_STAT);
		return false;
	}
	return true;
}

static void
k5_path_set_size_raw(struct caninos_vdc *priv, u32 path, u32 w, u32 h)
{
	u32 val = (((h - 1U) & 0xFFF) << 16) | ((w - 1U) & 0xFFF);
	writel(val, priv->base + DE_PATH_SIZE(path));
}

static void k5_enable_path_raw(struct caninos_vdc *priv, u32 path)
{
	const u32 msk = DE_PATH_CTL_PATH_ENABLE;
	u32 val = readl(priv->base + DE_PATH_CTL(path));
	writel(val | msk, priv->base + DE_PATH_CTL(path));
}

static void k5_disable_path_raw(struct caninos_vdc *priv, u32 path)
{
	const u32 msk = DE_PATH_CTL_PATH_ENABLE;
	u32 val = readl(priv->base + DE_PATH_CTL(path));
	writel(val & ~msk, priv->base + DE_PATH_CTL(path));
}

static bool k5_is_path_en_raw(struct caninos_vdc *priv, u32 path)
{
	const u32 msk = DE_PATH_CTL_PATH_ENABLE;
	return ((readl(priv->base + DE_PATH_CTL(path)) & msk) == msk);
}

static void
k5_plane_set_rotate_raw(struct caninos_vdc *priv, u32 plane, u32 rotation)
{
	u32 val;
	val = readl(priv->base + DE_PLANE_CFG(plane));
	val &= ~DE_PLANE_CFG_FLIP_MSK;
	val |= DE_PLANE_CFG_FLIP(rotation);
	writel(val, priv->base + DE_PLANE_CFG(plane));
}

static void
k5_plane_set_fbaddr_raw(struct caninos_vdc *priv, u32 plane, u32 paddr)
{
	writel(paddr, priv->base + DE_PLANE_FB0(plane));
}

static void
k5_plane_set_stride_raw(struct caninos_vdc *priv, u32 plane, u32 stride)
{
	writel((stride >> 3), priv->base + DE_PLANE_STR(plane));
}

static int
k5_plane_set_format_raw(struct caninos_vdc *priv, u32 plane, u32 fmt)
{
	u32 val1, val2, val3;
	
	val1 = readl(priv->base + DE_PLANE_CFG(plane));
	val2 = readl(priv->base + DE_PLANE_BLEND(plane));
	val3 = readl(priv->base + DE_PLANE_ALPHA(plane));
	
	val1 &= ~(DE_PLANE_CFG_FMT_MSK | DE_PLANE_CFG_CRITICAL_MSK);
	val2 &= ~DE_PLANE_BLEND_ALPHA_EN;
	val3 &= ~DE_PLANE_ALPHA_VAL_MSK;
	
	val1 |= DE_PLANE_CFG_CRITICAL(DE_PLANE_CFG_CRITICAL_OFF);
	
	switch (fmt)
	{
	case DRM_FORMAT_ARGB8888:
		val1 |= DE_PLANE_CFG_FMT(DE_PLANE_CFG_FMT_ARGB8888);
		break;
		
	case DRM_FORMAT_XRGB8888:
		val1 |= DE_PLANE_CFG_FMT(DE_PLANE_CFG_FMT_ARGB8888);
		val2 |= DE_PLANE_BLEND_ALPHA_EN;
		val3 |= DE_PLANE_ALPHA_VAL(0xFF);
		break;
		
	case DRM_FORMAT_RGBA8888:
		val1 |= DE_PLANE_CFG_FMT(DE_PLANE_CFG_FMT_RGBA8888);
		break;
		
	case DRM_FORMAT_RGBX8888:
		val1 |= DE_PLANE_CFG_FMT(DE_PLANE_CFG_FMT_RGBA8888);
		val2 |= DE_PLANE_BLEND_ALPHA_EN;
		val3 |= DE_PLANE_ALPHA_VAL(0xFF);
		break;
		
	case DRM_FORMAT_ABGR8888:
		val1 |= DE_PLANE_CFG_FMT(DE_PLANE_CFG_FMT_ABGR8888);
		break;
		
	case DRM_FORMAT_XBGR8888:
		val1 |= DE_PLANE_CFG_FMT(DE_PLANE_CFG_FMT_ABGR8888);
		val2 |= DE_PLANE_BLEND_ALPHA_EN;
		val3 |= DE_PLANE_ALPHA_VAL(0xFF);
		break;
		
	case DRM_FORMAT_BGRA8888:
		val1 |= DE_PLANE_CFG_FMT(DE_PLANE_CFG_FMT_BGRA8888);
		break;
		
	case DRM_FORMAT_BGRX8888:
		val1 |= DE_PLANE_CFG_FMT(DE_PLANE_CFG_FMT_BGRA8888);
		val2 |= DE_PLANE_BLEND_ALPHA_EN;
		val3 |= DE_PLANE_ALPHA_VAL(0xFF);
		break;
		
	default:
		return -EINVAL;
	}
	
	writel(val1, priv->base + DE_PLANE_CFG(plane));
	writel(val2, priv->base + DE_PLANE_BLEND(plane));
	writel(val3, priv->base + DE_PLANE_ALPHA(plane));
	return 0;
}

static void
k5_plane_init_scaler_raw(struct caninos_vdc *priv,
                         u32 plane, u32 factor, u32 hsr, u32 vsr)
{
	hsr &= 0xFFFF;
	vsr &= 0xFFFF;
	
	writel((vsr << 16) | hsr, priv->base + DE_PLANE_SR(plane));
	
	if (factor <= 1U) {
		writel(0x00400000, priv->base + DE_PLANE_SCOEF0(plane));
		writel(0xFC3E07FF, priv->base + DE_PLANE_SCOEF1(plane));
		writel(0xFA3810FE, priv->base + DE_PLANE_SCOEF2(plane));
		writel(0xF9301BFC, priv->base + DE_PLANE_SCOEF3(plane));
		writel(0xFA2626FA, priv->base + DE_PLANE_SCOEF4(plane));
		writel(0xFC1B30F9, priv->base + DE_PLANE_SCOEF5(plane));
		writel(0xFE1038FA, priv->base + DE_PLANE_SCOEF6(plane));
		writel(0xFF073EFC, priv->base + DE_PLANE_SCOEF7(plane));        
	}
	else if (factor <= 4U) {
		writel(0x00400000, priv->base + DE_PLANE_SCOEF0(plane));
		writel(0x00380800, priv->base + DE_PLANE_SCOEF1(plane));
		writel(0x00301000, priv->base + DE_PLANE_SCOEF2(plane));
		writel(0x00281800, priv->base + DE_PLANE_SCOEF3(plane));
		writel(0x00202000, priv->base + DE_PLANE_SCOEF4(plane));
		writel(0x00182800, priv->base + DE_PLANE_SCOEF5(plane));
		writel(0x00103000, priv->base + DE_PLANE_SCOEF6(plane));
		writel(0x00083800, priv->base + DE_PLANE_SCOEF7(plane));       
	}
	else {
		writel(0x10201000, priv->base + DE_PLANE_SCOEF0(plane));
		writel(0x0E1E1202, priv->base + DE_PLANE_SCOEF1(plane));
		writel(0x0C1C1404, priv->base + DE_PLANE_SCOEF2(plane));
		writel(0x0A1A1606, priv->base + DE_PLANE_SCOEF3(plane));
		writel(0x08181808, priv->base + DE_PLANE_SCOEF4(plane));
		writel(0x06161A0A, priv->base + DE_PLANE_SCOEF5(plane));
		writel(0x04141C0C, priv->base + DE_PLANE_SCOEF6(plane));
		writel(0x02121E0E, priv->base + DE_PLANE_SCOEF7(plane));
	}
}

static void
k5_plane_set_scaling_raw(struct caninos_vdc *priv,
                         u32 plane, u32 src_w, u32 src_h,
                         u32 dst_x, u32 dst_y, u32 dst_w, u32 dst_h)
{
	u32 factor, hsr, vsr, val;
	
	factor = (src_w * src_h) / (dst_w * dst_h);
	
	hsr = (src_w * DE_SCALE_CONST_VALUE + dst_w - 1U) / dst_w;
	vsr = (src_h * DE_SCALE_CONST_VALUE + dst_h - 1U) / dst_h;
	
	k5_plane_init_scaler_raw(priv, plane, factor, hsr, vsr);
	
	val = ((src_h - 1U) << 16) | (src_w - 1U);
	writel(val, priv->base + DE_PLANE_ISIZE(plane));
	
	val = ((dst_h - 1U) << 16) | (dst_w - 1U);
	writel(val, priv->base + DE_PLANE_OSIZE(plane));
	
	val = (dst_y << 16) | dst_x;
	writel(val, priv->base + DE_PLANE_COOR(plane));
}

static void k5_de_disable_irqs(struct caninos_vdc *priv)
{
	k5_disable_irqs_raw(priv);
}

static void k5_de_enable_irqs(struct caninos_vdc *priv)
{
	k5_enable_irqs_raw(priv);
}

static bool k5_de_is_enabled(struct caninos_vdc *priv)
{
	bool enabled = false;
	enabled = enabled || k5_is_path_en_raw(priv, DE_PATH1);
	enabled = enabled || k5_is_path_en_raw(priv, DE_PATH2);
	return enabled;
}

static void k5_de_disable(struct caninos_vdc *priv)
{
	k5_disable_path_raw(priv, DE_PATH1);
	k5_disable_path_raw(priv, DE_PATH2);
}

static void k5_de_enable(struct caninos_vdc *priv)
{
	u32 val;
	
	/* connect plane1 to path1, other planes and paths are not used */
	val = readl(priv->base + DE_PATH_CTL(DE_PATH1));
	val &= ~DE_PATH_CTL_PLANES_MSK;
	val |= DE_PATH_CTL_PLANES_SET(DE_PATH_CTL_PLANE1);
	writel(val, priv->base + DE_PATH_CTL(DE_PATH1));
	
	k5_enable_path_raw(priv, DE_PATH1);
}

static bool k5_de_set_go(struct caninos_vdc *priv)
{
	return k5_path_set_go_raw(priv, DE_PATH1);
}

static void k5_de_set_size(struct caninos_vdc *priv, u32 w, u32 h)
{
	k5_path_set_size_raw(priv, DE_PATH1, w, h);
	k5_plane_set_rotate_raw(priv, DE_PLANE1, 0x0);
	k5_plane_set_scaling_raw(priv, DE_PLANE1, w, h, 0x0, 0x0, w, h);
}

static void k5_de_set_format(struct caninos_vdc *priv, u32 format)
{
	k5_plane_set_format_raw(priv, DE_PLANE1, format);
}

static void k5_de_set_stride(struct caninos_vdc *priv, u32 stride)
{
	k5_plane_set_stride_raw(priv, DE_PLANE1, stride);
}

static void k5_de_set_framebuffer(struct caninos_vdc *priv, u32 fbaddr)
{
	k5_plane_set_fbaddr_raw(priv, DE_PLANE1, fbaddr);
}

static void k5_de_fini(struct caninos_vdc *priv)
{
	k5_de_disable_irqs(priv);
	k5_de_disable(priv);
	clk_disable_unprepare(priv->clk);
}

static int k5_de_init(struct caninos_vdc *priv)
{
	u32 val;
	
	/* check if the controller was already enabled by the bootloader */
	if (k5_de_is_enabled(priv))
	{
		/* ensure interrupts are disabled */
		k5_de_disable_irqs(priv);
		/*
		 * the hdmi controller driver has already disabled the hdmi output
		 * it should be safe to disable all display paths
		 */
		k5_de_disable(priv);
	}
	else {
		/* TODO: configure parent clock before enabling controller clock */
	}
	
	/*
	 * enable the controller
	 * or just sanitize clk/reset driver state if already enabled
	 */
	reset_control_deassert(priv->rst);
	clk_prepare_enable(priv->clk);
	
	/*
	 * disable all paths and disconnect all planes
	 * also disable interlace, set_go, 3d, dither, gamma and YUV
	 */
	writel(0x0, priv->base + DE_PATH_CTL(DE_PATH1));
	writel(0x0, priv->base + DE_PATH_CTL(DE_PATH2));
	
	/* special outstanding (whatever it is)*/
	writel(0x1f0000, priv->base + DE_IF_CON);
	
	/* disable mmu */
	writel(0x0, priv->base + DE_MMU_EN);
	
	/* route both paths to hdmi */
	val  = DE_OUTPUT_CON_PATH1_DEV(DE_OUTPUT_CON_HDMI);
	val |= DE_OUTPUT_CON_PATH2_DEV(DE_OUTPUT_CON_HDMI);
	writel(val, priv->base + DE_OUTPUT_CON);
	
	/* set background color of both paths */
	writel(0x0, priv->base + DE_PATH_BK(DE_PATH1));
	writel(0x0, priv->base + DE_PATH_BK(DE_PATH2));
	return 0;
}

static bool k5_handle_irqs(struct caninos_vdc *priv)
{
	u32 val;
	val = readl(priv->base + DE_IRQSTATUS);
	writel(val, priv->base + DE_IRQSTATUS);
	
	/* flush status */
	readl(priv->base + DE_IRQSTATUS);
	
	return !!(val & DE_IRQSTATUS_HDMI_PRE);
}

const struct de_hw_ops de_k5_ops = {
	.fini = k5_de_fini,
	.init = k5_de_init,
	.enable_irqs = k5_de_enable_irqs,
	.disable_irqs = k5_de_disable_irqs,
	.handle_irqs = k5_handle_irqs,
	.is_enabled = k5_de_is_enabled,
	.disable = k5_de_disable,
	.enable = k5_de_enable,
	.set_size = k5_de_set_size,
	.set_go = k5_de_set_go,
	.set_format = k5_de_set_format,
	.set_stride = k5_de_set_stride,
	.set_framebuffer = k5_de_set_framebuffer,
};

