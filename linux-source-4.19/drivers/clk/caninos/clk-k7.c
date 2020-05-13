// SPDX-License-Identifier: GPL-2.0
/*
 * Clock Controller Driver for Caninos Labrador
 * Copyright (c) 2018-2020 LSI-TEC - Caninos Loucos
 * Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2014 David Liu, Actions Semi Inc <liuwei@actions-semi.com>
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

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <dt-bindings/clock/caninos-clk.h>

#include "clk-caninos.h"

#define CMU_COREPLL     (0x0000)
#define CMU_DEVPLL      (0x0004)
#define CMU_DDRPLL      (0x0008)
#define CMU_NANDPLL     (0x000C)
#define CMU_DISPLAYPLL  (0x0010)
#define CMU_AUDIOPLL    (0x0014)
#define CMU_TVOUTPLL    (0x0018)
#define CMU_BUSCLK      (0x001C)
#define CMU_SENSORCLK   (0x0020)
#define CMU_LCDCLK      (0x0024)
#define CMU_DSIPLLCLK   (0x0028)
#define CMU_CSICLK      (0x002C)
#define CMU_DECLK       (0x0030)
#define CMU_SICLK       (0x0034)
#define CMU_BUSCLK1     (0x0038)
#define CMU_HDECLK      (0x003C)
#define CMU_VDECLK      (0x0040)
#define CMU_VCECLK      (0x0044)
#define CMU_NANDCCLK    (0x004C)
#define CMU_SD0CLK      (0x0050)
#define CMU_SD1CLK      (0x0054)
#define CMU_SD2CLK      (0x0058)
#define CMU_UART0CLK    (0x005C)
#define CMU_UART1CLK    (0x0060)
#define CMU_UART2CLK    (0x0064)
#define CMU_UART3CLK    (0x0068)
#define CMU_UART4CLK    (0x006C)
#define CMU_UART5CLK    (0x0070)
#define CMU_UART6CLK    (0x0074)
#define CMU_PWM0CLK     (0x0078)
#define CMU_PWM1CLK     (0x007C)
#define CMU_PWM2CLK     (0x0080)
#define CMU_PWM3CLK     (0x0084)
#define CMU_PWM4CLK     (0x0088)
#define CMU_PWM5CLK     (0x008C)
#define CMU_GPU3DCLK    (0x0090)
#define CMU_CORECTL     (0x009C)
#define CMU_DEVCLKEN0   (0x00A0)
#define CMU_DEVCLKEN1   (0x00A4)
#define CMU_DEVRST0     (0x00A8)
#define CMU_DEVRST1     (0x00AC)
#define CMU_USBPLL      (0x00B0)
#define CMU_ETHERNETPLL (0x00B4)
#define CMU_CVBSPLL     (0x00B8)
#define CMU_SSTSCLK     (0x00C0)

static struct clk_pll_table clk_audio_pll_table[] = {
    { 0, 45158400 }, { 1, 49152000 },
    { 0, 0 },
};

static struct clk_pll_table clk_cvbs_pll_table[] = {
	{ 27, 29*12000000 }, { 28, 30*12000000 }, { 29, 31*12000000 },
	{ 30, 32*12000000 }, { 31, 33*12000000 }, { 32, 34*12000000 },
	{ 33, 35*12000000 }, { 34, 36*12000000 }, { 35, 37*12000000 },
	{ 36, 38*12000000 }, { 37, 39*12000000 }, { 38, 40*12000000 },
	{ 39, 41*12000000 }, { 40, 42*12000000 }, { 41, 43*12000000 },
	{ 42, 44*12000000 }, { 43, 45*12000000 }, { 0, 0},
};

static const char *cpu_clk_mux_p[] __initdata = {
    "losc", "hosc", "core_pll", "noc1_clk_div"
};

static const char *dev_clk_p[] __initdata = {
    "hosc", "dev_pll"
};

static const char *noc_clk_mux_p[] __initdata = {
    "dev_clk", "display_pll", "nand_pll", "ddr_pll", "cvbs_pll"
};

static const char *csi_clk_mux_p[] __initdata = { "display_pll", "dev_clk" };

static const char *de_clk_mux_p[] __initdata = { "display_pll", "dev_clk" };

static const char *hde_clk_mux_p[] __initdata = {
    "dev_clk", "display_pll", "nand_pll", "ddr_pll"
};

static const char *nand_clk_mux_p[] __initdata = {
    "nand_pll", "display_pll", "dev_clk", "ddr_pll"
};

static const char *sd_clk_mux_p[] __initdata = { "dev_clk", "nand_pll" };

static const char *speed_sensor_clk_mux_p[] __initdata = { "hosc" };

static const char *uart_clk_mux_p[] __initdata = { "hosc", "dev_pll" };

static const char *pwm_clk_mux_p[] __initdata = { "losc", "hosc" };

static const char *gpu_clk_mux_p[] __initdata = {
    "dev_clk", "display_pll", "nand_pll", "ddr_clk", "cvbs_pll"
};

static const char *lcd_clk_mux_p[] __initdata = { "display_pll", "dev_clk" };

static const char *i2s_clk_mux_p[] __initdata = { "audio_pll" };

static const char *sensor_clk_mux_p[] __initdata = { "hosc", "si" };

static const char *ethernet_clk_mux_p[] __initdata = { "ethernet_pll" };

static const char *pcm_clk_mux_p[] __initdata = { "audio_pll" };

static struct clk_factor_table sd_factor_table[] = {
    /* bit0 ~ 4 */
    {0, 1, 1}, {1, 1, 2}, {2, 1, 3}, {3, 1, 4},
    {4, 1, 5}, {5, 1, 6}, {6, 1, 7}, {7, 1, 8},
    {8, 1, 9}, {9, 1, 10}, {10, 1, 11}, {11, 1, 12},
    {12, 1, 13}, {13, 1, 14}, {14, 1, 15}, {15, 1, 16},
    {16, 1, 17}, {17, 1, 18}, {18, 1, 19}, {19, 1, 20},
    {20, 1, 21}, {21, 1, 22}, {22, 1, 23}, {23, 1, 24},
    {24, 1, 25}, {25, 1, 26},
    
    /* bit8: /128 */
    {256, 1, 1 * 128}, {257, 1, 2 * 128}, {258, 1, 3*128}, {259, 1, 4*128},
    {260, 1, 5 * 128}, {261, 1, 6 * 128}, {262, 1, 7*128}, {263, 1, 8*128},
    {264, 1, 9 * 128}, {265, 1, 10 * 128}, {266, 1, 11*128}, {267, 1, 12*128},
    {268, 1, 13 * 128}, {269, 1, 14 * 128}, {270, 1, 15*128}, {271, 1, 16*128},
    {272, 1, 17 * 128}, {273, 1, 18 * 128}, {274, 1, 19*128}, {275, 1, 20*128},
    {276, 1, 21 * 128}, {277, 1, 22 * 128}, {278, 1, 23*128}, {279, 1, 24*128},
    {280, 1, 25 * 128}, {281, 1, 26 * 128},
    {0, 0},
};

static struct clk_factor_table lcd_factor_table[] = {
    /* bit0 ~ 3 */
    {0, 1, 1}, {1, 1, 2}, {2, 1, 3}, {3, 1, 4},
    {4, 1, 5}, {5, 1, 6}, {6, 1, 7}, {7, 1, 8},
    {8, 1, 9}, {9, 1, 10}, {10, 1, 11}, {11, 1, 12},
    
    /* bit8: /7 */
    {256, 1, 1 * 7}, {257, 1, 2 * 7}, {258, 1, 3 * 7}, {259, 1, 4 * 7},
    {260, 1, 5 * 7}, {261, 1, 6 * 7}, {262, 1, 7 * 7}, {263, 1, 8 * 7},
    {264, 1, 9 * 7}, {265, 1, 10 * 7}, {266, 1, 11 * 7}, {267, 1, 12 * 7},
    {0, 0},
};

static struct clk_div_table hdmia_div_table[] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 4},
    {4, 6}, {5, 8}, {6, 12}, {7, 16},
    {8, 24},
    {0, 0},
};

static struct clk_div_table rmii_div_table[] = {
	{0, 4},   {1, 10},
};

static struct clk_factor_table de_factor_table[] = {
    {0, 1, 1}, {1, 2, 3}, {2, 1, 2}, {3, 2, 5},
    {4, 1, 3}, {5, 1, 4}, {6, 1, 6}, {7, 1, 8},
    {8, 1, 12}, {0, 0, 0},
};

static struct clk_factor_table hde_factor_table[] = {
    {0, 1, 1}, {1, 2, 3}, {2, 1, 2}, {3, 2, 5},
    {4, 1, 3}, {5, 1, 4}, {6, 1, 6}, {7, 1, 8},
    {0, 0, 0},
};

/* fixed rate clocks */
static struct caninos_fixed_clock k7_fixed_clks[] __initdata = {

    CANINOS_FIXED_RATE(CLK_LOSC, "losc", NULL, 32768),
    
    CANINOS_FIXED_RATE(CLK_HOSC, "hosc", NULL, 24000000),
};

/* pll clocks */
static struct caninos_pll_clock k7_pll_clks[] __initdata = {

    CANINOS_PLL(CLK_CORE_PLL, "core_pll", NULL,
                CMU_COREPLL, 12000000, 9, 0, 8, 4, 174,
                CANINOS_CLK_RATE_READ_ONLY, NULL),
                
    CANINOS_PLL(CLK_DEV_PLL, "dev_pll", NULL,
                CMU_DEVPLL, 6000000, 8, 0, 8, 8, 126,
                CANINOS_CLK_RATE_READ_ONLY, NULL),
                
    CANINOS_PLL(CLK_DDR_PLL, "ddr_pll", NULL,
                CMU_DDRPLL, 6000000, 8, 0, 8, 2, 180,
                CANINOS_CLK_RATE_READ_ONLY, NULL),
                
    CANINOS_PLL(CLK_NAND_PLL, "nand_pll", NULL,
                CMU_NANDPLL, 6000000, 8, 0, 8, 2, 86,
                CANINOS_CLK_RATE_READ_ONLY, NULL),
                
    CANINOS_PLL(CLK_DISPLAY_PLL, "display_pll", NULL,
                CMU_DISPLAYPLL, 6000000, 8, 0, 8, 2, 140, 0, NULL),
                
    CANINOS_PLL(CLK_CVBS_PLL, "cvbs_pll", NULL,
                CMU_CVBSPLL, 0, 8, 0, 8, 27, 43, 0, clk_cvbs_pll_table),
                
    CANINOS_PLL(CLK_AUDIO_PLL, "audio_pll", NULL,
                CMU_AUDIOPLL, 0, 4, 0, 1, 0, 0, 0, clk_audio_pll_table),
                
    CANINOS_PLL(CLK_ETHERNET_PLL, "ethernet_pll", NULL,
                CMU_ETHERNETPLL, 500000000, 0, 0, 0, 0, 0,
                CANINOS_CLK_IS_CRITICAL, NULL),
};

/* mux clocks */
static struct caninos_mux_clock k7_mux_clks[] __initdata = {

    CANINOS_MUX(CLK_CPU, "cpu_clk", cpu_clk_mux_p,
                CMU_BUSCLK, 0, 2, CANINOS_CLK_RATE_READ_ONLY),
    
    CANINOS_MUX(CLK_DEV, "dev_clk", dev_clk_p,
                CMU_DEVPLL, 12, 1, CANINOS_CLK_RATE_READ_ONLY),
    
    CANINOS_MUX(CLK_NOC0_CLK_MUX, "noc0_clk_mux", noc_clk_mux_p,
                CMU_BUSCLK, 4, 3, CANINOS_CLK_RATE_READ_ONLY),
    
    CANINOS_MUX(CLK_NOC1_CLK_MUX, "noc1_clk_mux", noc_clk_mux_p,
                CMU_BUSCLK1, 4, 3, CANINOS_CLK_RATE_READ_ONLY),
    
    CANINOS_MUX(CLK_HP_CLK_MUX, "hp_clk_mux", noc_clk_mux_p,
                CMU_BUSCLK1, 8, 3, CANINOS_CLK_RATE_READ_ONLY),
};

/* divider clocks */
static struct caninos_div_clock k7_div_clks[] __initdata = {
        
    CANINOS_DIV(CLK_NOC0, "noc0_clk", "noc0_clk_mux",
                CMU_BUSCLK, 16, 2, CANINOS_CLK_RATE_READ_ONLY, NULL),
                
    CANINOS_DIV(CLK_NOC1, "noc1_clk", "noc1_clk_mux",
                CMU_BUSCLK1, 16, 2, CANINOS_CLK_RATE_READ_ONLY, NULL),
                
    CANINOS_DIV(CLK_NOC1_CLK_DIV, "noc1_clk_div", "noc1_clk",
                CMU_BUSCLK1, 20, 1, CANINOS_CLK_RATE_READ_ONLY, NULL),
                
    CANINOS_DIV(CLK_HP_CLK_DIV, "hp_clk_div", "hp_clk_mux",
                CMU_BUSCLK1, 12, 2, CANINOS_CLK_RATE_READ_ONLY, NULL),
                
    CANINOS_DIV(CLK_AHB, "ahb_clk", "hp_clk_div",
                CMU_BUSCLK1, 2, 2, CANINOS_CLK_RATE_READ_ONLY, NULL),
                
    CANINOS_DIV(CLK_APB, "apb_clk", "ahb_clk",
                CMU_BUSCLK1, 14, 2, CANINOS_CLK_RATE_READ_ONLY, NULL),
                
    CANINOS_DIV(CLK_SENSOR0, "sensor0", "sensor_src",
                CMU_SENSORCLK, 0, 4, 0, NULL),
                
    CANINOS_DIV(CLK_SENSOR1, "sensor1", "sensor_src",
                CMU_SENSORCLK, 8, 4, 0, NULL),
                
    CANINOS_DIV(CLK_RMII_REF, "rmii_ref", "ethernet_pll",
                CMU_ETHERNETPLL, 2, 1, 0, rmii_div_table),
};

/* gate clocks */
static struct caninos_gate_clock k7_gate_clks[] __initdata = {

    CANINOS_GATE(CLK_GPIO, "gpio", "apb_clk", CMU_DEVCLKEN1, 25, 0),
    
    CANINOS_GATE(CLK_DMAC, "dmac", "hp_clk_div", CMU_DEVCLKEN0, 17, 0),
    
    CANINOS_GATE(CLK_TIMER, "timer", "hosc", CMU_DEVCLKEN1, 27, 0),
    
    CANINOS_GATE(CLK_DSI, "dsi_clk", NULL, CMU_DEVCLKEN0, 2, 0),
    
    CANINOS_GATE(CLK_TVOUT, "tvout_clk", NULL, CMU_DEVCLKEN0, 3, 0),
    
    CANINOS_GATE(CLK_HDMI_DEV, "hdmi_dev", NULL, CMU_DEVCLKEN0, 5, 0),
    
    CANINOS_GATE(CLK_USB3_480MPLL0, "usb3_480mpll0", NULL, CMU_USBPLL, 3, 0),
    
    CANINOS_GATE(CLK_USB3_480MPHY0, "usb3_480mphy0", NULL, CMU_USBPLL, 2, 0),
    
    CANINOS_GATE(CLK_USB3_5GPHY, "usb3_5gphy", NULL, CMU_USBPLL, 1, 0),
    
    CANINOS_GATE(CLK_USB3_CCE, "usb3_cce", NULL, CMU_DEVCLKEN0, 25, 0),
    
    CANINOS_GATE(CLK_I2C0, "i2c0", "hosc", CMU_DEVCLKEN1, 0, 0),
    
    CANINOS_GATE(CLK_I2C1, "i2c1", "hosc", CMU_DEVCLKEN1, 1, 0),
    
    CANINOS_GATE(CLK_I2C2, "i2c2", "hosc", CMU_DEVCLKEN1, 2, 0),
    
    CANINOS_GATE(CLK_I2C3, "i2c3", "hosc", CMU_DEVCLKEN1, 3, 0),
    
    CANINOS_GATE(CLK_SPI0, "spi0", "ahb_clk", CMU_DEVCLKEN1, 4, 0),
    
    CANINOS_GATE(CLK_SPI1, "spi1", "ahb_clk", CMU_DEVCLKEN1, 5, 0),
    
    CANINOS_GATE(CLK_SPI2, "spi2", "ahb_clk", CMU_DEVCLKEN1, 6, 0),
    
    CANINOS_GATE(CLK_SPI3, "spi3", "ahb_clk", CMU_DEVCLKEN1, 7, 0),
    
    CANINOS_GATE(CLK_USB2H0_PLLEN, "usbh0_pllen", NULL, CMU_USBPLL, 12, 0),
    
    CANINOS_GATE(CLK_USB2H0_PHY, "usbh0_phy", NULL, CMU_USBPLL, 10, 0),
    
    CANINOS_GATE(CLK_USB2H0_CCE, "usbh0_cce", NULL, CMU_DEVCLKEN0, 26, 0),
    
    CANINOS_GATE(CLK_USB2H1_PLLEN, "usbh1_pllen", NULL, CMU_USBPLL, 13, 0),
    
    CANINOS_GATE(CLK_USB2H1_PHY, "usbh1_phy", NULL, CMU_USBPLL, 11, 0),
    
    CANINOS_GATE(CLK_USB2H1_CCE, "usbh1_cce", NULL, CMU_DEVCLKEN0, 27, 0),
    
    CANINOS_GATE(CLK_IRC_SWITCH, "irc_switch", NULL, CMU_DEVCLKEN1, 15, 0),
};

static struct caninos_composite_clock k7_comp_clks[] __initdata = {

    CANINOS_COMP_DIV_CLK(CLK_CSI, "csi",
            CANINOS_COMP_MUX(csi_clk_mux_p, CMU_CSICLK, 4, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 13),
            CANINOS_COMP_DIVIDER(CMU_CSICLK, 0, 4, NULL)),
        
    CANINOS_COMP_DIV_CLK(CLK_SI, "si",
            CANINOS_COMP_MUX(csi_clk_mux_p, CMU_SICLK, 4, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 14),
            CANINOS_COMP_DIVIDER(CMU_SICLK, 0, 4, NULL)),
        
    CANINOS_COMP_FACTOR_CLK(CLK_DE, "de",
            CANINOS_COMP_MUX(de_clk_mux_p, CMU_DECLK, 12, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 0),
            CANINOS_COMP_FACTOR(CMU_DECLK, 0, 3, de_factor_table)),
        
    CANINOS_COMP_FACTOR_CLK(CLK_HDE, "hde",
            CANINOS_COMP_MUX(hde_clk_mux_p, CMU_HDECLK, 4, 2),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 9),
            CANINOS_COMP_FACTOR(CMU_HDECLK, 0, 3, hde_factor_table)),

    CANINOS_COMP_FACTOR_CLK(CLK_VDE, "vde",
            CANINOS_COMP_MUX(hde_clk_mux_p, CMU_VDECLK, 4, 2),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 10),
            CANINOS_COMP_FACTOR(CMU_VDECLK, 0, 3, hde_factor_table)),

    CANINOS_COMP_FACTOR_CLK(CLK_VCE, "vce",
            CANINOS_COMP_MUX(hde_clk_mux_p, CMU_VCECLK, 4, 2),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 11),
            CANINOS_COMP_FACTOR(CMU_VCECLK, 0, 3, hde_factor_table)),

    CANINOS_COMP_DIV_CLK(CLK_NAND, "nand",
            CANINOS_COMP_MUX(nand_clk_mux_p, CMU_NANDCCLK, 8, 2),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 21),
            CANINOS_COMP_DIVIDER(CMU_NANDCCLK, 0, 3, NULL)),

    CANINOS_COMP_FACTOR_CLK(CLK_SD0, "sd0",
            CANINOS_COMP_MUX(sd_clk_mux_p, CMU_SD0CLK, 9, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 22),
            CANINOS_COMP_FACTOR(CMU_SD0CLK, 0, 9, sd_factor_table)),

    CANINOS_COMP_FACTOR_CLK(CLK_SD1, "sd1",
            CANINOS_COMP_MUX(sd_clk_mux_p, CMU_SD1CLK, 9, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 23),
            CANINOS_COMP_FACTOR(CMU_SD1CLK, 0, 9, sd_factor_table)),

    CANINOS_COMP_FACTOR_CLK(CLK_SD2, "sd2",
            CANINOS_COMP_MUX(sd_clk_mux_p, CMU_SD2CLK, 9, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 24),
            CANINOS_COMP_FACTOR(CMU_SD2CLK, 0, 9, sd_factor_table)),

    CANINOS_COMP_DIV_CLK(CLK_UART0, "uart0",
            CANINOS_COMP_MUX(uart_clk_mux_p, CMU_UART0CLK, 16, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 8),
            CANINOS_COMP_DIVIDER(CMU_UART0CLK, 0, 9, NULL)),

    CANINOS_COMP_DIV_CLK(CLK_UART1, "uart1",
            CANINOS_COMP_MUX(uart_clk_mux_p, CMU_UART1CLK, 16, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 9),
            CANINOS_COMP_DIVIDER(CMU_UART1CLK, 0, 9, NULL)),

    CANINOS_COMP_DIV_CLK(CLK_UART2, "uart2",
            CANINOS_COMP_MUX(uart_clk_mux_p, CMU_UART2CLK, 16, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 10),
            CANINOS_COMP_DIVIDER(CMU_UART2CLK, 0, 9, NULL)),

    CANINOS_COMP_DIV_CLK(CLK_UART3, "uart3",
            CANINOS_COMP_MUX(uart_clk_mux_p, CMU_UART3CLK, 16, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 11),
            CANINOS_COMP_DIVIDER(CMU_UART3CLK, 0, 9, NULL)),

    CANINOS_COMP_DIV_CLK(CLK_UART4, "uart4",
            CANINOS_COMP_MUX(uart_clk_mux_p, CMU_UART4CLK, 16, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 12),
            CANINOS_COMP_DIVIDER(CMU_UART4CLK, 0, 9, NULL)),

    CANINOS_COMP_DIV_CLK(CLK_UART5, "uart5",
            CANINOS_COMP_MUX(uart_clk_mux_p, CMU_UART5CLK, 16, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 13),
            CANINOS_COMP_DIVIDER(CMU_UART5CLK, 0, 9, NULL)),

    CANINOS_COMP_DIV_CLK(CLK_UART6, "uart6",
            CANINOS_COMP_MUX(uart_clk_mux_p, CMU_UART6CLK, 16, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 14),
            CANINOS_COMP_DIVIDER(CMU_UART6CLK, 0, 9, NULL)),

    CANINOS_COMP_DIV_CLK(CLK_PWM0, "pwm0",
            CANINOS_COMP_MUX(pwm_clk_mux_p, CMU_PWM0CLK, 12, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 16),
            CANINOS_COMP_DIVIDER(CMU_PWM0CLK, 0, 10, NULL)),

    CANINOS_COMP_DIV_CLK(CLK_PWM1, "pwm1",
            CANINOS_COMP_MUX(pwm_clk_mux_p, CMU_PWM1CLK, 12, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 17),
            CANINOS_COMP_DIVIDER(CMU_PWM1CLK, 0, 10, NULL)),

    CANINOS_COMP_DIV_CLK(CLK_PWM2, "pwm2",
            CANINOS_COMP_MUX(pwm_clk_mux_p, CMU_PWM2CLK, 12, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 18),
            CANINOS_COMP_DIVIDER(CMU_PWM2CLK, 0, 10, NULL)),

    CANINOS_COMP_DIV_CLK(CLK_PWM3, "pwm3",
            CANINOS_COMP_MUX(pwm_clk_mux_p, CMU_PWM3CLK, 12, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 19),
            CANINOS_COMP_DIVIDER(CMU_PWM3CLK, 0, 10, NULL)),

    CANINOS_COMP_DIV_CLK(CLK_PWM4, "pwm4",
            CANINOS_COMP_MUX(pwm_clk_mux_p, CMU_PWM4CLK, 12, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 20),
            CANINOS_COMP_DIVIDER(CMU_PWM4CLK, 0, 10, NULL)),

    CANINOS_COMP_DIV_CLK(CLK_PWM5, "pwm5",
            CANINOS_COMP_MUX(pwm_clk_mux_p, CMU_PWM5CLK, 12, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 21),
            CANINOS_COMP_DIVIDER(CMU_PWM5CLK, 0, 10, NULL)),

    CANINOS_COMP_FACTOR_CLK(CLK_GPU3D, "gpu3d",
            CANINOS_COMP_MUX(gpu_clk_mux_p, CMU_GPU3DCLK, 4, 3),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 8),
            CANINOS_COMP_FACTOR(CMU_GPU3DCLK, 0, 3, hde_factor_table)),

    CANINOS_COMP_FACTOR_CLK(CLK_LCD, "lcd",
            CANINOS_COMP_MUX(lcd_clk_mux_p, CMU_LCDCLK, 12, 2),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 1),
            CANINOS_COMP_FACTOR(CMU_LCDCLK, 0, 9, lcd_factor_table)),

    CANINOS_COMP_DIV_CLK(CLK_HDMI_AUDIO, "hdmia",
            CANINOS_COMP_MUX(i2s_clk_mux_p, CMU_AUDIOPLL, 24, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 28),
            CANINOS_COMP_DIVIDER(CMU_AUDIOPLL, 24, 4, hdmia_div_table)),

    CANINOS_COMP_DIV_CLK(CLK_I2SRX, "i2srx",
            CANINOS_COMP_MUX(i2s_clk_mux_p, CMU_AUDIOPLL, 24, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 27),
            CANINOS_COMP_DIVIDER(CMU_AUDIOPLL, 20, 4, hdmia_div_table)),

    CANINOS_COMP_DIV_CLK(CLK_I2STX, "i2stx",
            CANINOS_COMP_MUX(i2s_clk_mux_p, CMU_AUDIOPLL, 24, 1),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 26),
            CANINOS_COMP_DIVIDER(CMU_AUDIOPLL, 16, 4, hdmia_div_table)),

    CANINOS_COMP_FIXED_FACTOR_CLK(CLK_PCM1, "pcm1",
            CANINOS_COMP_MUX_FIXED(pcm_clk_mux_p),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 31),
            CANINOS_COMP_FIXED_FACTOR(1, 2)),

    CANINOS_COMP_DIV_CLK(CLK_SENOR_SRC, "sensor_src",
            CANINOS_COMP_MUX(sensor_clk_mux_p, CMU_SENSORCLK, 4, 1),
            CANINOS_COMP_NULL,
            CANINOS_COMP_DIVIDER(CMU_SENSORCLK, 5, 2, NULL)),

    CANINOS_COMP_FIXED_FACTOR_CLK(CLK_ETHERNET, "ethernet",
            CANINOS_COMP_MUX_FIXED(ethernet_clk_mux_p),
            CANINOS_COMP_GATE(CMU_DEVCLKEN1, 23),
            CANINOS_COMP_FIXED_FACTOR(1, 20)),
			    
    CANINOS_COMP_DIV_CLK(CLK_THERMAL_SENSOR, "thermal_sensor",
            CANINOS_COMP_MUX_FIXED(speed_sensor_clk_mux_p),
            CANINOS_COMP_GATE(CMU_DEVCLKEN0, 31),
            CANINOS_COMP_DIVIDER(CMU_SSTSCLK, 20, 10, NULL)),
};

void __init k7_clk_init(struct device_node *np)
{
    struct caninos_clk_provider *ctx;
    void __iomem *base = of_iomap(np, 0);
    
    if (!base)
    {
        panic("%s: unable to map iomap.\n", __func__);
        return;
    }
    
    ctx = caninos_clk_init(np, base, CLK_NR_CLKS);
    
    if (!ctx) {
        return;
    }
    
    caninos_clk_register_fixed(ctx, k7_fixed_clks, ARRAY_SIZE(k7_fixed_clks));
    
    caninos_clk_register_pll(ctx, k7_pll_clks, ARRAY_SIZE(k7_pll_clks));
    
    caninos_clk_register_div(ctx, k7_div_clks, ARRAY_SIZE(k7_div_clks));
    
    caninos_clk_register_mux(ctx, k7_mux_clks, ARRAY_SIZE(k7_mux_clks));
    
    caninos_clk_register_gate(ctx, k7_gate_clks, ARRAY_SIZE(k7_gate_clks));
    
    caninos_clk_register_composite(ctx, k7_comp_clks, ARRAY_SIZE(k7_comp_clks));
}

CLK_OF_DECLARE(k7_clk, "caninos,k7-cmu", k7_clk_init);

