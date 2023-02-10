#ifndef _K5_REGS_H_
#define _K5_REGS_H_

#define K5_SDRAM_BASE     0x00000000
#define K5_IO_DEVICE_BASE 0xB0000000
#define K5_IO_ADDR_BASE   0xF8000000

#define K5_PA_REG_BASE    0xB0000000
#define K5_PA_REG_SIZE    0x00600000
#define K5_PA_SCU         0xB0020000
#define K5_PA_BOOT_RAM    0xFFFF8000
#define K5_VA_BOOT_RAM    0xFFFF8000
#define K5_CPU1_ADDR      0xB0168050
#define K5_CPU2_ADDR      0xB0168054
#define K5_CPU3_ADDR      0xB0168058
#define K5_CPU1_FLAG      0xB016805C
#define K5_CPU2_FLAG      0xB0168060
#define K5_CPU3_FLAG      0xB0168064
#define K5_SPS_PG_CTL     0xB01B0100
#define K5_CMU_CORECTL    0xB016009C
#define K5_CMU_DEVRST1    0xB01600AC

#ifndef __ASSEMBLY__

#include <asm/io.h>

#define IO_ADDRESS(x) (K5_IO_ADDR_BASE + ((x) & 0x03ffffff))

static inline void io_writel(u32 val, u32 reg)
{
	__raw_writel(val, (volatile void __iomem *)IO_ADDRESS(reg));
}

static inline u32 io_readl(u32 reg)
{
	return __raw_readl((volatile void __iomem *)IO_ADDRESS(reg));
}

#endif /* __ASSEMBLY__ */

#endif /* _K5_REGS_H_ */
