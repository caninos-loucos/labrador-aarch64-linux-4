// SPDX-License-Identifier: GPL-2.0
/*
 * Adjustable divider clock implementation for Caninos Labrador
 * Copyright (c) 2018-2020 LSI-TEC - Caninos Loucos
 * Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2014 David Liu, Actions Semi Inc <liuwei@actions-semi.com>
 * Copyright (c) 2011 Sascha Hauer, Pengutronix <s.hauer@pengutronix.de>
 * Copyright (c) 2011 Richard Zhao, Linaro <richard.zhao@linaro.org>
 * Copyright (c) 2011-2012 Mike Turquette, Linaro Ltd <mturquette@linaro.org>
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

/*
 * DOC: basic adjustable divider clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clk_prepare only ensures that parents are prepared
 * enable - clk_enable only ensures that parents are enabled
 * rate - rate is adjustable. clk->rate = ceiling(parent->rate / divisor)
 * parent - fixed parent. No clk_set_parent support
 */

#define to_caninos_divider(_hw) container_of(_hw, struct caninos_divider, hw)
#define _mask(m) ((BIT((m)->width)) - 1)

static long caninos_divider_ro_round_rate(struct clk_hw *hw,
    unsigned long rate, unsigned long *prate)
{
    struct caninos_divider *divider = to_caninos_divider(hw);
    u32 val;
    
    val = clk_readl(divider->reg) >> divider->shift;
    val &= _mask(divider);
    
    return divider_ro_round_rate(hw, rate, prate, divider->table,
                                 divider->width, divider->flags, val);
}

static long caninos_divider_round_rate(struct clk_hw *hw, unsigned long rate,
    unsigned long *prate)
{
    struct caninos_divider *divider = to_caninos_divider(hw);
    
    return divider_round_rate(hw, rate, prate, divider->table,
                              divider->width, divider->flags);
}

static unsigned long caninos_divider_recalc_rate(struct clk_hw *hw,
                                                 unsigned long parent_rate)
{
    struct caninos_divider *divider = to_caninos_divider(hw);
    u32 val;
    
    val = clk_readl(divider->reg) >> divider->shift;
    val &= _mask(divider);
    
    return divider_recalc_rate(hw, parent_rate, val, divider->table,
                               divider->flags, divider->width);
}

static int caninos_divider_set_rate(struct clk_hw *hw, unsigned long rate,
                                    unsigned long parent_rate)
{
    struct caninos_divider *divider = to_caninos_divider(hw);
    unsigned long uninitialized_var(flags);
    int value;
    u32 val;
    
    value = divider_get_val(rate, parent_rate, divider->table,
                            divider->width, divider->flags);
    
    if (value < 0) {
        return value;
    }
    
    if (divider->lock) {
        spin_lock_irqsave(divider->lock, flags);
    }
    else {
        __acquire(divider->lock);
    }
    
    val = clk_readl(divider->reg);
    val &= ~(_mask(divider) << divider->shift);
	
    val |= (u32)value << divider->shift;
    clk_writel(val, divider->reg);
    
    if (divider->lock) {
        spin_unlock_irqrestore(divider->lock, flags);
    }
    else {
        __release(divider->lock);
    }
    
    return 0;
}

const struct clk_ops caninos_divider_ops = {
    .recalc_rate = caninos_divider_recalc_rate,
    .round_rate = caninos_divider_round_rate,
    .set_rate = caninos_divider_set_rate,
};

const struct clk_ops caninos_divider_ro_ops = {
    .recalc_rate = caninos_divider_recalc_rate,
    .round_rate = caninos_divider_ro_round_rate,
};

struct clk *caninos_register_div(const struct caninos_div_clock *info,
                                 void __iomem *reg, spinlock_t *lock)
{
    struct clk_init_data init = {};
    struct caninos_divider *div;
    struct clk_hw *hw;
    int ret;
    
    div = kzalloc(sizeof(*div), GFP_KERNEL);
    
    if (!div) {
        return ERR_PTR(-ENOMEM);
    }
    
    div->reg = reg + info->offset;
    div->shift = info->shift;
    div->width = info->width;
    div->flags = 0;
    div->lock = lock;
    div->table = info->table;
    
    init.name = info->name;
    init.flags = CLK_IGNORE_UNUSED;
    init.parent_names = (info->parent_name ? &info->parent_name: NULL);
    init.num_parents = (info->parent_name ? 1 : 0);
    
    if (info->div_flags & CANINOS_CLK_ROUND_RATE_CLOSEST) {
        div->flags |= CLK_DIVIDER_ROUND_CLOSEST;
    }
    
    if (info->div_flags & CANINOS_CLK_RATE_READ_ONLY)
    {
        div->flags |= CLK_DIVIDER_READ_ONLY;
        init.ops = &caninos_divider_ro_ops;
    }
    else {
        init.ops = &caninos_divider_ops;
    }
    
    if (info->div_flags & CANINOS_CLK_SET_RATE_PARENT) {
        init.flags |= CLK_SET_RATE_PARENT;
    }
    
    div->hw.init = &init;
    hw = &div->hw;
    
    ret = clk_hw_register(NULL, hw);
    
    if (ret)
    {
        kfree(div);
        return ERR_PTR(ret);
    }
    
    return hw->clk;
}

