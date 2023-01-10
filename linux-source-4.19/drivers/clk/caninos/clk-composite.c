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

#include "clk-caninos.h"
#include "clk-helpers.h"

struct caninos_composite
{
	struct clk_hw hw;
	int id;
	void __iomem *reg;
	spinlock_t *lock;
	unsigned int table_count;
	struct caninos_composite_data data;
};

static inline struct caninos_composite* to_caninos_comp(struct clk_hw *_hw) {
	return container_of(_hw, struct caninos_composite, hw);
}

static unsigned long
calc_factor_rate_from_val(const struct caninos_composite *comp,
                          u32 val, unsigned long parent_rate)
{
	const struct clk_factor_table *table = comp->data.rate.factor_table;
	unsigned int idx;
	
	for (idx = 0U; idx < comp->table_count; idx++)
	{
		if (table[idx].val == val)
		{
			const unsigned long mul = (unsigned long)(table[idx].mul);
			const unsigned long div = (unsigned long)(table[idx].div);
			
			if (div != 0UL) {
				return mult_frac(parent_rate, mul, div);
			}
		}
	}
	return 0UL;
}

static unsigned long
calc_simple_divider_rate_from_val(u32 val, unsigned long parent_rate)
{
	const unsigned long div = (unsigned long)(val) + 1UL;
	
	if (div != 0UL) {
		return DIV_ROUND_CLOSEST(parent_rate, div);
	}
	return 0UL;
}

static unsigned long
calc_divider_rate_from_val(const struct caninos_composite *comp,
                           u32 val, unsigned long parent_rate)
{
	const struct clk_div_table *table = comp->data.rate.div_table;
	unsigned int idx;
	
	for (idx = 0U; idx < comp->table_count; idx++)
	{
		if (table[idx].val == val)
		{
			const unsigned long div = (unsigned long)(table[idx].div);
			
			if (div != 0UL) {
				return DIV_ROUND_CLOSEST(parent_rate, div);
			}
		}
	}
	return 0UL;
}

static unsigned long
calc_best_factor(const struct caninos_composite *comp, unsigned long rate,
                 unsigned long parent_rate, u32 *val)
{
	const struct clk_factor_table *table = comp->data.rate.factor_table;
	unsigned long best_rate = 0UL, best_error = ULONG_MAX;
	unsigned long curr_rate, error;
	unsigned int idx;
	u32 best_val;
	
	if (parent_rate == 0UL) {
		return 0UL;
	}
	if (rate == 0UL) {
		rate++;
	}
	for (idx = 0U; idx < comp->table_count; idx++)
	{
		const unsigned long mul = (unsigned long)(table[idx].mul);
		const unsigned long div = (unsigned long)(table[idx].div);
		
		if (div == 0UL) {
			return 0UL;
		}
		
		curr_rate = mult_frac(parent_rate, mul, div);
		error = (rate > curr_rate) ? (rate - curr_rate) : (curr_rate - rate);
		
		if (error < best_error)
		{
			best_error = error;
			best_rate = curr_rate;
			best_val = table[idx].val;
		}
	}
	if ((best_rate != 0UL) && (val != NULL)) {
		*val = best_val;
	}
	return best_rate;
}

static unsigned long 
calc_best_simple_divider(const struct caninos_composite *comp,
                         unsigned long rate, unsigned long parent_rate,
                         u32 *val)
{
	const u32 width = comp->data.rate.width;
	unsigned long div;
	u32 best_val;
	
	if (parent_rate == 0UL) {
		return 0UL;
	}
	if (rate == 0UL) {
		rate++;
	}
	
	div = DIV_ROUND_CLOSEST(parent_rate, rate);
	div = min(div, (unsigned long)(U32_MAX));
	
	best_val = min((u32)(div - 1UL), MASK(width, 0U));
	div = (unsigned long)(best_val) + 1UL;
	
	rate = DIV_ROUND_CLOSEST(parent_rate, div);
	
	if (val) {
		*val = best_val;
	}
	return rate;
}

static unsigned long 
calc_best_divider(const struct caninos_composite *comp, unsigned long rate,
                  unsigned long parent_rate, u32 *val)
{
	const struct clk_div_table *table = comp->data.rate.div_table;
	unsigned long best_rate = 0UL, best_error = ULONG_MAX;
	unsigned long curr_rate, error;
	unsigned int idx;
	u32 best_val;
	
	if (parent_rate == 0UL) {
		return 0UL;
	}
	if (rate == 0UL) {
		rate++;
	}
	for (idx = 0U; idx < comp->table_count; idx++)
	{
		const unsigned long div = (unsigned long)(table[idx].div);
		
		if (div == 0UL) {
			return 0UL;
		}
		
		curr_rate = DIV_ROUND_CLOSEST(parent_rate, div);
		error = (rate > curr_rate) ? (rate - curr_rate) : (curr_rate - rate);
		
		if (error < best_error)
		{
			best_error = error;
			best_rate = curr_rate;
			best_val = table[idx].val;
		}
	}
	if ((best_rate != 0UL) && (val != NULL)) {
		*val = best_val;
	}
	return best_rate;
}

static long
comp_table_factor_round_rate(struct clk_hw *hw, unsigned long rate,
                             unsigned long *prate)
{
	unsigned long new_rate, parent_rate = (prate == NULL) ? 0UL : *prate;
	struct caninos_composite *comp = to_caninos_comp(hw);
	new_rate = calc_best_factor(comp, rate, parent_rate, NULL);
	return (long)(new_rate);
}

static long 
comp_table_divider_round_rate(struct clk_hw *hw, unsigned long rate,
                              unsigned long *prate)
{
	unsigned long new_rate, parent_rate = (prate == NULL) ? 0UL : *prate;
	struct caninos_composite *comp = to_caninos_comp(hw);
	new_rate = calc_best_divider(comp, rate, parent_rate, NULL);
	return (long)(new_rate);
}

static long 
comp_simple_divider_round_rate(struct clk_hw *hw, unsigned long rate,
                               unsigned long *prate)
{
	unsigned long new_rate, parent_rate = (prate == NULL) ? 0UL : *prate;
	struct caninos_composite *comp = to_caninos_comp(hw);
	new_rate = calc_best_simple_divider(comp, rate, parent_rate, NULL);
	return (long)(new_rate);
}

static int
comp_table_divider_set_rate(struct clk_hw *hw, unsigned long rate,
                            unsigned long parent_rate)
{
	struct caninos_composite *comp = to_caninos_comp(hw);
	void __iomem *rate_reg = comp->reg + comp->data.rate.offset;
	const u32 width = comp->data.rate.width;
	const u32 shift = comp->data.rate.shift;
	unsigned long uninitialized_var(flags);
	u32 raw, val;
	
	if (calc_best_divider(comp, rate, parent_rate, &val) != 0UL)
	{
		spin_lock_irqsave(comp->lock, flags);
		
		raw = clk_readl(rate_reg);
		
		if (GET_VALUE(raw, width, shift) != val) {
			clk_writel(SET_VALUE(raw, val, width, shift), rate_reg);
		}
		
		spin_unlock_irqrestore(comp->lock, flags);
	}
	return 0;
}

static int
comp_table_factor_set_rate(struct clk_hw *hw, unsigned long rate,
                           unsigned long parent_rate)
{
	struct caninos_composite *comp = to_caninos_comp(hw);
	void __iomem *rate_reg = comp->reg + comp->data.rate.offset;
	const u32 width = comp->data.rate.width;
	const u32 shift = comp->data.rate.shift;
	unsigned long uninitialized_var(flags);
	u32 raw, val;
	
	if (calc_best_factor(comp, rate, parent_rate, &val) != 0UL)
	{
		spin_lock_irqsave(comp->lock, flags);
		
		raw = clk_readl(rate_reg);
		
		if (GET_VALUE(raw, width, shift) != val) {
			clk_writel(SET_VALUE(raw, val, width, shift), rate_reg);
		}
		
		spin_unlock_irqrestore(comp->lock, flags);
	}
	return 0;
}

static unsigned long
comp_fixed_factor_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct caninos_composite *comp = to_caninos_comp(hw);
	const unsigned long mul = comp->data.rate.mult;
	const unsigned long div = comp->data.rate.div;
	unsigned long rate = 0UL;
	
	if ((parent_rate != 0UL) && (div != 0UL)) {
		rate = mult_frac(parent_rate, mul, div);
	}
	return rate;
}

static unsigned long
comp_table_factor_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct caninos_composite *comp = to_caninos_comp(hw);
	void __iomem *rate_reg = comp->reg + comp->data.rate.offset;
	const u32 width = comp->data.rate.width;
	const u32 shift = comp->data.rate.shift;
	unsigned long uninitialized_var(flags);
	u32 val;
	
	val = GET_VALUE(clk_readl(rate_reg), width, shift);
	
	return calc_factor_rate_from_val(comp, val, parent_rate);
}

static unsigned long
comp_table_divider_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct caninos_composite *comp = to_caninos_comp(hw);
	void __iomem *rate_reg = comp->reg + comp->data.rate.offset;
	const u32 width = comp->data.rate.width;
	const u32 shift = comp->data.rate.shift;
	unsigned long uninitialized_var(flags);
	u32 val;
	
	val = GET_VALUE(clk_readl(rate_reg), width, shift);
	
	return calc_divider_rate_from_val(comp, val, parent_rate);
}

static unsigned long
comp_simple_divider_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct caninos_composite *comp = to_caninos_comp(hw);
	void __iomem *rate_reg = comp->reg + comp->data.rate.offset;
	const u32 width = comp->data.rate.width;
	const u32 shift = comp->data.rate.shift;
	unsigned long uninitialized_var(flags);
	u32 val;
	
	val = GET_VALUE(clk_readl(rate_reg), width, shift);
	
	return calc_simple_divider_rate_from_val(val, parent_rate);
}

static int
comp_simple_divider_set_rate(struct clk_hw *hw, unsigned long rate,
                             unsigned long parent_rate)
{
	struct caninos_composite *comp = to_caninos_comp(hw);
	void __iomem *rate_reg = comp->reg + comp->data.rate.offset;
	const u32 width = comp->data.rate.width;
	const u32 shift = comp->data.rate.shift;
	unsigned long uninitialized_var(flags);
	u32 val, raw;
	
	if (calc_best_simple_divider(comp, rate, parent_rate, &val) != 0UL)
	{
		spin_lock_irqsave(comp->lock, flags);
		
		raw = clk_readl(rate_reg);
		
		if (GET_VALUE(raw, width, shift) != val) {
			clk_writel(SET_VALUE(raw, val, width, shift), rate_reg);
		}
		
		spin_unlock_irqrestore(comp->lock, flags);
	}
	return 0;
}

static int comp_gate_enable(struct clk_hw *hw)
{
	struct caninos_composite *comp = to_caninos_comp(hw);
	void __iomem *gate_reg = comp->reg + comp->data.gate.offset;
	const u32 bit_idx = comp->data.gate.bit_idx;
	unsigned long uninitialized_var(flags);
	u32 val;
	
	spin_lock_irqsave(comp->lock, flags);
	
	val = clk_readl(gate_reg);
	
	if (GET_BIT(val, bit_idx) == false) {
		clk_writel(SET_BIT(val, bit_idx), gate_reg);
	}
	
	spin_unlock_irqrestore(comp->lock, flags);
	return 0;
}

static void comp_gate_disable(struct clk_hw *hw)
{
	struct caninos_composite *comp = to_caninos_comp(hw);
	void __iomem *gate_reg = comp->reg + comp->data.gate.offset;
	const u32 bit_idx = comp->data.gate.bit_idx;
	unsigned long uninitialized_var(flags);
	u32 val;
	
	spin_lock_irqsave(comp->lock, flags);
	
	val = clk_readl(gate_reg);
	
	if (GET_BIT(val, bit_idx) == true) {
		clk_writel(CLEAR_BIT(val, bit_idx), gate_reg);
	}
	
	spin_unlock_irqrestore(comp->lock, flags);
}

static int comp_gate_is_enabled(struct clk_hw *hw)
{
	struct caninos_composite *comp = to_caninos_comp(hw);
	void __iomem *gate_reg = comp->reg + comp->data.gate.offset;
	return GET_BIT(clk_readl(gate_reg), comp->data.gate.bit_idx) ? 1 : 0;
}

static int comp_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct caninos_composite *comp = to_caninos_comp(hw);
	void __iomem *mux_reg = comp->reg + comp->data.mux.offset;
	const u32 width = comp->data.mux.width;
	const u32 shift = comp->data.mux.shift;
	unsigned long uninitialized_var(flags);
	u32 val;
	
	spin_lock_irqsave(comp->lock, flags);
	
	val = clk_readl(mux_reg);
	
	if (GET_MUX(val, width, shift) != index) {
		clk_writel(SET_MUX(val, index, width, shift), mux_reg);
	}
	
	spin_unlock_irqrestore(comp->lock, flags);
	return 0;
}

static u8 comp_mux_get_parent(struct clk_hw *hw)
{
	struct caninos_composite *comp = to_caninos_comp(hw);
	void __iomem *mux_reg = comp->reg + comp->data.mux.offset;
	const u32 width = comp->data.mux.width;
	const u32 shift = comp->data.mux.shift;
	return GET_MUX(clk_readl(mux_reg), width, shift);
}

static const struct clk_ops caninos_comp_rate_simple_divider_ops = {
	.recalc_rate = comp_simple_divider_recalc_rate,
	.round_rate = comp_simple_divider_round_rate,
	.set_rate = comp_simple_divider_set_rate,
};

static const struct clk_ops caninos_comp_rate_table_divider_ops = {
	.recalc_rate = comp_table_divider_recalc_rate,
	.round_rate = comp_table_divider_round_rate,
	.set_rate = comp_table_divider_set_rate,
};

static const struct clk_ops caninos_comp_rate_table_factor_ops = {
	.recalc_rate = comp_table_factor_recalc_rate,
	.round_rate = comp_table_factor_round_rate,
	.set_rate = comp_table_factor_set_rate,
};

static const struct clk_ops caninos_comp_rate_fixed_factor_ops = {
	.recalc_rate = comp_fixed_factor_recalc_rate,
};

static const struct clk_ops caninos_comp_gate_ops = {
	.is_enabled = comp_gate_is_enabled,
	.enable = comp_gate_enable,
	.disable = comp_gate_disable,
};

static const struct clk_ops caninos_comp_mux_ops = {
	.set_parent = comp_mux_set_parent,
	.get_parent = comp_mux_get_parent,
	.determine_rate = __clk_mux_determine_rate,
};

struct clk * __init
caninos_register_composite(const struct caninos_composite_clock *info,
                           struct device *dev, void __iomem *reg_base,
                           spinlock_t *lock)
{
	const struct caninos_composite_data *data = &info->data;
	struct clk_factor_table *factor_table = NULL;
	struct clk_div_table *div_table = NULL;
	const struct clk_ops *rate_ops = NULL;
	const struct clk_ops *gate_ops = NULL;
	const struct clk_ops *mux_ops = NULL;
	struct caninos_composite *comp;
	unsigned int count = 0;
	struct clk_hw *hw;
	
	if (data->gate.offset >= 0L)
	{
		if (data->gate.bit_idx >= 32U) {
			return ERR_PTR(-EINVAL);
		}
		gate_ops = &caninos_comp_gate_ops;
	}
	
	if (data->mux.offset >= 0L)
	{
		if (data->mux.width == 0U) {
			return ERR_PTR(-EINVAL);
		}
		if ((data->mux.width + data->mux.shift) > 32U) {
			return ERR_PTR(-EINVAL);
		}
		mux_ops = &caninos_comp_mux_ops;
	}
	
	/* this clock must have at least one valid parent */
	if ((info->num_parents == 0U) || (info->parent_names == NULL)) {
		return ERR_PTR(-EINVAL);
	}
	/* if this doesn't have a mux, limit it to just one parent */
	if ((mux_ops == NULL) && (info->num_parents > 1U)) {
		return ERR_PTR(-EINVAL);
	}
	
	if (data->rate.offset >= 0L)
	{
		bool fixed_factor = false;
		unsigned int len = 0U;
		
		if (data->rate.div_table != NULL)
		{
			/* cannot be divider and factor at the same time */
			if (data->rate.factor_table != NULL) {
				return ERR_PTR(-EINVAL);
			}
			
			rate_ops = &caninos_comp_rate_table_divider_ops;
			
			/* calculate the number of elements in div_table */
			while (data->rate.div_table[count].div != 0U) {
				count++;
			}
			if (count == 0U) {
				return ERR_PTR(-EINVAL);
			}
			len = (count + 1U) * sizeof(*div_table);
			
			/* allocate space for a copy of div_table */
			if ((div_table = kzalloc(len, GFP_KERNEL)) == NULL) {
				return ERR_PTR(-ENOMEM);
			}
			
			/* copy div_table to allow __initconst*/
			memcpy(div_table, data->rate.div_table, len);
		}
		else if (data->rate.factor_table != NULL)
		{
			rate_ops = &caninos_comp_rate_table_factor_ops;
			
			/* calculate the number of elements in factor_table */
			while (data->rate.factor_table[count].div != 0U) {
				count++;
			}
			if (count == 0U) {
				return ERR_PTR(-EINVAL);
			}
			len = (count + 1U) * sizeof(*factor_table);
			
			/* allocate space for a copy of factor_table */
			if ((factor_table = kzalloc(len, GFP_KERNEL)) == NULL) {
				return ERR_PTR(-ENOMEM);
			}
			
			/* copy factor_table to allow __initconst */
			memcpy(factor_table, data->rate.factor_table, len);
		}
		else if ((data->rate.mult != 0U) && (data->rate.div != 0U))
		{
			rate_ops = &caninos_comp_rate_fixed_factor_ops;
			fixed_factor = true;
		}
		else {
			rate_ops = &caninos_comp_rate_simple_divider_ops;
		}
		
		if (!fixed_factor)
		{
			if (data->rate.width == 0U) {
				return ERR_PTR(-EINVAL);
			}
			if ((data->rate.width + data->rate.shift) > 32U) {
				return ERR_PTR(-EINVAL);
			}
		}
	}
	
	if ((comp = kzalloc(sizeof(*comp), GFP_KERNEL)) == NULL)
	{
		if (div_table) {
			kfree(div_table);
		}
		if (factor_table) {
			kfree(factor_table);
		}
		return ERR_PTR(-ENOMEM);
	}
	
	/* copy data to allow __initconst */
	memcpy(&comp->data, data, sizeof(*data));
	
	comp->id = info->id;
	comp->reg = reg_base;
	comp->lock = lock;
	comp->data.rate.factor_table = factor_table;
	comp->data.rate.div_table = div_table;
	comp->table_count = count;
	
	hw = clk_hw_register_composite(dev, info->name, info->parent_names,
	                               info->num_parents, &comp->hw, mux_ops,
	                               &comp->hw, rate_ops, &comp->hw, gate_ops,
	                               info->flags);
	
	if (IS_ERR(hw))
	{
		kfree(comp);
		
		if (div_table) {
			kfree(div_table);
		}
		if (factor_table) {
			kfree(factor_table);
		}
		
		return ERR_CAST(hw);
	}
	return hw->clk;
}

