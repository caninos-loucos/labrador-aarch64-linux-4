#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/irqchip.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/memblock.h>
#include <linux/highmem.h>

#include <asm/system_info.h>
#include <asm/smp_plat.h>
#include <asm/smp_scu.h>
#include <asm/smp_twd.h>
#include <asm/smp.h>
#include <asm/cacheflush.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include <asm/hardware/cache-l2x0.h>

#define CANINOS_SDRAM_BASE      0x00000000
#define CANINOS_IO_DEVICE_BASE  0xB0000000
#define CANINOS_IO_ADDR_BASE    0xF8000000

#define CANINOS_PA_REG_BASE     0xB0000000
#define CANINOS_PA_REG_SIZE     0x00600000
#define CANINOS_PA_SCU          0xB0020000
#define CANINOS_PA_BOOT_RAM     0xFFFF8000
#define CANINOS_VA_BOOT_RAM     0xFFFF8000
#define CANINOS_CPU1_ADDR       0xB0168050
#define CANINOS_CPU2_ADDR       0xB0168054
#define CANINOS_CPU3_ADDR       0xB0168058
#define CANINOS_CPU1_FLAG       0xB016805C
#define CANINOS_CPU2_FLAG       0xB0168060
#define CANINOS_CPU3_FLAG       0xB0168064

#define IO_ADDRESS(x) (CANINOS_IO_ADDR_BASE + ((x) & 0x03ffffff))

#define DRIVER_NAME "board-setup"

#define INFO_MSG(fmt,...) pr_info(DRIVER_NAME ": " fmt, ##__VA_ARGS__)
#define ERR_MSG(fmt,...) pr_err(DRIVER_NAME ": " fmt, ##__VA_ARGS__)

#define KINFO_SIZE (SZ_1M)
#define FB_SIZE (8 * SZ_1M)

#define BOOT_FLAG (0x55aa)
#define CPU_SHIFT(cpu) (19 + cpu)

#define PMIC_I2C_ADDR  (0x65)
#define CPU_CORE_FREQ  (1104) // 1104 MHz
#define CPU_CORE_VOLT  (1175) // 1175 mV

// CPU Recommended Frequency/Voltage Pairs
//
//  408MHz at  950mV
//  720MHz at  975mV
//  900MHz at 1025mV
// 1104MHz at 1175mV
// 1308MHz at 1250mV
//
// PMIC DCDC1 Regulator Operating Limits
//
// Max Voltage : 1400mV
// Min Voltage :  700mV
// Voltage Step:   25mV
// Min Selector:    0
// Max Selector:   28 
// Stable After:  350us
//

extern void board_secondary_startup(void); // headsmp.S

static DEFINE_RAW_SPINLOCK(boot_lock);

static inline void io_writel(u32 val, u32 reg)
{
	*(volatile u32 *)(IO_ADDRESS(reg)) = val;
}

static void __iomem *scu_base_addr(void)
{
	return (void *)IO_ADDRESS(CANINOS_PA_SCU);
}

void __init board_smp_init_cpus(void)
{
	void __iomem *scu_base = scu_base_addr();
	unsigned int i, ncores;
	
	ncores = scu_base ? scu_get_core_count(scu_base) : 1;
	
	INFO_MSG("Number of cores %d\n", ncores);
	
	if (ncores > nr_cpu_ids) {
		ncores = nr_cpu_ids;
	}
	for (i = 0; i < ncores; i++) {
		set_cpu_possible(i, true);
	}
}

void __init board_smp_prepare_cpus(unsigned int max_cpus)
{
	scu_enable(scu_base_addr());
}

static void write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

void __init board_secondary_init(unsigned int cpu)
{
	trace_hardirqs_off();
	write_pen_release(-1);
	raw_spin_lock(&boot_lock);
	raw_spin_unlock(&boot_lock);
}

static void wakeup_secondary(unsigned int cpu)
{
	//enum owl_powergate_id cpuid;

	//cpuid = owl_cpu_powergate_id(cpu);
	
	//owl_powergate_power_on(cpuid);
	
	udelay(200);
	
	switch (cpu)
	{
	case 1:
		//module_reset(MODULE_RST_DBG1RESET);
		udelay(10);
		
		io_writel(virt_to_phys(board_secondary_startup), CANINOS_CPU1_ADDR);
		io_writel(BOOT_FLAG, CANINOS_CPU1_FLAG);
		break;
	case 2:
		io_writel(virt_to_phys(board_secondary_startup), CANINOS_CPU2_ADDR);
		io_writel(BOOT_FLAG, CANINOS_CPU2_FLAG);
		break;
	case 3:
		io_writel(virt_to_phys(board_secondary_startup), CANINOS_CPU3_ADDR);
		io_writel(BOOT_FLAG, CANINOS_CPU3_FLAG);
		break;
	}
	
	dsb_sev();
	mb();
}

int __init board_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;
	
	wakeup_secondary(cpu);
	
	udelay(10);
	
	raw_spin_lock(&boot_lock);
	
	write_pen_release(cpu_logical_map(cpu));
	
	smp_send_reschedule(cpu);

	timeout = jiffies + (1 * HZ);
	
	while (time_before(jiffies, timeout)) {
		if (pen_release == -1)
			break;
	}

	switch (cpu) {
	case 1:
		io_writel(0, CANINOS_CPU1_ADDR);
		io_writel(0, CANINOS_CPU1_FLAG);
		break;
	case 2:
		io_writel(0, CANINOS_CPU2_ADDR);
		io_writel(0, CANINOS_CPU2_FLAG);
		break;
	case 3:
		io_writel(0, CANINOS_CPU3_ADDR);
		io_writel(0, CANINOS_CPU3_FLAG);
		break;
	}
	
	raw_spin_unlock(&boot_lock);

	return pen_release != -1 ? -ENOSYS : 0;
}

static struct smp_operations board_smp_ops =
{
    .smp_init_cpus = board_smp_init_cpus,
    .smp_prepare_cpus = board_smp_prepare_cpus,
    .smp_secondary_init = board_secondary_init,
    .smp_boot_secondary = board_boot_secondary,
};

static bool __init board_smp_init(void)
{
    smp_set_ops(&board_smp_ops);
    return true;
}

static void __init board_init_early(void)
{
    int ret = -EINVAL;
    
	//board_check_revision();
	
	//ret = pmic_setup();
	
    if (ret < 0)
    {
        ERR_MSG("Could not setup the PMIC\n");
        return;
    }
    
    //ret = cpu_set_clock(CPU_CORE_FREQ, CPU_CORE_VOLT);
    
    if (ret < 0)
    {
        ERR_MSG("Could not set CPU core speed\n");
        return;
    }
}

static struct map_desc board_io_desc[] __initdata = {
	{
		.virtual	= IO_ADDRESS(CANINOS_PA_REG_BASE),
		.pfn		= __phys_to_pfn(CANINOS_PA_REG_BASE),
		.length		= CANINOS_PA_REG_SIZE,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= CANINOS_VA_BOOT_RAM,
		.pfn		= __phys_to_pfn(CANINOS_PA_BOOT_RAM),
		.length		= SZ_4K,
		.type		= MT_MEMORY_RWX,
	},
};

static void __init board_map_io(void)
{
	iotable_init(board_io_desc, ARRAY_SIZE(board_io_desc));
}

static void __init board_init_irq(void)
{
	irqchip_init();
}

static struct of_device_id board_dt_match_table[] __initdata = {
	{ .compatible = "simple-bus", },
	{}
};

static void board_init(void)
{
	int ret;
	
	//pm_power_off = board_pm_halt;
	
	INFO_MSG("System halt enabled\n");

	ret = of_platform_populate(NULL, board_dt_match_table, NULL, NULL);
	
	if (ret) {
		ERR_MSG("OF platform populate failed\n");
	}
}

static const char * board_dt_match[] __initconst = {
	"caninos,labrador",
	NULL,
};

// Instruction prefetch enable
// Data prefetch enable
// Round-robin replacement
// Use AWCACHE attributes for WA
// 32kB way size, 16 way associativity
// Disable exclusive cache

#define L310_MASK (0xc0000fff)

#define L310_VAL (L310_AUX_CTRL_INSTR_PREFETCH | L310_AUX_CTRL_DATA_PREFETCH \
                 | L310_AUX_CTRL_NS_INT_CTRL | L310_AUX_CTRL_NS_LOCKDOWN \
                 | L310_AUX_CTRL_CACHE_REPLACE_RR \
                 | L2C_AUX_CTRL_WAY_SIZE(2) | L310_AUX_CTRL_ASSOCIATIVITY_16)

DT_MACHINE_START(CANINOS, "labrador")
	.dt_compat    = board_dt_match,
	.atag_offset  = 0x00000100,
	.l2c_aux_val  = L310_VAL,
	.l2c_aux_mask = L310_MASK,
	.smp_init     = board_smp_init,
	.init_early   = board_init_early,
	.map_io       = board_map_io,
	//.reserve      = board_reserve,
	.init_irq     = board_init_irq,
	.init_machine = board_init,
MACHINE_END
