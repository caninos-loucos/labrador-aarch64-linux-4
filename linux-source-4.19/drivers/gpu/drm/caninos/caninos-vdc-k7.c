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

#define DE_OUTPUT_CON_HDMI (0x0)

#define DE_PATH1   (0x0)
#define DE_PATH2   (0x1)
#define DE_PLANE1  (0x0)
#define DE_PLANE2  (0x1)
#define DE_PLANE3  (0x2)
#define DE_PLANE4  (0x3)
#define DE_LAYER1  (0x0)
#define DE_LAYER2  (0x1)
#define DE_LAYER3  (0x2)
#define DE_LAYER4  (0x3)
#define DE_SCALER1 (0x0)
#define DE_SCALER2 (0x1)

#define DE_IRQENABLE        (0x0000)
#define DE_IRQSTATUS        (0x0004)
#define DE_MMU_EN           (0x0008)
#define DE_MMU_BASE         (0x000c)
#define DE_QOS              (0x0014)
#define DE_IF_CON           (0x0018)
#define DE_PATH_BASE        (0x0100)
#define DE_PLANE_BASE       (0x0400)
#define DE_SCALER_BASE      (0x0C00)
#define DE_OUTPUT_CON       (0x1000)
#define DE_OUTPUT_STAT      (0x1004)
#define DE_PATH_CTL(x)      (DE_PATH_BASE   + (x) * 0x100 + 0x00)
#define DE_PATH_FCR(x)      (DE_PATH_BASE   + (x) * 0x100 + 0x04)
#define DE_PATH_EN(x)       (DE_PATH_BASE   + (x) * 0x100 + 0x08)
#define DE_PATH_BK(x)       (DE_PATH_BASE   + (x) * 0x100 + 0x0c)
#define DE_PATH_SIZE(x)     (DE_PATH_BASE   + (x) * 0x100 + 0x10)
#define DE_PATH_COOR(x,y)   (DE_PATH_BASE   + (x) * 0x100 + (y) * 0x4 + 0x20)
#define DE_PLANE_CFG(y)     (DE_PLANE_BASE  + (y) * 0x200 + 0x00)
#define DE_PLANE_ISIZE(y)   (DE_PLANE_BASE  + (y) * 0x200 + 0x04)
#define DE_PLANE_CSC(y)     (DE_PLANE_BASE  + (y) * 0x200 + 0x14)
#define DE_PLANE_BK(y)      (DE_PLANE_BASE  + (y) * 0x200 + 0x1c)
#define DE_LAYER_CFG(y,z)   (DE_PLANE_BASE  + (y) * 0x200 + 0x20 + (z) * 0x80)
#define DE_LAYER_COOR(y,z)  (DE_PLANE_BASE  + (y) * 0x200 + 0x28 + (z) * 0x80)
#define DE_LAYER_FB(y,z)    (DE_PLANE_BASE  + (y) * 0x200 + 0x2c + (z) * 0x80)
#define DE_LAYER_STR(y,z)   (DE_PLANE_BASE  + (y) * 0x200 + 0x44 + (z) * 0x80)
#define DE_LAYER_CROP(y,z)  (DE_PLANE_BASE  + (y) * 0x200 + 0x48 + (z) * 0x80)
#define DE_SCALER_CFG(x)    (DE_SCALER_BASE + (x) * 0x080 + 0x00)
#define DE_SCALER_OSIZE(x)  (DE_SCALER_BASE + (x) * 0x080 + 0x04)
#define DE_SCALER_HSR(x)    (DE_SCALER_BASE + (x) * 0x080 + 0x08)
#define DE_SCALER_VSR(x)    (DE_SCALER_BASE + (x) * 0x080 + 0x0C)
#define DE_SCALER_SCOEF0(x) (DE_SCALER_BASE + (x) * 0x080 + 0x20)
#define DE_SCALER_SCOEF1(x) (DE_SCALER_BASE + (x) * 0x080 + 0x24)
#define DE_SCALER_SCOEF2(x) (DE_SCALER_BASE + (x) * 0x080 + 0x28)
#define DE_SCALER_SCOEF3(x) (DE_SCALER_BASE + (x) * 0x080 + 0x2C)
#define DE_SCALER_SCOEF4(x) (DE_SCALER_BASE + (x) * 0x080 + 0x30)
#define DE_SCALER_SCOEF5(x) (DE_SCALER_BASE + (x) * 0x080 + 0x34)
#define DE_SCALER_SCOEF6(x) (DE_SCALER_BASE + (x) * 0x080 + 0x38)
#define DE_SCALER_SCOEF7(x) (DE_SCALER_BASE + (x) * 0x080 + 0x3C)

#define DE_PATH_CTL_PLANE_EN_MSK    (GENMASK(3, 0))
#define DE_PATH_CTL_PLANE_EN(n)     (BIT(n) & DE_PATH_CTL_PLANE_EN_MSK)
#define DE_PATH_CTL_ILACE_EN        (BIT(9))
#define DE_PATH_CTL_YUV_EN          (BIT(13))
#define DE_PATH_EN_ENABLE           (BIT(0))
#define DE_PATH_FCR_SET_GO          (BIT(0))
#define DE_PLANE_CFG_LAYER_EN_MSK   (GENMASK(3, 0))
#define DE_PLANE_CFG_LAYER_EN(n)    (BIT(n))
#define DE_LAYER_CFG_FLIP_MSK       (GENMASK(21, 20))
#define DE_LAYER_CFG_FLIP(n)        ((n << 20) & DE_LAYER_CFG_FLIP_MSK)
#define DE_LAYER_CFG_FMT_MSK        (GENMASK(4, 0))
#define DE_LAYER_CFG_FMT(n)         ((n) & DE_LAYER_CFG_FMT_MSK)
#define DE_LAYER_CFG_DATA_MODE      (BIT(16))
#define DE_LAYER_CFG_DATA_ALPHA_MSK (GENMASK(15, 8))
#define DE_LAYER_CFG_DATA_ALPHA(n)  (((n) << 8) & DE_LAYER_CFG_DATA_ALPHA_MSK)
#define DE_SCALER_CFG_ENABLE        (BIT(0))
#define DE_SCALER_CFG_4K_SINGLE     (BIT(2))
#define DE_SCALER_CFG_RGB_LINE      (BIT(3))
#define DE_SCALER_CFG_PLANE_MSK     (GENMASK(5, 4))
#define DE_SCALER_CFG_PLANE(n)      ((n << 4) & DE_SCALER_CFG_PLANE_MSK)
#define DE_OUTPUT_CON_PATH1_DEV_MSK (GENMASK(1, 0))
#define DE_OUTPUT_CON_PATH1_DEV(x)  ((x) & DE_OUTPUT_CON_PATH1_DEV_MSK)
#define DE_OUTPUT_CON_PATH2_DEV_MSK (GENMASK(5, 4))
#define DE_OUTPUT_CON_PATH2_DEV(x)  (((x) << 4) & DE_OUTPUT_CON_PATH2_DEV_MSK)
#define DE_IRQENABLE_HDMI           (0x1)
#define DE_IRQSTATUS_HDMI_PRE       (0x1)

static void k7_enable_path_raw(struct caninos_vdc *priv, u32 path)
{
	const u32 msk = DE_PATH_EN_ENABLE;
	u32 val = readl(priv->base + DE_PATH_EN(path));
	writel(val | msk, priv->base + DE_PATH_EN(path));
}

static void k7_disable_path_raw(struct caninos_vdc *priv, u32 path)
{
	const u32 msk = DE_PATH_EN_ENABLE;
	u32 val = readl(priv->base + DE_PATH_EN(path));
	writel(val & ~msk, priv->base + DE_PATH_EN(path));
}

static bool k7_is_path_en_raw(struct caninos_vdc *priv, u32 path)
{
	const u32 msk = DE_PATH_EN_ENABLE;
	return ((readl(priv->base + DE_PATH_EN(path)) & msk) == msk);
}

static void
k7_plane_init_scaler_raw(struct caninos_vdc *priv, u32 factor, u32 hsr, u32 vsr)
{
	const u32 msk = DE_SCALER_CFG_ENABLE | DE_SCALER_CFG_PLANE_MSK;
	u32 val;
	
	/* enable first scaler and connect it to first plane */
	val = readl(priv->base + DE_SCALER_CFG(DE_SCALER1)) & ~msk;
	val |= DE_SCALER_CFG_ENABLE | DE_SCALER_CFG_PLANE(DE_PLANE1);
	writel(val, priv->base + DE_SCALER_CFG(DE_SCALER1));
	
	/* disable second scaler */
	val = readl(priv->base + DE_SCALER_CFG(DE_SCALER2)) & ~msk;
	writel(val, priv->base + DE_SCALER_CFG(DE_SCALER2));
	
	/* configure scaler */
	writel(hsr, priv->base + DE_SCALER_HSR(DE_SCALER1));
	writel(vsr, priv->base + DE_SCALER_VSR(DE_SCALER1));
	
	if (factor <= 10U) {
		writel(0x00004000, priv->base + DE_SCALER_SCOEF0(DE_SCALER1));
		writel(0xFF073EFC, priv->base + DE_SCALER_SCOEF1(DE_SCALER1));
		writel(0xFE1038FA, priv->base + DE_SCALER_SCOEF2(DE_SCALER1));
		writel(0xFC1B30F9, priv->base + DE_SCALER_SCOEF3(DE_SCALER1));
		writel(0xFA2626FA, priv->base + DE_SCALER_SCOEF4(DE_SCALER1));
		writel(0xF9301BFC, priv->base + DE_SCALER_SCOEF5(DE_SCALER1));
		writel(0xFA3810FE, priv->base + DE_SCALER_SCOEF6(DE_SCALER1));
		writel(0xFC3E07FF, priv->base + DE_SCALER_SCOEF7(DE_SCALER1));       
	}
	else if (factor <= 20U) {
		writel(0x00004000, priv->base + DE_SCALER_SCOEF0(DE_SCALER1));
		writel(0x00083800, priv->base + DE_SCALER_SCOEF1(DE_SCALER1));
		writel(0x00103000, priv->base + DE_SCALER_SCOEF2(DE_SCALER1));
		writel(0x00182800, priv->base + DE_SCALER_SCOEF3(DE_SCALER1));
		writel(0x00202000, priv->base + DE_SCALER_SCOEF4(DE_SCALER1));
		writel(0x00281800, priv->base + DE_SCALER_SCOEF5(DE_SCALER1));
		writel(0x00301000, priv->base + DE_SCALER_SCOEF6(DE_SCALER1));
		writel(0x00380800, priv->base + DE_SCALER_SCOEF7(DE_SCALER1));       
	}
	else {
		writel(0x00102010, priv->base + DE_SCALER_SCOEF0(DE_SCALER1));
		writel(0x02121E0E, priv->base + DE_SCALER_SCOEF1(DE_SCALER1));
		writel(0x04141C0C, priv->base + DE_SCALER_SCOEF2(DE_SCALER1));
		writel(0x06161A0A, priv->base + DE_SCALER_SCOEF3(DE_SCALER1));
		writel(0x08181808, priv->base + DE_SCALER_SCOEF4(DE_SCALER1));
		writel(0x0A1A1606, priv->base + DE_SCALER_SCOEF5(DE_SCALER1));
		writel(0x0C1C1404, priv->base + DE_SCALER_SCOEF6(DE_SCALER1));
		writel(0x0E1E1202, priv->base + DE_SCALER_SCOEF7(DE_SCALER1));
	}
}

static void
k7_plane_set_scaling_raw(struct caninos_vdc *priv, u32 src_w, u32 src_h,
                         u32 dst_x, u32 dst_y, u32 dst_w, u32 dst_h)
{
	u32 factor, hsr, vsr, val;
	
	factor = (src_w * src_h * 10U) / (dst_w * dst_h);
	
	hsr = (src_w * DE_SCALE_CONST_VALUE + dst_w - 1U) / dst_w;
	vsr = (src_h * DE_SCALE_CONST_VALUE + dst_h - 1U) / dst_h;
	
	k7_plane_init_scaler_raw(priv, factor, hsr, vsr);
	
	writel(0x0, priv->base + DE_LAYER_COOR(DE_PLANE1, DE_LAYER1));
	
	val = ((src_h - 1U) << 16) | (src_w - 1U);
	writel(val, priv->base + DE_PLANE_ISIZE(DE_PLANE1));
	
	val = ((dst_h - 1U) << 16) | (dst_w - 1U);
	writel(val, priv->base + DE_SCALER_OSIZE(DE_SCALER1));
	writel(val, priv->base + DE_LAYER_CROP(DE_PLANE1, DE_LAYER1));
	
	val = (dst_y << 16) | dst_x;
	writel(val, priv->base + DE_PATH_COOR(DE_PATH1, DE_PLANE1));
}

static void k7_plane_set_rotate_raw(struct caninos_vdc *priv, u32 rotation)
{
	u32 val;
	val = readl(priv->base + DE_LAYER_CFG(DE_PLANE1, DE_LAYER1));
	val &= ~DE_LAYER_CFG_FLIP_MSK;
	val |= DE_LAYER_CFG_FLIP(rotation);
	writel(val, priv->base + DE_LAYER_CFG(DE_PLANE1, DE_LAYER1));
}

static void k7_path_set_size_raw(struct caninos_vdc *priv, u32 w, u32 h)
{
	u32 val = (((h - 1U) & 0xFFF) << 16) | ((w - 1U) & 0xFFF);
	writel(val, priv->base + DE_PATH_SIZE(DE_PATH1));
}

static bool k7_de_is_enabled(struct caninos_vdc *priv)
{
	bool enabled = false;
	enabled = enabled || k7_is_path_en_raw(priv, DE_PATH1);
	enabled = enabled || k7_is_path_en_raw(priv, DE_PATH2);
	return enabled;
}

static void k7_de_disable(struct caninos_vdc *priv)
{
	k7_disable_path_raw(priv, DE_PATH1);
	k7_disable_path_raw(priv, DE_PATH2);
}

static void k7_de_enable(struct caninos_vdc *priv)
{
	u32 val;
	
	/* connect plane1 to path1, other planes and paths are not used */
	val = readl(priv->base + DE_PATH_CTL(DE_PATH1));
	val &= ~DE_PATH_CTL_PLANE_EN_MSK;
	val |= DE_PATH_CTL_PLANE_EN(DE_PLANE1);
	writel(val, priv->base + DE_PATH_CTL(DE_PATH1));
	
	/* connect layer1 to path1, other layers are not used */
	val = readl(priv->base + DE_PLANE_CFG(DE_PLANE1));
	val &= ~DE_PLANE_CFG_LAYER_EN_MSK;
	val |= DE_PLANE_CFG_LAYER_EN(DE_LAYER1);
	writel(val, priv->base + DE_PLANE_CFG(DE_PLANE1));
	
	k7_enable_path_raw(priv, DE_PATH1);
}

static bool k7_de_set_go(struct caninos_vdc *priv)
{
	u32 val;
	val = readl(priv->base + DE_PATH_FCR(DE_PATH1));
	val |= DE_PATH_FCR_SET_GO;
	writel(val, priv->base + DE_PATH_FCR(DE_PATH1));
	return true;
}

static void k7_de_set_size(struct caninos_vdc *priv, u32 w, u32 h)
{
	k7_path_set_size_raw(priv, w, h);
	k7_plane_set_rotate_raw(priv, 0x0);
	k7_plane_set_scaling_raw(priv, w, h, 0x0, 0x0, w, h);
}

static void k7_de_set_format(struct caninos_vdc *priv, u32 format)
{
	u32 val;
	val = readl(priv->base + DE_LAYER_CFG(DE_PLANE1, DE_LAYER1));
	val &= ~(DE_LAYER_CFG_FMT_MSK | DE_LAYER_CFG_DATA_MODE);
	val &= ~(DE_LAYER_CFG_DATA_ALPHA_MSK);
	
	switch (format)
	{
	case DRM_FORMAT_ARGB8888:
		val |= DE_LAYER_CFG_FMT(0x0) | DE_LAYER_CFG_DATA_MODE;
		break;
		
	case DRM_FORMAT_XRGB8888:
		val |= DE_LAYER_CFG_FMT(0x0) | DE_LAYER_CFG_DATA_ALPHA(0xff);
		break;
		
	case DRM_FORMAT_ABGR8888:
		val |= DE_LAYER_CFG_FMT(0x1) | DE_LAYER_CFG_DATA_MODE;
		break;
		
	case DRM_FORMAT_XBGR8888:
		val |= DE_LAYER_CFG_FMT(0x1) | DE_LAYER_CFG_DATA_ALPHA(0xff);
		break;
		
	case DRM_FORMAT_RGBA8888:
		val |= DE_LAYER_CFG_FMT(0x2) | DE_LAYER_CFG_DATA_MODE;
		break;
		
	case DRM_FORMAT_RGBX8888:
		val |= DE_LAYER_CFG_FMT(0x2) | DE_LAYER_CFG_DATA_ALPHA(0xff);
		break;
		
	case DRM_FORMAT_BGRA8888:
		val |= DE_LAYER_CFG_FMT(0x3) | DE_LAYER_CFG_DATA_MODE;
		break;
		
	case DRM_FORMAT_BGRX8888:
		val |= DE_LAYER_CFG_FMT(0x3) | DE_LAYER_CFG_DATA_ALPHA(0xff);
		break;
		
	default:
		return;
	}
	
	writel(val, priv->base + DE_LAYER_CFG(DE_PLANE1, DE_LAYER1));
}

static void k7_de_set_stride(struct caninos_vdc *priv, u32 stride)
{
	writel(stride, priv->base + DE_LAYER_STR(DE_PLANE1, DE_LAYER1));
}

static void k7_de_set_framebuffer(struct caninos_vdc *priv, u32 fbaddr)
{
	writel(fbaddr, priv->base + DE_LAYER_FB(DE_PLANE1, DE_LAYER1));
}

static void k7_de_disable_irqs(struct caninos_vdc *priv)
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

static void k7_de_enable_irqs(struct caninos_vdc *priv)
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

static bool k7_handle_irqs(struct caninos_vdc *priv)
{
	u32 val;
	val = readl(priv->base + DE_IRQSTATUS);
	writel(val, priv->base + DE_IRQSTATUS);
	
	/* flush status */
	readl(priv->base + DE_IRQSTATUS);
	
	return !!(val & DE_IRQSTATUS_HDMI_PRE);
}

static int k7_de_init(struct caninos_vdc *priv)
{
	u32 val;
	
	/* check if the controller was already enabled by the bootloader */
	if (k7_de_is_enabled(priv))
	{
		/* ensure interrupts are disabled */
		k7_de_disable_irqs(priv);
		/*
		 * the hdmi controller driver has already disabled the hdmi output
		 * it should be safe to disable all display paths
		 */
		k7_de_disable(priv);
	}
	else {
		/* TODO: configure parent clock before enabling controller clock */
		/* TODO: configure dcu */
	}
	
	/*
	 * enable the controller
	 * or just sanitize clk/reset driver state if already enabled
	 */
	reset_control_deassert(priv->rst);
	clk_prepare_enable(priv->clk);
	
	/*
	 * disable all paths and disconnect all planes
	 * also disable interlace, and YUV
	 */
	writel(0x0, priv->base + DE_PATH_CTL(DE_PATH1));
	writel(0x0, priv->base + DE_PATH_CTL(DE_PATH2));
	
	/* special outstanding (whatever it is)*/
	writel(0x3f, priv->base + DE_IF_CON);
	writel(0x0f, priv->base + DE_QOS);
	
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

static void k7_de_fini(struct caninos_vdc *priv)
{
	k7_de_disable_irqs(priv);
	k7_de_disable(priv);
	clk_disable_unprepare(priv->clk);
}

const struct de_hw_ops de_k7_ops = {
	.fini = k7_de_fini,
	.init = k7_de_init,
	.enable_irqs = k7_de_enable_irqs,
	.disable_irqs = k7_de_disable_irqs,
	.handle_irqs = k7_handle_irqs,
	.is_enabled = k7_de_is_enabled,
	.disable = k7_de_disable,
	.enable = k7_de_enable,
	.set_size = k7_de_set_size,
	.set_go = k7_de_set_go,
	.set_format = k7_de_set_format,
	.set_stride = k7_de_set_stride,
	.set_framebuffer = k7_de_set_framebuffer,
};

