// SPDX-License-Identifier: GPL-2.0
/*
 * Gated clock implementation for Caninos Labrador
 * Copyright (c) 2018-2020 LSI-TEC - Caninos Loucos
 * Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2014 David Liu, Actions Semi Inc <liuwei@actions-semi.com>
 * Copyright (c) 2011-2012 Mike Turquette, Linaro Ltd <mturquette@linaro.org>
 * Copyright (c) 2010-2011 Canonical Ltd <jeremy.kerr@canonical.com>
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
 * DOC: basic gatable clock which can gate and ungate it's ouput
 *
 * Traits of this clock:
 * prepare - clk_(un)prepare only ensures parent is (un)prepared
 * enable - clk_enable and clk_disable are functional & control gating
 * rate - inherits rate from parent. No clk_set_rate support
 * parent - fixed parent. No clk_set_parent support
 */

#define to_caninos_gate(_hw) container_of(_hw, struct caninos_gate, hw)

static void caninos_gate_endisable(struct clk_hw *hw, u8 enable)
{
    struct caninos_gate *gate = to_caninos_gate(hw);
    unsigned long uninitialized_var(flags);
    u32 val;
    
    if (gate->lock) {
        spin_lock_irqsave(gate->lock, flags);
    }
    else {
        __acquire(gate->lock);
    }
    
    val = clk_readl(gate->reg);
    
    if (enable) {
        val |= BIT(gate->bit_idx);
    }
    else {
        val &= ~BIT(gate->bit_idx);
    }
    
    clk_writel(val, gate->reg);
    
    if (gate->lock) {
        spin_unlock_irqrestore(gate->lock, flags);
    }
    else {
        __release(gate->lock);
    }
}

static int caninos_gate_is_enabled(struct clk_hw *hw)
{
    struct caninos_gate *gate = to_caninos_gate(hw);
    return (clk_readl(gate->reg) & BIT(gate->bit_idx)) ? 1 : 0;
}

static int caninos_gate_enable(struct clk_hw *hw)
{
    caninos_gate_endisable(hw, 1);
    return 0;
}

static void caninos_gate_disable(struct clk_hw *hw)
{
    caninos_gate_endisable(hw, 0);
}

const struct clk_ops caninos_gate_ops = {
    .enable = caninos_gate_enable,
    .disable = caninos_gate_disable,
    .is_enabled = caninos_gate_is_enabled,
};

struct clk *caninos_register_gate(const struct caninos_gate_clock *info,
                                  void __iomem *reg, spinlock_t *lock)
{
    struct clk_init_data init;
    struct caninos_gate *gate;
    struct clk_hw *hw;
    int ret;
    
    gate = kzalloc(sizeof(*gate), GFP_KERNEL);
    
    if (!gate) {
        return ERR_PTR(-ENOMEM);
    }
    
    init.name = info->name;
    init.ops = &caninos_gate_ops;
    
    init.flags = CLK_IGNORE_UNUSED;
    
    if (info->gate_flags & CANINOS_CLK_IS_CRITICAL) {
        init.flags |= CLK_IS_CRITICAL;
    }
    
    init.parent_names = info->parent_name ? &info->parent_name : NULL;
    init.num_parents = info->parent_name ? 1 : 0;
    
    gate->reg = reg + info->offset;
    gate->bit_idx = info->bit_idx;
    gate->lock = lock;
    gate->hw.init = &init;
    hw = &gate->hw;
    
    ret = clk_hw_register(NULL, hw);
    
    if (ret)
    {
        kfree(gate);
        return ERR_PTR(ret);
    }
    
    return hw->clk;
}

