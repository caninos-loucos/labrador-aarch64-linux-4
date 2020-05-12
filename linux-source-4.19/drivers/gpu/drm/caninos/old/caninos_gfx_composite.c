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

#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc_helper.h>

#include "caninos_gfx.h"

static int caninos_gfx_get_modes(struct drm_connector *conn)
{
    u32 max_width  = conn->dev->mode_config.max_width;
    u32 max_height = conn->dev->mode_config.max_height;
    int count;

    count = drm_add_modes_noedid(conn, max_width, max_height);
    drm_set_preferred_mode(conn, max_width, max_height);

    return count;
}

static const struct drm_connector_helper_funcs
    composite_connector_helper_funcs =
{
    .get_modes = caninos_gfx_get_modes,
};

static const struct drm_connector_funcs composite_connector_funcs =
{
    .fill_modes	= drm_helper_probe_single_connector_modes,
    .destroy = drm_connector_cleanup,
    .reset = drm_atomic_helper_connector_reset,
    .atomic_duplicate_state	= drm_atomic_helper_connector_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

int caninos_gfx_create_composite_output(struct drm_device *drm)
{
    struct caninos_gfx *priv = drm->dev_private;
    int ret;
    
    priv->connector.dpms = DRM_MODE_DPMS_OFF;
    priv->connector.polled = 0;
    
    drm_connector_helper_add(&priv->connector,
                             &composite_connector_helper_funcs);
    
    ret = drm_connector_init(drm, &priv->connector, 
                             &composite_connector_funcs,
                             DRM_MODE_CONNECTOR_Composite);
    return ret;
}

