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

#ifndef _K5_REGS_H_
#define _K5_REGS_H_

#define SDRAM_BASE         (0x00000000)
#define IO_DEVICE_BASE     (0xB0000000)
#define IO_ADDR_BASE       (0xF8000000)
#define PA_BOOT_RAM        (0xFFFF8000)
#define VA_BOOT_RAM        (0xFFFF8000)
#define PA_REG_BASE        (0xB0000000)
#define PA_REG_SIZE        (0x00600000)
#define PA_SCU             (0xB0020000)

#define SPS_PG_BASE        (0xB01B0100)
#define SPS_PG_CTL         (SPS_PG_BASE + 0x0000)
#define SPS_RST_CTL        (SPS_PG_BASE + 0x0004)

#define CMU_BASE           (0xB0160000)
#define CMU_COREPLL        (CMU_BASE + 0x0000)
#define CMU_ETHERNETPLL    (CMU_BASE + 0x0084)
#define CMU_CORECTL        (CMU_BASE + 0x009C)
#define CMU_DEVCLKEN0      (CMU_BASE + 0x00A0)
#define CMU_DEVCLKEN1      (CMU_BASE + 0x00A4)
#define CMU_DEVRST0        (CMU_BASE + 0x00A8)
#define CMU_DEVRST1        (CMU_BASE + 0x00AC)
#define CMU_COREPLLDEBUG   (CMU_BASE + 0x00D8)

#define I2C0_BASE          (0xB0170000)
#define I2C0_CTL           (I2C0_BASE + 0x0000)
#define I2C0_CLKDIV        (I2C0_BASE + 0x0004)
#define I2C0_STAT          (I2C0_BASE + 0x0008)
#define I2C0_ADDR          (I2C0_BASE + 0x000C)
#define I2C0_TXDAT         (I2C0_BASE + 0x0010)
#define I2C0_RXDAT         (I2C0_BASE + 0x0014)
#define I2C0_CMD           (I2C0_BASE + 0x0018)
#define I2C0_FIFOCTL       (I2C0_BASE + 0x001C)
#define I2C0_FIFOSTAT      (I2C0_BASE + 0x0020)
#define I2C0_DATCNT        (I2C0_BASE + 0x0024)
#define I2C0_RCNT          (I2C0_BASE + 0x0028)

#define TIMER_2HZ_BASE     (0xB0168000)
#define TWOHZ0_CTL         (TIMER_2HZ_BASE + 0x0000)
#define T0_CTL             (TIMER_2HZ_BASE + 0x0008)
#define T0_CMP             (TIMER_2HZ_BASE + 0x000C)
#define T0_VAL             (TIMER_2HZ_BASE + 0x0010)
#define T1_CTL             (TIMER_2HZ_BASE + 0x0014)
#define T1_CMP             (TIMER_2HZ_BASE + 0x0018)
#define T1_VAL             (TIMER_2HZ_BASE + 0x001C)
#define TWOHZ1_CTL         (TIMER_2HZ_BASE + 0x0020)
#define CPU1_ADDR          (TIMER_2HZ_BASE + 0x0050)
#define CPU2_ADDR          (TIMER_2HZ_BASE + 0x0054)
#define CPU3_ADDR          (TIMER_2HZ_BASE + 0x0058)
#define CPU1_FLAG          (TIMER_2HZ_BASE + 0x005C)
#define CPU2_FLAG          (TIMER_2HZ_BASE + 0x0060)
#define CPU3_FLAG          (TIMER_2HZ_BASE + 0x0064)

#ifndef __ASSEMBLY__

#include <asm/io.h>

#define IO_ADDRESS(x) (IO_ADDR_BASE + ((x) & 0x03ffffff))

static inline void io_writel(u32 val, u32 reg)
{
	__raw_writel(val, (volatile void __iomem *)IO_ADDRESS(reg));
}

static inline u32 io_readl(u32 reg)
{
	return __raw_readl((volatile void __iomem *)IO_ADDRESS(reg));
}

static inline void io_clearl(u32 val, u32 reg)
{
	io_writel(io_readl(reg) & (~val), reg);
}

static inline void io_setl(u32 val, u32 reg)
{
	io_writel(io_readl(reg) | val, reg);
}

#endif /* __ASSEMBLY__ */

#endif /* _K5_REGS_H_ */
