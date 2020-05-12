// SPDX-License-Identifier: GPL-2.0
/*
 * Simple multiplexer clock implementation for Caninos Labrador
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
 * DOC: basic adjustable multiplexer clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clk_prepare only ensures that parents are prepared
 * enable - clk_enable only ensures that parents are enabled
 * rate - rate is only affected by parent switching. No clk_set_rate support
 * parent - parent is adjustable through clk_set_parent
 */

#define to_caninos_mux(_hw) container_of(_hw, struct caninos_mux, hw)
#define _mask(m) ((BIT((m)->width)) - 1)

static u8 caninos_mux_get_parent(struct clk_hw *hw)
{
    struct caninos_mux *mux = to_caninos_mux(hw);
    u32 val;
    
    val = clk_readl(mux->reg) >> mux->shift;
    val &= _mask(mux);
    
    return clk_mux_val_to_index(hw, NULL, mux->flags, val);
}

static int caninos_mux_set_parent(struct clk_hw *hw, u8 index)
{
    struct caninos_mux *mux = to_caninos_mux(hw);
    unsigned long uninitialized_var(flags);
    u32 val, aux;
    
    aux = clk_mux_index_to_val(NULL, mux->flags, index);
    aux &= _mask(mux);
    
    if (mux->lock) {
        spin_lock_irqsave(mux->lock, flags);
    }
    else {
        __acquire(mux->lock);
    }
    
    val = clk_readl(mux->reg);
    val &= ~(_mask(mux) << mux->shift);
    val |= aux << mux->shift;
    clk_writel(val, mux->reg);
    
    if (mux->lock) {
        spin_unlock_irqrestore(mux->lock, flags);
    }
    else {
        __release(mux->lock);
    }
    return 0;
}

static int caninos_mux_determine_rate(struct clk_hw *hw,
                                      struct clk_rate_request *req)
{
    struct caninos_mux *mux = to_caninos_mux(hw);
    
    return clk_mux_determine_rate_flags(hw, req, mux->flags);
}

const struct clk_ops caninos_mux_ops = {
    .get_parent = caninos_mux_get_parent,
    .set_parent = caninos_mux_set_parent,
    .determine_rate = caninos_mux_determine_rate,
};

const struct clk_ops caninos_mux_ro_ops = {
    .get_parent = caninos_mux_get_parent,
};

struct clk *caninos_register_mux(const struct caninos_mux_clock *info,
                                 void __iomem *reg, spinlock_t *lock)
{
    struct clk_init_data init = {};
    struct caninos_mux *mux;
    struct clk_hw *hw;
    int ret;
    
    mux = kzalloc(sizeof(*mux), GFP_KERNEL);
    
    if (!mux) {
        return ERR_PTR(-ENOMEM);
    }
    
    mux->reg = reg + info->offset;
    mux->shift = info->shift;
    mux->width = info->width;
    mux->flags = 0;
    mux->lock = lock;
    
    init.name = info->name;
    init.flags = CLK_IGNORE_UNUSED;
    init.parent_names = info->parent_names;
    init.num_parents = info->num_parents;
    
    if (info->mux_flags & CANINOS_CLK_RATE_READ_ONLY)
    {
        init.ops = &caninos_mux_ro_ops;
        mux->flags |= CLK_MUX_READ_ONLY;
    }
    else {
        init.ops = &caninos_mux_ops;
    }
    
    if (info->mux_flags & CANINOS_CLK_ROUND_RATE_CLOSEST) {
        mux->flags |= CLK_MUX_ROUND_CLOSEST;
    }
    
    if (info->mux_flags & CANINOS_CLK_SET_RATE_NO_REPARENT) {
        init.flags |= CLK_SET_RATE_NO_REPARENT;
    }
    if (info->mux_flags & CANINOS_CLK_SET_RATE_PARENT) {
        init.flags |= CLK_SET_RATE_PARENT;
    }
    
    mux->hw.init = &init;
    hw = &mux->hw;
    
    ret = clk_hw_register(NULL, hw);
    
    if (ret)
    {
        kfree(mux);
        return ERR_PTR(ret);
    }
    
    return hw->clk;
}

