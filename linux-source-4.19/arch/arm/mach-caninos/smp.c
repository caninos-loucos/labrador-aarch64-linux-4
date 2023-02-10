#include <linux/delay.h>
#include <asm/smp.h>
#include <asm/smp_plat.h>
#include <asm/smp_scu.h>
#include <asm/cacheflush.h>
#include <mach/platform.h>

static DEFINE_RAW_SPINLOCK(boot_lock);

static void __init caninos_k5_smp_init_cpus(void)
{
	unsigned int i, ncores;
	
	ncores = scu_get_core_count((void __iomem *)IO_ADDRESS(K5_PA_SCU));
	
	if (ncores > nr_cpu_ids) {
		ncores = nr_cpu_ids;
	}
	for (i = 0U; i < ncores; i++) {
		set_cpu_possible(i, true);
	}
}

static void __init caninos_k5_smp_prepare_cpus(unsigned int max_cpus)
{
	scu_enable((void __iomem *)IO_ADDRESS(K5_PA_SCU));
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
	return !!(io_readl(K5_SPS_PG_CTL) & BIT(ackbit));
}

static inline bool __init sps_power_on(u32 pwrbit, u32 ackbit)
{
	const int timestep = 50;
	int timeout = 5000;
	
	if (sps_power_check(ackbit)) {
		return true;
	}
	
	io_writel(io_readl(K5_SPS_PG_CTL) | BIT(pwrbit), K5_SPS_PG_CTL);
	
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
		clk_rstbit = K5_DBG1_CLOCK_RST_BIT;
		
		/* assert DBG1 reset */
		io_writel(io_readl(K5_CMU_DEVRST1) & ~BIT(clk_rstbit), K5_CMU_CORECTL);
		
		udelay(10);
		
		/* deassert DBG1 reset */
		io_writel(io_readl(K5_CMU_DEVRST1) | BIT(clk_rstbit), K5_CMU_CORECTL);
		
		udelay(10);
		return true;
	
	case 2:
		sps_pwrbit = K5_CPU2_POWER_PWR_BIT;
		sps_ackbit = K5_CPU2_POWER_ACK_BIT;
		clk_rstbit = K5_CPU2_CLOCK_RST_BIT;
		clk_pwrbit = K5_CPU2_CLOCK_PWR_BIT;
		break;
		
	case 3:
		sps_pwrbit = K5_CPU3_POWER_PWR_BIT;
		sps_ackbit = K5_CPU3_POWER_ACK_BIT;
		clk_rstbit = K5_CPU3_CLOCK_RST_BIT;
		clk_pwrbit = K5_CPU3_CLOCK_PWR_BIT;
		break;
		
	default:
		return false;
	}
	
	/* assert cpu core reset */
	io_writel(io_readl(K5_CMU_CORECTL) & ~BIT(clk_rstbit), K5_CMU_CORECTL);
	
	/* try to power on */
	if (!sps_power_on(sps_pwrbit, sps_ackbit)) {
		return false;
	}
	
	/* enable cpu core clock */
	io_writel(io_readl(K5_CMU_CORECTL) | BIT(clk_pwrbit), K5_CMU_CORECTL);
	
	/* deassert cpu core reset */
	io_writel(io_readl(K5_CMU_CORECTL) | BIT(clk_rstbit), K5_CMU_CORECTL);
	
	udelay(10);
	return true;
}

static int __init caninos_k5_smp_boot_secondary(unsigned int cpu,
                                                struct task_struct *idle)
{
	unsigned long timeout;
	
	if (!cpu_power_on(cpu)) {
		return -ENOSYS;
	}
	
	switch (cpu)
	{
	case 1:
		io_writel(virt_to_phys(caninos_k5_secondary_startup), K5_CPU1_ADDR);
		io_writel(K5_BOOT_FLAG, K5_CPU1_FLAG);
		break;
		
	case 2:
		io_writel(virt_to_phys(caninos_k5_secondary_startup), K5_CPU2_ADDR);
		io_writel(K5_BOOT_FLAG, K5_CPU2_FLAG);
		break;
	
	case 3:
		io_writel(virt_to_phys(caninos_k5_secondary_startup), K5_CPU3_ADDR);
		io_writel(K5_BOOT_FLAG, K5_CPU3_FLAG);
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
		io_writel(0U, K5_CPU1_ADDR);
		io_writel(0U, K5_CPU1_FLAG);
		break;
		
	case 2:
		io_writel(0U, K5_CPU2_ADDR);
		io_writel(0U, K5_CPU2_FLAG);
		break;
		
	case 3:
		io_writel(0U, K5_CPU3_ADDR);
		io_writel(0U, K5_CPU3_FLAG);
		break;
	}
	
	raw_spin_unlock(&boot_lock);
	
	return pen_release != -1 ? -ENOSYS : 0;
}

static struct smp_operations caninos_k5_smp_ops __initdata =
{
    .smp_init_cpus = caninos_k5_smp_init_cpus,
    .smp_prepare_cpus = caninos_k5_smp_prepare_cpus,
    .smp_secondary_init = caninos_k5_smp_secondary_init,
    .smp_boot_secondary = caninos_k5_smp_boot_secondary,
};

bool __init caninos_k5_smp_init(void)
{
    smp_set_ops(&caninos_k5_smp_ops);
    return true;
}
