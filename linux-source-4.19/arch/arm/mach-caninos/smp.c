// SPDX-License-Identifier: GPL-2.0
/*
 * Support Routines for the Caninos Labrador 32bits Architecture
 *
 * Copyright (c) 2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/smp.h>
#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <asm/smp_scu.h>
#include <mach/platform.h>

/*
	CPU Recommended Frequency/Voltage Pairs
	
	 408MHz at  950mV
	 720MHz at  975mV
	 900MHz at 1025mV
	1104MHz at 1175mV
	1308MHz at 1250mV
	
	PMIC DCDC1 Regulator Operating Limits
	
	Max Voltage : 1400mV
	Min Voltage :  700mV
	Voltage Step:   25mV
	Min Selector:    0
	Max Selector:   28 
	Stable After:  350us
*/

#define CPU_CORE_FREQ (1104U) /* MHz */
#define CPU_CORE_VOLT (1175U) /* mV  */
#define NAND_PLL_FREQ (12U)   /* MHz */

#define BOOT_FLAG (0x55AA)

#define SPS_PG_CTL_CPU2_PWR_BIT  (5U)
#define SPS_PG_CTL_CPU2_ACK_BIT  (21U)
#define SPS_PG_CTL_CPU3_PWR_BIT  (6U)
#define SPS_PG_CTL_CPU3_ACK_BIT  (22U)

#define CMU_CORECTL_CPU2_RST_BIT (6U)
#define CMU_CORECTL_CPU2_PWR_BIT (2U)
#define CMU_CORECTL_CPU3_RST_BIT (7U)
#define CMU_CORECTL_CPU3_PWR_BIT (3U)

#define CMU_DEVRST1_DBG1_BIT     (29U)

static DEFINE_RAW_SPINLOCK(boot_lock);

static void __iomem *scu_base_addr;
static void __iomem *sps_base_addr;

static void __init caninos_k5_smp_init_cpus(void)
{
	struct device_node *node;
	unsigned int i, ncores;
	
	if (!caninos_k5_pmic_setup()) {
		panic("Could not setup the PMIC\n");
	}
	//else if (!caninos_k5_nandpll_set_clock(NAND_PLL_FREQ)) {
	//	panic("Unable to set NAND pll frequency\n");
	//}
	else if (!caninos_k5_cpu_set_clock(CPU_CORE_FREQ, CPU_CORE_VOLT)) {
		panic("Unable to set CPU core frequency and voltage\n");
	}
	
	node = of_find_compatible_node(NULL, NULL, "caninos,k5-sps");
	
	if (!node) {
		pr_err("%s: missing sps\n", __func__);
		return;
	}
	
	sps_base_addr = of_iomap(node, 0);
	
	if (!sps_base_addr) {
		pr_err("%s: could not map sps registers\n", __func__);
		return;
	}
	
	node = of_find_compatible_node(NULL, NULL, "arm,cortex-a9-scu");
	
	if (!node) {
		pr_err("%s: missing scu\n", __func__);
		return;
	}
	
	scu_base_addr = of_iomap(node, 0);
	
	if (!scu_base_addr) {
		pr_err("%s: could not map scu registers\n", __func__);
		return;
	}
	
	ncores = scu_get_core_count(scu_base_addr);
	
	if (ncores > nr_cpu_ids) {
		ncores = nr_cpu_ids;
	}
	for (i = 0U; i < ncores; i++) {
		set_cpu_possible(i, true);
	}
}

static void __init caninos_k5_smp_prepare_cpus(unsigned int max_cpus)
{
	scu_enable(scu_base_addr);
}

static void __init write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

static void __init caninos_k5_smp_secondary_init(unsigned int cpu)
{
	trace_hardirqs_off();
	write_pen_release(-1);
	raw_spin_lock(&boot_lock);
	raw_spin_unlock(&boot_lock);
}

static inline bool __init sps_power_check(u32 ackbit)
{
	return !!(readl(sps_base_addr) & BIT(ackbit));
}

static inline bool __init sps_power_on(u32 pwrbit, u32 ackbit)
{
	const int timestep = 50;
	int timeout = 5000;
	
	if (sps_power_check(ackbit)) {
		return true;
	}
	
	writel(readl(sps_base_addr) | BIT(pwrbit), sps_base_addr);
	
	while (timeout > 0 && !sps_power_check(ackbit)) {
		udelay(timestep);
		timeout -= timestep;
	}
	if (timeout <= 0) {
		return false;
	}
	
	udelay(10);
	return true;
}

static inline bool __init cpu_power_on(unsigned int cpu)
{
	u32 sps_pwrbit, sps_ackbit, clk_rstbit, clk_pwrbit;
	
	switch (cpu)
	{
	case 1:
		clk_rstbit = CMU_DEVRST1_DBG1_BIT;
		
		/* assert DBG1 reset */
		io_writel(io_readl(CMU_DEVRST1) & ~BIT(clk_rstbit), CMU_DEVRST1);
		udelay(10);
		
		/* deassert DBG1 reset */
		io_writel(io_readl(CMU_DEVRST1) | BIT(clk_rstbit), CMU_DEVRST1);
		
		udelay(10);
		return true;
	
	case 2:
		sps_pwrbit = SPS_PG_CTL_CPU2_PWR_BIT;
		sps_ackbit = SPS_PG_CTL_CPU2_ACK_BIT;
		clk_rstbit = CMU_CORECTL_CPU2_RST_BIT;
		clk_pwrbit = CMU_CORECTL_CPU2_PWR_BIT;
		break;
		
	case 3:
		sps_pwrbit = SPS_PG_CTL_CPU3_PWR_BIT;
		sps_ackbit = SPS_PG_CTL_CPU3_ACK_BIT;
		clk_rstbit = CMU_CORECTL_CPU3_RST_BIT;
		clk_pwrbit = CMU_CORECTL_CPU3_PWR_BIT;
		break;
		
	default:
		return false;
	}
	
	/* assert cpu core reset */
	io_writel(io_readl(CMU_CORECTL) & ~BIT(clk_rstbit), CMU_CORECTL);
	
	/* try to power on */
	if (!sps_power_on(sps_pwrbit, sps_ackbit)) {
		return false;
	}
	
	/* enable cpu core clock */
	io_writel(io_readl(CMU_CORECTL) | BIT(clk_pwrbit), CMU_CORECTL);
	
	/* deassert cpu core reset */
	io_writel(io_readl(CMU_CORECTL) | BIT(clk_rstbit), CMU_CORECTL);
	
	udelay(10);
	return true;
}

static int __init caninos_k5_smp_boot_secondary(unsigned int cpu,
                                                struct task_struct *idle)
{
	unsigned long timeout;
	
	if (!cpu_power_on(cpu))
	{
		pr_err("Could not power on cpu%u\n", cpu);
		return -ENOSYS;
	}
	
	switch (cpu)
	{
	case 1:
		io_writel(virt_to_phys(caninos_k5_secondary_startup), CPU1_ADDR);
		io_writel(BOOT_FLAG, CPU1_FLAG);
		break;
		
	case 2:
		udelay(250);
		io_writel(virt_to_phys(caninos_k5_secondary_startup), CPU2_ADDR);
		io_writel(BOOT_FLAG, CPU2_FLAG);
		break;
	
	case 3:
		udelay(250);
		io_writel(virt_to_phys(caninos_k5_secondary_startup), CPU3_ADDR);
		io_writel(BOOT_FLAG, CPU3_FLAG);
		break;
	}
	
	dsb_sev();
	mb();
	
	raw_spin_lock(&boot_lock);
	
	write_pen_release(cpu_logical_map(cpu));
	smp_send_reschedule(cpu);

	timeout = jiffies + (1 * HZ);
	
	while (time_before(jiffies, timeout)) {
		if (pen_release == -1)
			break;
	}

	switch (cpu)
	{
	case 1:
		io_writel(0U, CPU1_ADDR);
		io_writel(0U, CPU1_FLAG);
		break;
		
	case 2:
		io_writel(0U, CPU2_ADDR);
		io_writel(0U, CPU2_FLAG);
		break;
		
	case 3:
		io_writel(0U, CPU3_ADDR);
		io_writel(0U, CPU3_FLAG);
		break;
	}
	
	raw_spin_unlock(&boot_lock);
	
	if (pen_release != -1)
	{
		pr_err("Could not run secondary boot code at cpu%u\n", cpu);
		return -ENOSYS;
	}
	return 0;
}

static const struct smp_operations caninos_k5_smp_ops __initconst = {
	.smp_init_cpus = caninos_k5_smp_init_cpus,
	.smp_prepare_cpus = caninos_k5_smp_prepare_cpus,
	.smp_secondary_init = caninos_k5_smp_secondary_init,
	.smp_boot_secondary = caninos_k5_smp_boot_secondary,
};
CPU_METHOD_OF_DECLARE(k5_smp, "caninos,k5-smp", &caninos_k5_smp_ops);
