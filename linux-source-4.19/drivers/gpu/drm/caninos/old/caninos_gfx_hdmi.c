/*
 * Caninos DRM/KMS driver
 *
 * Copyright (c) 2020 LSI-TEC - Caninos Loucos
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

#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc_helper.h>

#include "caninos_gfx.h"

static int caninos_gfx_get_modes(struct drm_connector *conn)
{
	int count;
	
	count = drm_add_modes_noedid(conn, 1920, 1080);
	
	drm_set_preferred_mode(conn, 1920, 1080);
	
	return count;
}

static int caninos_gfx_mode_valid(struct drm_connector *connector,
                                  struct drm_display_mode *mode)
{
    
    //int w = mode->hdisplay, h = mode->vdisplay;
    
    
    //int vrefresh = drm_mode_vrefresh(mode);
    
    return MODE_OK;
}

static const struct drm_connector_funcs hdmi_connector_funcs = {
    .reset = drm_atomic_helper_connector_reset,
    
    .fill_modes	= drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	
	.atomic_duplicate_state	= drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs hdmi_connector_helper_funcs = {
    .get_modes = caninos_gfx_get_modes,
    .mode_valid = caninos_gfx_mode_valid,
};





int caninos_gfx_create_hdmi_output(struct drm_device *drm)
{
	struct caninos_gfx *priv = drm->dev_private;
	int ret;
	
	
	
	

	priv->connector.dpms = DRM_MODE_DPMS_OFF;
	priv->connector.polled = 0;
	
	ret = drm_connector_init(drm, &priv->connector, &hdmi_connector_funcs,
	                         DRM_MODE_CONNECTOR_HDMIA);
    
	drm_connector_helper_add(&priv->connector, &hdmi_connector_helper_funcs);
	
	
	return ret;
}
