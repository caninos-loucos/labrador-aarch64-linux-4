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

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <mach/k5-regs.h>

void caninos_k5_map_io(void);
bool caninos_k5_smp_init(void);
void caninos_k5_secondary_startup(void);
void caninos_k5_init_irq(void);
void caninos_k5_init(void);
void caninos_k5_reserve(void);
void caninos_k5_init_early(void);
bool caninos_k5_pmic_setup(void);
bool caninos_k5_cpu_set_clock(unsigned int freq, unsigned int voltage);
bool caninos_k5_nandpll_set_clock(unsigned int freq);

#endif /* _PLATFORM_H_ */
