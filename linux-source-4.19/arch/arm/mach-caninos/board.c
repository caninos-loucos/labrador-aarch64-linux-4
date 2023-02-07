
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
#include <asm/cacheflush.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>

#include <asm/hardware/cache-l2x0.h>

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
	//.smp_init     = board_smp_init,
	//.init_early   = board_init_early,
	//.map_io       = board_map_io,
	//.reserve      = board_reserve,
	//.init_irq     = board_init_irq,
	//.init_machine = board_init,
MACHINE_END
