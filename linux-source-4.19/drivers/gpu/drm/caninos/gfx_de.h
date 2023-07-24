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

#ifndef _GFX_DE_H_
#define _GFX_DE_H_

struct caninos_de_hw;
struct caninos_de_ops;
struct caninos_de_ovl;

typedef struct caninos_de_hw  caninos_de_hw_t;
typedef struct caninos_de_ops caninos_de_ops_t;
typedef struct caninos_de_ovl caninos_de_ovl_t;

struct caninos_de_ops
{
	int  (*init)(caninos_de_hw_t *hw);
	void (*fini)(caninos_de_hw_t *hw);
	int  (*reset)(caninos_de_hw_t *hw);
	int  (*power_on)(caninos_de_hw_t *hw);
	void (*power_off)(caninos_de_hw_t *hw);
	void (*path_disable)(caninos_de_hw_t *hw);
	void (*path_enable)(caninos_de_hw_t *hw);
	void (*path_set_go)(caninos_de_hw_t *hw);
	bool (*path_check_config)(caninos_de_hw_t *hw, u32 w, u32 h, u32 bk);
	void (*plane_enable)(caninos_de_hw_t *hw);
	void (*plane_disable)(caninos_de_hw_t *hw);
	void (*plane_rotate)(caninos_de_hw_t *hw);
	void (*plane_format_set)(caninos_de_hw_t *hw);
	void (*plane_stride_set)(caninos_de_hw_t *hw);
	void (*plane_overlay_set)(caninos_de_hw_t *hw);
	void (*plane_fb_addr_set)(caninos_de_hw_t *hw);
	bool (*plane_check_overlay)(caninos_de_hw_t *hw,
	                            const caninos_de_ovl_t *ovl);
	bool (*plane_check_rotation)(caninos_de_hw_t *hw, u32 rotation);
	bool (*plane_check_format)(caninos_de_hw_t *hw, u32 color_mode);
	bool (*plane_check_stride)(caninos_de_hw_t *hw, u32 stride);
	bool (*plane_check_fb_addr)(caninos_de_hw_t *hw, phys_addr_t paddr);
};

struct caninos_de_ovl
{
	struct {
		u32 w, h;
	} src;
	
	struct {
		u32 x, y;
		u32 w, h;
	} dst;
};

struct caninos_de_hw
{
	caninos_de_ops_t ops;
	spinlock_t lock;
	bool inited;
	
	struct {
		bool enabled;
		u32 bk_color;
		u32 width;
		u32 height;
	} path;
	
	struct {
		bool enabled;
		phys_addr_t fb_paddr;
		u32 color_mode;
		u32 stride; /* in bytes */
		u32 rotation;
		caninos_de_ovl_t ovl;
	} plane;
};

extern int
caninos_de_plane_fb_addr_set(caninos_de_hw_t *hw, phys_addr_t paddr);

extern int
caninos_de_plane_overlay_set(caninos_de_hw_t *hw, const caninos_de_ovl_t *ovl);

extern int 
caninos_de_plane_format_set(caninos_de_hw_t *hw, u32 color_mode);

extern int 
caninos_de_plane_stride_set(caninos_de_hw_t *hw, u32 stride);

extern int 
caninos_de_plane_rotate(caninos_de_hw_t *hw, u32 rotation);

extern int 
caninos_de_plane_enable(caninos_de_hw_t *hw);

extern void 
caninos_de_plane_disable(caninos_de_hw_t *hw);

extern bool
caninos_de_plane_is_enabled(caninos_de_hw_t *hw);

extern int 
caninos_de_path_config(caninos_de_hw_t *hw, u32 w, u32 h, u32 bk);

extern void 
caninos_de_path_set_go(caninos_de_hw_t *hw);

extern int 
caninos_de_path_enable(caninos_de_hw_t *hw);

extern void 
caninos_de_path_disable(caninos_de_hw_t *hw);

extern int 
caninos_de_reset(caninos_de_hw_t *hw);

extern int 
caninos_de_init(caninos_de_hw_t *hw, const caninos_de_ops_t *ops);

extern void 
caninos_de_fini(caninos_de_hw_t *hw);

#endif

