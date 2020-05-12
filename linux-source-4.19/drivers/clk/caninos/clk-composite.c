// SPDX-License-Identifier: GPL-2.0
/*
 * Composite clock implementation for Caninos Labrador
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

struct clk *caninos_register_composite(struct caninos_composite_clock *cclk,
                                       void __iomem *reg_base,
                                       spinlock_t *lock)
{
    struct clk_hw *mux_hw = NULL;
    struct clk_hw *rate_hw = NULL;
    struct clk_hw *gate_hw = NULL;
    struct clk_hw *hw = NULL;
    
    struct caninos_mux *mux = NULL;
    struct caninos_gate *gate = NULL;
    struct caninos_fixed_rate *fixed = NULL;
    struct caninos_divider *divider = NULL;
    struct caninos_factor *factor = NULL;
    struct caninos_fixed_factor *fixed_factor = NULL;
    
    const struct clk_ops *mux_ops = NULL;
    const struct clk_ops *rate_ops = NULL;
    const struct clk_ops *gate_ops = NULL;
    
    const char **parent_names = NULL;
    int ret, i, num_parents = 0;
    
    if (cclk->mux.id)
    {
        num_parents = cclk->mux.num_parents;
        
        if (num_parents > 0)
        {
            parent_names = kzalloc(sizeof(char*) * num_parents, GFP_KERNEL);
            
            if (!parent_names)
            {
                ret = -ENOMEM;
                goto error;
            }
            
            for (i = 0; i < num_parents; i++)
            {
                parent_names[i] = kstrdup(cclk->mux.parent_names[i],
                                          GFP_KERNEL);
                if (!parent_names[i])
                {
                    ret = -ENOMEM;
                    goto error;
                }
            }
        }
    }
    
    if (num_parents > 1)
    {
        mux = kzalloc(sizeof(*mux), GFP_KERNEL);
        
        if (!mux)
        {
            ret = -ENOMEM;
            goto error;
        }
        
        mux->reg = reg_base + cclk->mux.offset;
        mux->shift = cclk->mux.shift;
        mux->width = cclk->mux.width;
        mux->flags = 0;
        mux->lock = lock;
        mux_hw = &mux->hw;
        mux_ops = &caninos_mux_ops;
    }
    
    if (cclk->gate.id)
    {
        gate = kzalloc(sizeof(*gate), GFP_KERNEL);
        
        if (!gate)
        {
            ret = -ENOMEM;
            goto error;
        }
        
        gate->reg = reg_base + cclk->gate.offset;
        gate->bit_idx = cclk->gate.bit_idx;

        gate->lock = lock;
        gate_hw = &gate->hw;
        gate_ops = &caninos_gate_ops;
    }
    
    switch(cclk->type)
    {
    case CANINOS_COMPOSITE_TYPE_FIXED_RATE:
        
        fixed = kzalloc(sizeof(*fixed), GFP_KERNEL);
        
        if (!fixed)
        {
            ret = -ENOMEM;
            goto error;
        }
        
        fixed->fixed_rate = cclk->rate.fixed.fixed_rate;
        
        rate_hw = &fixed->hw;
        rate_ops = &caninos_fixed_rate_ops;
        
        break;
        
    case CANINOS_COMPOSITE_TYPE_DIVIDER:
        
        divider = kzalloc(sizeof(*divider), GFP_KERNEL);
        
        if (!divider)
        {
            ret = -ENOMEM;
            goto error;
        }
        
        divider->reg = reg_base + cclk->rate.div.offset;
        divider->shift = cclk->rate.div.shift;
        divider->width = cclk->rate.div.width;
        divider->flags = 0;
        divider->lock = lock;
        divider->table = cclk->rate.div.table;
        
        rate_hw = &divider->hw;
        rate_ops = &caninos_divider_ops;
        
        break;
        
    case CANINOS_COMPOSITE_TYPE_FACTOR:
        
        factor = kzalloc(sizeof(*factor), GFP_KERNEL);
        
        if (!factor)
        {
            ret = -ENOMEM;
            goto error;
        }
        
        factor->reg = reg_base + cclk->rate.factor.offset;
        factor->shift = cclk->rate.factor.shift;
        factor->width = cclk->rate.factor.width;
        factor->lock = lock;
        factor->table = cclk->rate.factor.table;
        
        rate_hw = &factor->hw;
        rate_ops = &caninos_factor_ops;
        
        break;
        
    case CANINOS_COMPOSITE_TYPE_FIXED_FACTOR:
        
        fixed_factor = kzalloc(sizeof(*fixed_factor), GFP_KERNEL);
        
        if (!fixed_factor)
        {
            ret = -ENOMEM;
            goto error;
        }
        
        fixed_factor->mult = cclk->rate.fixed_factor.mult;
	    fixed_factor->div = cclk->rate.fixed_factor.div;
        fixed_factor->lock = lock;
        
        rate_hw = &fixed_factor->hw;
        rate_ops = &caninos_fixed_factor_ops;
        
        break;
    }
    
    hw = clk_hw_register_composite(NULL, cclk->name, parent_names, num_parents,
                                   mux_hw, mux_ops, rate_hw, rate_ops,
                                   gate_hw, gate_ops, CLK_IGNORE_UNUSED);
    
    if (IS_ERR(hw))
    {
        ret = PTR_ERR(hw);
        goto error;
    }
    return hw->clk;
    
error:
    if (fixed_factor) {
        kfree(fixed_factor);
    }
    if (factor) {
        kfree(factor);
    }
    if (divider) {
        kfree(divider);
    }
    if (fixed) {
        kfree(fixed);
    }
    if (gate) {
        kfree(gate);
    }
    if (mux) {
        kfree(mux);
    }
    if (parent_names) 
    {
        for (i = 0; i < num_parents; i++) {
            if (parent_names[i]) {
                kfree(parent_names[i]);
            }
        }
        kfree(parent_names);
    }
    return ERR_PTR(ret);
}

