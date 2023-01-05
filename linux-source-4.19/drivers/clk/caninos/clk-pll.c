// SPDX-License-Identifier: GPL-2.0
/*
 * PLL clock implementation for Caninos Labrador
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
#include <linux/delay.h>
#include "clk-caninos.h"

struct caninos_pll
{
    struct clk_hw hw;
    void __iomem *reg;
    spinlock_t *lock;
    unsigned long bfreq;
    unsigned int enable_bit;
    unsigned int shift;
    unsigned int width;
    unsigned int min_mul;
    unsigned int max_mul;
    unsigned int pll_flags;
    const struct clk_pll_table *table;
};

#define to_caninos_pll(_hw) container_of(_hw, struct caninos_pll, hw)
#define _mask(m) ((BIT((m)->width)) - 1)
#define PLL_STABILITY_WAIT_US (50)

static u32 pll_calculate_mul(struct caninos_pll *pll, unsigned long rate)
{
    u32 mul = DIV_ROUND_CLOSEST(rate, pll->bfreq);
    
    if (mul < pll->min_mul) {
        mul = pll->min_mul;
    }
    else if (mul > pll->max_mul) {
        mul = pll->max_mul;
    }
    
    return mul & _mask(pll);
}

static unsigned int pll_get_table_val(const struct clk_pll_table *table,
                                      unsigned long rate)
{
    const struct clk_pll_table *clkt;
    unsigned long best_rate = 0;
    unsigned int val = 0;
    
    for (clkt = table; clkt->rate; clkt++)
    {
        if (clkt->rate == rate) {
            return clkt->val;
        }
        else if ((clkt->rate < rate) && (clkt->rate > best_rate))
        {
            val = clkt->val;
            best_rate = clkt->rate;
        }
    }
    
    return val;
}

static unsigned long pll_get_table_round_rate(const struct clk_pll_table *table,
                                              unsigned long rate)
{
    const struct clk_pll_table *clkt;
    unsigned long best_rate = 0;
    
    for (clkt = table; clkt->rate; clkt++)
    {
        if (clkt->rate == rate) {
            return clkt->rate;
        }
        else if ((clkt->rate < rate) && (clkt->rate > best_rate)){
            best_rate = clkt->rate;
        }
    }
    return best_rate;
}

static unsigned int pll_get_table_rate(const struct clk_pll_table *table,
                                       unsigned int val)
{
    const struct clk_pll_table *clkt;
    
    for (clkt = table; clkt->rate; clkt++)
    {
        if (clkt->val == val) {
            return clkt->rate;
        }
    }
    
    return 0;
}

static long caninos_pll_round_rate(struct clk_hw *hw, unsigned long rate,
                                   unsigned long *parent_rate)
{
    struct caninos_pll *pll = to_caninos_pll(hw);
    
    if (pll->width == 0) {
        return pll->bfreq;
    }
    if (pll->table) {
        return pll_get_table_round_rate(pll->table, rate);
    }
    
    return pll->bfreq * pll_calculate_mul(pll, rate);
}

static int caninos_pll_set_rate(struct clk_hw *hw, unsigned long rate,
                                unsigned long parent_rate)
{
    struct caninos_pll *pll = to_caninos_pll(hw);
    unsigned long uninitialized_var(flags);
    u32 val, aux;
    
    if (pll->width == 0) {
        return 0;
    }
    
    if (pll->table) {
        aux = pll_get_table_val(pll->table, rate);
    }
    else {
        aux = pll_calculate_mul(pll, rate);
    }
    
    aux &= _mask(pll);
    
    if (pll->lock) {
        spin_lock_irqsave(pll->lock, flags);
    }
    else {
        __acquire(pll->lock);
    }
    
	val = clk_readl(pll->reg);
	val &= ~(_mask(pll) << pll->shift);
	val |= aux << pll->shift;
	clk_writel(val, pll->reg);
	
	udelay(PLL_STABILITY_WAIT_US);
	
	if (pll->lock) {
        spin_unlock_irqrestore(pll->lock, flags);
    }
    else {
        __release(pll->lock);
    }
	
	return 0;
}

static unsigned long pll_get_rate(struct clk_hw *hw)
{
    struct caninos_pll *pll = to_caninos_pll(hw);
    u32 val;
    
    if (pll->width == 0) {
        return pll->bfreq;
    }
    
    val = clk_readl(pll->reg) >> pll->shift;
    val &= _mask(pll);
    
    if (pll->table) {
        return pll_get_table_rate(pll->table, val);
    }
    
    return pll->bfreq * val;
}

static unsigned long caninos_pll_recalc_rate
    (struct clk_hw *hw, unsigned long parent_rate)
{
    return pll_get_rate(hw);
}

static long caninos_pll_ro_round_rate
    (struct clk_hw *hw, unsigned long rate, unsigned long *parent_rate)
{
	return pll_get_rate(hw);
}

static int caninos_pll_is_enabled(struct clk_hw *hw)
{
    struct caninos_pll *pll = to_caninos_pll(hw);
    u32 val;

    val = clk_readl(pll->reg);
    val &= BIT(pll->enable_bit);

    return val ? 1 : 0;
}

static void caninos_pll_endisable(struct clk_hw *hw, u8 enable)
{
    struct caninos_pll *pll = to_caninos_pll(hw);
    unsigned long uninitialized_var(flags);
    u32 val;
    
    if (pll->lock) {
        spin_lock_irqsave(pll->lock, flags);
    }
    else {
        __acquire(pll->lock);
    }
    
    val = clk_readl(pll->reg);
    
    if (enable) {
        val |= BIT(pll->enable_bit);
    }
    else {
        val &= ~BIT(pll->enable_bit);
    }
    
    clk_writel(val, pll->reg);
    
    if (enable) {
        udelay(PLL_STABILITY_WAIT_US);
    }
    
    if (pll->lock) {
        spin_unlock_irqrestore(pll->lock, flags);
    }
    else {
        __release(pll->lock);
    }
}

static int caninos_pll_enable(struct clk_hw *hw)
{
	if (!caninos_pll_is_enabled(hw)) {
		caninos_pll_endisable(hw, 1);
	}
	return 0;
}

static void caninos_pll_disable(struct clk_hw *hw)
{
	if (caninos_pll_is_enabled(hw)) {
		caninos_pll_endisable(hw, 0);
	}
}

static const struct clk_ops caninos_pll_ops = {
    .enable = caninos_pll_enable,
    .disable = caninos_pll_disable,
    .is_enabled = caninos_pll_is_enabled,
    .round_rate = caninos_pll_round_rate,
    .recalc_rate = caninos_pll_recalc_rate,
    .set_rate = caninos_pll_set_rate,
};

static const struct clk_ops caninos_pll_ro_ops = {
	.enable = caninos_pll_enable,
    .disable = caninos_pll_disable,
    .is_enabled = caninos_pll_is_enabled,
    .recalc_rate = caninos_pll_recalc_rate,
    .round_rate = caninos_pll_ro_round_rate,
};

struct clk *__init caninos_register_pll(const struct caninos_pll_clock *info,
                                 struct device *dev, void __iomem *reg,
                                 spinlock_t *lock)
{
	struct clk_init_data init;
	struct caninos_pll *pll;
	struct clk_hw *hw;
	int ret;
	
	pll = kmalloc(sizeof(*pll), GFP_KERNEL);
	
	if (!pll) {
		return ERR_PTR(-ENOMEM);
	}
	
	init.name = info->name;
	
	if (info->pll_flags & CANINOS_CLK_PLL_READ_ONLY) {
		init.ops = &caninos_pll_ro_ops;
	}
	else {
		init.ops = &caninos_pll_ops;
	}
	
	init.flags = info->flags;
	init.parent_names = (info->parent_name ? &info->parent_name : NULL);
	init.num_parents = (info->parent_name ? 1 : 0);
	
	pll->bfreq = info->bfreq;
	pll->enable_bit = info->enable_bit;
	pll->shift = info->shift;
	pll->width = info->width;
	pll->min_mul = info->min_mul;
	pll->max_mul = info->max_mul;
	pll->table = info->table;
	pll->reg = reg + info->offset;
	pll->lock = lock;
	pll->pll_flags = info->pll_flags;
	pll->hw.init = &init;
	
	hw = &pll->hw;
	ret = clk_hw_register(dev, hw);
	
	if (ret) {
		kfree(pll);
		return ERR_PTR(ret);
	}
	
	return hw->clk;
}

