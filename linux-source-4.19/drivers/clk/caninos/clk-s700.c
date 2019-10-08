#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <dt-bindings/clock/caninos-clk.h>

#include "clk.h"

#define CMU_COREPLL		(0x0000)
#define CMU_DEVPLL		(0x0004)
#define CMU_DDRPLL		(0x0008)
#define CMU_NANDPLL		(0x000C)
#define CMU_DISPLAYPLL	(0x0010)
#define CMU_AUDIOPLL	(0x0014)
#define CMU_TVOUTPLL	(0x0018)
#define CMU_BUSCLK		(0x001C)
#define CMU_SENSORCLK	(0x0020)
#define CMU_LCDCLK		(0x0024)
#define CMU_DSIPLLCLK	(0x0028)
#define CMU_CSICLK		(0x002C)
#define CMU_DECLK		(0x0030)
#define CMU_SICLK		(0x0034)
#define CMU_BUSCLK1		(0x0038)
#define CMU_HDECLK		(0x003C)
#define CMU_VDECLK		(0x0040)
#define CMU_VCECLK		(0x0044)
#define CMU_NANDCCLK	(0x004C)
#define CMU_SD0CLK		(0x0050)
#define CMU_SD1CLK		(0x0054)
#define CMU_SD2CLK		(0x0058)
#define CMU_UART0CLK	(0x005C)
#define CMU_UART1CLK	(0x0060)
#define CMU_UART2CLK	(0x0064)
#define CMU_UART3CLK	(0x0068)
#define CMU_UART4CLK	(0x006C)
#define CMU_UART5CLK	(0x0070)
#define CMU_UART6CLK	(0x0074)
#define CMU_PWM0CLK		(0x0078)
#define CMU_PWM1CLK		(0x007C)
#define CMU_PWM2CLK		(0x0080)
#define CMU_PWM3CLK		(0x0084)
#define CMU_PWM4CLK		(0x0088)
#define CMU_PWM5CLK		(0x008C)
#define CMU_GPU3DCLK	(0x0090)
#define CMU_CORECTL		(0x009C)
#define CMU_DEVCLKEN0	(0x00A0)
#define CMU_DEVCLKEN1	(0x00A4)
#define CMU_DEVRST0		(0x00A8)
#define CMU_DEVRST1		(0x00AC)
#define CMU_USBPLL		(0x00B0)
#define CMU_ETHERNETPLL	(0x00B4)
#define CMU_CVBSPLL		(0x00B8)
#define CMU_SSTSCLK		(0x00C0)

/* fixed rate clocks */
static struct owl_fixed_rate_clock s700_fixed_rate_clks[] __initdata = {
	{ CLK_LOSC, "losc", NULL, CLK_IS_CRITICAL, 32768, },
	{ CLK_HOSC, "hosc", NULL, CLK_IS_CRITICAL, 24000000, },
};

static struct clk_pll_table clk_audio_pll_table[] = {
	{0, 45158400}, {1, 49152000},
	{0, 0},
};

static struct clk_pll_table clk_cvbs_pll_table[] = {
	{27, 29*12000000}, {28, 30*12000000}, {29, 31*12000000},
	{30, 32*12000000}, {31, 33*12000000}, {32, 34*12000000},
	{33, 35*12000000}, {34, 36*12000000}, {35, 37*12000000},
	{36, 38*12000000}, {37, 39*12000000}, {38, 40*12000000},
	{39, 41*12000000}, {40, 42*12000000}, {41, 43*12000000},
	{42, 44*12000000}, {43, 45*12000000}, {0, 0},
};

/* pll clocks */
static struct owl_pll_clock s700_pll_clks[] __initdata = {
	{ CLK_CORE_PLL,   "core_pll", NULL, CLK_IGNORE_UNUSED, CMU_COREPLL, 12000000, 9, 0, 8,  4, 174, 0, NULL},
	{ CLK_DEV_PLL,    "dev_pll", NULL, CLK_IGNORE_UNUSED, CMU_DEVPLL,  6000000, 8, 0, 8, 8, 126, 0, NULL},
	{ CLK_DDR_PLL,    "ddr_pll",  NULL, CLK_IGNORE_UNUSED, CMU_DDRPLL, 6000000, 8, 0, 8,  2,  180, 0, NULL},
	{ CLK_NAND_PLL,   "nand_pll", NULL, CLK_IGNORE_UNUSED, CMU_NANDPLL,  6000000, 8, 0, 8,  2, 86, 0, NULL},
	{ CLK_DISPLAY_PLL, "display_pll", NULL, CLK_IGNORE_UNUSED, CMU_DISPLAYPLL, 6000000, 8, 0, 8, 2, 140, 0, NULL},
	{ CLK_CVBS_PLL, "cvbs_pll", NULL, CLK_IGNORE_UNUSED, CMU_CVBSPLL, 0, 8, 0, 8, 27, 43, 0, clk_cvbs_pll_table},
	{ CLK_AUDIO_PLL,  "audio_pll", NULL, CLK_IGNORE_UNUSED, CMU_AUDIOPLL, 0, 4, 0, 1, 0, 0, 0, clk_audio_pll_table},
	{ CLK_ETHERNET_PLL, "ethernet_pll", NULL, CLK_IGNORE_UNUSED, CMU_ETHERNETPLL, 500000000, 0, 0, 0, 0, 0, CLK_OWL_PLL_FIXED_FREQ, NULL},

};

static const char *cpu_clk_mux_p[] __initdata = {"losc", "hosc", "core_pll", "noc1_clk_div"};
static const char *dev_clk_p[] __initdata = { "hosc", "dev_pll"};
static const char *noc_clk_mux_p[] __initdata = { "dev_clk", "display_pll", "nand_pll", "ddr_pll", "cvbs_pll"};

static const char *csi_clk_mux_p[] __initdata = { "display_pll", "dev_clk"};
static const char *de_clk_mux_p[] __initdata = { "display_pll", "dev_clk"};
static const char *hde_clk_mux_p[] __initdata = { "dev_clk", "display_pll", "nand_pll", "ddr_pll"};
static const char *nand_clk_mux_p[] __initdata = { "nand_pll", "display_pll", "dev_clk", "ddr_pll"};
static const char *sd_clk_mux_p[] __initdata = { "dev_clk", "nand_pll", };
static const char *speed_sensor_clk_mux_p[] __initdata = { "hosc",};
static const char *uart_clk_mux_p[] __initdata = { "hosc", "dev_pll"};
static const char *pwm_clk_mux_p[] __initdata = { "losc", "hosc"};
static const char *gpu_clk_mux_p[] __initdata = { "dev_clk", "display_pll", "nand_pll", "ddr_clk", "cvbs_pll"};
static const char *lcd_clk_mux_p[] __initdata = { "display_pll", "dev_clk" };
static const char *i2s_clk_mux_p[] __initdata = { "audio_pll" };
static const char *sensor_clk_mux_p[] __initdata = { "hosc", "si"};
static const char *ethernet_clk_mux_p[] __initdata = { "ethernet_pll" };
static const char *pcm_clk_mux_p[] __initdata = { "audio_pll", };

/* mux clocks */
static struct owl_mux_clock s700_mux_clks[] __initdata = {
	{ CLK_CPU,  "cpu_clk", cpu_clk_mux_p, ARRAY_SIZE(cpu_clk_mux_p), CLK_SET_RATE_PARENT, CMU_BUSCLK, 0, 2, 0, "cpu_clk" },
	{ CLK_DEV,  "dev_clk", dev_clk_p, ARRAY_SIZE(dev_clk_p), CLK_SET_RATE_PARENT, CMU_DEVPLL, 12, 1, 0, "dev_clk" },
	{ CLK_NOC0_CLK_MUX,  "noc0_clk_mux", noc_clk_mux_p, ARRAY_SIZE(noc_clk_mux_p), CLK_SET_RATE_PARENT, CMU_BUSCLK, 4, 3, 0, },
	{ CLK_NOC1_CLK_MUX,	"noc1_clk_mux", noc_clk_mux_p, ARRAY_SIZE(noc_clk_mux_p), CLK_SET_RATE_PARENT, CMU_BUSCLK1, 4, 3, 0, },
	{ CLK_HP_CLK_MUX, "hp_clk_mux", noc_clk_mux_p, ARRAY_SIZE(noc_clk_mux_p), CLK_SET_RATE_PARENT, CMU_BUSCLK1, 8, 3, 0, },

};

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
	{256, 1, 1 * 128}, {257, 1, 2 * 128}, {258, 1, 3 * 128}, {259, 1, 4 * 128},
	{260, 1, 5 * 128}, {261, 1, 6 * 128}, {262, 1, 7 * 128}, {263, 1, 8 * 128},
	{264, 1, 9 * 128}, {265, 1, 10 * 128}, {266, 1, 11 * 128}, {267, 1, 12 * 128},
	{268, 1, 13 * 128}, {269, 1, 14 * 128}, {270, 1, 15 * 128}, {271, 1, 16 * 128},
	{272, 1, 17 * 128}, {273, 1, 18 * 128}, {274, 1, 19 * 128}, {275, 1, 20 * 128},
	{276, 1, 21 * 128}, {277, 1, 22 * 128}, {278, 1, 23 * 128}, {279, 1, 24 * 128},
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
	{0, 1},   {1, 2},   {2, 3},   {3, 4},
	{4, 6},   {5, 8},   {6, 12},  {7, 16},
	{8, 24},
	{0, 0},
};

static struct clk_div_table rmii_div_table[] = {
	{0, 4},   {1, 10},
};


/* divider clocks */

static struct owl_divider_clock s700_div_clks[] __initdata = {
	{ CLK_NOC0, "noc0_clk", "noc0_clk_mux", CLK_IGNORE_UNUSED, CMU_BUSCLK, 16, 2, 0, NULL,},
	{ CLK_NOC1, "noc1_clk", "noc1_clk_mux", CLK_IGNORE_UNUSED, CMU_BUSCLK1, 16, 2, 0, NULL,},
	{ CLK_NOC1_CLK_DIV, "noc1_clk_div", "noc1_clk", CLK_IGNORE_UNUSED, CMU_BUSCLK1, 20, 1, 0, NULL, "noc1_clk_div"},
	{ CLK_HP_CLK_DIV, "hp_clk_div", "hp_clk_mux", CLK_IGNORE_UNUSED, CMU_BUSCLK1, 12, 2, 0, NULL,},
	{ CLK_AHB, "ahb_clk", "hp_clk_div", CLK_IGNORE_UNUSED, CMU_BUSCLK1, 2, 2, 0, NULL, "ahb_clk"},
	{ CLK_APB, "apb_clk", "ahb_clk", CLK_IGNORE_UNUSED, CMU_BUSCLK1, 14, 2, 0, NULL, "apb_clk"},

	{ CLK_SENSOR0, "sensor0", "sensor_src", CLK_IGNORE_UNUSED, CMU_SENSORCLK, 0, 4, 0, NULL, "sensor0"},
	{ CLK_SENSOR1, "sensor1", "sensor_src", CLK_IGNORE_UNUSED, CMU_SENSORCLK, 8, 4, 0, NULL, "sensor1"},
	{ CLK_RMII_REF, "rmii_ref", "ethernet_pll", CLK_IGNORE_UNUSED, CMU_ETHERNETPLL, 2, 1, 0, rmii_div_table, "rmii_ref"},
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






/* gate clocks */
static struct owl_gate_clock s700_gate_clks[] __initdata = {
	{ CLK_GPIO,  "gpio", "apb_clk", CLK_IGNORE_UNUSED, CMU_DEVCLKEN1, 25, 0, "gpio"},
	{ CLK_DMAC,  "dmac", "hp_clk_div", CLK_IGNORE_UNUSED, CMU_DEVCLKEN0, 17, 0, "dmac"},
	{ CLK_TIMER,  "timer", "hosc", CLK_IGNORE_UNUSED, CMU_DEVCLKEN1, 27, 0, "timer"},
	{ CLK_DSI,  "dsi_clk", NULL, CLK_IGNORE_UNUSED, CMU_DEVCLKEN0, 2, 0, "dsi"},
	{ CLK_TVOUT,  "tvout_clk", NULL, CLK_IGNORE_UNUSED, CMU_DEVCLKEN0, 3, 0, "tvout"},
	{ CLK_HDMI_DEV,  "hdmi_dev", NULL, CLK_IGNORE_UNUSED, CMU_DEVCLKEN0, 5, 0, "hdmi_dev"},
	{ CLK_USB3_480MPLL0,	"usb3_480mpll0",	NULL, CLK_IGNORE_UNUSED, CMU_USBPLL, 3, 0, "usb3_480mpll0"},
	{ CLK_USB3_480MPHY0,	"usb3_480mphy0",	NULL, CLK_IGNORE_UNUSED, CMU_USBPLL, 2, 0, "usb3_480mphy0"},
	{ CLK_USB3_5GPHY,	"usb3_5gphy",		NULL, CLK_IGNORE_UNUSED, CMU_USBPLL, 1, 0, "usb3_5gphy"},
	{ CLK_USB3_CCE,		"usb3_cce",		NULL, CLK_IGNORE_UNUSED, CMU_DEVCLKEN0, 25, 0, "usb3_cce"},

	{ CLK_I2C0, "i2c0",	"hosc", CLK_IGNORE_UNUSED, CMU_DEVCLKEN1, 0, 0, "i2c0"},
	{ CLK_I2C1, "i2c1",	"hosc", CLK_IGNORE_UNUSED, CMU_DEVCLKEN1, 1, 0, "i2c1"},
	{ CLK_I2C2, "i2c2",	"hosc", CLK_IGNORE_UNUSED, CMU_DEVCLKEN1, 2, 0, "i2c2"},
	{ CLK_I2C3, "i2c3",	"hosc", CLK_IGNORE_UNUSED, CMU_DEVCLKEN1, 3, 0, "i2c3"},



	{ CLK_SPI0, "spi0",	"ahb_clk", CLK_IGNORE_UNUSED, CMU_DEVCLKEN1, 4, 0, "spi0"},
	{ CLK_SPI1, "spi1",	"ahb_clk", CLK_IGNORE_UNUSED, CMU_DEVCLKEN1, 5, 0, "spi1"},
	{ CLK_SPI2, "spi2",	"ahb_clk", CLK_IGNORE_UNUSED, CMU_DEVCLKEN1, 6, 0, "spi2"},
	{ CLK_SPI3, "spi3",	"ahb_clk", CLK_IGNORE_UNUSED, CMU_DEVCLKEN1, 7, 0, "spi3"},

	{ CLK_USB2H0_PLLEN,	"usbh0_pllen",	NULL, CLK_IGNORE_UNUSED, CMU_USBPLL, 12, 0, "usbh0_pllen"},
	{ CLK_USB2H0_PHY,	"usbh0_phy",	NULL, CLK_IGNORE_UNUSED, CMU_USBPLL, 10, 0, "usbh0_phy"},
	{ CLK_USB2H0_CCE,	"usbh0_cce",	NULL, CLK_IGNORE_UNUSED, CMU_DEVCLKEN0, 26, 0, "usbh0_cce"},

	{ CLK_USB2H1_PLLEN,	"usbh1_pllen",	NULL, CLK_IGNORE_UNUSED, CMU_USBPLL, 13, 0, "usbh1_pllen"},
	{ CLK_USB2H1_PHY,	"usbh1_phy",	NULL, CLK_IGNORE_UNUSED, CMU_USBPLL, 11, 0, "usbh1_phy"},
	{ CLK_USB2H1_CCE,	"usbh1_cce",	NULL, CLK_IGNORE_UNUSED, CMU_DEVCLKEN0, 27, 0, "usbh1_cce"},
	{ CLK_IRC_SWITCH,	"irc_switch",	NULL, CLK_IGNORE_UNUSED, CMU_DEVCLKEN1, 15, 0, "irc_switch"},


};

static struct owl_composite_clock s700_composite_clks[] __initdata = {

	COMP_DIV_CLK(CLK_CSI, "csi", CLK_IGNORE_UNUSED,
			C_MUX(csi_clk_mux_p, CMU_CSICLK, 4, 1,  0),
			C_GATE(CMU_DEVCLKEN0, 13, 0),
			C_DIVIDER(CMU_CSICLK, 0, 4, NULL, 0)),

	COMP_DIV_CLK(CLK_SI, "si", CLK_IGNORE_UNUSED,
			C_MUX(csi_clk_mux_p, CMU_SICLK, 4, 1, 0),
			C_GATE(CMU_DEVCLKEN0, 14,  0),
			C_DIVIDER(CMU_SICLK, 0, 4, NULL, 0)),

	COMP_FACTOR_CLK(CLK_DE, "de", CLK_IGNORE_UNUSED,
			C_MUX(de_clk_mux_p, CMU_DECLK, 12, 1,  0),
			C_GATE(CMU_DEVCLKEN0, 0,  0),
			C_FACTOR(CMU_DECLK, 0, 3, de_factor_table,  0)),

	COMP_FACTOR_CLK(CLK_HDE, "hde", CLK_IGNORE_UNUSED,
			C_MUX(hde_clk_mux_p, CMU_HDECLK, 4, 2, 0),
			C_GATE(CMU_DEVCLKEN0, 9, 0),
			C_FACTOR(CMU_HDECLK, 0, 3, hde_factor_table, 0)),

	COMP_FACTOR_CLK(CLK_VDE, "vde", CLK_IGNORE_UNUSED,
			C_MUX(hde_clk_mux_p, CMU_VDECLK, 4, 2,  0),
			C_GATE(CMU_DEVCLKEN0, 10,  0),
			C_FACTOR(CMU_VDECLK, 0, 3, hde_factor_table, 0)),

	COMP_FACTOR_CLK(CLK_VCE, "vce", CLK_IGNORE_UNUSED,
			C_MUX(hde_clk_mux_p, CMU_VCECLK, 4, 2, 0),
			C_GATE(CMU_DEVCLKEN0, 11, 0),
			C_FACTOR(CMU_VCECLK, 0, 3, hde_factor_table, 0)),

	COMP_DIV_CLK(CLK_NAND, "nand", CLK_SET_RATE_PARENT,
			C_MUX(nand_clk_mux_p, CMU_NANDCCLK, 8, 2, 0),
			C_GATE(CMU_DEVCLKEN0, 21,  0),
			C_DIVIDER(CMU_NANDCCLK, 0, 3, NULL,  0)),

	COMP_FACTOR_CLK(CLK_SD0, "sd0", CLK_IGNORE_UNUSED,
			C_MUX(sd_clk_mux_p, CMU_SD0CLK, 9, 1,  0),
			C_GATE(CMU_DEVCLKEN0, 22,  0),
			C_FACTOR(CMU_SD0CLK, 0, 9, sd_factor_table, 0)),

	COMP_FACTOR_CLK(CLK_SD1, "sd1", CLK_IGNORE_UNUSED,
			C_MUX(sd_clk_mux_p, CMU_SD1CLK, 9, 1, 0),
			C_GATE(CMU_DEVCLKEN0, 23,  0),
			C_FACTOR(CMU_SD1CLK, 0, 9, sd_factor_table,  0)),


	COMP_FACTOR_CLK(CLK_SD2, "sd2", CLK_IGNORE_UNUSED,
			C_MUX(sd_clk_mux_p, CMU_SD2CLK, 9, 1,  0),
			C_GATE(CMU_DEVCLKEN0, 24,  0),
			C_FACTOR(CMU_SD2CLK, 0, 9, sd_factor_table, 0)),

	COMP_DIV_CLK(CLK_UART0, "uart0", CLK_IGNORE_UNUSED,
			C_MUX(uart_clk_mux_p, CMU_UART0CLK, 16, 1, 0),
			C_GATE(CMU_DEVCLKEN1, 8, 0),
			C_DIVIDER(CMU_UART0CLK, 0, 9, NULL, CLK_DIVIDER_ROUND_CLOSEST)),

	COMP_DIV_CLK(CLK_UART1, "uart1", CLK_IGNORE_UNUSED,
			C_MUX(uart_clk_mux_p, CMU_UART1CLK, 16, 1, 0),
			C_GATE(CMU_DEVCLKEN1, 9, 0),
			C_DIVIDER(CMU_UART1CLK, 0, 9, NULL,  CLK_DIVIDER_ROUND_CLOSEST)),

	COMP_DIV_CLK(CLK_UART2, "uart2", CLK_IGNORE_UNUSED,
			C_MUX(uart_clk_mux_p, CMU_UART2CLK, 16, 1,  0),
			C_GATE(CMU_DEVCLKEN1, 10,  0),
			C_DIVIDER(CMU_UART2CLK, 0, 9, NULL,  CLK_DIVIDER_ROUND_CLOSEST)),

	COMP_DIV_CLK(CLK_UART3, "uart3", CLK_IGNORE_UNUSED,
			C_MUX(uart_clk_mux_p, CMU_UART3CLK, 16, 1,  0),
			C_GATE(CMU_DEVCLKEN1, 11,  0),
			C_DIVIDER(CMU_UART3CLK, 0, 9, NULL,  CLK_DIVIDER_ROUND_CLOSEST)),

	COMP_DIV_CLK(CLK_UART4, "uart4", CLK_IGNORE_UNUSED,
			C_MUX(uart_clk_mux_p, CMU_UART4CLK, 16, 1,  0),
			C_GATE(CMU_DEVCLKEN1, 12,  0),
			C_DIVIDER(CMU_UART4CLK, 0, 9, NULL,  CLK_DIVIDER_ROUND_CLOSEST)),

	COMP_DIV_CLK(CLK_UART5, "uart5", CLK_IGNORE_UNUSED,
			C_MUX(uart_clk_mux_p, CMU_UART5CLK, 16, 1,  0),
			C_GATE(CMU_DEVCLKEN1, 13,  0),
			C_DIVIDER(CMU_UART5CLK, 0, 9, NULL,  CLK_DIVIDER_ROUND_CLOSEST)),

	COMP_DIV_CLK(CLK_UART6, "uart6", CLK_IGNORE_UNUSED,
			C_MUX(uart_clk_mux_p, CMU_UART6CLK, 16, 1,  0),
			C_GATE(CMU_DEVCLKEN1, 14,  0),
			C_DIVIDER(CMU_UART6CLK, 0, 9, NULL,  CLK_DIVIDER_ROUND_CLOSEST)),

	COMP_DIV_CLK(CLK_PWM0, "pwm0", CLK_IGNORE_UNUSED,
			C_MUX(pwm_clk_mux_p, CMU_PWM0CLK, 12, 1,  0),
			C_GATE(CMU_DEVCLKEN1, 16,  0),
			C_DIVIDER(CMU_PWM0CLK, 0, 10, NULL,  0)),

	COMP_DIV_CLK(CLK_PWM1, "pwm1", CLK_IGNORE_UNUSED,
			C_MUX(pwm_clk_mux_p, CMU_PWM1CLK, 12, 1,  0),
			C_GATE(CMU_DEVCLKEN1, 17,  0),
			C_DIVIDER(CMU_PWM1CLK, 0, 10, NULL,  0)),

	COMP_DIV_CLK(CLK_PWM2, "pwm2", CLK_IGNORE_UNUSED,
			C_MUX(pwm_clk_mux_p, CMU_PWM2CLK, 12, 1, 0),
			C_GATE(CMU_DEVCLKEN1, 18, 0),
			C_DIVIDER(CMU_PWM2CLK, 0, 10, NULL, 0)),

	COMP_DIV_CLK(CLK_PWM3, "pwm3", CLK_IGNORE_UNUSED,
			C_MUX(pwm_clk_mux_p, CMU_PWM3CLK, 12, 1, 0),
			C_GATE(CMU_DEVCLKEN1, 19,  0),
			C_DIVIDER(CMU_PWM3CLK, 0, 10, NULL, 0)),

	COMP_DIV_CLK(CLK_PWM4, "pwm4", CLK_IGNORE_UNUSED,
			C_MUX(pwm_clk_mux_p, CMU_PWM4CLK, 12, 1, 0),
			C_GATE(CMU_DEVCLKEN1, 20, 0),
			C_DIVIDER(CMU_PWM4CLK, 0, 10, NULL, 0)),

	COMP_DIV_CLK(CLK_PWM5, "pwm5", CLK_IGNORE_UNUSED,
			C_MUX(pwm_clk_mux_p, CMU_PWM5CLK, 12, 1,  0),
			C_GATE(CMU_DEVCLKEN1, 21, 0),
			C_DIVIDER(CMU_PWM5CLK, 0, 10, NULL, 0)),

	COMP_FACTOR_CLK(CLK_GPU3D, "gpu3d", CLK_IGNORE_UNUSED,
			C_MUX(gpu_clk_mux_p, CMU_GPU3DCLK, 4, 3, 0),
			C_GATE(CMU_DEVCLKEN0, 8, 0),
			C_FACTOR(CMU_GPU3DCLK, 0, 3, hde_factor_table, 0)),

	COMP_FACTOR_CLK(CLK_LCD, "lcd", CLK_IGNORE_UNUSED,
			C_MUX(lcd_clk_mux_p, CMU_LCDCLK, 12, 2, 0),
			C_GATE(CMU_DEVCLKEN0, 1, 0),
			C_FACTOR(CMU_LCDCLK, 0, 9, lcd_factor_table, 0)),

	COMP_DIV_CLK(CLK_HDMI_AUDIO, "hdmia", CLK_IGNORE_UNUSED,
			C_MUX(i2s_clk_mux_p, CMU_AUDIOPLL, 24, 1, 0),/*CMU_AUDIOPLL 24,1 unused*/
			C_GATE(CMU_DEVCLKEN1, 28, 0),
			C_DIVIDER(CMU_AUDIOPLL, 24, 4, hdmia_div_table, 0)),

	COMP_DIV_CLK(CLK_I2SRX, "i2srx", CLK_IGNORE_UNUSED,
			C_MUX(i2s_clk_mux_p, CMU_AUDIOPLL, 24, 1,  0),
			C_GATE(CMU_DEVCLKEN1, 27, 0),
			C_DIVIDER(CMU_AUDIOPLL, 20, 4, hdmia_div_table, 0)),

	COMP_DIV_CLK(CLK_I2STX, "i2stx", CLK_IGNORE_UNUSED,
			C_MUX(i2s_clk_mux_p, CMU_AUDIOPLL, 24, 1, 0),
			C_GATE(CMU_DEVCLKEN1, 26,  0),
			C_DIVIDER(CMU_AUDIOPLL, 16, 4, hdmia_div_table, 0)),

	/* for bluetooth pcm communication */
	COMP_FIXED_FACTOR_CLK(CLK_PCM1, "pcm1", CLK_IGNORE_UNUSED,
			C_MUX_F(pcm_clk_mux_p, 0),
			C_GATE(CMU_DEVCLKEN1, 31, 0),
			C_FIXED_FACTOR(1, 2)),

	COMP_DIV_CLK(CLK_SENOR_SRC, "sensor_src", CLK_IGNORE_UNUSED,
			C_MUX(sensor_clk_mux_p, CMU_SENSORCLK, 4, 1, 0),
			C_NULL,
			C_DIVIDER(CMU_SENSORCLK, 5, 2, NULL, 0)),

	COMP_FIXED_FACTOR_CLK(CLK_ETHERNET, "ethernet", CLK_IGNORE_UNUSED,
			C_MUX_F(ethernet_clk_mux_p, 0),
			C_GATE(CMU_DEVCLKEN1, 23, 0),
			C_FIXED_FACTOR(1, 20)),
	COMP_DIV_CLK(CLK_THERMAL_SENSOR, "thermal_sensor", CLK_IGNORE_UNUSED,
			C_MUX_F(speed_sensor_clk_mux_p, 0),
			C_GATE(CMU_DEVCLKEN0, 31, 0),
			C_DIVIDER(CMU_SSTSCLK, 20, 10, NULL, 0)),


};


void __init s700_clk_init(struct device_node *np)
{
	struct owl_clk_provider *ctx;
	void __iomem *base;

	pr_info("[OWL] s700 clock initialization");

	base = of_iomap(np, 0);
	if (!base)
		return;

	ctx = owl_clk_init(np, base, CLK_NR_CLKS);
	
	if (!ctx)
		panic("%s: unable to allocate context.\n", __func__);

	owl_clk_register_fixed_rate(ctx, s700_fixed_rate_clks,
			ARRAY_SIZE(s700_fixed_rate_clks));

	owl_clk_register_pll(ctx, s700_pll_clks,
			ARRAY_SIZE(s700_pll_clks));

	owl_clk_register_divider(ctx, s700_div_clks,
			ARRAY_SIZE(s700_div_clks));

	/*owl_clk_register_factor(ctx, s700_factor_clks,
			ARRAY_SIZE(s700_factor_clks));*/

	owl_clk_register_mux(ctx, s700_mux_clks,
			ARRAY_SIZE(s700_mux_clks));

	owl_clk_register_gate(ctx, s700_gate_clks,
			ARRAY_SIZE(s700_gate_clks));

	owl_clk_register_composite(ctx, s700_composite_clks,
			ARRAY_SIZE(s700_composite_clks));
}

CLK_OF_DECLARE(s700_clk, "caninos,k7-cmu", s700_clk_init);

