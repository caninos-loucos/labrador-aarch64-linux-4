#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <mach/k5-regs.h>
#include <asm/hardware/cache-l2x0.h>

/*
 1) Instruction prefetch enable
 2) Data prefetch enable
 3) Round-robin replacement
 4) Use AWCACHE attributes for WA
 5) 32kB way size, 16 way associativity
 6) Disable exclusive cache
*/

#define K5_L310_MASK 0xc0000fff

#define K5_L310_VAL \
	(L310_AUX_CTRL_INSTR_PREFETCH | L310_AUX_CTRL_DATA_PREFETCH | \
	 L310_AUX_CTRL_NS_INT_CTRL | L310_AUX_CTRL_NS_LOCKDOWN | \
	 L310_AUX_CTRL_CACHE_REPLACE_RR | L2C_AUX_CTRL_WAY_SIZE(2) | \
	 L310_AUX_CTRL_ASSOCIATIVITY_16)

#define K5_KINFO_SIZE            0x00100000
#define K5_FRAME_BUFFER_SIZE     0x00800000
#define K5_DDR_DQS_TRAINING_SIZE 0x00004000

#define K5_BOOT_FLAG 0x55AA

#define K5_CPU2_POWER_PWR_BIT  5U
#define K5_CPU2_POWER_ACK_BIT 21U
#define K5_CPU2_CLOCK_RST_BIT  6U
#define K5_CPU2_CLOCK_PWR_BIT  2U

#define K5_CPU3_POWER_PWR_BIT  6U
#define K5_CPU3_POWER_ACK_BIT 22U
#define K5_CPU3_CLOCK_RST_BIT  7U
#define K5_CPU3_CLOCK_PWR_BIT  3U

#define K5_DBG1_CLOCK_RST_BIT 29U

void caninos_k5_map_io(void);
bool caninos_k5_smp_init(void);
void caninos_k5_secondary_startup(void);
void caninos_k5_init_irq(void);
void caninos_k5_init(void);
void caninos_k5_reserve(void);

#endif /* _PLATFORM_H_ */
