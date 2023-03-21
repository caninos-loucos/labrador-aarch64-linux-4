// SPDX-License-Identifier: GPL-2.0
/*
 * Support Routines for the Caninos Labrador 32bits Architecture
 *
 * Copyright (c) 2023 ITEX - LSITEC - Caninos Loucos
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

#include <linux/irqchip.h>
#include <linux/of_platform.h>
#include <linux/memblock.h>
#include <asm/mach/map.h>
#include <../mm/mm.h>
#include <mach/platform.h>

static struct map_desc caninos_k5_io_desc[] __initdata = {
	{
		.virtual = IO_ADDRESS(PA_REG_BASE),
		.pfn     = __phys_to_pfn(PA_REG_BASE),
		.length  = PA_REG_SIZE,
		.type    = MT_DEVICE,
	},
	{
		.virtual = VA_BOOT_RAM,
		.pfn     = __phys_to_pfn(PA_BOOT_RAM),
		.length  = SZ_4K,
		.type    = MT_MEMORY_RWX,
	},
};

void __init caninos_k5_map_io(void)
{
	iotable_init(caninos_k5_io_desc, ARRAY_SIZE(caninos_k5_io_desc));
}

