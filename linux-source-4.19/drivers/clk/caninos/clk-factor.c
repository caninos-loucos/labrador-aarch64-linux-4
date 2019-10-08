/*
 * Actions SOC facter divider table clock driver
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

#include <linux/clk-provider.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include "clk.h"

#define to_owl_factor(_hw)	container_of(_hw, struct owl_factor, hw)
#define div_mask(d)		((1 << ((d)->width)) - 1)

static unsigned int _get_table_maxval(const struct clk_factor_table *table)
{
	unsigned int maxval = 0;
	const struct clk_factor_table *clkt;

	for (clkt = table; clkt->div; clkt++)
		if (clkt->val > maxval)
			maxval = clkt->val;
	return maxval;
}

static int _get_table_div_mul(const struct clk_factor_table *table,
			unsigned int val, unsigned int *mul, unsigned int *div)
{
	const struct clk_factor_table *clkt;

	for (clkt = table; clkt->div; clkt++) {
		if (clkt->val == val) {
			*mul = clkt->mul;
			*div = clkt->div;
			return 1;
		}
	}
	return 0;
}

static unsigned int _get_table_val(const struct clk_factor_table *table,
			unsigned long rate, unsigned long parent_rate)
{
	const struct clk_factor_table *clkt;
	int val = -1;
	u64 calc_rate;

	for (clkt = table; clkt->div; clkt++) {
		calc_rate = parent_rate * clkt->mul;
		do_div(calc_rate, clkt->div);

		if ((unsigned long)calc_rate <= rate) {
			val = clkt->val;
			break;
		}
	}

	if (val == -1)
		val = _get_table_maxval(table);

	return val;
}

static int clk_val_best(struct clk_hw *hw, unsigned long rate,
		unsigned long *best_parent_rate)
{
	struct owl_factor *factor = to_owl_factor(hw);
	const struct clk_factor_table *clkt = factor->table;
	unsigned long parent_rate, try_parent_rate, best = 0, now;
	unsigned long parent_rate_saved = *best_parent_rate;
	unsigned int maxval;
	int bestval = 0;

	if (!rate)
		rate = 1;

	maxval = _get_table_maxval(clkt);

	if (!(__clk_get_flags(hw->clk) & CLK_SET_RATE_PARENT)) {
		parent_rate = *best_parent_rate;
		bestval = _get_table_val(clkt, rate, parent_rate);
		return bestval;
	}

	while (1) {
		if (!(clkt->div))
			break;

		try_parent_rate = rate * clkt->div / clkt->mul;

		if (try_parent_rate == parent_rate_saved) {
			pr_debug("%s: [%d %d %d] found try_parent_rate %ld\n",
				__func__, clkt->val, clkt->mul, clkt->div,
				try_parent_rate);
			/*
			 * It's the most ideal case if the requested rate can be
			 * divided from parent clock without needing to change
			 * parent rate, so return the divider immediately.
			 */
			*best_parent_rate = parent_rate_saved;
			return clkt->val;
		}

		parent_rate = clk_round_rate(clk_get_parent(hw->clk), try_parent_rate);
		
		now = DIV_ROUND_UP(parent_rate, clkt->div) * clkt->mul;
		
		if (now <= rate && now > best) {
			bestval = clkt->val;
			best = now;
			*best_parent_rate = parent_rate;
		}

		clkt++;
	}

	if (!bestval) {
		bestval = _get_table_maxval(clkt);
		*best_parent_rate = clk_round_rate(
				clk_get_parent(hw->clk), 1);
	}

	pr_debug("%s: return bestval %d\n", __func__, bestval);

	return bestval;
}

/**
 * owl_factor_round_rate() - Round a clock frequency
 * @hw:		Handle between common and hardware-specific interfaces
 * @rate:	Desired clock frequency
 * @prate:	Clock frequency of parent clock
 * Returns frequency closest to @rate the hardware can generate.
 */
static long owl_factor_round_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long *parent_rate)
{
	struct owl_factor *factor = to_owl_factor(hw);
	const struct clk_factor_table *clkt = factor->table;
	unsigned int val, mul = 0, div = 1;

	val = clk_val_best(hw, rate, parent_rate);
	_get_table_div_mul(clkt, val, &mul, &div);

	return *parent_rate * mul / div;
}

/**
 * owl_factor_recalc_rate() - Recalculate clock frequency
 * @hw:			Handle between common and hardware-specific interfaces
 * @parent_rate:	Clock frequency of parent clock
 * Returns current clock frequency.
 */
static unsigned long owl_factor_recalc_rate(struct clk_hw *hw,
			unsigned long parent_rate)
{
	struct owl_factor *factor = to_owl_factor(hw);
	const struct clk_factor_table *clkt = factor->table;
	u64 rate;
	u32 val, mul, div;

	div = 0;
	mul = 0;

	val = readl(factor->reg) >> factor->shift;
	val &= div_mask(factor);

	_get_table_div_mul(clkt, val, &mul, &div);
	if (!div) {
		WARN(!(factor->flags & CLK_DIVIDER_ALLOW_ZERO),
			"%s: Zero divisor and CLK_DIVIDER_ALLOW_ZERO not set\n",
			__clk_get_name(hw->clk));
		return parent_rate;
	}

	rate = (u64)parent_rate * mul;
	do_div(rate, div);

	return (unsigned long)rate;
}

static int owl_factor_set_rate(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	struct owl_factor *factor = to_owl_factor(hw);
	unsigned long flags = 0;
	u32 val, v;

	val = _get_table_val(factor->table, rate, parent_rate);

	pr_debug("%s: get_table_val %d\n", __func__, val);

	if (val > div_mask(factor))
		val = div_mask(factor);

	if (factor->lock)
		spin_lock_irqsave(factor->lock, flags);

	v = readl(factor->reg);
	v &= ~(div_mask(factor) << factor->shift);
	v |= val << factor->shift;
	writel(v, factor->reg);

	if (factor->lock)
		spin_unlock_irqrestore(factor->lock, flags);

	return 0;
}

struct clk_ops owl_factor_ops = {
	.round_rate	= owl_factor_round_rate,
	.recalc_rate	= owl_factor_recalc_rate,
	.set_rate	= owl_factor_set_rate,
};

/**
 * owl_factor_clk_register() - Register PLL with the clock framework
 * @name	PLL name
 * @parent	Parent clock name
 * @reg	Pointer to PLL control register
 * @pll_status	Pointer to PLL status register
 * @lock_index	Bit index to this PLL's lock status bit in @pll_status
 * @lock	Register lock
 * Returns handle to the registered clock.
 */
struct clk *owl_factor_clk_register(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		void __iomem *reg, u8 shift, u8 width,
		u8 clk_factor_flags, const struct clk_factor_table *table,
		spinlock_t *lock)

{
	struct owl_factor *factor;
	struct clk *clk;
	struct clk_init_data initd;

	/* allocate the factor */
	factor = kzalloc(sizeof(*factor), GFP_KERNEL);
	if (!factor) {
		pr_err("%s: could not allocate factor clk\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	initd.name = name;
	initd.ops = &owl_factor_ops;
	initd.flags = flags | CLK_IS_BASIC;
	initd.parent_names = (parent_name ? &parent_name : NULL);
	initd.num_parents = (parent_name ? 1 : 0);

	/* struct owl_factor assignments */
	factor->reg = reg;
	factor->shift = shift;
	factor->width = width;
	factor->flags = clk_factor_flags;
	factor->lock = lock;
	factor->hw.init = &initd;
	factor->table = table;

	/* register the clock */
	clk = clk_register(dev, &factor->hw);

	if (IS_ERR(clk))
		kfree(factor);

	return clk;
}
