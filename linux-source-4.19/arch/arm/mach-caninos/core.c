#include <linux/irqchip.h>
#include <linux/of_platform.h>
#include <linux/memblock.h>
#include <asm/mach/map.h>
#include <../mm/mm.h>
#include <mach/platform.h>

static struct map_desc caninos_k5_io_desc[] __initdata = {
	{
		.virtual = IO_ADDRESS(K5_PA_REG_BASE),
		.pfn     = __phys_to_pfn(K5_PA_REG_BASE),
		.length  = K5_PA_REG_SIZE,
		.type    = MT_DEVICE,
	},
	{
		.virtual = K5_VA_BOOT_RAM,
		.pfn     = __phys_to_pfn(K5_PA_BOOT_RAM),
		.length  = SZ_4K,
		.type    = MT_MEMORY_RWX,
	},
};

void __init caninos_k5_map_io(void)
{
	iotable_init(caninos_k5_io_desc, ARRAY_SIZE(caninos_k5_io_desc));
}

void __init caninos_k5_init_irq(void)
{
	irqchip_init();
}

static struct of_device_id caninos_k5_dt_match_table[] __initdata = {
	{ .compatible = "simple-bus", },
	{}
};

void __init caninos_k5_init(void)
{
	int ret;
	
	//pm_power_off = board_pm_halt;
	
	ret = of_platform_populate(NULL, caninos_k5_dt_match_table, NULL, NULL);
	
	if (ret) {
		panic("of_platform_populate() failed\n");
	}
}

void __init caninos_k5_reserve(void)
{
	phys_addr_t kinfo_start, framebuffer_start;
	
	framebuffer_start = arm_lowmem_limit - K5_FRAME_BUFFER_SIZE;
	kinfo_start = framebuffer_start - K5_KINFO_SIZE;
	
	memblock_reserve(0x0, K5_DDR_DQS_TRAINING_SIZE);
	memblock_reserve(framebuffer_start, K5_FRAME_BUFFER_SIZE);
	memblock_reserve(kinfo_start, K5_KINFO_SIZE);
}

