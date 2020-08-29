// SPDX-License-Identifier: GPL-2.0
/*
 * Thermal Sensor Driver for Caninos Labrador
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

#ifndef __CANINOS_THERMAL_H
#define __CANINOS_THERMAL_H

#include <linux/platform_device.h>
#include <linux/thermal.h>

#define CANINOS_TMU_ACTIVE_INTERVAL (1000)
#define CANINOS_TMU_IDLE_INTERVAL   (3000)

enum caninos_tmu_sensor_type
{
	CANINOS_TMU_CPU          = 0,
	CANINOS_TMU_GPU          = 1,
	CANINOS_TMU_CORELOGIC    = 2,
	CANINOS_TMU_SENSOR_COUNT = 3,
};

struct caninos_tmu_sensor
{
	struct thermal_zone_device *zone;
	struct device *dev;
	int temperature;
	int id;
};

struct caninos_tmu_data
{
	void __iomem *base;
	struct clk *tmu_clk;
	
	struct delayed_work work;
	struct mutex lock;
	
	struct caninos_tmu_sensor sensor[CANINOS_TMU_SENSOR_COUNT];
};

#endif /* __CANINOS_THERMAL_H */
