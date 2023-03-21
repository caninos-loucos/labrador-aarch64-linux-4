// SPDX-License-Identifier: GPL-2.0
/*
 * Pinctrl/GPIO driver for Caninos Labrador
 *
 * Copyright (c) 2022-2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2018-2020 LSITEC - Caninos Loucos
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

#include "pinctrl-caninos.h"

static const char * const uart0_groups[] = {
	"uart0_extio_grp", "uart0_dummy_grp"
};

static const char * const i2c2_groups[] = {
	"i2c2_extio_grp", "i2c2_dummy_grp"
};

static const char * const pwm_groups[] = {
	"pwm_extio_grp", "pwm_dummy_grp"
};

static const char * const eth_groups[] = {
	"eth_rmii_grp"
};

static const struct caninos_pmx_func caninos_functions[] = {
	{
		.name = "uart0",
		.groups = uart0_groups,
		.num_groups = ARRAY_SIZE(uart0_groups),
	},
	{
		.name = "i2c2",
		.groups = i2c2_groups,
		.num_groups = ARRAY_SIZE(i2c2_groups),
	},
	{
		.name = "pwm",
		.groups = pwm_groups,
		.num_groups = ARRAY_SIZE(pwm_groups),
	},
	{
		.name = "eth",
		.groups = eth_groups,
		.num_groups = ARRAY_SIZE(eth_groups),
	},
};

static const struct pinctrl_pin_desc caninos_pins[] = {
	PINCTRL_PIN(0, "DUMMY0"),            // GPIOA0
	PINCTRL_PIN(1, "DUMMY1"),            // GPIOA1
	PINCTRL_PIN(2, "DUMMY2"),            // GPIOA2
	PINCTRL_PIN(3, "DUMMY3"),            // GPIOA3
	PINCTRL_PIN(4, "DUMMY4"),            // GPIOA4
	PINCTRL_PIN(5, "DUMMY5"),            // GPIOA5
	PINCTRL_PIN(6, "DUMMY6"),            // GPIOA6
	PINCTRL_PIN(7, "DUMMY7"),            // GPIOA7
	PINCTRL_PIN(8, "DUMMY8"),            // GPIOA8
	PINCTRL_PIN(9, "DUMMY9"),            // GPIOA9
	PINCTRL_PIN(10, "DUMMY10"),          // GPIOA10
	PINCTRL_PIN(11, "DUMMY11"),          // GPIOA11
	PINCTRL_PIN(12, "C3_NAND_DQS"),      // GPIOA12
	PINCTRL_PIN(13, "D2_NAND_DQSN"),     // GPIOA13
	PINCTRL_PIN(14, "H2_ETH_TXD0"),      // GPIOA14
	PINCTRL_PIN(15, "H3_ETH_TXD1"),      // GPIOA15
	PINCTRL_PIN(16, "G1_ETH_TXEN"),      // GPIOA16
	PINCTRL_PIN(17, "H4_ETH_RXER"),      // GPIOA17
	PINCTRL_PIN(18, "J4_ETH_CRS_DV"),    // GPIOA18
	PINCTRL_PIN(19, "J2_ETH_RXD1"),      // GPIOA19
	PINCTRL_PIN(20, "J1_ETH_RXD0"),      // GPIOA20
	PINCTRL_PIN(21, "G2_ETH_REF_CLK"),   // GPIOA21
	PINCTRL_PIN(22, "J3_ETH_MDC"),       // GPIOA22
	PINCTRL_PIN(23, "G3_ETH_MDIO"),      // GPIOA23
	PINCTRL_PIN(24, "V22_SIRQ0"),        // GPIOA24
	PINCTRL_PIN(25, "V21_SIRQ1"),        // GPIOA25
	PINCTRL_PIN(26, "U21_SIRQ2"),        // GPIOA26
	PINCTRL_PIN(27, "AD16_I2S_D0"),      // GPIOA27
	PINCTRL_PIN(28, "AC16_I2S_BCLK0"),   // GPIOA28
	PINCTRL_PIN(29, "AD17_I2S_LRCLK0"),  // GPIOA29
	PINCTRL_PIN(30, "AC17_I2S_MCLK0"),   // GPIOA30
	PINCTRL_PIN(31, "AB17_I2S_D1"),      // GPIOA31
	PINCTRL_PIN(32, "AD18_I2S_BCLK1"),   // GPIOB0
	PINCTRL_PIN(33, "AC18_I2S_LRCLK1"),  // GPIOB1
	PINCTRL_PIN(34, "AD19_I2S_MCLK1"),   // GPIOB2
	PINCTRL_PIN(35, "N21_KS_IN0"),       // GPIOB3
	PINCTRL_PIN(36, "M21_KS_IN1"),       // GPIOB4
	PINCTRL_PIN(37, "L21_KS_IN2"),       // GPIOB5
	PINCTRL_PIN(38, "K21_KS_IN3"),       // GPIOB6
	PINCTRL_PIN(39, "J21_KS_OUT0"),      // GPIOB7
	PINCTRL_PIN(40, "L22_KS_OUT1"),      // GPIOB8
	PINCTRL_PIN(41, "L23_KS_OUT2"),      // GPIOB9
	PINCTRL_PIN(42, "B10_OEP"),          // GPIOB10
	PINCTRL_PIN(43, "A10_OEN"),          // GPIOB11
	PINCTRL_PIN(44, "C10_ODP"),          // GPIOB12
	PINCTRL_PIN(45, "B11_ODN"),          // GPIOB13
	PINCTRL_PIN(46, "C11_OCP"),          // GPIOB14
	PINCTRL_PIN(47, "B12_OCN"),          // GPIOB15
	PINCTRL_PIN(48, "A12_OBP"),          // GPIOB16
	PINCTRL_PIN(49, "C12_OBN"),          // GPIOB17
	PINCTRL_PIN(50, "A13_OAP"),          // GPIOB18
	PINCTRL_PIN(51, "B13_OAN"),          // GPIOB19
	PINCTRL_PIN(52, "C14_EEP"),          // GPIOB20
	PINCTRL_PIN(53, "B14_EEN"),          // GPIOB21
	PINCTRL_PIN(54, "B15_EDP"),          // GPIOB22
	PINCTRL_PIN(55, "A15_EDN"),          // GPIOB23
	PINCTRL_PIN(56, "C15_ECP"),          // GPIOB24
	PINCTRL_PIN(57, "A16_ECN"),          // GPIOB25
	PINCTRL_PIN(58, "B16_EBP"),          // GPIOB26
	PINCTRL_PIN(59, "C16_EBN"),          // GPIOB27
	PINCTRL_PIN(60, "B17_EAP"),          // GPIOB28
	PINCTRL_PIN(61, "C17_EAN"),          // GPIOB29
	PINCTRL_PIN(62, "T19_LCD0_D18"),     // GPIOB30
	PINCTRL_PIN(63, "R19_LCD0_D2"),      // GPIOB31
	PINCTRL_PIN(64, "A9_DSI_DP3"),       // GPIOC0
	PINCTRL_PIN(65, "B9_DSI_DN3"),       // GPIOC1
	PINCTRL_PIN(66, "B8_DSI_DP1"),       // GPIOC2
	PINCTRL_PIN(67, "C8_DSI_DN1"),       // GPIOC3
	PINCTRL_PIN(68, "A7_DSI_CP"),        // GPIOC4
	PINCTRL_PIN(69, "B7_DSI_CN"),        // GPIOC5
	PINCTRL_PIN(70, "A6_DSI_DP0"),       // GPIOC6
	PINCTRL_PIN(71, "B6_DSI_DN0"),       // GPIOC7
	PINCTRL_PIN(72, "C6_DSI_DP2"),       // GPIOC8
	PINCTRL_PIN(73, "B5_DSI_DN2"),       // GPIOC9
	PINCTRL_PIN(74, "Y23_SD0_D0"),       // GPIOC10
	PINCTRL_PIN(75, "Y22_SD0_D1"),       // GPIOC11
	PINCTRL_PIN(76, "V24_SD0_D2"),       // GPIOC12
	PINCTRL_PIN(77, "W23_SD0_D3"),       // GPIOC13
	PINCTRL_PIN(78, "U22_SDIO1_D0"),     // GPIOC14
	PINCTRL_PIN(79, "V23_SDIO1_D1"),     // GPIOC15
	PINCTRL_PIN(80, "T23_SDIO1_D2"),     // GPIOC16
	PINCTRL_PIN(81, "T24_SDIO1_D3"),     // GPIOC17
	PINCTRL_PIN(82, "W24_SD0_CMD"),      // GPIOC18
	PINCTRL_PIN(83, "W22_SD0_CLK"),      // GPIOC19
	PINCTRL_PIN(84, "T22_SDIO1_CMD"),    // GPIOC20
	PINCTRL_PIN(85, "U23_SDIO1_CLK"),    // GPIOC21
	PINCTRL_PIN(86, "AB18_SPI0_SCLK"),   // GPIOC22
	PINCTRL_PIN(87, "AC19_SPI0_SS"),     // GPIOC23
	PINCTRL_PIN(88, "AB19_SPI0_MISO"),   // GPIOC24
	PINCTRL_PIN(89, "AA19_SPI0_MOSI"),   // GPIOC25
	PINCTRL_PIN(90, "AC22_UART0_RX"),    // GPIOC26
	PINCTRL_PIN(91, "AD22_UART0_TX"),    // GPIOC27
	PINCTRL_PIN(92, "AA23_TWI0_SCLK"),   // GPIOC28
	PINCTRL_PIN(93, "AA24_TWI0_SDATA"),  // GPIOC29
	PINCTRL_PIN(94, "DUMMY12"),          // GPIOC30
	PINCTRL_PIN(95, "R22_SR0_PCLK"),     // GPIOC31
	PINCTRL_PIN(96, "DUMMY13"),          // GPIOD0
	PINCTRL_PIN(97, "DUMMY14"),          // GPIOD1
	PINCTRL_PIN(98, "DUMMY15"),          // GPIOD2
	PINCTRL_PIN(99, "DUMMY16"),          // GPIOD3
	PINCTRL_PIN(100, "DUMMY17"),         // GPIOD4
	PINCTRL_PIN(101, "DUMMY18"),         // GPIOD5
	PINCTRL_PIN(102, "DUMMY19"),         // GPIOD6
	PINCTRL_PIN(103, "DUMMY20"),         // GPIOD7
	PINCTRL_PIN(104, "DUMMY21"),         // GPIOD8
	PINCTRL_PIN(105, "DUMMY22"),         // GPIOD9
	PINCTRL_PIN(106, "T21_SR0_CKOUT"),   // GPIOD10
	PINCTRL_PIN(107, "DUMMY23"),         // GPIOD11
	PINCTRL_PIN(108, "B2_NAND_ALE"),     // GPIOD12
	PINCTRL_PIN(109, "A2_NAND_CLE"),     // GPIOD13
	PINCTRL_PIN(110, "B3_NAND_CE0B"),    // GPIOD14
	PINCTRL_PIN(111, "A3_NAND_CE1B"),    // GPIOD15
	PINCTRL_PIN(112, "E3_NAND_CE2B"),    // GPIOD16
	PINCTRL_PIN(113, "C5_NAND_CE3B"),    // GPIOD17
	PINCTRL_PIN(114, "AC21_UART2_RX"),   // GPIOD18
	PINCTRL_PIN(115, "AD21_UART2_TX"),   // GPIOD19
	PINCTRL_PIN(116, "AB22_UART2_RTSB"), // GPIOD20
	PINCTRL_PIN(117, "AB21_UART2_CTSB"), // GPIOD21
	PINCTRL_PIN(118, "AD23_UART3_RX"),   // GPIOD22
	PINCTRL_PIN(119, "AD24_UART3_TX"),   // GPIOD23
	PINCTRL_PIN(120, "AC23_UART3_RTSB"), // GPIOD24
	PINCTRL_PIN(121, "AC24_UART3_CTSB"), // GPIOD25
	PINCTRL_PIN(122, "DUMMY24"),         // GPIOD26
	PINCTRL_PIN(123, "DUMMY25"),         // GPIOD27
	PINCTRL_PIN(124, "AA21_PCM1_IN"),    // GPIOD28
	PINCTRL_PIN(125, "Y21_PCM1_CLK"),    // GPIOD29
	PINCTRL_PIN(126, "AA22_PCM1_SYNC"),  // GPIOD30
	PINCTRL_PIN(127, "W21_PCM1_OUT"),    // GPIOD31
	PINCTRL_PIN(128, "AC20_TWI1_SCLK"),  // GPIOE0
	PINCTRL_PIN(129, "AB20_TWI1_SDATA"), // GPIOE1
	PINCTRL_PIN(130, "AB23_TWI2_SCLK"),  // GPIOE2
	PINCTRL_PIN(131, "AB24_TWI2_SDATA"), // GPIOE3
};

static const unsigned int uart0_extio_pins[] = { GPIOC(27), GPIOC(26) };
static const unsigned int i2c2_extio_pins[]  = { GPIOE(3), GPIOE(2) };
static const unsigned int pwm_extio_pins[]   = { GPIOB(8) };

static const unsigned int uart0_dummy_pins[] = { GPIOA(0), GPIOA(1) };
static const unsigned int i2c2_dummy_pins[]  = { GPIOA(2), GPIOA(3) };
static const unsigned int pwm_dummy_pins[]   = { GPIOA(4) };

static const unsigned int eth_rmii_pins[] = {
	GPIOA(21), GPIOA(15), GPIOA(14), GPIOA(16), GPIOA(17), 
	GPIOA(19), GPIOA(20), GPIOA(18), GPIOA(22), GPIOA(23)
};

//      RMII
// RMII_REF_CLK  --> GPIOA21
// RMII_TXD1     --> GPIOA15
// RMII_TXD0     --> GPIOA14
// RMII_TX_EN    --> GPIOA16
// RMII_RX_ER    --> GPIOA17
// RMII_RXD1     --> GPIOA19
// RMII_RXD0     --> GPIOA20
// RMII_CRS_DV   --> GPIOA18
// MDC           --> GPIOA22
// MDIO          --> GPIOA23

static const struct caninos_group caninos_groups[] = {
	{
		.name = "uart0_extio_grp",
		.pins = uart0_extio_pins,
		.num_pins = ARRAY_SIZE(uart0_extio_pins),
	},
	{
		.name = "i2c2_extio_grp",
		.pins = i2c2_extio_pins,
		.num_pins = ARRAY_SIZE(i2c2_extio_pins),
	},
	{
		.name = "pwm_extio_grp",
		.pins = pwm_extio_pins,
		.num_pins = ARRAY_SIZE(pwm_extio_pins),
	},
	{
		.name = "uart0_dummy_grp",
		.pins = uart0_dummy_pins,
		.num_pins = ARRAY_SIZE(uart0_dummy_pins),
	},
	{
		.name = "i2c2_dummy_grp",
		.pins = i2c2_dummy_pins,
		.num_pins = ARRAY_SIZE(i2c2_dummy_pins),
	},
	{
		.name = "pwm_dummy_grp",
		.pins = pwm_dummy_pins,
		.num_pins = ARRAY_SIZE(pwm_dummy_pins),
	},
	{
		.name = "eth_rmii_grp",
		.pins = eth_rmii_pins,
		.num_pins = ARRAY_SIZE(eth_rmii_pins),
	},
};

static int caninos_pinctrl_hwinit(struct caninos_pinctrl *pctl)
{
	writel(0x00000000, pctl->base + MFP_CTL0);
	writel(0x2e400070, pctl->base + MFP_CTL1);
	writel(0x10000600, pctl->base + MFP_CTL2);
	writel(0x40000008, pctl->base + MFP_CTL3);
	writel(0x00000000, pctl->base + PAD_PULLCTL0);
	writel(0x0003e001, pctl->base + PAD_PULLCTL1);
	writel(0x00000000, pctl->base + PAD_PULLCTL2);
	writel(0x40401880, pctl->base + PAD_ST0);
	writel(0x00000140, pctl->base + PAD_ST1);
	writel(0x00000002, pctl->base + PAD_CTL);
	writel(0x20aaaaaa, pctl->base + PAD_DRV0);
	writel(0xaa8a0800, pctl->base + PAD_DRV1);
	writel(0xa9482008, pctl->base + PAD_DRV2);
	return 0;
}

const struct caninos_pinctrl_hwdiff k5_pinctrl_hw = {
	.functions = caninos_functions,
	.nfuncs = ARRAY_SIZE(caninos_functions),
	.groups = caninos_groups,
	.ngroups = ARRAY_SIZE(caninos_groups),
	.pins = caninos_pins,
	.npins = ARRAY_SIZE(caninos_pins),
	.hwinit = caninos_pinctrl_hwinit,
};
