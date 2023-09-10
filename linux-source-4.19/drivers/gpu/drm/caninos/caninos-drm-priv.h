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

#ifndef _CANINOS_DRM_PRIV_H_
#define _CANINOS_DRM_PRIV_H_

#include <drm/drm_simple_kms_helper.h>

#include "caninos-drm.h"

struct caninos_gfx
{
	struct drm_simple_display_pipe pipe;
	struct drm_connector connector;
	struct drm_device *drm;
	struct device *dev;
	struct caninos_hdmi *caninos_hdmi;
	struct caninos_vdc *caninos_vdc;
};

#endif /* _CANINOS_DRM_PRIV_H_ */

