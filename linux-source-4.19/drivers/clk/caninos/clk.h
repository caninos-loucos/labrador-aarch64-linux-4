/*
 * Utility functions to register clocks to common clock framework for
 * Actions platforms.
 *
 * Copyright (C) 2014 Actions Semi Inc.
 * David Liu <liuwei@actions-semi.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __OWL_CLK_H
#define __OWL_CLK_H

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

/**
 * struct owl_clk_provider: information about clock provider
 * @reg_base: virtual address for the register base.
 * @clk_data: holds clock related data like clk* and number of clocks.
 * @lock: maintains exclusion bwtween callbacks for a given clock-provider.
 */
struct owl_clk_provider {
	void __iomem		*reg_base;
	struct clk_onecell_data clk_data;
	spinlock_t		lock;
};

/* fixed rate clock */
struct owl_fixed_rate_clock {
	unsigned int		id;
	char			*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		fixed_rate;
};

/* fixed factor clock */
struct owl_fixed_factor_clock {
	unsigned int		id;
	char			*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned int		mult;
	unsigned int		div;
};

/* PLL clock */

/* last entry should have rate = 0 */
struct clk_pll_table {
	unsigned int		val;
	unsigned long		rate;
};

struct owl_pll_clock {
	unsigned int		id;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		offset;

	unsigned long		bfreq;
	u8			enable_bit;
	u8			shift;
	u8			width;
	u8			min_mul;
	u8			max_mul;
	u8			pll_flags;
	const struct clk_pll_table *table;
};

/* pll_flags*/
#define CLK_OWL_PLL_FIXED_FREQ		BIT(0)

/* divider clock */
struct owl_divider_clock {
	unsigned int		id;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		offset;
	u8			shift;
	u8			width;
	u8			div_flags;
	struct clk_div_table	*table;
	const char		*alias;
};

/* factor divider table clock */

struct clk_factor_table {
	unsigned int		val;
	unsigned int		mul;
	unsigned int		div;
};

struct owl_factor_clock {
	unsigned int		id;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		offset;
	u8			shift;
	u8			width;
	u8			div_flags;
	struct clk_factor_table	*table;
	const char		*alias;
};

/**
 * struct owl_factor - factor divider clock
 *
 * @hw:		handle between common and hardware-specific interfaces
 * @reg:	register containing the factor divider
 * @shift:	shift to the divider bit field
 * @width:	width of the divider bit field
 * @table:	array of value/multiplier/divider pairs, last entry should
 *			have div = 0
 * @lock:	register lock
 *
 * Clock with an factor divider table affecting its output frequency.
 * Implements .recalc_rate, .set_rate and .round_rate
 */
struct owl_factor {
	struct clk_hw		hw;
	void __iomem		*reg;
	u8			shift;
	u8			width;
	u8			flags;
	const struct clk_factor_table	*table;
	spinlock_t		*lock;
};

extern struct clk_ops owl_factor_ops;

/* mux clock */
struct owl_mux_clock {
	unsigned int		id;
	const char		*name;
	const char		**parent_names;
	u8			num_parents;
	unsigned long		flags;
	unsigned long		offset;
	u8			shift;
	u8			width;
	u8			mux_flags;
	const char		*alias;
};

/* gate clock */
struct owl_gate_clock {
	unsigned int		id;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		offset;
	u8			bit_idx;
	u8			gate_flags;
	const char		*alias;
};

/* composite clock */

union rate_clock {
	struct owl_fixed_rate_clock	fixed;
	struct owl_fixed_factor_clock	fixed_factor;
	struct owl_divider_clock	div;
	struct owl_factor_clock		factor;
};

struct owl_composite_clock {
	unsigned int		id;
	const char		*name;
	unsigned int		type;
	unsigned long		flags;

	struct owl_mux_clock	mux;
	struct owl_gate_clock	gate;
	union rate_clock	rate;
};

#define OWL_COMPOSITE_TYPE_FIXED_RATE      1
#define OWL_COMPOSITE_TYPE_DIVIDER         2
#define OWL_COMPOSITE_TYPE_FACTOR          3
#define OWL_COMPOSITE_TYPE_FIXED_FACTOR    4
#define OWL_COMPOSITE_TYPE_PASS            10

#define COMP_FIXED_CLK(_id, _name, _flags, _mux, _gate, _div)		\
	{								\
		.id		= _id,					\
		.name		= _name,				\
		.type		= OWL_COMPOSITE_TYPE_FIXED_RATE,	\
		.flags		= _flags,				\
		.mux		= _mux,					\
		.gate		= _gate,				\
		.rate.fixed	= _div,					\
	}

#define COMP_FIXED_FACTOR_CLK(_id, _name, _flags, _mux, _gate, _fixed_factor)	\
	{								\
		.id		= _id,					\
		.name		= _name,				\
		.type		= OWL_COMPOSITE_TYPE_FIXED_FACTOR,	\
		.flags		= _flags,				\
		.mux		= _mux,					\
		.gate		= _gate,				\
		.rate.fixed_factor = _fixed_factor,			\
	}

#define COMP_DIV_CLK(_id, _name, _flags, _mux, _gate, _div)		\
	{								\
		.id		= _id,					\
		.name		= _name,				\
		.type		= OWL_COMPOSITE_TYPE_DIVIDER,		\
		.flags		= _flags,				\
		.mux		= _mux,					\
		.gate		= _gate,				\
		.rate.div	= _div,					\
	}

#define COMP_FACTOR_CLK(_id, _name, _flags, _mux, _gate, _factor)	\
	{								\
		.id		= _id,					\
		.name		= _name,				\
		.type		= OWL_COMPOSITE_TYPE_FACTOR,		\
		.flags		= _flags,				\
		.mux		= _mux,					\
		.gate		= _gate,				\
		.rate.factor	= _factor,				\
	}

#define COMP_PASS_CLK(_id, _name, _flags, _mux, _gate)			\
	{								\
		.id		= _id,					\
		.name		= _name,				\
		.type		= OWL_COMPOSITE_TYPE_PASS,		\
		.flags		= _flags,				\
		.mux		= _mux,					\
		.gate		= _gate,				\
	}


#define C_MUX(p, o, s, w, mf)						\
	{								\
		.id		= -1,					\
		.parent_names	= p,					\
		.num_parents	= ARRAY_SIZE(p),			\
		.offset		= o,					\
		.shift		= s,					\
		.width		= w,					\
		.mux_flags	= mf,					\
	}

/* fixed mux, only one parent */
#define C_MUX_F(p, mf)							\
	{								\
		.id		= -1,					\
		.parent_names	= p,					\
		.num_parents	= 1,					\
		.mux_flags = mf,					\
	}

#define C_GATE(o, b, gf)						\
	{								\
		.id		= -1,					\
		.offset		= o,					\
		.bit_idx	= b,					\
		.gate_flags	= gf,					\
	}

#define C_NULL								\
	{								\
		.id		= 0,					\
	}

#define C_FIXED_RATE(r)							\
	{								\
		.id		= -1,					\
		.fixed_rate	= r,					\
	}

#define C_FIXED_FACTOR(m, d)						\
	{								\
		.id		= -1,					\
		.mult		= m,					\
		.div		= d,					\
	}

#define C_DIVIDER(o, s, w, t, df)					\
	{								\
		.id		= -1,					\
		.offset		= o,					\
		.shift		= s,					\
		.width		= w,					\
		.table		= t,					\
		.div_flags	= df,					\
	}

#define C_FACTOR(o, s, w, t, df)					\
	{								\
		.id		= -1,					\
		.offset		= o,					\
		.shift		= s,					\
		.width		= w,					\
		.table		= t,					\
		.div_flags	= df,					\
	}

extern struct owl_clk_provider * __init owl_clk_init(struct device_node *np,
		void __iomem *base, unsigned long nr_clks);

extern void __init owl_clk_register_fixed_rate(struct owl_clk_provider *ctx,
		struct owl_fixed_rate_clock *clks, int nums);

extern void __init owl_clk_register_pll(struct owl_clk_provider *ctx,
		struct owl_pll_clock *clks, int nums);

extern void __init owl_clk_register_fixed_factor(
		struct owl_clk_provider *ctx,
		struct owl_fixed_factor_clock *clks,
		int nums);

extern void __init owl_clk_register_divider(struct owl_clk_provider *ctx,
		struct owl_divider_clock *clks, int nums);

extern void __init owl_clk_register_factor(struct owl_clk_provider *ctx,
		struct owl_factor_clock *clks, int nums);

extern void __init owl_clk_register_mux(struct owl_clk_provider *ctx,
		struct owl_mux_clock *clks, int nums);

extern void __init owl_clk_register_gate(struct owl_clk_provider *ctx,
		struct owl_gate_clock *clks, int nums);

extern void __init owl_clk_register_composite(struct owl_clk_provider *ctx,
		struct owl_composite_clock *clks, int nums);

extern struct clk *owl_pll_clk_register(const char *name, const char *parent_name,
		unsigned long flags, void __iomem *reg,
		unsigned long bfreq, u8 enable_bit, u8 shift, u8 width,
		u8 min_mul, u8 max_mul, u8 pll_flags,
		const struct clk_pll_table *table, spinlock_t *lock);

extern struct clk *owl_factor_clk_register(struct device *dev,
		const char *name, const char *parent_name,
		unsigned long flags, void __iomem *reg, u8 shift,
		u8 width, u8 clk_factor_flags,
		const struct clk_factor_table *table, spinlock_t *lock);

extern unsigned long _owl_corepll_recalc_rate(void);
extern int _owl_corepll_set_rate(unsigned long rate);
#endif /* __OWL_CLK_H */
