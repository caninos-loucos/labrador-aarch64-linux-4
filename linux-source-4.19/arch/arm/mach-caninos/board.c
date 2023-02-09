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

#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include <mach/platform.h>

#define DRIVER_NAME "board-setup"

#define INFO_MSG(fmt,...) pr_info(DRIVER_NAME ": " fmt, ##__VA_ARGS__)
#define ERR_MSG(fmt,...) pr_err(DRIVER_NAME ": " fmt, ##__VA_ARGS__)

#define KINFO_SIZE (SZ_1M)
#define FB_SIZE (8 * SZ_1M)

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

static const char *const caninos_dt_compat[] __initconst = {
	"caninos,k5",
	NULL,
};

DT_MACHINE_START(CANINOS, "Caninos Labrador 32bits Core Board")
	.dt_compat    = caninos_dt_compat,
	.atag_offset  = 0x00000100,
	.l2c_aux_val  = K5_L310_VAL,
	.l2c_aux_mask = K5_L310_MASK,
	.map_io       = caninos_k5_map_io,
	.smp_init     = caninos_k5_smp_init,
	
	.init_early   = board_init_early,
	
	.reserve      = caninos_k5_reserve,
	.init_irq     = caninos_k5_init_irq,
	.init_machine = caninos_k5_init,
MACHINE_END
