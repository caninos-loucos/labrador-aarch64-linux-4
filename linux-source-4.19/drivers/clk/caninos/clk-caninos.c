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

#include <linux/kernel.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include "clk-caninos.h"

static void
caninos_clk_add_lookup(struct caninos_clk_provider *ctx,
                       struct clk *clk, int id)
{
    if (ctx->clk_data.clks && id >= 0) {
        ctx->clk_data.clks[id] = clk;
    }
}

void __init
caninos_clk_register_fixed(struct caninos_clk_provider *ctx,
                           const struct caninos_fixed_clock *clks,
                           int num)
{
	struct clk *clk;
	int i, ret;
	
	for (i = 0; i < num; i++)
	{
		const struct caninos_fixed_clock *info = &clks[i];
		
		clk = clk_register_fixed_rate(NULL, info->name, info->parent_name,
		                              info->flags, info->fixed_rate);
		
		if (IS_ERR(clk))
		{
			pr_err("%s: failed to register clock %s\n", __func__, info->name);
			continue;
		}
		
		caninos_clk_add_lookup(ctx, clk, info->id);
		
		ret = clk_register_clkdev(clk, info->name, NULL);
		
		if (ret) {
			pr_err("%s: failed to register lookup %s\n",__func__, info->name);
		}
	}
}

void __init
caninos_clk_register_fixed_factor(struct caninos_clk_provider *ctx,
                                  const struct caninos_fixed_factor_clock *clks,
                                  int num)
{
	struct clk *clk;
	int i, ret;
	
	for (i = 0; i < num; i++)
	{
		const struct caninos_fixed_factor_clock *info = &clks[i];
		
		clk = clk_register_fixed_factor(NULL, info->name, info->parent_name,
		                                info->flags, info->mult, info->div);
		
		if (IS_ERR(clk))
		{
			pr_err("%s: failed to register clock %s\n", __func__, info->name);
			continue;
		}
		
		caninos_clk_add_lookup(ctx, clk, info->id);
		
		ret = clk_register_clkdev(clk, info->name, NULL);
		
		if (ret) {
			pr_err("%s: failed to register lookup %s\n",__func__, info->name);
		}
	}
}

void __init
caninos_clk_register_mux(struct caninos_clk_provider *ctx,
                         const struct caninos_mux_clock *clks, int num)
{
	struct clk *clk;
	int i, ret;
	
	for (i = 0; i < num; i++)
	{
		const struct caninos_mux_clock *info = &clks[i];
		
		clk = clk_register_mux(NULL, info->name, info->parent_names,
		                       info->num_parents, info->flags,
		                       ctx->reg_base + info->offset,
		                       info->shift, info->width,
		                       info->mux_flags, &ctx->lock);
		
		if (IS_ERR(clk))
		{
			pr_err("%s: failed to register clock %s\n",__func__, info->name);
			continue;
		}
		
		caninos_clk_add_lookup(ctx, clk, info->id);
		
		ret = clk_register_clkdev(clk, info->name, NULL);
		
		if (ret) {
			pr_err("%s: failed to register lookup %s\n",__func__, info->name);
		}
	}
}

void __init 
caninos_clk_register_div(struct caninos_clk_provider *ctx,
                         const struct caninos_div_clock *clks,
                         int num)
{
    struct clk *clk;
    int i, ret;
    
    for (i = 0; i < num; i++)
    {
    	const struct caninos_div_clock *info = &clks[i];
    	
        clk = clk_register_divider_table(NULL, info->name,
                                   info->parent_name, info->flags,
		                           ctx->reg_base + info->offset,
		                           info->shift, info->width,
		                           info->div_flags, info->table,
                                   &ctx->lock);
        
        if (IS_ERR(clk))
        {
            pr_err("%s: failed to register clock %s\n",__func__, info->name);
            continue;
        }
        
        caninos_clk_add_lookup(ctx, clk, info->id);

        ret = clk_register_clkdev(clk, info->name, NULL);

        if (ret) {
            pr_err("%s: failed to register lookup %s\n",__func__, info->name);
        }		
    }
}

void __init
caninos_clk_register_gate(struct caninos_clk_provider *ctx,
                          const struct caninos_gate_clock *clks, int num)
{
    struct clk *clk;
    int i, ret;
    
    for (i = 0; i < num; i++)
    {
    	const struct caninos_gate_clock *info = &clks[i];
    	
        clk = clk_register_gate(NULL, info->name,
		                        info->parent_name, info->flags,
		                        ctx->reg_base + info->offset, info->bit_idx,
		                        info->gate_flags, &ctx->lock);
        
        if (IS_ERR(clk))
        {
            pr_err("%s: failed to register clock %s\n",__func__, info->name);
            continue;
        }
        
        caninos_clk_add_lookup(ctx, clk, info->id);
        
        ret = clk_register_clkdev(clk, info->name, NULL);
        
        if (ret) {
            pr_err("%s: failed to register lookup %s\n",__func__, info->name);
        }
    }
}

void __init
caninos_clk_register_pll(struct caninos_clk_provider *ctx,
                         const struct caninos_pll_clock *clks, int num)
{
    struct clk *clk;
    int i, ret;
    
    for (i = 0; i < num; i++)
    {
        clk = caninos_register_pll(&clks[i], NULL, ctx->reg_base, &ctx->lock);
        
		if (IS_ERR(clk))
		{
		    pr_err("%s: failed to register clock %s\n", __func__, clks[i].name);
		    continue;
		}
		
		caninos_clk_add_lookup(ctx, clk, clks[i].id);
		
		ret = clk_register_clkdev(clk, clks[i].name, NULL);
		
		if (ret) {
            pr_err("%s: failed to register lookup %s\n",__func__, clks[i].name);
        }
	}
}

void __init
caninos_clk_register_composite(struct caninos_clk_provider *ctx,
                               const struct caninos_composite_clock *clks,
                               int num)
{
    struct clk *clk;
    int i, ret;
    
    for (i = 0; i < num; i++)
    {
        clk = caninos_register_composite(&clks[i], NULL,
                                         ctx->reg_base, &ctx->lock);
        
        if (IS_ERR(clk))
        {
            pr_err("%s: failed to register clock %s\n", __func__, clks[i].name);
            continue;
        }
        
        caninos_clk_add_lookup(ctx, clk, clks[i].id);
        
        ret = clk_register_clkdev(clk, clks[i].name, NULL);
        
        if (ret) {
            pr_err("%s: failed to register lookup %s\n",__func__, clks[i].name);
        }
    }
}

struct caninos_clk_provider *__init
caninos_clk_init(struct device_node *np,
                 void __iomem *base, unsigned long nr_clks)
{
    struct caninos_clk_provider *ctx;
    struct clk **clk_table;
    int ret;
    int i;
    
    if (!np)
    {
        panic("%s: invalid device node.\n",__func__);
        return NULL;
    }
    
    ctx = kzalloc(sizeof(struct caninos_clk_provider), GFP_KERNEL);
    
    if (!ctx)
    {
        panic("%s: could not allocate clock provider context.\n",__func__);
        return NULL;
    }
    
    clk_table = kcalloc(nr_clks, sizeof(struct clk *), GFP_KERNEL);
    
    if (!clk_table)
    {
        panic("%s: could not allocate clock lookup table.\n",__func__);
        kfree(ctx);
        return NULL;
    }
    
    for (i = 0; i < nr_clks; ++i) {
        clk_table[i] = ERR_PTR(-ENOENT);
    }
    
    ctx->reg_base = base;
    ctx->clk_data.clks = clk_table;
    ctx->clk_data.clk_num = nr_clks;
    
    spin_lock_init(&ctx->lock);
    
    ret = of_clk_add_provider(np, of_clk_src_onecell_get, &ctx->clk_data);
    
    if (ret)
    {
        panic("%s: could not register clock provider.\n",__func__);
        return NULL;
    }
    
    return ctx;
}

void __init
caninos_register_clk_tree(struct caninos_clk_provider *ctx,
                          const struct caninos_clock_tree *tree)
{
	caninos_clk_register_fixed(ctx, tree->fixed.clks, tree->fixed.num);
	caninos_clk_register_fixed_factor(ctx, tree->factor.clks, tree->factor.num);
	caninos_clk_register_pll(ctx, tree->pll.clks, tree->pll.num);
	caninos_clk_register_div(ctx, tree->div.clks, tree->div.num);
	caninos_clk_register_mux(ctx, tree->mux.clks, tree->mux.num);
	caninos_clk_register_gate(ctx, tree->gate.clks, tree->gate.num);
	caninos_clk_register_composite(ctx, tree->comp.clks, tree->comp.num);
}

