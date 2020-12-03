// SPDX-License-Identifier: GPL-2.0
/*
 * Clock Controller Driver for Caninos Labrador
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

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>

#include <dt-bindings/clock/caninos-gpuclk.h>

#define SPS_PG_CTL (0xE01B0100)
#define PWM_CTL1   (0xE01B0054)

static bool __init gpu_is_isolated(void __iomem *sps_base)
{
    return (readl(sps_base) & BIT(3)) == 0;
}

static int __init gpu_isolation_enable(void __iomem *sps_base)
{
    int timeout = 1000;
    
    if (gpu_is_isolated(sps_base)) {
        return 0;
    }
    
    while (timeout > 0)
    {
        writel(readl(sps_base) & ~BIT(3), sps_base);
        
        if (gpu_is_isolated(sps_base)) {
            return 0;
        }
        
        timeout--;
    }
    
    return -1;
}

static int __init gpu_isolation_disable(void __iomem *sps_base)
{
    int timeout = 1000;
    
    if (!gpu_is_isolated(sps_base)) {
        return 0;
    }
    
    while (timeout > 0)
    {
        writel(readl(sps_base) | BIT(3), sps_base);
        
        if (!gpu_is_isolated(sps_base)) {
            return 0;
        }
        
        timeout--;
    }
    
    return -1;
}

static void __init pwm_setup(void __iomem *pwm_base, int duty_act, int duty_all)
{
    u32 val = readl(pwm_base);
    
    val &= ~(0xFFFFF);
	val |= (duty_all & 0x3FF);
	val |= (duty_act & 0x3FF) << 10;
	val |= BIT(20); // high within active period
	
	writel(val, pwm_base);
}

struct caninos_gpu_cmu {
	struct clk *parent, *core, *pwm, *hosc;
};

static struct clk_hw_onecell_data *clk_data;
static struct caninos_gpu_cmu *priv_data;

void __init k7_gpu_clk_init(struct device_node *np)
{
    unsigned long bus_rate, core_rate;
    void __iomem *sps_base, *pwm_base;
    struct clk_hw **hws;
    int ret, i;
	
    priv_data = kzalloc(sizeof(*priv_data), GFP_KERNEL);
    
    if (!priv_data)
    {
        pr_err("Could not allocate gpu clock provider context.");
        return;
    }

    priv_data->parent = of_clk_get_by_name(np, "parent");
	
	if (IS_ERR(priv_data->parent))
	{
		pr_err("Could not get gpu clock parent.");
		return;
	}
	
	priv_data->core = of_clk_get_by_name(np, "core");
	
	if (IS_ERR(priv_data->core))
	{
		pr_err("Could not get gpu core clock.");
		return;
	}
	
	priv_data->pwm = of_clk_get_by_name(np, "pwm1");
	
	if (IS_ERR(priv_data->pwm))
	{
		pr_err("Could not get voltage regulator pwm clock.");
		return;
	}
	
	priv_data->hosc = of_clk_get_by_name(np, "hosc");
	
	if (IS_ERR(priv_data->hosc))
	{
		pr_err("Could not get hosc clock.");
		return;
	}
	
    sps_base = ioremap(SPS_PG_CTL, 4);

    if (!sps_base)
    {
        pr_err("Could not remap sps memory base.");
        return;
    }
    
    // PWM1 is at KS_IN3
    pwm_base = ioremap(PWM_CTL1, 4);
    
    if (!pwm_base)
    {
        pr_err("Could not remap pwm memory base.");
        return;
    }
    
    // make sure voltage isolation is enabled
    ret = gpu_isolation_enable(sps_base);
    
    if (ret)
    {
        pr_err("Could not enable gpu voltage isolation.");
        return;
    }
    
    // set the gpu clock to 75MHz and enable it
    clk_set_parent(priv_data->core, priv_data->parent);
    clk_set_rate(priv_data->core, 75000000);
    clk_prepare_enable(priv_data->core);
    
    // configure pwm to 14/64 and max source clock (HOSC and div=1)
    // this translates to around 1100mV at GPU power supply
    
    clk_set_parent(priv_data->pwm, priv_data->hosc);
    clk_set_rate(priv_data->pwm, clk_get_rate(priv_data->hosc));
    pwm_setup(pwm_base, 14, 64);
    
    // wait for the voltage to become stable
    usleep_range(10000, 15000);

    // disable voltage isolation
    ret = gpu_isolation_disable(sps_base);
    
    if (ret)
    {
        pr_err("Could not disable gpu voltage isolation.");
        return;
    }

    // increase the gpu clock to 400MHz
    clk_set_rate(priv_data->core, 400000000);
    
    // free memory resources
    iounmap(sps_base);
    iounmap(pwm_base);
    
    bus_rate = clk_get_rate(priv_data->parent);
    core_rate = clk_get_rate(priv_data->core);
    
	clk_data = kzalloc(struct_size(clk_data, hws, CLK_GPU_NR), GFP_KERNEL);
	
	if (!clk_data) {
		return;
	}
	
	hws = clk_data->hws;

	for (i = 0; i < CLK_GPU_NR; ++i) {
		hws[i] = ERR_PTR(-ENOENT);
	}
	
	hws[CLK_GPU_BUS] = clk_hw_register_fixed_rate(NULL, "clk_gpu_bus",
	                                              NULL, 0, bus_rate);
	
	hws[CLK_GPU_CORE] = clk_hw_register_fixed_rate(NULL, "clk_gpu_core",
	                                               "clk_gpu_bus", 0, core_rate);
	
	of_clk_add_hw_provider(np, of_clk_hw_onecell_get, clk_data);
}

CLK_OF_DECLARE(k7_gpu_clk, "caninos,k7-gpu-cmu", k7_gpu_clk_init);

