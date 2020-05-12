// SPDX-License-Identifier: GPL-2.0
/*
 * Factor clock implementation for Caninos Labrador
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

#include <linux/clk-provider.h>
#include "clk-caninos.h"
#include "clk-priv.h"

#define to_caninos_factor(_hw) container_of(_hw, struct caninos_factor, hw)
#define _mask(m) ((BIT((m)->width)) - 1)

static long caninos_factor_round_rate(struct clk_hw *hw, unsigned long rate,
                                      unsigned long *prate)
{
    struct caninos_factor *factor = to_caninos_factor(hw);
    unsigned long parent_rate, best_rate, new_rate;
    const struct clk_factor_table *clkt;
    struct clk_hw *parent;
    
    parent = clk_hw_get_parent(hw);
    
    if (!parent) {
        return -EINVAL;
    }
    
    parent_rate = clk_hw_get_rate(parent);
    
    for (best_rate = 0, clkt = factor->table; clkt->div; clkt++)
	{
	    new_rate = mult_frac(parent_rate, clkt->mul, clkt->div);
	    
        if ((new_rate <= rate) && (new_rate > best_rate)) {
            best_rate = new_rate;
        }
    }
    
    if (prate) {
        *prate = parent_rate;
    }
    return best_rate;
}

static unsigned long caninos_factor_recalc_rate(struct clk_hw *hw,
                                                unsigned long prate)
{
    struct caninos_factor *factor = to_caninos_factor(hw);
    const struct clk_factor_table *clkt;
    u32 val;
    
    val = clk_readl(factor->reg) >> factor->shift;
	val &= _mask(factor);
	
	for (clkt = factor->table; clkt->div; clkt++)
	{
        if (clkt->val == val) {
            break;
        }
    }
    
    if (!clkt->div) {
        return 0;
    }
    
	return mult_frac(prate, clkt->mul, clkt->div);
}

static int caninos_factor_set_rate(struct clk_hw *hw, unsigned long rate,
                                   unsigned long prate)
{
    struct caninos_factor *factor = to_caninos_factor(hw);
    const struct clk_factor_table *clkt, *best_clkt = NULL;
    unsigned long parent_rate, best_rate, new_rate;
    unsigned long uninitialized_var(flags);
    u32 val;
    
    parent_rate = clk_hw_get_rate(clk_hw_get_parent(hw));
    
    for (best_rate = 0, clkt = factor->table; clkt->div; clkt++)
	{
	    new_rate = mult_frac(parent_rate, clkt->mul, clkt->div);
	    
        if ((new_rate <= rate) && (new_rate > best_rate))
        {
            best_rate = new_rate;
            best_clkt = clkt;
        }
    }
    
    if (best_rate == 0) {
        return -EINVAL;
    }
    
    if (factor->lock) {
        spin_lock_irqsave(factor->lock, flags);
    }
    else {
        __acquire(factor->lock);
    }
    
    val = clk_readl(factor->reg);
    val &= ~(_mask(factor) << factor->shift);
    val |= best_clkt->val << factor->shift;
    clk_writel(val, factor->reg);
    
    if (factor->lock) {
        spin_unlock_irqrestore(factor->lock, flags);
    }
    else {
        __release(factor->lock);
    }
    
    return 0;
}

const struct clk_ops caninos_factor_ops = {
    .recalc_rate = caninos_factor_recalc_rate,
    .round_rate = caninos_factor_round_rate,
    .set_rate = caninos_factor_set_rate,
};

struct clk *caninos_register_factor_table(const char *name,
    const char *parent_name, void __iomem *reg, u8 shift, u8 width,
    const struct clk_factor_table *table, spinlock_t *lock)
{
    struct clk_init_data init = {};
    struct caninos_factor *factor;
    struct clk_hw *hw;
    int ret;
    
    if (!parent_name) {
        return ERR_PTR(-EINVAL);
    }
    
    factor = kzalloc(sizeof(*factor), GFP_KERNEL);
    
    if (!factor) {
        return ERR_PTR(-ENOMEM);
    }
    
    init.name = name;
    init.flags = CLK_IGNORE_UNUSED;
    init.parent_names = &parent_name;
    init.num_parents = 1;
    init.ops = &caninos_factor_ops;
    
    factor->reg = reg;
    factor->shift = shift;
    factor->width = width;
    factor->lock = lock;
    factor->table = table;
    factor->hw.init = &init;
    hw = &factor->hw;
    
    ret = clk_hw_register(NULL, hw);
    
    if (ret)
    {
        kfree(factor);
        return ERR_PTR(ret);
    }
    
    return hw->clk;
}

