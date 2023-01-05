// SPDX-License-Identifier: GPL-2.0
/*
 * Clock Controller Driver for Caninos Labrador
 *
 * Copyright (c) 2022-2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2018-2020 LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2014 Actions Semi Inc.
 * Author: David Liu <liuwei@actions-semi.com>
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

#define CMU_COREPLL          (0x0000)
#define CMU_DEVPLL           (0x0004)
#define CMU_DDRPLL           (0x0008)
#define CMU_NANDPLL          (0x000C)
#define CMU_DISPLAYPLL       (0x0010)
#define CMU_AUDIOPLL         (0x0014)
#define CMU_TVOUTPLL         (0x0018)
#define CMU_BUSCLK           (0x001C)
#define CMU_SENSORCLK        (0x0020)
#define CMU_LCDCLK           (0x0024)
#define CMU_DSIPLLCLK        (0x0028)
#define CMU_CSICLK           (0x002C)
#define CMU_DECLK            (0x0030)
#define CMU_SICLK            (0x0034)
#define CMU_BUSCLK1          (0x0038)
#define CMU_HDECLK           (0x003C)
#define CMU_VDECLK           (0x0040)
#define CMU_VCECLK           (0x0044)
#define CMU_NANDCCLK         (0x004C)
#define CMU_SD0CLK           (0x0050)
#define CMU_SD1CLK           (0x0054)
#define CMU_SD2CLK           (0x0058)
#define CMU_UART0CLK         (0x005C)
#define CMU_UART1CLK         (0x0060)
#define CMU_UART2CLK         (0x0064)
#define CMU_UART3CLK         (0x0068)
#define CMU_UART4CLK         (0x006C)
#define CMU_UART5CLK         (0x0070)
#define CMU_UART6CLK         (0x0074)
#define CMU_PWM0CLK          (0x0078)
#define CMU_PWM1CLK          (0x007C)
#define CMU_PWM2CLK          (0x0080)
#define CMU_PWM3CLK          (0x0084)
#define CMU_PWM4CLK          (0x0088)
#define CMU_PWM5CLK          (0x008C)
#define CMU_GPU3DCLK         (0x0090)
#define CMU_CORECTL          (0x009C)
#define CMU_DEVCLKEN0        (0x00A0)
#define CMU_DEVCLKEN1        (0x00A4)
#define CMU_DEVRST0          (0x00A8)
#define CMU_DEVRST1          (0x00AC)
#define CMU_USBPLL           (0x00B0)
#define CMU_ETHERNETPLL      (0x00B4)
#define CMU_CVBSPLL          (0x00B8)
#define CMU_SSTSCLK          (0x00C0)
#define CMU_TVOUTPLL_DEBUG0  (0x00F0)
#define CMU_TVOUTPLL_DEBUG1  (0x00F4)

static const char * const cpu_clk_mux_p[] __initdata = {
    "losc", "hosc", "core_pll", "noc1_clk_div"
};

static const char * const dev_clk_p[] __initdata = {
    "hosc", "dev_pll"
};

static const char * const noc_clk_mux_p[] __initdata = {
    "dev_clk", "display_pll", "nand_pll", "ddr_pll", "cvbs_pll"
};

static const char * const csi_clk_mux_p[] __initdata = { 
	"display_pll", "dev_clk"
};

static const char * const de_clk_mux_p[] __initdata = { 
	"display_pll", "dev_clk"
};

static const char * const codec_clk_mux_p[] __initdata = {
    "dev_clk", "display_pll", "nand_pll", "ddr_pll"
};

static const char * const nand_clk_mux_p[] __initdata = {
    "nand_pll", "display_pll", "dev_clk", "ddr_pll"
};

static const char * const sd_clk_mux_p[] __initdata = { 
	"dev_clk", "nand_pll"
};

static const char * const uart_clk_mux_p[] __initdata = {
	"hosc", "dev_pll"
};

static const char * const pwm_clk_mux_p[] __initdata = {
	"losc", "hosc"
};

static const char * const gpu_clk_mux_p[] __initdata = {
    "dev_clk", "display_pll", "nand_pll", "ddr_pll", "cvbs_pll"
};

static const char * const lcd_clk_mux_p[] __initdata = {
	"display_pll", "dev_clk"
};

static const char * const sensor_clk_mux_p[] __initdata = {
	"hosc", "si"
};

static const char * const audio_clk_mux_p[] __initdata = {
	"audio_pll"
};

static const char * const eth_clk_mux_p[] __initdata = {
	"ethernet_pll"
};

static const char * const sst_clk_mux_p[] __initdata = {
	"hosc" 
};

static const struct clk_pll_table clk_audio_pll_table[] __initdata = {
	{0, 45158400}, {1, 49152000},
	{0, 0},
};

static const struct clk_pll_table clk_cvbs_pll_table[] __initdata = {
	{27, 29 * 12000000}, {28, 30 * 12000000}, {29, 31 * 12000000},
	{30, 32 * 12000000}, {31, 33 * 12000000}, {32, 34 * 12000000},
	{33, 35 * 12000000}, {34, 36 * 12000000}, {35, 37 * 12000000},
	{36, 38 * 12000000}, {37, 39 * 12000000}, {38, 40 * 12000000},
	{39, 41 * 12000000}, {40, 42 * 12000000}, {41, 43 * 12000000},
	{42, 44 * 12000000}, {43, 45 * 12000000}, 
	{0, 0},
};

static const struct clk_div_table sd_div_table[] __initdata = {
	/* bit0 ~ 4 */
	{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}, {7, 8},
	{8, 9}, {9, 10}, {10, 11}, {11, 12}, {12, 13}, {13, 14}, {14, 15}, 
	{15, 16}, {16, 17}, {17, 18}, {18, 19}, {19, 20}, {20, 21}, {21, 22}, 
	{22, 23}, {23, 24}, {24, 25},
	/* bit8: /128 */
	{256, 1 * 128}, {257, 2 * 128}, {258, 3 * 128}, {259, 4 * 128},
	{260, 5 * 128}, {261, 6 * 128}, {262, 7 * 128}, {263, 8 * 128},
	{264, 9 * 128}, {265, 10 * 128}, {266, 11 * 128}, {267, 12 * 128},
	{268, 13 * 128}, {269, 14 * 128}, {270, 15 * 128}, {271, 16 * 128},
	{272, 17 * 128}, {273, 18 * 128}, {274, 19 * 128}, {275, 20 * 128},
	{276, 21 * 128}, {277, 22 * 128}, {278, 23 * 128}, {279, 24 * 128},
	{280, 25 * 128},
	{0, 0},
};

static const struct clk_div_table uart_div_table[] __initdata = {
	/* bit0 ~ 8 */
	{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}, {7, 8},
	{8, 9}, {9, 10}, {10, 11}, {11, 12}, {12, 13}, {13, 14}, {14, 15}, 
	{15, 16}, {16, 17}, {17, 18}, {18, 19}, {19, 20}, {20, 21}, {21, 22},
	{22, 23}, {23, 24}, {24, 25}, {25, 26}, {26, 27}, {27, 28}, {28, 29}, 
	{29, 30}, {30, 31}, {31, 32}, {32, 33}, {33, 34}, {34, 35}, {35, 36}, 
	{36, 37}, {37, 38}, {38, 39}, {39, 40}, {40, 41}, {41, 42}, {42, 43}, 
	{43, 44}, {44, 45}, {45, 46}, {46, 47}, {47, 48}, {48, 49}, {49, 50}, 
	{50, 51}, {51, 52}, {52, 53}, {53, 54}, {54, 55}, {55, 56}, {56, 57}, 
	{57, 58}, {58, 59}, {59, 60}, {60, 61}, {61, 62}, {62, 63}, {63, 64}, 
	{64, 65}, {65, 66}, {66, 67}, {67, 68}, {68, 69}, {69, 70}, {70, 71}, 
	{71, 72}, {72, 73}, {73, 74}, {74, 75}, {75, 76}, {76, 77}, {77, 78}, 
	{78, 79}, {79, 80}, {80, 81}, {81, 82}, {82, 83}, {83, 84}, {84, 85}, 
	{85, 86}, {86, 87}, {87, 88}, {88, 89}, {89, 90}, {90, 91}, {91, 92}, 
	{92, 93}, {93, 94}, {94, 95}, {95, 96}, {96, 97}, {97, 98}, {98, 99}, 
	{99, 100}, {100, 101}, {101, 102}, {102, 103}, {103, 104}, {104, 105}, 
	{105, 106}, {106, 107}, {107, 108}, {108, 109}, {109, 110}, {110, 111}, 
	{111, 112}, {112, 113}, {113, 114}, {114, 115}, {115, 116}, {116, 117}, 
	{117, 118}, {118, 119}, {119, 120}, {120, 121}, {121, 122}, {122, 123}, 
	{123, 124}, {124, 125}, {125, 126}, {126, 127}, {127, 128}, {128, 129}, 
	{129, 130}, {130, 131}, {131, 132}, {132, 133}, {133, 134}, {134, 135}, 
	{135, 136}, {136, 137}, {137, 138}, {138, 139}, {139, 140}, {140, 141}, 
	{141, 142}, {142, 143}, {143, 144}, {144, 145}, {145, 146}, {146, 147}, 
	{147, 148}, {148, 149}, {149, 150}, {150, 151}, {151, 152}, {152, 153}, 
	{153, 154}, {154, 155}, {155, 156}, {156, 157}, {157, 158}, {158, 159}, 
	{159, 160}, {160, 161}, {161, 162}, {162, 163}, {163, 164}, {164, 165}, 
	{165, 166}, {166, 167}, {167, 168}, {168, 169}, {169, 170}, {170, 171}, 
	{171, 172}, {172, 173}, {173, 174}, {174, 175}, {175, 176}, {176, 177}, 
	{177, 178}, {178, 179}, {179, 180}, {180, 181}, {181, 182}, {182, 183}, 
	{183, 184}, {184, 185}, {185, 186}, {186, 187}, {187, 188}, {188, 189}, 
	{189, 190}, {190, 191}, {191, 192}, {192, 193}, {193, 194}, {194, 195}, 
	{195, 196}, {196, 197}, {197, 198}, {198, 199}, {199, 200}, {200, 201}, 
	{201, 202}, {202, 203}, {203, 204}, {204, 205}, {205, 206}, {206, 207}, 
	{207, 208}, {208, 209}, {209, 210}, {210, 211}, {211, 212}, {212, 213}, 
	{213, 214}, {214, 215}, {215, 216}, {216, 217}, {217, 218}, {218, 219}, 
	{219, 220}, {220, 221}, {221, 222}, {222, 223}, {223, 224}, {224, 225}, 
	{225, 226}, {226, 227}, {227, 228}, {228, 229}, {229, 230}, {230, 231}, 
	{231, 232}, {232, 233}, {233, 234}, {234, 235}, {235, 236}, {236, 237}, 
	{237, 238}, {238, 239}, {239, 240}, {240, 241}, {241, 242}, {242, 243}, 
	{243, 244}, {244, 245}, {245, 246}, {246, 247}, {247, 248}, {248, 249}, 
	{249, 250}, {250, 251}, {251, 252}, {252, 253}, {253, 254}, {254, 255}, 
	{255, 256}, {256, 257}, {257, 258}, {258, 259}, {259, 260}, {260, 261}, 
	{261, 262}, {262, 263}, {263, 264}, {264, 265}, {265, 266}, {266, 267}, 
	{267, 268}, {268, 269}, {269, 270}, {270, 271}, {271, 272}, {272, 273}, 
	{273, 274}, {274, 275}, {275, 276}, {276, 277}, {277, 278}, {278, 279}, 
	{279, 280}, {280, 281}, {281, 282}, {282, 283}, {283, 284}, {284, 285}, 
	{285, 286}, {286, 287}, {287, 288}, {288, 289}, {289, 290}, {290, 291}, 
	{291, 292}, {292, 293}, {293, 294}, {294, 295}, {295, 296}, {296, 297}, 
	{297, 298}, {298, 299}, {299, 300}, {300, 301}, {301, 302}, {302, 303}, 
	{303, 304}, {304, 305}, {305, 306}, {306, 307}, {307, 308}, {308, 309}, 
	{309, 310}, {310, 311}, {311, 312}, {312, 624}, /* last one is special */
	{0, 0},
};

static const struct clk_div_table lcd_div_table[] __initdata = {
	/* bit0 ~ 3 */
	{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}, {7, 8},
	{8, 9}, {9, 10}, {10, 11}, {11, 12},
	/* bit8: /7 */
	{256, 1 * 7}, {257, 2 * 7}, {258, 3 * 7}, {259, 4 * 7}, {260, 5 * 7},
	{261, 6 * 7}, {262, 7 * 7}, {263, 8 * 7}, {264, 9 * 7}, {265, 10 * 7}, 
	{266, 11 * 7}, {267, 12 * 7},
	{0, 0},
};

static const struct clk_div_table hdmia_div_table[] __initdata = {
	{0, 1}, {1, 2}, {3, 4}, {5, 8},
	{0, 0},
};

static const struct clk_div_table spdif_div_table[] __initdata = {
	{1, 2}, {3, 4}, {5, 8},
	{0, 0},
};

static const struct clk_div_table i2s_div_table[] __initdata = {
	{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 6}, {5, 8}, {6, 12}, {7, 16}, {8, 24},
	{0, 0},
};

static const struct clk_div_table rmii_div_table[] __initdata = {
	{0, 4}, {1, 10},
	{0, 0},
};

static const struct clk_div_table csi_si_div_table[] __initdata = {
	{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}, {7, 8}, {8, 9},
	{9, 10}, {10, 11}, {11, 12}, 
	{0, 0}
};

static const struct clk_factor_table de_factor_table[] __initdata = {
	{0, 1, 1}, {1, 2, 3}, {2, 1, 2}, {3, 2, 5}, {4, 1, 3},
	{5, 1, 4}, {6, 1, 6}, {7, 1, 8}, {8, 1, 12},
	{0, 0, 0},
};

static const struct clk_factor_table codec_gpu_factor_table[] __initdata = {
	{0, 1, 1}, {1, 2, 3}, {2, 1, 2}, {3, 2, 5}, {4, 1, 3}, 
	{5, 1, 4}, {6, 1, 6}, {7, 1, 8},
	{0, 0, 0},
};

/* fixed rate clocks */
static const struct caninos_fixed_clock k7_fixed_clks[] __initdata = {

	CANINOS_FIXED_RATE(CLK_LOSC, "losc", NULL, 32768, CLK_IS_CRITICAL),
	
	CANINOS_FIXED_RATE(CLK_HOSC, "hosc", NULL, 24000000, CLK_IS_CRITICAL),
};

/* mux clocks */
static const struct caninos_mux_clock k7_mux_clks[] __initdata = {

    CANINOS_MUX(CLK_CPU, "cpu_clk", cpu_clk_mux_p,
                CMU_BUSCLK, 0, 2, CLK_MUX_READ_ONLY,
                CLK_IS_CRITICAL),
    
    CANINOS_MUX(CLK_DEV, "dev_clk", dev_clk_p,
                CMU_DEVPLL, 12, 1, CLK_MUX_READ_ONLY,
                CLK_IS_CRITICAL),
    
    CANINOS_MUX(CLK_NOC0_CLK_MUX, "noc0_clk_mux", noc_clk_mux_p,
                CMU_BUSCLK, 4, 3, CLK_MUX_READ_ONLY,
                CLK_IS_CRITICAL),
    
    CANINOS_MUX(CLK_NOC1_CLK_MUX, "noc1_clk_mux", noc_clk_mux_p,
                CMU_BUSCLK1, 4, 3, CLK_MUX_READ_ONLY,
                CLK_IS_CRITICAL),
    
    CANINOS_MUX(CLK_HP_CLK_MUX, "hp_clk_mux", noc_clk_mux_p,
                CMU_BUSCLK1, 8, 3, CLK_MUX_READ_ONLY,
                CLK_IS_CRITICAL),
};

/* divider clocks */
static const struct caninos_div_clock k7_div_clks[] __initdata = {
        
    CANINOS_DIV(CLK_NOC0, "noc0_clk", "noc0_clk_mux",
                CMU_BUSCLK, 16, 2, CLK_DIVIDER_READ_ONLY, NULL,
                CLK_IS_CRITICAL),
                
    CANINOS_DIV(CLK_NOC1, "noc1_clk", "noc1_clk_mux",
                CMU_BUSCLK1, 16, 2, CLK_DIVIDER_READ_ONLY, NULL,
                CLK_IS_CRITICAL),
                
    CANINOS_DIV(CLK_NOC1_CLK_DIV, "noc1_clk_div", "noc1_clk",
                CMU_BUSCLK1, 20, 1, CLK_DIVIDER_READ_ONLY, NULL,
                CLK_IS_CRITICAL),
                
    CANINOS_DIV(CLK_HP_CLK_DIV, "hp_clk_div", "hp_clk_mux",
                CMU_BUSCLK1, 12, 2, CLK_DIVIDER_READ_ONLY, NULL,
                CLK_IS_CRITICAL),
                
    CANINOS_DIV(CLK_AHB, "ahb_clk", "hp_clk_div",
                CMU_BUSCLK1, 2, 2, CLK_DIVIDER_READ_ONLY, NULL,
                CLK_IS_CRITICAL),
                
    CANINOS_DIV(CLK_APB, "apb_clk", "ahb_clk",
                CMU_BUSCLK1, 14, 2, CLK_DIVIDER_READ_ONLY, NULL,
                CLK_IS_CRITICAL),
                
    CANINOS_DIV(CLK_SENSOR0, "sensor0", "sensor_src",
                CMU_SENSORCLK, 0, 4, 0, NULL,
                CLK_IGNORE_UNUSED),
                
    CANINOS_DIV(CLK_SENSOR1, "sensor1", "sensor_src",
                CMU_SENSORCLK, 8, 4, 0, NULL,
                CLK_IGNORE_UNUSED),
                
    CANINOS_DIV(CLK_RMII_REF, "rmii_ref", "ethernet_pll",
                CMU_ETHERNETPLL, 2, 1, 0, rmii_div_table,
                CLK_IGNORE_UNUSED),
};

/* gate clocks */
static const struct caninos_gate_clock k7_gate_clks[] __initdata = {

    CANINOS_GATE(CLK_GPIO, "gpio", "apb_clk",
                 CMU_DEVCLKEN1, 25, 0, CLK_IS_CRITICAL),
    
    CANINOS_GATE(CLK_TIMER, "timer", "hosc",
                 CMU_DEVCLKEN1, 27, 0, CLK_IS_CRITICAL),
    
    CANINOS_GATE(CLK_DMAC, "dmac", "hp_clk_div",
                 CMU_DEVCLKEN0, 17, 0, CLK_IS_CRITICAL),
    
    CANINOS_GATE(CLK_DSI, "dsi_clk", "hosc",
                 CMU_DEVCLKEN0, 2, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_TVOUT, "tvout_clk", "hosc",
                 CMU_DEVCLKEN0, 3, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_HDMI_DEV, "hdmi_dev", "hosc",
                 CMU_DEVCLKEN0, 5, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_USB3_480MPLL0, "usb3_480mpll0", "hosc",
                 CMU_USBPLL, 3, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_USB3_480MPHY0, "usb3_480mphy0", "hosc",
                 CMU_USBPLL, 2, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_USB3_5GPHY, "usb3_5gphy", "hosc",
                 CMU_USBPLL, 1, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_USB3_CCE, "usb3_cce", "hosc",
                 CMU_DEVCLKEN0, 25, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_I2C0, "i2c0", "hosc",
                 CMU_DEVCLKEN1, 0, 0, CLK_IS_CRITICAL), /* PMIC */
    
    CANINOS_GATE(CLK_I2C1, "i2c1", "hosc",
                 CMU_DEVCLKEN1, 1, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_I2C2, "i2c2", "hosc",
                 CMU_DEVCLKEN1, 2, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_I2C3, "i2c3", "hosc",
                 CMU_DEVCLKEN1, 3, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_SPI0, "spi0", "ahb_clk",
                 CMU_DEVCLKEN1, 4, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_SPI1, "spi1", "ahb_clk",
                 CMU_DEVCLKEN1, 5, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_SPI2, "spi2", "ahb_clk",
                 CMU_DEVCLKEN1, 6, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_SPI3, "spi3", "ahb_clk",
                 CMU_DEVCLKEN1, 7, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_USB2H0_PLLEN, "usbh0_pllen", "hosc",
                 CMU_USBPLL, 12, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_USB2H0_PHY, "usbh0_phy", "hosc",
                 CMU_USBPLL, 10, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_USB2H0_CCE, "usbh0_cce", "hosc",
                 CMU_DEVCLKEN0, 26, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_USB2H1_PLLEN, "usbh1_pllen", "hosc",
                 CMU_USBPLL, 13, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_USB2H1_PHY, "usbh1_phy", "hosc",
                 CMU_USBPLL, 11, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_USB2H1_CCE, "usbh1_cce", "hosc",
                 CMU_DEVCLKEN0, 27, 0, CLK_IGNORE_UNUSED),
    
    CANINOS_GATE(CLK_IRC_SWITCH, "irc_switch", "hosc",
                 CMU_DEVCLKEN1, 15, 0, CLK_IGNORE_UNUSED),
};

/* pll clocks */
static const struct caninos_pll_clock k7_pll_clks[] __initdata = {

    CANINOS_PLL(CLK_CORE_PLL, "core_pll", "hosc",
                CMU_COREPLL, 12000000, 9, 0, 8, 4, 174,
                CANINOS_CLK_PLL_READ_ONLY, NULL,
                CLK_IS_CRITICAL),
                
    CANINOS_PLL(CLK_DEV_PLL, "dev_pll", "hosc",
                CMU_DEVPLL, 6000000, 8, 0, 8, 8, 126,
                CANINOS_CLK_PLL_READ_ONLY, NULL,
                CLK_IS_CRITICAL),
                
    CANINOS_PLL(CLK_DDR_PLL, "ddr_pll", "hosc",
                CMU_DDRPLL, 6000000, 8, 0, 8, 2, 180,
                CANINOS_CLK_PLL_READ_ONLY, NULL,
                CLK_IS_CRITICAL),
                
    CANINOS_PLL(CLK_NAND_PLL, "nand_pll", "hosc",
                CMU_NANDPLL, 6000000, 8, 0, 8, 2, 86,
                CANINOS_CLK_PLL_READ_ONLY, NULL,
                CLK_IS_CRITICAL),
    
    CANINOS_PLL(CLK_DISPLAY_PLL, "display_pll", "hosc",
                CMU_DISPLAYPLL, 6000000, 8, 0, 8, 2, 140,
                0, NULL,
                CLK_IS_CRITICAL),
                
    CANINOS_PLL(CLK_CVBS_PLL, "cvbs_pll", "hosc",
                CMU_CVBSPLL, 0, 8, 0, 8, 27, 43, 
                0, clk_cvbs_pll_table,
                CLK_IS_CRITICAL),
                
    CANINOS_PLL(CLK_AUDIO_PLL, "audio_pll", "hosc",
                CMU_AUDIOPLL, 0, 4, 0, 1, 0, 0, 
                0, clk_audio_pll_table,
                CLK_IS_CRITICAL),
                
    CANINOS_PLL(CLK_ETHERNET_PLL, "ethernet_pll", "hosc",
                CMU_ETHERNETPLL, 500000000, 0, 0, 0, 0, 0,
                0, NULL,
                CLK_IS_CRITICAL),
};

static const struct caninos_composite_clock k7_comp_clks[] __initdata = {
	
	CANINOS_COMPOSITE(CLK_CSI, "csi", csi_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_CSICLK, 4, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN0, 13),
		CANINOS_COMP_DIVIDER(CMU_CSICLK, 0, 4, csi_si_div_table)),
	
	CANINOS_COMPOSITE(CLK_SI, "si", csi_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_SICLK, 4, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN0, 14),
		CANINOS_COMP_DIVIDER(CMU_SICLK, 0, 4, csi_si_div_table)),
	
	CANINOS_COMPOSITE(CLK_DE, "de", de_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_DECLK, 12, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN0, 0),
		CANINOS_COMP_FACTOR(CMU_DECLK, 0, 4, de_factor_table)),
	
	CANINOS_COMPOSITE(CLK_HDE, "hde", codec_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_HDECLK, 4, 2),
		CANINOS_COMP_GATE(CMU_DEVCLKEN0, 9),
		CANINOS_COMP_FACTOR(CMU_HDECLK, 0, 3, codec_gpu_factor_table)),

	CANINOS_COMPOSITE(CLK_VDE, "vde", codec_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_VDECLK, 4, 2),
		CANINOS_COMP_GATE(CMU_DEVCLKEN0, 10),
		CANINOS_COMP_FACTOR(CMU_VDECLK, 0, 3, codec_gpu_factor_table)),

	CANINOS_COMPOSITE(CLK_VCE, "vce", codec_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_VCECLK, 4, 2),
		CANINOS_COMP_GATE(CMU_DEVCLKEN0, 11),
		CANINOS_COMP_FACTOR(CMU_VCECLK, 0, 3, codec_gpu_factor_table)),

	CANINOS_COMPOSITE(CLK_GPU3D, "gpu3d", gpu_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_GPU3DCLK, 4, 3),
		CANINOS_COMP_GATE(CMU_DEVCLKEN0, 8),
		CANINOS_COMP_FACTOR(CMU_GPU3DCLK, 0, 3, codec_gpu_factor_table)),
	
	CANINOS_COMPOSITE(CLK_LCD, "lcd", lcd_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_LCDCLK, 12, 2),
		CANINOS_COMP_GATE(CMU_DEVCLKEN0, 1),
		CANINOS_COMP_DIVIDER(CMU_LCDCLK, 0, 9, lcd_div_table)),
	
	CANINOS_COMPOSITE(CLK_NAND, "nand", nand_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_NANDCCLK, 8, 2),
		CANINOS_COMP_GATE(CMU_DEVCLKEN0, 21),
		CANINOS_COMP_DIVIDER(CMU_NANDCCLK, 0, 3, NULL)),
	
	CANINOS_COMPOSITE(CLK_SD0, "sd0", sd_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_SD0CLK, 9, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN0, 22),
		CANINOS_COMP_DIVIDER(CMU_SD0CLK, 0, 9, sd_div_table)),
	
	CANINOS_COMPOSITE(CLK_SD1, "sd1", sd_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_SD1CLK, 9, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN0, 23),
		CANINOS_COMP_DIVIDER(CMU_SD1CLK, 0, 9, sd_div_table)),
	
	CANINOS_COMPOSITE(CLK_SD2, "sd2", sd_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_SD2CLK, 9, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN0, 24),
		CANINOS_COMP_DIVIDER(CMU_SD2CLK, 0, 9, sd_div_table)),
	
	CANINOS_COMPOSITE(CLK_UART0, "uart0", uart_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_UART0CLK, 16, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 8),
		CANINOS_COMP_DIVIDER(CMU_UART0CLK, 0, 9, uart_div_table)),
	
	CANINOS_COMPOSITE(CLK_UART1, "uart1", uart_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_UART1CLK, 16, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 9),
		CANINOS_COMP_DIVIDER(CMU_UART1CLK, 0, 9, uart_div_table)),
	
	CANINOS_COMPOSITE(CLK_UART2, "uart2", uart_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_UART2CLK, 16, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 10),
		CANINOS_COMP_DIVIDER(CMU_UART2CLK, 0, 9, uart_div_table)),
	
	CANINOS_COMPOSITE(CLK_UART3, "uart3", uart_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_UART3CLK, 16, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 11),
		CANINOS_COMP_DIVIDER(CMU_UART3CLK, 0, 9, uart_div_table)),
	
	CANINOS_COMPOSITE(CLK_UART4, "uart4", uart_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_UART4CLK, 16, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 12),
		CANINOS_COMP_DIVIDER(CMU_UART4CLK, 0, 9, uart_div_table)),
	
	CANINOS_COMPOSITE(CLK_UART5, "uart5", uart_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_UART5CLK, 16, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 13),
		CANINOS_COMP_DIVIDER(CMU_UART5CLK, 0, 9, uart_div_table)),
	
	CANINOS_COMPOSITE(CLK_UART6, "uart6", uart_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_UART6CLK, 16, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 14),
		CANINOS_COMP_DIVIDER(CMU_UART6CLK, 0, 9, uart_div_table)),
	
	CANINOS_COMPOSITE(CLK_PWM0, "pwm0", pwm_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_PWM0CLK, 12, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 16),
		CANINOS_COMP_DIVIDER(CMU_PWM0CLK, 0, 10, NULL)),
	
	CANINOS_COMPOSITE(CLK_PWM1, "pwm1", pwm_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_PWM1CLK, 12, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 17),
		CANINOS_COMP_DIVIDER(CMU_PWM1CLK, 0, 10, NULL)),
	
	CANINOS_COMPOSITE(CLK_PWM2, "pwm2", pwm_clk_mux_p,
		CLK_IS_CRITICAL, /* used for VDD_CORE */
		CANINOS_COMP_MUX(CMU_PWM2CLK, 12, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 18),
		CANINOS_COMP_DIVIDER(CMU_PWM2CLK, 0, 10, NULL)),
	
	CANINOS_COMPOSITE(CLK_PWM3, "pwm3", pwm_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_PWM3CLK, 12, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 19),
		CANINOS_COMP_DIVIDER(CMU_PWM3CLK, 0, 10, NULL)),
	
	CANINOS_COMPOSITE(CLK_PWM4, "pwm4", pwm_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_PWM4CLK, 12, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 20),
		CANINOS_COMP_DIVIDER(CMU_PWM4CLK, 0, 10, NULL)),
	
	CANINOS_COMPOSITE(CLK_PWM5, "pwm5", pwm_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_PWM5CLK, 12, 1),
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 21),
		CANINOS_COMP_DIVIDER(CMU_PWM5CLK, 0, 10, NULL)),
	
	CANINOS_COMPOSITE(CLK_PCM1, "pcm1", audio_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_NO_MUX,
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 31),
		CANINOS_COMP_FIXED_FACTOR(1, 2)),
	
	CANINOS_COMPOSITE(CLK_PCM0, "pcm0", audio_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_NO_MUX,
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 30),
		CANINOS_COMP_FIXED_FACTOR(1, 2)),
	
	CANINOS_COMPOSITE(CLK_SPDIF, "spdif", audio_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_NO_MUX,
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 29),
		CANINOS_COMP_DIVIDER(CMU_AUDIOPLL, 28, 4, spdif_div_table)),
	
	CANINOS_COMPOSITE(CLK_HDMI_AUDIO, "hdmia", audio_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_NO_MUX,
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 28),
		CANINOS_COMP_DIVIDER(CMU_AUDIOPLL, 24, 4, hdmia_div_table)),
	
	CANINOS_COMPOSITE(CLK_I2SRX, "i2srx", audio_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_NO_MUX,
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 27),
		CANINOS_COMP_DIVIDER(CMU_AUDIOPLL, 20, 4, i2s_div_table)),
	
	CANINOS_COMPOSITE(CLK_I2STX, "i2stx", audio_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_NO_MUX,
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 26),
		CANINOS_COMP_DIVIDER(CMU_AUDIOPLL, 16, 4, i2s_div_table)),
	
	CANINOS_COMPOSITE(CLK_SENSOR_SRC, "sensor_src", sensor_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_MUX(CMU_SENSORCLK, 4, 1),
		CANINOS_COMP_NO_GATE,
		CANINOS_COMP_DIVIDER(CMU_SENSORCLK, 5, 2, NULL)),
	
	CANINOS_COMPOSITE(CLK_ETHERNET, "ethernet", eth_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_NO_MUX,
		CANINOS_COMP_GATE(CMU_DEVCLKEN1, 23),
		CANINOS_COMP_FIXED_FACTOR(1, 20)),
	
	CANINOS_COMPOSITE(CLK_THERMAL_SENSOR, "thermal_sensor", sst_clk_mux_p,
		CLK_IGNORE_UNUSED,
		CANINOS_COMP_NO_MUX,
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

