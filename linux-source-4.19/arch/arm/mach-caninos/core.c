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

/*
	CPU Recommended Frequency/Voltage Pairs
	
	 408MHz at  950mV
	 720MHz at  975mV
	 900MHz at 1025mV
	1104MHz at 1175mV
	1308MHz at 1250mV
	
	PMIC DCDC1 Regulator Operating Limits
	
	Max Voltage : 1400mV
	Min Voltage :  700mV
	Voltage Step:   25mV
	Min Selector:    0
	Max Selector:   28 
	Stable After:  350us
*/

#define CPU_CORE_FREQ (1104U) /* MHz */
#define CPU_CORE_VOLT (1175U) /* mV  */

#define KINFO_SIZE            (0x00100000)
#define FRAME_BUFFER_SIZE     (0x00800000)
#define DDR_DQS_TRAINING_BASE (0x00000000)
#define DDR_DQS_TRAINING_SIZE (0x00004000)

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

void __init caninos_k5_init_irq(void)
{
	irqchip_init();
}

static struct of_device_id caninos_k5_dt_match_table[] __initdata = {
	{ .compatible = "simple-bus", },
	{ /* sentinel */ }
};

void __init caninos_k5_init(void)
{
	if (of_platform_populate(NULL, caninos_k5_dt_match_table, NULL, NULL)) {
		panic("of_platform_populate() failed\n");
	}
}

void __init caninos_k5_reserve(void)
{
	phys_addr_t kinfo_start, framebuffer_start;
	
	framebuffer_start = arm_lowmem_limit - FRAME_BUFFER_SIZE;
	kinfo_start = framebuffer_start - KINFO_SIZE;
	
	memblock_reserve(DDR_DQS_TRAINING_BASE, DDR_DQS_TRAINING_SIZE);
	memblock_reserve(framebuffer_start, FRAME_BUFFER_SIZE);
	memblock_reserve(kinfo_start, KINFO_SIZE);
}

void __init caninos_k5_init_early(void)
{
	if (!caninos_k5_pmic_setup()) {
		panic("Could not setup the PMIC\n");
	}
	else if (!caninos_k5_cpu_set_clock(CPU_CORE_FREQ, CPU_CORE_VOLT)) {
		panic("Could not set CPU core speed\n");
	}
}

