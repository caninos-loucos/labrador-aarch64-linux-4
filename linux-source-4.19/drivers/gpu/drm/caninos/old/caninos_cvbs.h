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

extern int cvbs_display_enable(struct caninos_gfx *priv, int mode_id,
                               int preline);

extern int cvbs_display_disable(struct caninos_gfx *priv);

extern void cvbs_power_on(struct caninos_gfx *priv);

extern void cvbs_power_off(struct caninos_gfx *priv);

