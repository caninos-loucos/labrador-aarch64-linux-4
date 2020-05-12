// SPDX-License-Identifier: GPL-2.0
/*
 * Clock Controller Driver for Caninos Labrador
 * Copyright (c) 2018-2020 LSI-TEC - Caninos Loucos
 * Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2014 David Liu, Actions Semi Inc <liuwei@actions-semi.com>
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

#ifndef __CLK_CANINOS_PRIV_H
#define __CLK_CANINOS_PRIV_H

struct caninos_fixed_rate
{
    struct clk_hw hw;
    unsigned long fixed_rate;
};

extern const struct clk_ops caninos_fixed_rate_ops;

struct caninos_mux
{
    struct clk_hw hw;
    void __iomem *reg;
    unsigned int shift;
    unsigned int width;
    unsigned long flags;
    spinlock_t *lock;
};

extern const struct clk_ops caninos_mux_ops;

struct caninos_gate
{
    struct clk_hw hw;
    void __iomem *reg;
    unsigned int bit_idx;
    spinlock_t *lock;
};

extern const struct clk_ops caninos_gate_ops;

struct caninos_divider
{
    struct clk_hw hw;
    void __iomem *reg;
    unsigned int shift;
    unsigned int width;
    unsigned long flags;
    const struct clk_div_table *table;
    spinlock_t *lock;
};

extern const struct clk_ops caninos_divider_ops;

struct caninos_factor
{
    struct clk_hw hw;
    void __iomem *reg;
    unsigned int shift;
    unsigned int width;
    const struct clk_factor_table *table;
    spinlock_t *lock;
};

extern const struct clk_ops caninos_factor_ops;

struct caninos_fixed_factor
{
    struct clk_hw hw;
    unsigned int mult;
	unsigned int div;
    spinlock_t *lock;
};

extern const struct clk_ops caninos_fixed_factor_ops;

#endif

