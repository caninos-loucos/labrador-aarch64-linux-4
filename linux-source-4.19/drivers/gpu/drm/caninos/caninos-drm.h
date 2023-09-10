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

#ifndef _CANINOS_DRM_H_
#define _CANINOS_DRM_H_

#include <linux/platform_device.h>
#include <uapi/drm/drm_fourcc.h>

struct caninos_vdc_mode {
	u32 width;
	u32 height;
	u32 format;
};

/* HDMI video resolutions defined by EDID */

enum hdmi_vid_table {
	VID640x480P_60_4VS3    =  1,
	VID720x480P_60_4VS3    =  2,
	VID1280x720P_60_16VS9  =  4,
	VID1920x1080I_60_16VS9 =  5,
	VID720x480I_60_4VS3    =  6,
	VID1920x1080P_60_16VS9 = 16,
	VID720x576P_50_4VS3    = 17,
	VID1280x720P_50_16VS9  = 19,
	VID1920x1080I_50_16VS9 = 20,
	VID720x576I_50_4VS3    = 21,
	VID1440x576P_50_4VS3   = 29,
	VID1920x1080P_50_16VS9 = 31,
	VID1920x1080P_24_16VS9 = 32,
};

struct caninos_vdc;
struct caninos_gfx;
struct caninos_hdmi;

extern struct platform_driver caninos_vdc_plat_driver;
extern struct platform_driver caninos_hdmi_plat_driver;
#ifdef CONFIG_DRM_CANINOS_HDMI_AUDIO
extern struct platform_driver caninos_hdmi_audio_plat_driver;
#endif

extern int caninos_hdmi_set_mode(struct caninos_hdmi*, enum hdmi_vid_table);

extern int caninos_hdmi_enable(struct caninos_hdmi*);

extern int caninos_hdmi_disable(struct caninos_hdmi*);

extern int caninos_vdc_set_mode(struct caninos_vdc*, struct caninos_vdc_mode*);

extern int caninos_vdc_disable(struct caninos_vdc*);

extern int caninos_vdc_enable(struct caninos_vdc*);

extern int caninos_vdc_set_fbaddr(struct caninos_vdc*, u32);

#endif /* _CANINOS_DRM_H_ */
