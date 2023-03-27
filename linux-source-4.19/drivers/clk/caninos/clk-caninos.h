// SPDX-License-Identifier: GPL-2.0
/*
 * Clock Controller Driver for Caninos Labrador
 *
 * Copyright (c) 2022-2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2018-2020 LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2014 Actions Semi Inc.
 * Author: David Liu <liuwei@actions-semi.com>
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

#ifndef __CLK_CANINOS_H
#define __CLK_CANINOS_H

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

struct clk_factor_table
{
    unsigned int val;
    unsigned int mul;
    unsigned int div;
};

struct caninos_clk_provider
{
    void __iomem *reg_base;
    struct clk_onecell_data clk_data;
    spinlock_t lock;
};

struct caninos_fixed_clock
{
    int id;
    const char *name;
    const char *parent_name;
    unsigned long fixed_rate;
    unsigned long flags;
};

struct caninos_fixed_factor_clock
{
    int id;
    const char *name;
    const char *parent_name;
    unsigned int mult;
    unsigned int div;
    unsigned long flags;
};

struct caninos_gate_clock
{
    int id;
    const char *name;
    const char *parent_name;
    unsigned long offset;
    unsigned int bit_idx;
    unsigned int gate_flags;
    unsigned long flags;
};

struct caninos_mux_clock
{
    int id;
    const char *name;
    const char * const *parent_names;
    unsigned int num_parents;
    unsigned long offset;
    unsigned int shift;
    unsigned int width;
    unsigned int mux_flags;
    unsigned long flags;
};

struct caninos_div_clock
{
    int id;
    const char *name;
    const char *parent_name;
    unsigned long offset;
    unsigned int shift;
    unsigned int width;
    unsigned int div_flags;
    const struct clk_div_table *table;
    unsigned long flags;
};

struct clk_pll_table
{
    unsigned int val;
    unsigned long rate;
};

struct caninos_pll_clock
{
    int id;
    const char *name;
    const char *parent_name;
    unsigned long offset;
    unsigned long bfreq;
    unsigned int enable_bit;
    unsigned int shift;
    unsigned int width;
    unsigned int min_mul;
    unsigned int max_mul;
    unsigned int pll_flags;
    const struct clk_pll_table *table;
    unsigned long flags;
};

struct caninos_composite_data
{
	struct {
		long offset;
		unsigned int bit_idx;
	} gate;
	
	struct {
		long offset;
		unsigned int shift;
		unsigned int width;
	} mux;
	
	struct {
		long offset;
    	unsigned int shift;
    	unsigned int width;
    	unsigned int mult;
    	unsigned int div;
    	const struct clk_div_table *div_table;
    	const struct clk_factor_table *factor_table;
    } rate;
};

struct caninos_composite_clock
{
	int id;
	const char *name;
	const char * const *parent_names;
	unsigned int num_parents;
	unsigned long flags;
	struct caninos_composite_data data;
};

struct caninos_clock_tree
{
	struct {
		const struct caninos_gate_clock *clks;
		int num;
	} gate;
	
	struct {
		const struct caninos_mux_clock *clks;
		int num;
	} mux;
	
	struct {
		const struct caninos_pll_clock *clks;
		int num;
	} pll;
	
	struct {
		const struct caninos_fixed_clock *clks;
		int num;
	} fixed;
	
	struct {
		const struct caninos_fixed_factor_clock *clks;
		int num;
	} factor;
	
	struct {
		const struct caninos_div_clock *clks;
		int num;
	} div;
	
	struct {
		const struct caninos_composite_clock *clks;
		int num;
	} comp;
};

#define CANINOS_CLK_PLL_READ_ONLY  BIT(0)

#define CANINOS_FIXED_RATE(i, n, p, rate, f) \
{                                            \
    .id = i,                                 \
    .name = n,                               \
    .parent_name = p,                        \
    .fixed_rate = rate,                      \
    .flags = f,                              \
}

#define CANINOS_GATE(i, n, p, o, bit, gf, f) \
{                                            \
    .id = i,                                 \
    .name = n,                               \
    .parent_name = p,                        \
    .offset = o,                             \
    .bit_idx = bit,                          \
    .gate_flags = gf,                        \
    .flags = f,                              \
}

#define CANINOS_MUX(i, n, p, o, s, w, mf, f) \
{                                            \
    .id = i,                                 \
    .name = n,                               \
    .parent_names = p,                       \
    .num_parents = ARRAY_SIZE(p),            \
    .offset = o,                             \
    .shift = s,                              \
    .width = w,                              \
    .mux_flags = mf,                         \
    .flags = f,                              \
}

#define CANINOS_PLL(i, n, p, o, bf, e, s, w, mn, mx, pf, t, f) \
{                                                              \
    .id = i,                                                   \
    .name = n,                                                 \
    .parent_name = p,                                          \
    .offset = o,                                               \
    .bfreq = bf,                                               \
    .enable_bit = e,                                           \
    .shift = s,                                                \
    .width = w,                                                \
    .min_mul = mn,                                             \
    .max_mul = mx,                                             \
    .pll_flags = pf,                                           \
    .table = t,                                                \
    .flags = f,                                                \
}

#define CANINOS_DIV(i, n, p, o, s, w, df, t, f) \
{                                               \
    .id = i,                                    \
    .name = n,                                  \
    .parent_name = p,                           \
    .offset = o,                                \
    .shift = s,                                 \
    .width = w,                                 \
    .div_flags = df,                            \
    .table = t,                                 \
    .flags = f,                                 \
}

#define CANINOS_FIXED_FACTOR(i, n, p, f, m, d) \
{                                              \
	.id = i,                                   \
	.name = n,                                 \
	.parent_name = p,                          \
	.flags = f,                                \
	.mult = m,                                 \
	.div = d,                                  \
}

#define CANINOS_COMPOSITE(i, n, p, f, m, g, r) \
{                                              \
	.id = i,                                   \
	.name = n,                                 \
	.parent_names = p,                         \
	.num_parents = ARRAY_SIZE(p),              \
	.flags = f,                                \
	.data.mux = m,                             \
	.data.gate = g,                            \
	.data.rate = r,                            \
}

#define CANINOS_COMP_NULL \
{                         \
    .offset = -1,         \
}

#define CANINOS_COMP_NO_MUX   CANINOS_COMP_NULL
#define CANINOS_COMP_NO_GATE  CANINOS_COMP_NULL

#define CANINOS_COMP_GATE(o, b) \
{                               \
    .offset = o,                \
    .bit_idx = b,               \
}

#define CANINOS_COMP_MUX(o, s, w) \
{                                 \
    .offset = o,                  \
    .shift = s,                   \
    .width = w,                   \
}

#define CANINOS_COMP_FIXED_FACTOR(m, d) \
{                                       \
	.offset = 0,                        \
	.mult = m,                          \
	.div = d,                           \
	.factor_table = NULL,               \
	.div_table = NULL,                  \
}

#define CANINOS_COMP_FACTOR(o, s, w, t) \
{                                       \
	.offset = o,                        \
	.shift = s,                         \
	.width = w,                         \
	.factor_table = t,                  \
	.div_table = NULL,                  \
}

#define CANINOS_COMP_DIVIDER(o, s, w, t) \
{                                        \
    .offset = o,                         \
    .shift = s,                          \
    .width = w,                          \
    .factor_table = NULL,                \
    .div_table = t,                      \
}

extern struct caninos_clk_provider * __init
caninos_clk_init(struct device_node *np,
                 void __iomem *base, unsigned long nr_clks);

extern void __init
caninos_clk_register_gate(struct caninos_clk_provider *ctx,
                          const struct caninos_gate_clock *clks,
                          int num);

extern void __init
caninos_clk_register_mux(struct caninos_clk_provider *ctx,
                         const struct caninos_mux_clock *clks,
                         int num);

extern void __init
caninos_clk_register_pll(struct caninos_clk_provider *ctx,
                         const struct caninos_pll_clock *clks,
                         int num);

extern void __init
caninos_clk_register_div(struct caninos_clk_provider *ctx,
                         const struct caninos_div_clock *clks,
                         int num);

extern struct clk * __init
caninos_register_pll(const struct caninos_pll_clock *info,
                     struct device *dev, void __iomem *reg,
                     spinlock_t *lock);

extern void __init
caninos_clk_register_fixed(struct caninos_clk_provider *ctx,
                           const struct caninos_fixed_clock *clks,
                           int num);

extern void __init
caninos_clk_register_fixed_factor(struct caninos_clk_provider *ctx,
                                  const struct caninos_fixed_factor_clock *clks,
                                  int num);

extern void __init 
caninos_clk_register_composite(struct caninos_clk_provider *ctx,
                               const struct caninos_composite_clock *clks,
                               int num);

extern struct clk * __init
caninos_register_composite(const struct caninos_composite_clock *info,
                           struct device *dev, void __iomem *reg_base,
                           spinlock_t *lock);

extern void __init
caninos_register_clk_tree(struct caninos_clk_provider *ctx,
                          const struct caninos_clock_tree *tree);

#endif

