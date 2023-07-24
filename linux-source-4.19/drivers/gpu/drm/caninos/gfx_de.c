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

#define SAFE_READ(c) ({ typeof(c) __v = READ_ONCE(c); rmb(); __v; })
#define SAFE_WRITE(c,v) ({ wmb(); WRITE_ONCE(c,v); })

static inline bool hw_inited(const caninos_de_hw_t *hw)
{
	return (hw != NULL) && SAFE_READ(hw->inited);
}

static inline bool ovl_eq(const caninos_de_ovl_t *a, const caninos_de_ovl_t *b)
{
	bool equal = true;
	equal = equal && (a->dst.x == b->dst.x) && (a->dst.y == b->dst.y);
	equal = equal && (a->dst.w == b->dst.w) && (a->dst.h == b->dst.h);
	equal = equal && (a->src.w == b->src.w) && (a->src.h == b->src.h);
	return equal;
}

static inline bool check_ops(const caninos_de_ops_t *ops)
{
	bool valid = true;
	valid = valid && (ops->init                 != NULL);
	valid = valid && (ops->fini                 != NULL);
	valid = valid && (ops->reset                != NULL);
	valid = valid && (ops->power_on             != NULL);
	valid = valid && (ops->power_off            != NULL);
	valid = valid && (ops->path_disable         != NULL);
	valid = valid && (ops->path_enable          != NULL);
	valid = valid && (ops->path_set_go          != NULL);
	valid = valid && (ops->path_check_config    != NULL);
	valid = valid && (ops->plane_enable         != NULL);
	valid = valid && (ops->plane_disable        != NULL);
	valid = valid && (ops->plane_rotate         != NULL);
	valid = valid && (ops->plane_format_set     != NULL);
	valid = valid && (ops->plane_stride_set     != NULL);
	valid = valid && (ops->plane_overlay_set    != NULL);
	valid = valid && (ops->plane_fb_addr_set    != NULL);
	valid = valid && (ops->plane_check_overlay  != NULL);
	valid = valid && (ops->plane_check_rotation != NULL);
	valid = valid && (ops->plane_check_format   != NULL);
	valid = valid && (ops->plane_check_stride   != NULL);
	valid = valid && (ops->plane_check_fb_addr  != NULL);
	return valid;
}

int caninos_de_plane_fb_addr_set(caninos_de_hw_t *hw, phys_addr_t paddr)
{
	unsigned long flags;
	
	if (hw_inited(hw) == false) {
		return -EINVAL;
	}
	if (hw->ops.plane_check_fb_addr(hw, paddr) == false) {
		return -EINVAL;
	}
	
	spin_lock_irqsave(&hw->lock, flags);
	
	if (hw->plane.fb_paddr != paddr)
	{
		hw->plane.fb_paddr = paddr;
		hw->ops.plane_fb_addr_set(hw);
	}
	
	spin_unlock_irqrestore(&hw->lock, flags);
	return 0;
}

int caninos_de_plane_overlay_set(caninos_de_hw_t *hw,
                                 const caninos_de_ovl_t *ovl)
{
	unsigned long flags;
	
	if ((hw_inited(hw) == false) || (ovl == NULL)) {
		return -EINVAL;
	}
	if (hw->ops.plane_check_overlay(hw, ovl) == false) {
		return -EINVAL;
	}
	
	spin_lock_irqsave(&hw->lock, flags);
	
	if (ovl_eq(&hw->plane.ovl, ovl) == false)
	{
		hw->plane.ovl = *ovl;
		hw->ops.plane_overlay_set(hw);
	}
	
	spin_unlock_irqrestore(&hw->lock, flags);
	return 0;
}

int caninos_de_plane_format_set(caninos_de_hw_t *hw, u32 color_mode)
{
	unsigned long flags;
	
	if (hw_inited(hw) == false) {
		return -EINVAL;
	}
	if (hw->ops.plane_check_format(hw, color_mode) == false) {
		return -EINVAL;
	}
	
	spin_lock_irqsave(&hw->lock, flags);
	
	if (hw->plane.color_mode != color_mode)
	{
		hw->plane.color_mode = color_mode;
		hw->ops.plane_format_set(hw);
	}
	
	spin_unlock_irqrestore(&hw->lock, flags);
	return 0;
}

int caninos_de_plane_stride_set(caninos_de_hw_t *hw, u32 stride)
{
	unsigned long flags;
	
	if (hw_inited(hw) == false) {
		return -EINVAL;
	}
	if (hw->ops.plane_check_stride(hw, stride) == false) {
		return -EINVAL;
	}
	
	spin_lock_irqsave(&hw->lock, flags);
	
	if (hw->plane.stride != stride)
	{
		hw->plane.stride = stride;
		hw->ops.plane_stride_set(hw);
	}
	
	spin_unlock_irqrestore(&hw->lock, flags);
	return 0;
}

int caninos_de_plane_rotate(caninos_de_hw_t *hw, u32 rotation)
{
	unsigned long flags;
	
	if (hw_inited(hw) == false) {
		return -EINVAL;
	} 
	if (hw->ops.plane_check_rotation(hw, rotation) == false) {
		return -EINVAL;
	}
	
	spin_lock_irqsave(&hw->lock, flags);
	
	if (hw->plane.rotation != rotation)
	{
		hw->plane.rotation = rotation;
		hw->ops.plane_rotate(hw);
	}
	
	spin_unlock_irqrestore(&hw->lock, flags);
	return 0;
}

bool caninos_de_plane_is_enabled(caninos_de_hw_t *hw)
{
	unsigned long flags;
	bool enabled;
	
	if (hw_inited(hw) == false) {
		return false;
	}
	
	spin_lock_irqsave(&hw->lock, flags);
	enabled = hw->plane.enabled;
	spin_unlock_irqrestore(&hw->lock, flags);
	return enabled;
}

int caninos_de_plane_enable(caninos_de_hw_t *hw)
{
	unsigned long flags;
	
	if (hw_inited(hw) == false) {
		return -EINVAL;
	}
	
	spin_lock_irqsave(&hw->lock, flags);
	
	if (hw->plane.enabled == false)
	{
		hw->plane.enabled = true;
		hw->ops.plane_enable(hw);
	}
	
	spin_unlock_irqrestore(&hw->lock, flags);
	return 0;
}

void caninos_de_plane_disable(caninos_de_hw_t *hw)
{
	unsigned long flags;

	if (hw_inited(hw) == false) {
		return;
	}
	
	spin_lock_irqsave(&hw->lock, flags);
	
	if (hw->plane.enabled == true) {
		hw->plane.enabled = false;
		hw->ops.plane_disable(hw);
	}
	
	spin_unlock_irqrestore(&hw->lock, flags);
}

int caninos_de_path_config(caninos_de_hw_t *hw, u32 w, u32 h, u32 bk)
{
	unsigned long flags;
	
	if (hw_inited(hw) == false) {
		return -EINVAL;
	}
	if (hw->ops.path_check_config(hw, w, h, bk) == false) {
		return -EINVAL;
	}
	
	spin_lock_irqsave(&hw->lock, flags);
	
	hw->path.width = w;
	hw->path.height = h;
	hw->path.bk_color = bk;
	
	spin_unlock_irqrestore(&hw->lock, flags);
	return 0;
}

void caninos_de_path_set_go(caninos_de_hw_t *hw)
{
	unsigned long flags;
	
	if (hw_inited(hw) == false) {
		return;
	}
	
	spin_lock_irqsave(&hw->lock, flags);
	
	if (hw->path.enabled == true) {
		hw->ops.path_set_go(hw);
	}
	
	spin_unlock_irqrestore(&hw->lock, flags);
}

int caninos_de_path_enable(caninos_de_hw_t *hw)
{
	unsigned long flags;
	
	if (hw_inited(hw) == false) {
		return -EINVAL;
	}
	
	spin_lock_irqsave(&hw->lock, flags);
	
	if (hw->path.enabled == false) {
		hw->path.enabled = true;
		hw->ops.path_enable(hw);
	}
	
	spin_unlock_irqrestore(&hw->lock, flags);
	return 0;
}

void caninos_de_path_disable(caninos_de_hw_t *hw)
{
	unsigned long flags;
	
	if (hw_inited(hw) == false) {
		return;
	}
	
	spin_lock_irqsave(&hw->lock, flags);
	
	if (hw->path.enabled == true) {
		hw->path.enabled = false;
		hw->ops.path_disable(hw);
	}
	
	spin_unlock_irqrestore(&hw->lock, flags);
}

int caninos_de_reset(caninos_de_hw_t *hw)
{
	int ret;
	
	if (hw == NULL) {
		return -EINVAL;
	}
	
	SAFE_WRITE(hw->inited, false);
	ret = hw->ops.reset(hw);
	
	if (ret == 0) {
		SAFE_WRITE(hw->inited, true);
	}
	return ret;
}

int caninos_de_init(caninos_de_hw_t *hw, const caninos_de_ops_t *ops)
{
	int ret;
	
	if ((hw == NULL) || (check_ops(ops) == false)) {
		return -EINVAL;
	}
	
	spin_lock_init(&hw->lock);
	hw->ops = *ops;
	
	SAFE_WRITE(hw->inited, false);
	
	ret = hw->ops.power_on(hw);
	
	if (ret < 0) {
		return ret;
	}
	
	ret = hw->ops.init(hw);
	
	if (ret < 0) {
		return ret;
	}
	
	SAFE_WRITE(hw->inited, true);
	return 0;
}

void caninos_de_fini(caninos_de_hw_t *hw)
{
	if (hw_inited(hw) == true) {
		SAFE_WRITE(hw->inited, false);
		hw->ops.fini(hw);
		hw->ops.power_off(hw);
	}
}

