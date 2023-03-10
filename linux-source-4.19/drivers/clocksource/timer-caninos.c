// SPDX-License-Identifier: GPL-2.0
/*
 * Caninos Timer driver
 *
 * Copyright (c) 2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2019 LSI-TEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2017 SUSE Linux GmbH
 * Author: Andreas Färber
 *
 * Copyright (c) 2012 Actions Semi Inc.
 * Author: Actions Semi, Inc.
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

#define DRIVER_NAME "caninos-timer"
#define pr_fmt(fmt) DRIVER_NAME": "fmt

#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/sched_clock.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#define TX_CTL 0x0
#define TX_CMP 0x4
#define TX_VAL 0x8

#define TX_CTL_PD    BIT(0)
#define TX_CTL_INTEN BIT(1)
#define TX_CTL_EN    BIT(2)

static void __iomem *timer_base;
static void __iomem *clksrc_base;
static void __iomem *clkevt_base;

static inline void timer_reset(void __iomem *base)
{
	writel(0, base + TX_CTL);
	writel(0, base + TX_VAL);
	writel(0, base + TX_CMP);
}

static inline void timer_set_enabled(void __iomem *base, bool enabled)
{
	u32 ctl = readl(base + TX_CTL);
	
	/* PD bit is cleared when set */
	ctl &= ~TX_CTL_PD;
	
	if (enabled) {
		ctl |= TX_CTL_EN;
	}
	else {
		ctl &= ~TX_CTL_EN;
	}
	writel(ctl, base + TX_CTL);
}

static u64 notrace timer_sched_read(void)
{
	return ((u64)(readl(clksrc_base + TX_VAL))) & 0xffffffff;
}

static int set_state_shutdown(struct clock_event_device *evt)
{
	timer_set_enabled(clkevt_base, false);
	return 0;
}

static int set_state_oneshot(struct clock_event_device *evt)
{
	timer_reset(clkevt_base);
	return 0;
}

static int tick_resume(struct clock_event_device *evt)
{
	return 0;
}

static int set_next_event(unsigned long evt, struct clock_event_device *ev)
{
	void __iomem *base = clkevt_base;
	
	timer_set_enabled(base, false);
	
	writel(TX_CTL_INTEN, base + TX_CTL);
	writel(0, base + TX_VAL);
	writel(evt, base + TX_CMP);
	
	timer_set_enabled(base, true);
	
	return 0;
}

static struct clock_event_device caninos_clockevent = {
	.name     = "caninos_tick",
	.rating   = 200,
	.features = CLOCK_EVT_FEAT_ONESHOT | CLOCK_EVT_FEAT_DYNIRQ,
	.set_state_shutdown = set_state_shutdown,
	.set_state_oneshot  = set_state_oneshot,
	.tick_resume        = tick_resume,
	.set_next_event     = set_next_event,
};

static irqreturn_t timer1_interrupt_handler(int irq, void *dev_id)
{
	struct clock_event_device *evt = (struct clock_event_device *)dev_id;
	
	writel(TX_CTL_PD, clkevt_base + TX_CTL);
	
	evt->event_handler(evt);
	
	return IRQ_HANDLED;
}

static int __init caninos_timer_init(struct device_node *node)
{
	const unsigned long rate = 24000000;
	int timer1_irq, ret;
	
	timer_base = of_iomap(node, 0);
	
	if (IS_ERR(timer_base))
	{
		pr_err("Could not map timer base registers\n");
		return PTR_ERR(timer_base);
	}
	
	clksrc_base = timer_base + 0x08;
	clkevt_base = timer_base + 0x14;
	
	timer1_irq = of_irq_get_byname(node, "timer1");
	
	if (timer1_irq <= 0)
	{
		pr_err("could not get timer IRQ from DTS\n");
		return -EINVAL;
	}
	
	timer_reset(clksrc_base);
	timer_set_enabled(clksrc_base, true);
	
	sched_clock_register(timer_sched_read, 32, rate);
	
	clocksource_mmio_init(clksrc_base + TX_VAL, node->name,
	                      rate, 200, 32, clocksource_mmio_readl_up);
	
	timer_reset(clkevt_base);
	
	ret = request_irq(timer1_irq, timer1_interrupt_handler, IRQF_TIMER,
	                  DRIVER_NAME, &caninos_clockevent);
	
	if (ret)
	{
		pr_err("could not request IRQ%d\n", timer1_irq);
		return ret;
	}
	
	caninos_clockevent.cpumask = cpumask_of(0);
	caninos_clockevent.irq = timer1_irq;
	
	clockevents_config_and_register(&caninos_clockevent, rate, 0xf, 0xffffffff);
	pr_info("probe finished\n");
	return 0;
}

TIMER_OF_DECLARE(caninos5, "caninos,k5-timer", caninos_timer_init);
TIMER_OF_DECLARE(caninos7, "caninos,k7-timer", caninos_timer_init);
TIMER_OF_DECLARE(caninos9, "caninos,k9-timer", caninos_timer_init);
