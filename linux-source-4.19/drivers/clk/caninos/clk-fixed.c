// SPDX-License-Identifier: GPL-2.0
/*
 * Fixed rate clock implementation for Caninos Labrador
 * Copyright (c) 2018-2020 LSI-TEC - Caninos Loucos
 * Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2014 David Liu, Actions Semi Inc <liuwei@actions-semi.com>
 * Copyright (c) 2010-2011 Canonical Ltd <jeremy.kerr@canonical.com>
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
 * DOC: basic fixed-rate clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clk_(un)prepare only ensures parents are prepared
 * enable - clk_enable only ensures parents are enabled
 * rate - rate is always a fixed value. No clk_set_rate support
 * parent - fixed parent. No clk_set_parent support
 */

#define to_caninos_fixed_rate(_hw) \
    container_of(_hw, struct caninos_fixed_rate, hw)

static unsigned long caninos_fixed_rate_recalc_rate(struct clk_hw *hw,
    unsigned long parent_rate)
{
    return to_caninos_fixed_rate(hw)->fixed_rate;
}

static unsigned long caninos_fixed_rate_recalc_accuracy(struct clk_hw *hw,
    unsigned long parent_accuracy)
{
    return 0;
}

const struct clk_ops caninos_fixed_rate_ops = {
    .recalc_rate = caninos_fixed_rate_recalc_rate,
    .recalc_accuracy = caninos_fixed_rate_recalc_accuracy,
};

struct clk *caninos_register_fixed(const struct caninos_fixed_clock *info)
{
    struct caninos_fixed_rate *fixed;
    struct clk_init_data init = {};
    struct clk_hw *hw;
    int ret;
    
    fixed = kzalloc(sizeof(*fixed), GFP_KERNEL);
    
    if (!fixed) {
        return ERR_PTR(-ENOMEM);
    }
    
    init.name = info->name;
    init.ops = &caninos_fixed_rate_ops;
    init.flags = CLK_IGNORE_UNUSED;
    init.parent_names = (info->parent_name ? &info->parent_name: NULL);
    init.num_parents = (info->parent_name ? 1 : 0);
    
    fixed->fixed_rate = info->fixed_rate;
    fixed->hw.init = &init;
    hw = &fixed->hw;
    
    ret = clk_hw_register(NULL, hw);
    
    if (ret)
    {
        kfree(fixed);
        return ERR_PTR(ret);
    }
    
    return hw->clk;
}

