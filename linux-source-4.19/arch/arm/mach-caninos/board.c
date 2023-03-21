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

#include <asm/mach/arch.h>
#include <mach/platform.h>
#include <asm/hardware/cache-l2x0.h>

/*
	1) Instruction prefetch enable
	2) Data prefetch enable
	3) Round-robin replacement
	4) Use AWCACHE attributes for WA
	5) 32kB way size, 16 way associativity
	6) Disable exclusive cache
*/

#define L310_MASK 0xc0000fff

#define L310_VAL \
	(L310_AUX_CTRL_INSTR_PREFETCH | L310_AUX_CTRL_DATA_PREFETCH | \
	 L310_AUX_CTRL_NS_INT_CTRL | L310_AUX_CTRL_NS_LOCKDOWN | \
	 L310_AUX_CTRL_CACHE_REPLACE_RR | L2C_AUX_CTRL_WAY_SIZE(2) | \
	 L310_AUX_CTRL_ASSOCIATIVITY_16)

static const char *const caninos_dt_compat[] __initconst = {
	"caninos,k5",
	NULL,
};

DT_MACHINE_START(CANINOS, "Caninos Labrador 32bits Core Board")
	.dt_compat    = caninos_dt_compat,
	.atag_offset  = 0x00000100,
	.l2c_aux_val  = L310_VAL,
	.l2c_aux_mask = L310_MASK,
	.map_io       = caninos_k5_map_io,
	.smp_init     = caninos_k5_smp_init,
	.reserve      = caninos_k5_reserve,
	.init_irq     = caninos_k5_init_irq,
	.init_machine = caninos_k5_init,
MACHINE_END
