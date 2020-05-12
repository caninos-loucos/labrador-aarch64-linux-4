// SPDX-License-Identifier: GPL-2.0
/*
 * Fixed Factor clock implementation for Caninos Labrador
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

#define to_caninos_fixed_factor(_hw) \
    container_of(_hw, struct caninos_fixed_factor, hw)

static unsigned long caninos_fixed_factor_recalc_rate(struct clk_hw *hw,
                                                      unsigned long prate)
{
    struct caninos_fixed_factor *fixed_factor = to_caninos_fixed_factor(hw);
    unsigned long parent_rate;
    struct clk_hw *parent;
    
    parent = clk_hw_get_parent(hw);
    
    if (!parent) {
        return -EINVAL;
    }
    
    parent_rate = clk_hw_get_rate(parent);
    
    return mult_frac(parent_rate, fixed_factor->mult, fixed_factor->div);
}

static unsigned long caninos_fixed_factor_recalc_accuracy
    (struct clk_hw *hw, unsigned long parent_accuracy)
{
    return 0;
}

const struct clk_ops caninos_fixed_factor_ops = {
    .recalc_rate = caninos_fixed_factor_recalc_rate,
    .recalc_accuracy = caninos_fixed_factor_recalc_accuracy,
};

struct clk *caninos_register_fixed_factor(const char *name,
    const char *parent_name, void __iomem *reg, unsigned int mult,
    unsigned int div, spinlock_t *lock)
{
    struct caninos_fixed_factor *fixed_factor;
    struct clk_init_data init = {};
    struct clk_hw *hw;
    int ret;
    
    if (!parent_name) {
        return ERR_PTR(-EINVAL);
    }
    
    fixed_factor = kzalloc(sizeof(*fixed_factor), GFP_KERNEL);
    
    if (!fixed_factor) {
        return ERR_PTR(-ENOMEM);
    }
    
    init.name = name;
    init.flags = CLK_IGNORE_UNUSED;
    init.parent_names = &parent_name;
    init.num_parents = 1;
    init.ops = &caninos_fixed_factor_ops;
    
    fixed_factor->mult = mult;
	fixed_factor->div = div;
    fixed_factor->lock = lock;
    
    fixed_factor->hw.init = &init;
    hw = &fixed_factor->hw;
    
    ret = clk_hw_register(NULL, hw);
    
    if (ret)
    {
        kfree(fixed_factor);
        return ERR_PTR(ret);
    }
    
    return hw->clk;
}

