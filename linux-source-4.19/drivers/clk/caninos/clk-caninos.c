/*
 * Copyright (c) 2019 LSI-TEC - Caninos Loucos
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

#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clkdev.h>

#include <dt-bindings/clock/caninos-clk.h>

/* Base1 */
#define CMU_DEVPLL 		0x04
#define CMU_NANDPLL 	0x0C
#define CMU_AUDIOPLL 	0x14
#define CMU_BUSCLK1  	0x38
#define CMU_SD0CLK 		0x50
#define CMU_SD1CLK 		0x54
#define CMU_SD2CLK 		0x58
#define CMU_UART0CLK 	0x5C
#define CMU_UART1CLK 	0x60
#define CMU_UART2CLK 	0x64
#define CMU_UART3CLK 	0x68
#define CMU_UART4CLK 	0x6C
#define CMU_UART5CLK 	0x70
#define CMU_UART6CLK 	0x74
#define CMU_DEVCLKEN0 	0xA0
#define CMU_DEVCLKEN1 	0xA4
/* Base2 */
#define CMU_USBPLL 		0xB0
#define CMU_ETHERNETPLL	0xB4
#define CMU_CVBSPLL		0xB8
#define CMU_SSTSCLK		0xC0

struct caninos_clk_provider
{
	void __iomem *base1;
	void __iomem *base2;
	
	struct clk_onecell_data clk_data;
	
	unsigned long hosc;
	unsigned long losc;
	unsigned long dev_clk;
	unsigned long dev_pll;
	unsigned long nand_pll;
	unsigned long ethernet_pll;
};

struct caninos_clk_hw
{
	struct clk_hw hw;
	struct caninos_clk_provider *common;
	unsigned long bit, offset; /* gate enable */
	unsigned long val_offset; /* uart/sdc clk value */
	int id;
};

static struct caninos_clk_hw *to_caninos_clk_hw(struct clk_hw *hw) {
	return container_of(hw, struct caninos_clk_hw, hw);
}

static void __iomem *get_reg_addr(struct caninos_clk_hw *clk, unsigned long off)
{
	if (off >= CMU_USBPLL) {
		return clk->common->base2 + (off - CMU_USBPLL);
	}
	else {
		return clk->common->base1 + off;
	}
}

static inline long calc_clk_error(long rate1, long rate2)
{
	if (rate1 > rate2) {
		return (rate1 - rate2);
	}
	else {
		return (rate2 - rate1);
	}
}

static long calc_clk_div(long rate, long src, long min, long max, long mul)
{
	long div1, div2, rate1, rate2;
	
	min++;
	
	if (rate > 0) {
		div1 = (src + (rate * mul) - 1) / (rate * mul);
	}
	else {
		div1 = max;
	}
	
	if (div1 < min) {
		div1 = min;
	}
	if (div1 > max) {
		div1 = max;
	}
	
	div2 = (div1 - 1);
	
	rate1 = src / (div1 * mul);
	rate2 = src / (div2 * mul);
	
	if (calc_clk_error(rate1, rate) < calc_clk_error(rate2, rate)) {
		return div1;
	}
	else {
		return div2;
	}
}

static unsigned long uart_clk_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, clk->val_offset);
	u32 sourcesel, uartclk, uartdiv;
	unsigned long rate;
	
	uartclk = readl(base);
	sourcesel = (uartclk >> 16) & 0x1;
	uartdiv = (uartclk & 0x1FF) + 1;
	
	if (sourcesel) {
		rate = clk->common->dev_pll;
	}
	else {
		rate = clk->common->hosc;
	}
	
	if (uartdiv > 312) {
		rate /= 624;
	}
	else {
		rate /= uartdiv;
	}
	return rate;
}

static long uart_clk_round_rate(struct clk_hw *hw, unsigned long rate, 
                                unsigned long *parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	long rate1, rate2, hosc, dev_pll;
	
	hosc = clk->common->hosc;
	dev_pll = clk->common->dev_pll;
	
	rate1 = hosc / calc_clk_div(rate, hosc, 1, 312, 1);
	
	if (calc_clk_error(hosc / 624, rate) < calc_clk_error(rate1, rate)) {
		rate1 = hosc / 624;
	}
	
	rate2 = dev_pll / calc_clk_div(rate, dev_pll, 1, 312, 1);
	
	if (calc_clk_error(dev_pll / 624, rate) < calc_clk_error(rate2, rate)) {
		rate2 = dev_pll / 624;
	}
	
	if (calc_clk_error(rate1, rate) < calc_clk_error(rate2, rate)) {
		return rate1;
	}
	else {
		return rate2;
	}
}

static int uart_clk_set_rate(struct clk_hw *hw, unsigned long rate, 
                             unsigned long parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, clk->val_offset);
	long div1, div2, rate1, rate2, hosc, dev_pll;
	u32 uartclk;
	
	hosc = clk->common->hosc;
	dev_pll = clk->common->dev_pll;
	
	div1 = calc_clk_div(rate, hosc, 1, 312, 1);
	rate1 = hosc / div1;
	
	if (calc_clk_error(hosc / 624, rate) < calc_clk_error(rate1, rate))
	{
		rate1 = hosc / 624;
		div1 = 624;
	}
	
	div2 = calc_clk_div(rate, dev_pll, 1, 312, 1);
	rate2 = dev_pll / div2;
	
	if (calc_clk_error(dev_pll / 624, rate) < calc_clk_error(rate2, rate))
	{
		rate2 = dev_pll / 624;
		div2 = 624;
	}
	
	uartclk = readl(base);
	
	if (calc_clk_error(rate1, rate) < calc_clk_error(rate2, rate))
	{
		uartclk &= ~(0x1FF);
		uartclk &= ~(0x1 << 16); /* use hosc */
		
		if (div1 == 624) {
			uartclk |= 312u;
		}
		else {
			uartclk |= (u32)(div1 - 1) & 0x1FF;
		}
	}
	else
	{
		uartclk &= ~(0x1FF);
		uartclk |= (0x1 << 16); /* use dev_pll */
		
		if (div2 == 624) {
			uartclk |= 312u;
		}
		else {
			uartclk |= (u32)(div2 - 1) & 0x1FF;
		}
	}
	writel(uartclk, base);
	return 0;
}

static inline long sdc_div_calc(struct caninos_clk_hw *clk, long rate,
                                bool use_devclk, bool* use_div128, long *div)
{
	long rate1, rate2, div1, div2, tmp1, tmp2;
	
	if (use_devclk) {
		rate1 = rate2 = clk->common->dev_clk;
	}
	else {
		rate1 = rate2 = clk->common->nand_pll;
	}
	
	div1 = (rate1 + rate * 128 - 1) / (rate * 128);
	div2 = (rate2 + rate - 1) / (rate);
	
	if (div1 <= 0) {
		div1 = 1;
	}
	if (div1 > 24) {
		div1 = 24;
	}
	if (div2 <= 0) {
		div2 = 1;
	}
	if (div2 > 24) {
		div2 = 24;
	}
	
	tmp1 = rate1 / (div1 * 128);
	tmp2 = rate1 / ((div1 + 1) * 128);
	
	if (calc_clk_error(tmp1, rate) < calc_clk_error(tmp2, rate)) {
		rate1 = tmp1;
	}
	else
	{
		rate1 = tmp2;
		div1++;
	}
	
	tmp1 = rate2 / (div2);
	tmp2 = rate2 / (div2 + 1);
	
	if (calc_clk_error(tmp1, rate) < calc_clk_error(tmp2, rate)) {
		rate2 = tmp1;
	}
	else
	{
		rate2 = tmp2;
		div2++;
	}
	
	if (calc_clk_error(rate1, rate) < calc_clk_error(rate2, rate))
	{
		*use_div128 = true;
		*div = div1;
		return rate1;
	}
	else
	{
		*use_div128 = false;
		*div = div2;
		return rate2;
	}
}

static long sdc_clk_best(struct caninos_clk_hw *clk, long rate,
                         bool *use_devclk, bool *use_div128, u32 *sdcdiv)
{	
	long rate_devclk, div_devclk;
	long rate_nandpll, div_nandpll;
	bool use_div128_devclk;
	bool use_div128_nandpll;
	
	rate_devclk = sdc_div_calc(clk, rate, true,
	                           &use_div128_devclk, &div_devclk);
	rate_nandpll = sdc_div_calc(clk, rate, false,
	                           &use_div128_nandpll, &div_nandpll);
	
	if (calc_clk_error(rate_devclk, rate) < calc_clk_error(rate_nandpll, rate))
	{
		*use_devclk = true;
		*use_div128 = use_div128_devclk;
		*sdcdiv = div_devclk;
		return rate_devclk;
	}
	else
	{
		*use_devclk = false;
		*use_div128 = use_div128_nandpll;
		*sdcdiv = div_nandpll;
		return rate_nandpll;
	}
}

static unsigned long sdc_clk_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, clk->val_offset);
	u32 clksel, sdclk, div, div128;
	unsigned long rate;
	
	if (!base) {
		return (unsigned long)(-ENODEV);
	}
	
	sdclk = readl(base);
	div = (sdclk & 0x1F) + 1;
	clksel = (sdclk >> 9) & 0x1;
	div128 = (sdclk >> 8) & 0x1;
	
	if (clksel) {
		rate = clk->common->nand_pll;
	}
	else {
		rate = clk->common->dev_clk;
	}
	
	if (div > 25) {
		div = 25;
	}
	
	if (div128) {
		rate /= (128 * div);
	}
	else {
		rate /= div;
	}
	
	return rate;
}

static long sdc_clk_round_rate(struct clk_hw *hw, unsigned long rate, 
                                unsigned long *parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	bool use_devclk, use_div128;
	u32 sdcdiv;
	
	if (!rate) {
		return -EINVAL;
	}
	
	return sdc_clk_best(clk, rate, &use_devclk, &use_div128, &sdcdiv);
}

static int sdc_clk_set_rate(struct clk_hw *hw, unsigned long rate, 
                            unsigned long parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, clk->val_offset);
	bool use_devclk, use_div128;
	u32 sdcdiv, sdclk;
	
	if (!rate) {
		return -EINVAL;
	}
	
	sdc_clk_best(clk, rate, &use_devclk, &use_div128, &sdcdiv);
	
	sdclk = readl(base);
	
	if (use_devclk) {
		sdclk &= ~(0x1 << 9);
	}
	else {
		sdclk |= (0x1 << 9);
	}
	
	if (use_div128) {
		sdclk |= (0x1 << 8);
	}
	else {
		sdclk &= ~(0x1 << 8);
	}
	
	sdclk &= ~(0x1F);
	sdclk |= (sdcdiv - 1) & 0x1F;
	
	writel(sdclk, base);
	return 0;
}

static unsigned long i2c_clk_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	return clk->common->hosc;
}

static unsigned long audio_pll_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, CMU_AUDIOPLL);
	u32 audiopll;
	
	audiopll = readl(base);
	
	if (audiopll & 0x1) {
		return 49152000;
	}
	else {
		return 45158400;
	}
}

static long audio_pll_round_rate(struct clk_hw *hw, unsigned long rate, 
                                 unsigned long *parent_rate)
{	
	if (!rate) {
		return -EINVAL;
	}
	
	if (calc_clk_error(rate, 49152000) < calc_clk_error(rate, 45158400)) {
		return 49152000;
	}
	else {
		return 45158400;
	}
}

static int audio_pll_set_rate(struct clk_hw *hw, unsigned long rate, 
                            unsigned long parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, CMU_AUDIOPLL);
	u32 audiopll;
	
	if (!rate) {
		return -EINVAL;
	}
	
	audiopll = readl(base);
	
	if (calc_clk_error(rate, 49152000) < calc_clk_error(rate, 45158400)) {
		audiopll |= 0x1;
	}
	else {
		audiopll &= ~(0x1);
	}
	
	writel(audiopll, base);
	return 0;
}

static int gate_clk_enable(struct clk_hw *hw)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, clk->offset);
	u32 regval, mask = (0x1 << clk->bit);
	
	regval = readl(base);
	
	if ((regval & mask) == 0)
	{
		writel(regval | mask, base);
		regval = readl(base);
	}
	
	if (regval & mask) {
		return 0;
	}
	else {
		return -EIO;
	}
}

static void gate_clk_disable(struct clk_hw *hw)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, clk->offset);
	u32 regval, mask = (0x1 << clk->bit);
	
	regval = readl(base);
	
	if (regval & mask) {
		writel(regval & ~mask, base);
	}
}

static int gate_clk_enabled(struct clk_hw *hw)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, clk->offset);
	u32 regval, mask = (0x1 << clk->bit);
	
	regval = readl(base);
	
	if (regval & mask) {
		return 1;
	}
	else {
		return 0;
	}
}

static long i2s_clk_round_rate(struct clk_hw *hw, unsigned long rate, 
                               unsigned long *parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, CMU_AUDIOPLL);
	long div, pll_rate;
	u32 audiopll;
	
	if (!rate) {
		return -EINVAL;
	}
	
	audiopll = readl(base);
	
	if (audiopll & 0x1) {
		pll_rate = 49152000;
	}
	else {
		pll_rate = 45158400;
	}
	
	div = pll_rate / rate;
	
	if (div < 1) {
		div = 1;
	}
	else if (div > 24) {
		div = 24;
	}
	else if (div == 5) {
		div = 4;
	}
	else if (div == 7) {
		div = 6;
	}
	else if ((div > 8) && (div < 12)) {
		div = 8;
	}
	else if ((div > 12) && (div < 16)) {
		div = 12;
	}
	else if ((div > 16) && (div < 24)) {
		div = 16;
	}
	
	return pll_rate / div;
}

static unsigned long i2s_clk_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, CMU_AUDIOPLL);
	long pll_rate, div;
	u32 audiopll;
	
	audiopll = readl(base);
	
	if (audiopll & 0x1) {
		pll_rate = 49152000;
	}
	else {
		pll_rate = 45158400;
	}
	
	if (clk->id == CLK_I2SRX) {
		div = (audiopll >> 20) & 0xF;
	}
	else {
		div = (audiopll >> 16) & 0xF;
	}
	
	switch(div)
	{
	case 0:
		div = 1;
		break;
	case 1:
		div = 2;
		break;
	case 2:
		div = 3;
		break;
	case 3:
		div = 4;
		break;
	case 4:
		div = 6;
		break;
	case 5:
		div = 8;
		break;
	case 6:
		div = 12;
		break;
	case 7:
		div = 16;
		break;
	case 8:
		div = 24;
		break;
	}
	
	return pll_rate / div;
}

static int i2s_clk_set_rate(struct clk_hw *hw, unsigned long rate, 
                            unsigned long parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, CMU_AUDIOPLL);
	long div, pll_rate;
	u32 audiopll, aux;
	
	if (!rate) {
		return -EINVAL;
	}
	
	audiopll = readl(base);
	
	if (audiopll & 0x1) {
		pll_rate = 49152000;
	}
	else {
		pll_rate = 45158400;
	}
	
	div = pll_rate / rate;
	
	if (div <= 1)
	{
		div = 1;
		aux = 0;
	}
	else if (div == 2)
	{
		aux = 1;
	}
	else if (div == 3)
	{
		aux = 2;
	}
	else if ((div > 3) && (div <= 5))
	{
		div = 4;
		aux = 3;
	}
	else if ((div > 5) && (div <= 6))
	{
		div = 6;
		aux = 4;
	}
	else if ((div > 6) && (div <= 8))
	{
		div = 8;
		aux = 5;
	}
	else if ((div > 8) && (div <= 12))
	{
		div = 12;
		aux = 6;
	}
	else if ((div > 12) && (div <= 16))
	{
		div = 16;
		aux = 7;
	}
	else
	{
		div = 24;
		aux = 8;
	}
	
	if (clk->id == CLK_I2SRX)
	{
		audiopll &= ~(0xF << 20);
		audiopll |=  (aux << 20);
	}
	else
	{
		audiopll &= ~(0xF << 16);
		audiopll |=  (aux << 16);
	}
	
	writel(audiopll, base);
	return 0;
}

static unsigned long ether_clk_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	return clk->common->ethernet_pll;
}

static unsigned long rmii_clk_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, CMU_ETHERNETPLL);
	u32 ethpll;
	
	ethpll = readl(base);
	
	if (ethpll & BIT(2)) {
		return clk->common->ethernet_pll / 10;
	}
	else {
		return clk->common->ethernet_pll / 4;
	}
}

static long rmii_clk_round_rate(struct clk_hw *hw, unsigned long rate, 
                                unsigned long *parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	long rate4, rate10;
	
	if (!rate) {
		return -EINVAL;
	}
	
	rate10 = clk->common->ethernet_pll / 10;
	rate4 = clk->common->ethernet_pll / 4;
	
	if (calc_clk_error(rate, rate4) < calc_clk_error(rate, rate10)) {
		return rate4;
	}
	else {
		return rate10;
	}
}

static int rmii_clk_set_rate(struct clk_hw *hw, unsigned long rate, 
                             unsigned long parent_rate)
{
	struct caninos_clk_hw *clk = to_caninos_clk_hw(hw);
	void __iomem *base = get_reg_addr(clk, CMU_ETHERNETPLL);
	long rate4, rate10;
	u32 ethpll;
	
	if (!rate) {
		return -EINVAL;
	}
	
	ethpll = readl(base);
	
	rate10 = clk->common->ethernet_pll / 10;
	rate4 = clk->common->ethernet_pll / 4;
	
	ethpll &= ~BIT(1);
	
	if (calc_clk_error(rate, rate4) < calc_clk_error(rate, rate10)) {
		ethpll &= ~BIT(2);
	}
	else {
		ethpll |= BIT(2);
	}
	
	writel(ethpll, base);
	return 0;
}

const static struct clk_ops uart_clk_ops = {
	.enable      = gate_clk_enable,
	.disable     = gate_clk_disable,
	.is_enabled  = gate_clk_enabled,
	.recalc_rate = uart_clk_rate,
	.round_rate  = uart_clk_round_rate,
	.set_rate    = uart_clk_set_rate,
};

const static struct clk_ops sdc_clk_ops = {
	.enable      = gate_clk_enable,
	.disable     = gate_clk_disable,
	.is_enabled  = gate_clk_enabled,
	.recalc_rate = sdc_clk_rate,
	.round_rate  = sdc_clk_round_rate,
	.set_rate    = sdc_clk_set_rate,
};

const static struct clk_ops i2c_clk_ops = {
	.enable      = gate_clk_enable,
	.disable     = gate_clk_disable,
	.is_enabled  = gate_clk_enabled,
	.recalc_rate = i2c_clk_rate,
};

const static struct clk_ops i2s_clk_ops = {
	.enable      = gate_clk_enable,
	.disable     = gate_clk_disable,
	.is_enabled  = gate_clk_enabled,
	.recalc_rate = i2s_clk_rate,
	.round_rate  = i2s_clk_round_rate,
	.set_rate    = i2s_clk_set_rate,
};

const static struct clk_ops audio_pll_ops = {
	.enable      = gate_clk_enable,
	.disable     = gate_clk_disable,
	.is_enabled  = gate_clk_enabled,
	.recalc_rate = audio_pll_rate,
	.round_rate  = audio_pll_round_rate,
	.set_rate    = audio_pll_set_rate,
};

const static struct clk_ops ether_pll_ops = {
	.enable      = gate_clk_enable,
	.disable     = gate_clk_disable,
	.is_enabled  = gate_clk_enabled,
	.recalc_rate = ether_clk_rate,
};

const static struct clk_ops gate_clk_ops = {
	.enable      = gate_clk_enable,
	.disable     = gate_clk_disable,
	.is_enabled  = gate_clk_enabled,
};

const static struct clk_ops rmii_pll_ops = {
	.enable      = gate_clk_enable,
	.disable     = gate_clk_disable,
	.is_enabled  = gate_clk_enabled,
	.recalc_rate = rmii_clk_rate,
	.round_rate  = rmii_clk_round_rate,
	.set_rate    = rmii_clk_set_rate,
};

#define CANINOS_CLK_HW_UART(x, name, msk, off, voff) \
	{ .hw = { \
		.init = CLK_HW_INIT_NO_PARENT(name, &uart_clk_ops, CLK_IGNORE_UNUSED), \
	}, .id = x, .bit = msk, .offset = off, .val_offset = voff, }

#define CANINOS_CLK_HW_SDC(x, name, msk, off, voff) \
	{ .hw = { \
		.init = CLK_HW_INIT_NO_PARENT(name, &sdc_clk_ops, CLK_IGNORE_UNUSED), \
	}, .id = x, .bit = msk, .offset = off, .val_offset = voff, }
	
#define CANINOS_CLK_HW_I2C(x, name, msk, off) \
	{ .hw = { \
		.init = CLK_HW_INIT_NO_PARENT(name, &i2c_clk_ops, CLK_IGNORE_UNUSED), \
	}, .id = x, .bit = msk, .offset = off, }

#define CANINOS_CLK_HW_I2S(x, name, msk, off) \
	{ .hw = { \
		.init = CLK_HW_INIT_NO_PARENT(name, &i2s_clk_ops, CLK_IGNORE_UNUSED), \
	}, .id = x, .bit = msk, .offset = off}

#define CANINOS_CLK_HW_AUDIOPLL(x, name, msk, off) \
	{ .hw = { \
		.init = CLK_HW_INIT_NO_PARENT(name, &audio_pll_ops, CLK_IGNORE_UNUSED),\
	}, .id = x, .bit = msk, .offset = off }

#define CANINOS_CLK_HW_ETHERNETPLL(x, name, msk, off) \
	{ .hw = { \
		.init = CLK_HW_INIT_NO_PARENT(name, &ether_pll_ops, CLK_IGNORE_UNUSED),\
	}, .id = x, .bit = msk, .offset = off }
	
#define CANINOS_CLK_HW_RMIIREF(x, name, msk, off) \
	{ .hw = { \
		.init = CLK_HW_INIT_NO_PARENT(name, &rmii_pll_ops, CLK_IGNORE_UNUSED),\
	}, .id = x, .bit = msk, .offset = off }

#define CANINOS_CLK_HW_GATE(x, name, msk, off) \
	{ .hw = { \
		.init = CLK_HW_INIT_NO_PARENT(name, &gate_clk_ops, CLK_IGNORE_UNUSED),\
	}, .id = x, .bit = msk, .offset = off }

static struct caninos_clk_hw caninos_clk_tree[] __initdata =
{
	CANINOS_CLK_HW_GATE(CLK_DMAC, "clk-dmac", 17, CMU_DEVCLKEN0),
	
	CANINOS_CLK_HW_UART(CLK_UART0, "clk-uart0", 8, CMU_DEVCLKEN1, CMU_UART0CLK),
	CANINOS_CLK_HW_UART(CLK_UART1, "clk-uart1", 9, CMU_DEVCLKEN1, CMU_UART1CLK),
	CANINOS_CLK_HW_UART(CLK_UART2, "clk-uart2", 10, CMU_DEVCLKEN1, CMU_UART2CLK),
	CANINOS_CLK_HW_UART(CLK_UART3, "clk-uart3", 11, CMU_DEVCLKEN1, CMU_UART3CLK),
	CANINOS_CLK_HW_UART(CLK_UART4, "clk-uart4", 12, CMU_DEVCLKEN1, CMU_UART4CLK),
	CANINOS_CLK_HW_UART(CLK_UART5, "clk-uart5", 13, CMU_DEVCLKEN1, CMU_UART5CLK),
	CANINOS_CLK_HW_UART(CLK_UART6, "clk-uart6", 14, CMU_DEVCLKEN1, CMU_UART6CLK),
	
	CANINOS_CLK_HW_SDC(CLK_SD0, "clk-sd0", 22, CMU_DEVCLKEN0, CMU_SD0CLK),
	CANINOS_CLK_HW_SDC(CLK_SD1, "clk-sd1", 23, CMU_DEVCLKEN0, CMU_SD1CLK),
	CANINOS_CLK_HW_SDC(CLK_SD2, "clk-sd2", 24, CMU_DEVCLKEN0, CMU_SD2CLK),
	
	CANINOS_CLK_HW_I2C(CLK_I2C0, "clk-i2c0", 0, CMU_DEVCLKEN1),
	CANINOS_CLK_HW_I2C(CLK_I2C1, "clk-i2c1", 1, CMU_DEVCLKEN1),
	CANINOS_CLK_HW_I2C(CLK_I2C2, "clk-i2c2", 2, CMU_DEVCLKEN1),
	CANINOS_CLK_HW_I2C(CLK_I2C3, "clk-i2c3", 3, CMU_DEVCLKEN1),
	
	CANINOS_CLK_HW_I2S(CLK_I2SRX, "clk-i2srx", 27, CMU_DEVCLKEN1),
	CANINOS_CLK_HW_I2S(CLK_I2STX, "clk-i2stx", 26, CMU_DEVCLKEN1),
	
	CANINOS_CLK_HW_AUDIOPLL(CLK_AUDIO_PLL, "clk-audio-pll", 4, CMU_AUDIOPLL),
	
	CANINOS_CLK_HW_GATE(CLK_GPIO, "clk-gpio", 25, CMU_DEVCLKEN1),
	CANINOS_CLK_HW_GATE(CLK_USB2H0_PLLEN, "clk-usb2h0-pllen", 12, CMU_USBPLL),
	CANINOS_CLK_HW_GATE(CLK_USB2H0_PHY, "clk-usb2h0-phy", 10, CMU_USBPLL),
	CANINOS_CLK_HW_GATE(CLK_USB2H0_CCE, "clk-usb2h0-cce", 26, CMU_USBPLL),
	CANINOS_CLK_HW_GATE(CLK_USB2H1_PLLEN, "clk-usb2h1-pllen", 13, CMU_USBPLL),
	CANINOS_CLK_HW_GATE(CLK_USB2H1_PHY, "clk-usb2h1-phy", 11, CMU_USBPLL),
	CANINOS_CLK_HW_GATE(CLK_USB2H1_CCE, "clk-usb2h1-cce", 27, CMU_USBPLL),
	
	CANINOS_CLK_HW_ETHERNETPLL(CLK_ETHERNET_PLL, "clk-ethernet-pll", 0, CMU_ETHERNETPLL),
	
	CANINOS_CLK_HW_RMIIREF(CLK_RMII_REF, "clk-rmii-ref", 23, CMU_DEVCLKEN1),
};

void __init caninos_clk_register
	(struct caninos_clk_provider *ctx, struct caninos_clk_hw *models, int nums)
{
	struct caninos_clk_hw *clks;
	int i, err;
	
	clks = kcalloc(nums, sizeof(struct caninos_clk_hw), GFP_KERNEL);
	
	if (!clks) {
		panic("%s: unable to allocate context.\n", __func__);
	}
	
	for (i = 0; i < nums; i++)
	{
		clks[i] = models[i];
		
		clks[i].common = ctx;
		
		err = clk_hw_register(NULL, &clks[i].hw);
		
		if (err)
		{
			pr_err("%s: failed to register clock %s\n", __func__,
			       clk_hw_get_name(&clks[i].hw));
			continue;
		}
		
		ctx->clk_data.clks[clks[i].id] = clks[i].hw.clk;
		
		clk_hw_register_clkdev(&clks[i].hw, clk_hw_get_name(&clks[i].hw), NULL);
	}
}

struct caninos_clk_provider * __init caninos_clk_init(struct device_node *np,
	void __iomem *base1, void __iomem *base2, unsigned long nr_clks)
{
	struct caninos_clk_provider *ctx;
	struct clk **clk_table;
	int err, i;
	
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	
	if (!ctx) {
		panic("could not allocate clock provider context.\n");
	}
	
	clk_table = kcalloc(nr_clks, sizeof(struct clk *), GFP_KERNEL);
	
	if (!clk_table) {
		panic("could not allocate clock lookup table.\n");
	}
	
	for (i = 0; i < nr_clks; ++i) {
		clk_table[i] = ERR_PTR(-ENOENT);
	}
	
	ctx->base1 = base1;
	ctx->base2 = base2;
	ctx->clk_data.clks = clk_table;
	ctx->clk_data.clk_num = nr_clks;
	
	if (!np) {
		return ctx;
	}
	
	err = of_clk_add_provider(np, of_clk_src_onecell_get, &ctx->clk_data);
	
	if (err) {
		panic("could not register clock provider.\n");
	}
	
	return ctx;
}

void __init  caninos_clk_get_common_rates(struct caninos_clk_provider *ctx)
{
	u32 devpll, nandpll, devmul, nandmul, devpen, devclkss, npen;
	
	devpll = readl(ctx->base1 + CMU_DEVPLL);
	nandpll = readl(ctx->base1 + CMU_NANDPLL);
	
	ctx->hosc = 24000000;
	ctx->losc = 32768;
	
	ctx->ethernet_pll = 500000000;
	
	devmul = (devpll & 0x7F);
	devclkss = (devpll >> 12) & 0x1;
	devpen = (devpll >> 8) & 0x1;
	npen = (nandpll >> 8) & 0x1;
	nandmul = (nandpll & 0x7F);
	
	if ((devmul > 126) || (devmul < 8) || !devpen)
	{
		panic("%s: invalid CMU_DEVPLL hardware state 0x%x.\n",
		      __func__, devpll);
	}
	
	if ((nandmul > 86) || (nandmul < 2) || !npen)
	{
		panic("%s: invalid CMU_NANDPLL hardware state 0x%x.\n",
		      __func__, nandpll);
	}
	
	ctx->dev_pll = devmul * 6000000;
	ctx->nand_pll = nandmul * 6000000;
	
	if (devclkss) {
		ctx->dev_clk = ctx->dev_pll;
	}
	else {
		ctx->dev_clk = ctx->hosc;
	}
	
	pr_info("HOSC = %lu Hz.\n", ctx->hosc);
	pr_info("LOSC = %lu Hz.\n", ctx->losc);
	pr_info("DEVPLL = %lu Hz.\n", ctx->dev_pll);
	pr_info("DEVCLK = %lu Hz.\n", ctx->dev_clk);
	pr_info("NANDPLL = %lu Hz.\n", ctx->nand_pll);
}

void __init caninos_clk_driver_init(struct device_node *np)
{
	struct caninos_clk_provider *ctx;
	void __iomem *base1;
	void __iomem *base2;
	
	pr_info("Caninos CMU driver loaded.\n");
	
	base1 = of_iomap(np, 0);
	
	if (!base1) {
		return;
	}
	
	base2 = of_iomap(np, 1);
	
	if (!base2) {
		return;
	}
	
	ctx = caninos_clk_init(np, base1, base2, CLK_NR_CLKS);
	
	if (!ctx) {
		panic("%s: unable to allocate context.\n", __func__);
	}
	
	caninos_clk_get_common_rates(ctx);
	
	caninos_clk_register(ctx, caninos_clk_tree, ARRAY_SIZE(caninos_clk_tree));
}

CLK_OF_DECLARE(caninos_clk_driver, "caninos,k7-cmu", caninos_clk_driver_init);

