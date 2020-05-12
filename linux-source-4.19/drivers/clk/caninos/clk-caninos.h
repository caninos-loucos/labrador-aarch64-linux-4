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

#ifndef __CLK_CANINOS_H
#define __CLK_CANINOS_H

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

struct caninos_clk_provider
{
    void __iomem *reg_base;
    struct clk_onecell_data clk_data;
    spinlock_t lock;
};

extern struct caninos_clk_provider * __init caninos_clk_init
    (struct device_node *np, void __iomem *base, unsigned long nr_clks);

#define CANINOS_CLK_IS_CRITICAL          BIT(0)
#define CANINOS_CLK_RATE_READ_ONLY       BIT(1)
#define CANINOS_CLK_ROUND_RATE_CLOSEST   BIT(2)
#define CANINOS_CLK_SET_RATE_NO_REPARENT BIT(3)
#define CANINOS_CLK_SET_RATE_PARENT      BIT(4)

struct caninos_gate_clock
{
    int id;
    const char *name;
    const char *parent_name;
    unsigned long offset;
    unsigned int bit_idx;
    unsigned int gate_flags;
};

extern void __init caninos_clk_register_gate(struct caninos_clk_provider *ctx,
                                             struct caninos_gate_clock *clks,
                                             int num);

extern struct clk *caninos_register_gate(const struct caninos_gate_clock *info,
                                         void __iomem *reg, spinlock_t *lock);

#define CANINOS_GATE(i, n, p, o, bit, gf) \
{                                         \
    .id = i,                              \
    .name = n,                            \
    .parent_name = p,                     \
    .offset = o,                          \
    .bit_idx = bit,                       \
    .gate_flags = gf,                     \
}

#define CANINOS_COMP_GATE(o, b) \
{                               \
    .id = -1,                   \
    .offset = o,                \
    .bit_idx = b,               \
}

#define CANINOS_COMP_NULL \
{                         \
    .id = 0,              \
}

struct caninos_mux_clock
{
    int id;
    const char *name;
    const char **parent_names;
    unsigned int num_parents;
    unsigned long offset;
    unsigned int shift;
    unsigned int width;
    unsigned int mux_flags;
};

extern void __init caninos_clk_register_mux(struct caninos_clk_provider *ctx,
                                            struct caninos_mux_clock *clks,
                                            int num);

extern struct clk *caninos_register_mux(const struct caninos_mux_clock *info,
                                        void __iomem *reg, spinlock_t *lock);

#define CANINOS_MUX(i, n, p, o, s, w, mf) \
{                                         \
    .id = i,                              \
    .name = n,                            \
    .parent_names = p,                    \
    .num_parents = ARRAY_SIZE(p),         \
    .offset = o,                          \
    .shift = s,                           \
    .width = w,                           \
    .mux_flags = mf,                      \
}

#define CANINOS_COMP_MUX(p, o, s, w) \
{                                    \
    .id = -1,                        \
    .parent_names = p,               \
    .num_parents = ARRAY_SIZE(p),    \
    .offset = o,                     \
    .shift = s,                      \
    .width = w,                      \
}

#define CANINOS_COMP_MUX_FIXED(p) \
{                                 \
    .id	= -1,                     \
    .parent_names = p,            \
    .num_parents = 1,             \
}

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
};

#define CANINOS_PLL(i, n, p, o, f, e, s, w, mn, mx, pf, t) \
{                                                          \
    .id = i,                                               \
    .name = n,                                             \
    .parent_name = p,                                      \
    .offset = o,                                           \
    .bfreq = f,                                            \
    .enable_bit = e,                                       \
    .shift = s,                                            \
    .width = w,                                            \
    .min_mul = mn,                                         \
    .max_mul = mx,                                         \
    .pll_flags = pf,                                       \
    .table = t,                                            \
}

extern struct clk *caninos_register_pll(const struct caninos_pll_clock *info,
                                        void __iomem *reg, spinlock_t *lock);

extern void __init caninos_clk_register_pll(struct caninos_clk_provider *ctx,
                                            struct caninos_pll_clock *clks,
                                            int num);

struct caninos_div_clock
{
    int id;
    const char *name;
    const char *parent_name;
    unsigned long offset;
    unsigned int shift;
    unsigned int width;
    unsigned int div_flags;
    struct clk_div_table *table;
};

#define CANINOS_DIV(i, n, p, o, s, w, df, t) \
{                                            \
    .id = i,                                 \
    .name = n,                               \
    .parent_name = p,                        \
    .offset = o,                             \
    .shift = s,                              \
    .width = w,                              \
    .div_flags = df,                         \
    .table = t,                              \
}

#define CANINOS_COMP_DIVIDER(o, s, w, t) \
{                                        \
    .id = -1,                            \
    .offset = o,                         \
    .shift = s,                          \
    .width = w,                          \
    .table = t,                          \
}

extern void __init caninos_clk_register_div(struct caninos_clk_provider *ctx,
                                            struct caninos_div_clock *clks,
                                            int num);

extern struct clk *caninos_register_div(const struct caninos_div_clock *info,
                                        void __iomem *reg, spinlock_t *lock);

struct caninos_fixed_clock
{
    int id;
    char *name;
    const char *parent_name;
    unsigned long fixed_rate;
};

#define CANINOS_FIXED_RATE(i, n, p, rate) \
{                                         \
    .id = i,                              \
    .name = n,                            \
    .parent_name = p,                     \
    .fixed_rate = rate,                   \
}

#define CANINOS_COMP_FIXED_RATE(r) \
{                                  \
    .id = -1,                      \
    .fixed_rate	= r,               \
}

extern void __init caninos_clk_register_fixed(struct caninos_clk_provider *ctx,
                                              struct caninos_fixed_clock *clks,
                                              int num);

extern struct clk *caninos_register_fixed
    (const struct caninos_fixed_clock *info);

struct caninos_fixed_factor_clock
{
    int id;
    char *name;
    const char *parent_name;
    unsigned long flags;
    unsigned int mult;
    unsigned int div;
};

#define CANINOS_COMP_FIXED_FACTOR(m, d) \
{                                       \
    .id = -1,                           \
    .mult = m,                          \
    .div = d,                           \
}

struct clk_factor_table
{
    unsigned int val;
    unsigned int mul;
    unsigned int div;
};

struct caninos_factor_clock
{
    int id;
    const char *name;
    const char *parent_name;
    unsigned long offset;
    unsigned int shift;
    unsigned int width;
    const struct clk_factor_table *table;
};

#define CANINOS_COMP_FACTOR(o, s, w, t) \
{                                       \
    .id = -1,                           \
    .offset = o,                        \
    .shift = s,                         \
    .width = w,                         \
    .table = t,                         \
}

union rate_clock
{
    struct caninos_fixed_clock fixed;
    struct caninos_fixed_factor_clock fixed_factor;
    struct caninos_div_clock div;
    struct caninos_factor_clock factor;
};

struct caninos_composite_clock
{
    int id;
    const char *name;
    unsigned int type;
    struct caninos_mux_clock mux;
    struct caninos_gate_clock gate;
    union rate_clock rate;
};

#define CANINOS_COMPOSITE_TYPE_PASS         (0)
#define CANINOS_COMPOSITE_TYPE_FIXED_RATE   (1)
#define CANINOS_COMPOSITE_TYPE_DIVIDER      (2)
#define CANINOS_COMPOSITE_TYPE_FACTOR       (3)
#define CANINOS_COMPOSITE_TYPE_FIXED_FACTOR (4)

#define CANINOS_COMP_FIXED_CLK(_id, _name, _mux, _gate, _div) \
{                                                             \
    .id = _id,                                                \
    .name = _name,                                            \
    .type = CANINOS_COMPOSITE_TYPE_FIXED_RATE,                \
    .mux = _mux,                                              \
    .gate = _gate,                                            \
    .rate.fixed = _div,                                       \
}

#define CANINOS_COMP_FIXED_FACTOR_CLK(_id, _name, _mux, _gate, _fixed_factor) \
{                                                                             \
    .id = _id,                                                                \
    .name = _name,                                                            \
    .type = CANINOS_COMPOSITE_TYPE_FIXED_FACTOR,                              \
    .mux = _mux,                                                              \
    .gate = _gate,                                                            \
    .rate.fixed_factor = _fixed_factor,                                       \
}

#define CANINOS_COMP_DIV_CLK(_id, _name, _mux, _gate, _div) \
{                                                           \
    .id = _id,                                              \
    .name = _name,                                          \
    .type = CANINOS_COMPOSITE_TYPE_DIVIDER,                 \
    .mux = _mux,                                            \
    .gate = _gate,                                          \
    .rate.div = _div,                                       \
}

#define CANINOS_COMP_FACTOR_CLK(_id, _name, _mux, _gate, _factor) \
{                                                                 \
    .id = _id,                                                    \
    .name = _name,                                                \
    .type = CANINOS_COMPOSITE_TYPE_FACTOR,                        \
    .mux = _mux,                                                  \
    .gate = _gate,                                                \
    .rate.factor = _factor,                                       \
}

#define CANINOS_COMP_PASS_CLK(_id, _name, _mux, _gate) \
{                                                      \
    .id = _id,                                         \
    .name = _name,                                     \
    .type = CANINOS_COMPOSITE_TYPE_PASS,               \
    .mux = _mux,                                       \
    .gate = _gate,                                     \
}

extern void __init caninos_clk_register_composite
    (struct caninos_clk_provider *ctx,
     struct caninos_composite_clock *clks,
     int num);

extern struct clk *caninos_register_composite
    (struct caninos_composite_clock *cclk,
     void __iomem *reg_base, spinlock_t *lock);

#endif

