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

#include <mach/platform.h>
#include <linux/mfd/atc260x/atc260x.h>

#define PMIC_I2C_ADDR (0x65)

static void __init timer0_setup(void)
{
	/* enable the clock of timer */
	io_setl(BIT(27), CMU_DEVCLKEN1);
	
	/* disable timer0 and timer1 */
	io_writel(0U, T0_CTL);
	io_writel(0U, T1_CTL);
	
	/* clear value and comparator */
	io_writel(0U, T0_VAL);
	io_writel(0U, T0_CMP);
	io_writel(0U, T1_VAL);
	io_writel(0U, T1_CMP);
	
	/* enable timer0 with IRQs disabled */
	io_writel(4U, T0_CTL);
}

static void __init timer0_delay_us(unsigned int delay)
{
	u32 val = delay * 24U; /* 1us = 24 clock cycles (24MHz) */
	
	if (!delay) {
		return;
	}
	
	/* clear timer0 value (start counting) */
	io_writel(0U, T0_VAL);
	
	/* busy wait */
	while (io_readl(T0_VAL) < val) {
		nop();
	}
}

static void __init i2c0_setup(void)
{
	io_setl(BIT(0), CMU_ETHERNETPLL);
	
	timer0_delay_us(600);
	
	io_setl(BIT(14), CMU_DEVCLKEN1);
	io_readl(CMU_DEVCLKEN1);
	
	io_clearl(BIT(12), CMU_DEVRST1);
	io_readl(CMU_DEVRST1);
	
	timer0_delay_us(20);
	
	io_setl(BIT(12), CMU_DEVRST1);
	io_readl(CMU_DEVRST1);
	
	timer0_delay_us(50);
	
	/* I2C clock divider (400kHz) */
	io_writel((5U << 8) | (16U << 0), I2C0_CLKDIV);
}

static bool __init i2c0_write_reg(u8 bus_addr, u8 reg, u16 data)
{
	u32 val = 0U;
	
	/* reset all stats */
	io_writel(0xff, I2C0_STAT);
	
	/* disable interrupt */
	io_writel(0xc0, I2C0_CTL);
	
	/* enable i2c without interrupt */
	io_writel(0x80, I2C0_CTL);
	
	/* write data count */
	io_writel(2U, I2C0_DATCNT);
	
	/* write slave addr */
	io_writel(bus_addr << 1, I2C0_TXDAT);
	
	/* write register addr */
	io_writel(reg, I2C0_TXDAT);
	
	/* write data */
	io_writel((data >> 8) & 0xff, I2C0_TXDAT);
	io_writel(data & 0xff, I2C0_TXDAT);
	
	/* write fifo command */
	io_writel(0x8d05, I2C0_CMD);
	
	/* wait for the command to complete */
	while (1) {
		val = io_readl(I2C0_FIFOSTAT);
		
		if (val & (0x1 << 1)) /* nack */
		{
			/* clear error bit */
			io_writel((0x1 << 1), I2C0_FIFOSTAT);
			
			/* reset fifo */
			io_writel(0x06, I2C0_FIFOCTL);
			io_readl(I2C0_FIFOCTL);
			
			/* disable adapter */
			io_writel(0U, I2C0_CTL);
			return false;
		}
		
		/* execute complete */
		if (val & (0x1 << 0)) {
			break;
		}
	}
	
	/* disable adapter */
	io_writel(0U, I2C0_CTL);
	return true;
}

static bool __init i2c0_read_reg(u8 bus_addr, u8 reg, u16 * data)
{
	u32 val = 0U;
	
	/* reset all stats */
	io_writel(0xff, I2C0_STAT);
	
	/* disable interrupt */
	io_writel(0xc0, I2C0_CTL);
	
	/* enable i2c without interrupt */
	io_writel(0x80, I2C0_CTL);
	
	/* write data count */
	io_writel(2U, I2C0_DATCNT);
	
	/* write slave addr */
	io_writel(bus_addr << 1, I2C0_TXDAT);
	
	/* write register addr */
	io_writel(reg, I2C0_TXDAT);
	
	/* write (slave addr | read_flag) */
	io_writel((bus_addr << 1) | 0x1, I2C0_TXDAT);
	
	/* write fifo command */
	io_writel(0x8f35, I2C0_CMD);
	
	/* wait command complete */
	while (1) {
		val = io_readl(I2C0_FIFOSTAT);
		
		if (val & (0x1 << 1)) /* nack */
		{
			/* clear error bit */
			io_writel((0x1 << 1), I2C0_FIFOSTAT);
			
			/* reset fifo */
			io_writel(0x06, I2C0_FIFOCTL);
			io_readl(I2C0_FIFOCTL);
			
			/* disable adapter */
			io_writel(0U, I2C0_CTL);
			return false;
		}
		
		/* execute complete */
		if (val & (0x1 << 0)) {
			break;
		}
	}
	
	/* read data from rxdata */
	*data = (io_readl(I2C0_RXDAT) & 0xff) << 8;
	*data |= io_readl(I2C0_RXDAT) & 0xff;
	
	/* disable adapter */
	io_writel(0U, I2C0_CTL);
	return true;
}

static bool __init pmic_reg_read(u8 reg, u16 * data) {
	return i2c0_read_reg(PMIC_I2C_ADDR, reg, data);
}

static bool __init pmic_reg_write(u8 reg, u16 data) {
	return i2c0_write_reg(PMIC_I2C_ADDR, reg, data);
}

static bool __init pmic_reg_setbits(u8 reg, u16 mask, u16 val)
{
	u16 tmp, orig;
	
	if (!pmic_reg_read(reg, &orig)) {
		return false;
	}
	
	tmp = orig & ~mask;
	tmp |= val & mask;
	
	if (tmp != orig) {
		return pmic_reg_write(reg, tmp);
	}
	return true;
}

static bool __init pmic_cmu_reset(void)
{
	u16 reg_val;
	
	if (!pmic_reg_read(ATC2603C_CMU_DEVRST, &reg_val)) {
		return false;
	}
	if (!pmic_reg_write(ATC2603C_CMU_DEVRST, reg_val & ~(0x1 << 2))) {
		return false;
	}
	
	timer0_delay_us(50);
	
	if (!pmic_reg_write(ATC2603C_CMU_DEVRST, reg_val)) {
		return false;
	}
	timer0_delay_us(50);
	return true;
}

bool __init caninos_k5_pmic_setup(void)
{
	u16 data;
	
	timer0_setup();
	i2c0_setup();
	
	if (!pmic_reg_read(ATC2603C_PMU_OC_INT_EN, &data)) {
		return false;
	}
	if (data != 0x1bc0)
	{
		pr_err("Invalid PMIC model\n");
		return false;
	}
	
	/* setup dbg ctl reg */
	if (!pmic_reg_read(ATC2603C_PMU_BDG_CTL, &data)) {
		return false;
	}
	
	data |= (0x1 << 7);   /* dbg enable */
	data |= (0x1 << 6);   /* dbg filter */
	data &= ~(0x1 << 5);  /* disable pulldown resistor */
	data &= ~(0x1 << 11); /* efuse */
	
	if (!pmic_reg_write(ATC2603C_PMU_BDG_CTL, data)) {
		return false;
	}
	
	/* setup interrupts and reset ATC260X INTC */
	if (!pmic_cmu_reset()) {
		return false;
	}
	
	/* disable all sources */
	if (!pmic_reg_write(ATC2603C_INTS_MSK, 0)) {
		return false;
	}
	
	/* enable P_EXTIRQ pad */
	if (!pmic_reg_setbits(ATC2603C_PAD_EN, 0x1, 0x1)) {
		return false;
	}
	return true;
}

static bool __init pmic_dcdc1_set_selector(u32 selector)
{
	if (!pmic_reg_setbits(ATC2603C_PMU_DC1_CTL0, (0x1F << 7), selector << 7)) {
		return false;
	}
	timer0_delay_us(350);
	return true;
}

bool __init caninos_k5_cpu_set_clock(unsigned int freq, unsigned int voltage)
{
	bool skip = false, safe_mode = false;
	u32 val, old_freq, selector;
	
	/* find the nearest valid voltage and validate it (must round up) */
	if (voltage % 25U) {
		voltage = ((voltage / 25U) + 1) * 25U;
	}
	if (voltage < 700U || voltage > 1400U) {
		safe_mode = true;
	}
	
	/* find the nearest valid frequency and validate it (must round down) */
	freq = (freq / 12U) * 12U;
	
	if (freq < 408U || freq > 1308U) {
		safe_mode = true;
	}
	
	if (safe_mode)
	{
		voltage = 975U;
		freq = 720U;
	}
	
	/* calculate the voltage selector */
	selector = (voltage - 700U) / 25U;
	
	/* read current core clock frequency in MHz */
	val = io_readl(CMU_COREPLL);
	old_freq = (val & 0xff) * 12U;
	
	/* if both frequencies are equal, only update the core voltage */
	if (freq == old_freq)
	{
		if (!pmic_dcdc1_set_selector(selector)) {
			return false;
		}
		skip = true;
	}
	
	/* if the new frequency is bigger than the current running frequency */
	/* the core voltage must be increased/updated before upping the clock */
	if (freq > old_freq) {
		if (!pmic_dcdc1_set_selector(selector)) {
			return false;
		}
	}
	
	/* update the core clock */
	if (!skip) {
		val &= ~(0xff);
		io_writel(val | (freq / 12U), CMU_COREPLL);
		
		/* wait for the core PLL to lock */
		while (!((io_readl(CMU_COREPLLDEBUG) >> 11) & 0x1)) {
			nop();
		}
	}
	
	/* if the new frequency is smaller than the old frequency */
	/* the core voltage must be decreased/updated now */
	if (freq < old_freq) {
		if (!pmic_dcdc1_set_selector(selector)) {
			return false;
		}
	}
	
	pr_info("CPU running at %uMHz and %umV\n", freq, voltage);
	return true;
}

