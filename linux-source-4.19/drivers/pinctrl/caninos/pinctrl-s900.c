/*
 * Pinctrl driver for Actions S900 SoC
 *
 * Copyright (C) 2014 Actions Semi Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinctrl.h>
#include "pinctrl-owl.h"

/* Pinctrl registers offset */
#define MFP_CTL0		(0x0040)
#define MFP_CTL1		(0x0044)
#define MFP_CTL2		(0x0048)
#define MFP_CTL3		(0x004C)
#define PAD_PULLCTL0		(0x0060)
#define PAD_PULLCTL1		(0x0064)
#define PAD_PULLCTL2		(0x0068)
#define PAD_ST0			(0x006C)
#define PAD_ST1			(0x0070)
#define PAD_CTL			(0x0074)
#define PAD_DRV0		(0x0080)
#define PAD_DRV1		(0x0084)
#define PAD_DRV2		(0x0088)
#define PAD_SR0			(0x0270)
#define PAD_SR1			(0x0274)
#define PAD_SR2			(0x0278)

/*
 * Most pins affected by the pinmux can also be GPIOs. Define these first.
 * These must match how the GPIO driver names/numbers its pins.
 */
#define _GPIOA(offset)		(offset)
#define _GPIOB(offset)		(32 + (offset))
#define _GPIOC(offset)		(64 + (offset))
#define _GPIOD(offset)		(96 + (offset))
#define _GPIOE(offset)		(128 + (offset))
#define _GPIOF(offset)		(160 + (offset))

/* All non-GPIO pins follow */
#define NUM_GPIOS		(_GPIOF(7) + 1)
#define _PIN(offset)		(NUM_GPIOS + (offset))

/* Ethernet MAC */
#define P_ETH_TXD0		_GPIOA(0)
#define P_ETH_TXD1		_GPIOA(1)
#define P_ETH_TXEN		_GPIOA(2)
#define P_ETH_RXER		_GPIOA(3)
#define P_ETH_CRS_DV		_GPIOA(4)
#define P_ETH_RXD1		_GPIOA(5)
#define P_ETH_RXD0		_GPIOA(6)
#define P_ETH_REF_CLK		_GPIOA(7)
#define P_ETH_MDC		_GPIOA(8)
#define P_ETH_MDIO		_GPIOA(9)

/* SIRQ */
#define P_SIRQ0			_GPIOA(10)
#define P_SIRQ1			_GPIOA(11)
#define P_SIRQ2			_GPIOA(12)

/* I2S */
#define P_I2S_D0		_GPIOA(13)
#define P_I2S_BCLK0		_GPIOA(14)
#define P_I2S_LRCLK0		_GPIOA(15)
#define P_I2S_MCLK0		_GPIOA(16)
#define P_I2S_D1		_GPIOA(17)
#define P_I2S_BCLK1		_GPIOA(18)
#define P_I2S_LRCLK1		_GPIOA(19)
#define P_I2S_MCLK1		_GPIOA(20)

/* PCM1 */
#define P_PCM1_IN		_GPIOF(0)
#define P_PCM1_CLK		_GPIOF(1)
#define P_PCM1_SYNC		_GPIOF(2)
#define P_PCM1_OUT		_GPIOF(3)

/* ERAM */
#define P_ERAM_A5		_GPIOA(21)
#define P_ERAM_A6		_GPIOA(22)
#define P_ERAM_A7		_GPIOA(23)
#define P_ERAM_A8		_GPIOA(24)
#define P_ERAM_A9		_GPIOA(25)
#define P_ERAM_A10		_GPIOA(26)
#define P_ERAM_A11		_GPIOA(27)

/* LCD0 */
#define P_LVDS_OEP		_GPIOD(0)
#define P_LVDS_OEN		_GPIOD(1)
#define P_LVDS_ODP		_GPIOD(2)
#define P_LVDS_ODN		_GPIOD(3)
#define P_LVDS_OCP		_GPIOD(4)
#define P_LVDS_OCN		_GPIOD(5)
#define P_LVDS_OBP		_GPIOD(6)
#define P_LVDS_OBN		_GPIOD(7)
#define P_LVDS_OAP		_GPIOD(8)
#define P_LVDS_OAN		_GPIOD(9)
#define P_LVDS_EEP		_GPIOD(10)
#define P_LVDS_EEN		_GPIOD(11)
#define P_LVDS_EDP		_GPIOD(12)
#define P_LVDS_EDN		_GPIOD(13)
#define P_LVDS_ECP		_GPIOD(14)
#define P_LVDS_ECN		_GPIOD(15)
#define P_LVDS_EBP		_GPIOD(16)
#define P_LVDS_EBN		_GPIOD(17)
#define P_LVDS_EAP		_GPIOD(18)
#define P_LVDS_EAN		_GPIOD(19)

/* SD */
#define P_SD0_D0		_GPIOA(28)
#define P_SD0_D1		_GPIOA(29)
#define P_SD0_D2		_GPIOA(30)
#define P_SD0_D3		_GPIOA(31)
#define P_SD1_D0		_GPIOB(0)
#define P_SD1_D1		_GPIOB(1)
#define P_SD1_D2		_GPIOB(2)
#define P_SD1_D3		_GPIOB(3)
#define P_SD0_CMD		_GPIOB(4)
#define P_SD0_CLK		_GPIOB(5)
#define P_SD1_CMD		_GPIOB(6)
#define P_SD1_CLK		_GPIOB(7)

/* SPI */
#define P_SPI0_SCLK		_GPIOB(8)
#define P_SPI0_SS		_GPIOB(9)
#define P_SPI0_MISO		_GPIOB(10)
#define P_SPI0_MOSI		_GPIOB(11)

/* UART for console */
#define P_UART0_RX		_GPIOB(12)
#define P_UART0_TX		_GPIOB(13)

/* UART for Bluetooth */
#define P_UART2_RX		_GPIOB(14)
#define P_UART2_TX		_GPIOB(15)
#define P_UART2_RTSB		_GPIOB(16)
#define P_UART2_CTSB		_GPIOB(17)

/* UART for 3G */
#define P_UART3_RX		_GPIOF(4)
#define P_UART3_TX		_GPIOF(5)
#define P_UART3_RTSB		_GPIOF(6)
#define P_UART3_CTSB		_GPIOF(7)

/* UART for GPS */
#define P_UART4_RX		_GPIOB(18)
#define P_UART4_TX		_GPIOB(19)

/* I2C */
#define P_I2C0_SCLK		_GPIOB(20)
#define P_I2C0_SDATA		_GPIOB(21)
#define P_I2C1_SCLK		_GPIOB(22)
#define P_I2C1_SDATA		_GPIOB(23)
#define P_I2C2_SCLK		_GPIOB(24)
#define P_I2C2_SDATA		_GPIOB(25)

/* MIPI */
#define P_CSI0_DN0		_GPIOB(26)
#define P_CSI0_DP0		_GPIOB(27)
#define P_CSI0_DN1		_GPIOB(28)
#define P_CSI0_DP1		_GPIOB(29)
#define P_CSI0_CN		_GPIOB(30)
#define P_CSI0_CP		_GPIOB(31)
#define P_CSI0_DN2		_GPIOC(0)
#define P_CSI0_DP2		_GPIOC(1)
#define P_CSI0_DN3		_GPIOC(2)
#define P_CSI0_DP3		_GPIOC(3)

#define P_DSI_DP3		_GPIOD(20)
#define P_DSI_DN3		_GPIOD(21)
#define P_DSI_DP1		_GPIOD(22)
#define P_DSI_DN1		_GPIOD(23)
#define P_DSI_CP		_GPIOD(24)
#define P_DSI_CN		_GPIOD(25)
#define P_DSI_DP0		_GPIOD(26)
#define P_DSI_DN0		_GPIOD(27)
#define P_DSI_DP2		_GPIOD(28)
#define P_DSI_DN2		_GPIOD(29)

/* Sensor */
#define P_SENSOR0_PCLK		_GPIOC(4)

#define P_CSI1_DN0		_GPIOC(5)
#define P_CSI1_DP0		_GPIOC(6)
#define P_CSI1_DN1		_GPIOC(7)
#define P_CSI1_DP1		_GPIOC(8)
#define P_CSI1_CN		_GPIOC(9)
#define P_CSI1_CP		_GPIOC(10)

#define P_SENSOR0_CKOUT		_GPIOC(11)

/* NAND (1.8v / 3.3v) */
#define P_NAND0_D0		_GPIOE(0)
#define P_NAND0_D1		_GPIOE(1)
#define P_NAND0_D2		_GPIOE(2)
#define P_NAND0_D3		_GPIOE(3)
#define P_NAND0_D4		_GPIOE(4)
#define P_NAND0_D5		_GPIOE(5)
#define P_NAND0_D6		_GPIOE(6)
#define P_NAND0_D7		_GPIOE(7)
#define P_NAND0_DQS		_GPIOE(8)
#define P_NAND0_DQSN		_GPIOE(9)
#define P_NAND0_ALE		_GPIOE(10)
#define P_NAND0_CLE		_GPIOE(11)
#define P_NAND0_CEB0		_GPIOE(12)
#define P_NAND0_CEB1		_GPIOE(13)
#define P_NAND0_CEB2		_GPIOE(14)
#define P_NAND0_CEB3		_GPIOE(15)

#define P_NAND1_D0		_GPIOE(16)
#define P_NAND1_D1		_GPIOE(17)
#define P_NAND1_D2		_GPIOE(18)
#define P_NAND1_D3		_GPIOE(19)
#define P_NAND1_D4		_GPIOE(20)
#define P_NAND1_D5		_GPIOE(21)
#define P_NAND1_D6		_GPIOE(22)
#define P_NAND1_D7		_GPIOE(23)
#define P_NAND1_DQS		_GPIOE(24)
#define P_NAND1_DQSN		_GPIOE(25)
#define P_NAND1_ALE		_GPIOE(26)
#define P_NAND1_CLE		_GPIOE(27)
#define P_NAND1_CEB0		_GPIOE(28)
#define P_NAND1_CEB1		_GPIOE(29)
#define P_NAND1_CEB2		_GPIOE(30)
#define P_NAND1_CEB3		_GPIOE(31)

/* System */
#define P_SGPIO0		_PIN(0)
#define P_SGPIO1		_PIN(1)
#define P_SGPIO2		_PIN(2)
#define P_SGPIO3		_PIN(3)

#define _FIRSTPAD		P_ETH_TXD0
#define _LASTPAD		P_SGPIO3
#define NUM_PADS		(_LASTPAD - _FIRSTPAD + 1)

#define PINCTRL_PIN(a, b)	{ .number = a, .name = b }

/* Pad names for the pinmux subsystem */
const struct pinctrl_pin_desc s900_pads[] = {
	PINCTRL_PIN(P_ETH_TXD0, "P_ETH_TXD0"),
	PINCTRL_PIN(P_ETH_TXD1, "P_ETH_TXD1"),
	PINCTRL_PIN(P_ETH_TXEN, "P_ETH_TXEN"),
	PINCTRL_PIN(P_ETH_RXER, "P_ETH_RXER"),
	PINCTRL_PIN(P_ETH_CRS_DV, "P_ETH_CRS_DV"),
	PINCTRL_PIN(P_ETH_RXD1, "P_ETH_RXD1"),
	PINCTRL_PIN(P_ETH_RXD0, "P_ETH_RXD0"),
	PINCTRL_PIN(P_ETH_REF_CLK, "P_ETH_REF_CLK"),
	PINCTRL_PIN(P_ETH_MDC, "P_ETH_MDC"),
	PINCTRL_PIN(P_ETH_MDIO, "P_ETH_MDIO"),
	PINCTRL_PIN(P_SIRQ0, "P_SIRQ0"),
	PINCTRL_PIN(P_SIRQ1, "P_SIRQ1"),
	PINCTRL_PIN(P_SIRQ2, "P_SIRQ2"),
	PINCTRL_PIN(P_I2S_D0, "P_I2S_D0"),
	PINCTRL_PIN(P_I2S_BCLK0, "P_I2S_BCLK0"),
	PINCTRL_PIN(P_I2S_LRCLK0, "P_I2S_LRCLK0"),
	PINCTRL_PIN(P_I2S_MCLK0, "P_I2S_MCLK0"),
	PINCTRL_PIN(P_I2S_D1, "P_I2S_D1"),
	PINCTRL_PIN(P_I2S_BCLK1, "P_I2S_BCLK1"),
	PINCTRL_PIN(P_I2S_LRCLK1, "P_I2S_LRCLK1"),
	PINCTRL_PIN(P_I2S_MCLK1, "P_I2S_MCLK1"),
	PINCTRL_PIN(P_PCM1_IN, "P_PCM1_IN"),
	PINCTRL_PIN(P_PCM1_CLK, "P_PCM1_CLK"),
	PINCTRL_PIN(P_PCM1_SYNC, "P_PCM1_SYNC"),
	PINCTRL_PIN(P_PCM1_OUT, "P_PCM1_OUT"),
	PINCTRL_PIN(P_ERAM_A5, "P_ERAM_A5"),
	PINCTRL_PIN(P_ERAM_A6, "P_ERAM_A6"),
	PINCTRL_PIN(P_ERAM_A7, "P_ERAM_A7"),
	PINCTRL_PIN(P_ERAM_A8, "P_ERAM_A8"),
	PINCTRL_PIN(P_ERAM_A9, "P_ERAM_A9"),
	PINCTRL_PIN(P_ERAM_A10, "P_ERAM_A10"),
	PINCTRL_PIN(P_ERAM_A11, "P_ERAM_A11"),
	PINCTRL_PIN(P_LVDS_OEP, "P_LVDS_OEP"),
	PINCTRL_PIN(P_LVDS_OEN, "P_LVDS_OEN"),
	PINCTRL_PIN(P_LVDS_ODP, "P_LVDS_ODP"),
	PINCTRL_PIN(P_LVDS_ODN, "P_LVDS_ODN"),
	PINCTRL_PIN(P_LVDS_OCP, "P_LVDS_OCP"),
	PINCTRL_PIN(P_LVDS_OCN, "P_LVDS_OCN"),
	PINCTRL_PIN(P_LVDS_OBP, "P_LVDS_OBP"),
	PINCTRL_PIN(P_LVDS_OBN, "P_LVDS_OBN"),
	PINCTRL_PIN(P_LVDS_OAP, "P_LVDS_OAP"),
	PINCTRL_PIN(P_LVDS_OAN, "P_LVDS_OAN"),
	PINCTRL_PIN(P_LVDS_EEP, "P_LVDS_EEP"),
	PINCTRL_PIN(P_LVDS_EEN, "P_LVDS_EEN"),
	PINCTRL_PIN(P_LVDS_EDP, "P_LVDS_EDP"),
	PINCTRL_PIN(P_LVDS_EDN, "P_LVDS_EDN"),
	PINCTRL_PIN(P_LVDS_ECP, "P_LVDS_ECP"),
	PINCTRL_PIN(P_LVDS_ECN, "P_LVDS_ECN"),
	PINCTRL_PIN(P_LVDS_EBP, "P_LVDS_EBP"),
	PINCTRL_PIN(P_LVDS_EBN, "P_LVDS_EBN"),
	PINCTRL_PIN(P_LVDS_EAP, "P_LVDS_EAP"),
	PINCTRL_PIN(P_LVDS_EAN, "P_LVDS_EAN"),
	PINCTRL_PIN(P_SD0_D0, "P_SD0_D0"),
	PINCTRL_PIN(P_SD0_D1, "P_SD0_D1"),
	PINCTRL_PIN(P_SD0_D2, "P_SD0_D2"),
	PINCTRL_PIN(P_SD0_D3, "P_SD0_D3"),
	PINCTRL_PIN(P_SD1_D0, "P_SD1_D0"),
	PINCTRL_PIN(P_SD1_D1, "P_SD1_D1"),
	PINCTRL_PIN(P_SD1_D2, "P_SD1_D2"),
	PINCTRL_PIN(P_SD1_D3, "P_SD1_D3"),
	PINCTRL_PIN(P_SD0_CMD, "P_SD0_CMD"),
	PINCTRL_PIN(P_SD0_CLK, "P_SD0_CLK"),
	PINCTRL_PIN(P_SD1_CMD, "P_SD1_CMD"),
	PINCTRL_PIN(P_SD1_CLK, "P_SD1_CLK"),
	PINCTRL_PIN(P_SPI0_SCLK, "P_SPI0_SCLK"),
	PINCTRL_PIN(P_SPI0_SS, "P_SPI0_SS"),
	PINCTRL_PIN(P_SPI0_MISO, "P_SPI0_MISO"),
	PINCTRL_PIN(P_SPI0_MOSI, "P_SPI0_MOSI"),
	PINCTRL_PIN(P_UART0_RX, "P_UART0_RX"),
	PINCTRL_PIN(P_UART0_TX, "P_UART0_TX"),
	PINCTRL_PIN(P_UART2_RX, "P_UART2_RX"),
	PINCTRL_PIN(P_UART2_TX, "P_UART2_TX"),
	PINCTRL_PIN(P_UART2_RTSB, "P_UART2_RTSB"),
	PINCTRL_PIN(P_UART2_CTSB, "P_UART2_CTSB"),
	PINCTRL_PIN(P_UART3_RX, "P_UART3_RX"),
	PINCTRL_PIN(P_UART3_TX, "P_UART3_TX"),
	PINCTRL_PIN(P_UART3_RTSB, "P_UART3_RTSB"),
	PINCTRL_PIN(P_UART3_CTSB, "P_UART3_CTSB"),
	PINCTRL_PIN(P_UART4_RX, "P_UART4_RX"),
	PINCTRL_PIN(P_UART4_TX, "P_UART4_TX"),
	PINCTRL_PIN(P_I2C0_SCLK, "P_I2C0_SCLK"),
	PINCTRL_PIN(P_I2C0_SDATA, "P_I2C0_SDATA"),
	PINCTRL_PIN(P_I2C1_SCLK, "P_I2C1_SCLK"),
	PINCTRL_PIN(P_I2C1_SDATA, "P_I2C1_SDATA"),
	PINCTRL_PIN(P_I2C2_SCLK, "P_I2C2_SCLK"),
	PINCTRL_PIN(P_I2C2_SDATA, "P_I2C2_SDATA"),
	PINCTRL_PIN(P_CSI0_DN0, "P_CSI0_DN0"),
	PINCTRL_PIN(P_CSI0_DP0, "P_CSI0_DP0"),
	PINCTRL_PIN(P_CSI0_DN1, "P_CSI0_DN1"),
	PINCTRL_PIN(P_CSI0_DP1, "P_CSI0_DP1"),
	PINCTRL_PIN(P_CSI0_CN, "P_CSI0_CN"),
	PINCTRL_PIN(P_CSI0_CP, "P_CSI0_CP"),
	PINCTRL_PIN(P_CSI0_DN2, "P_CSI0_DN2"),
	PINCTRL_PIN(P_CSI0_DP2, "P_CSI0_DP2"),
	PINCTRL_PIN(P_CSI0_DN3, "P_CSI0_DN3"),
	PINCTRL_PIN(P_CSI0_DP3, "P_CSI0_DP3"),
	PINCTRL_PIN(P_DSI_DP3, "P_DSI_DP3"),
	PINCTRL_PIN(P_DSI_DN3, "P_DSI_DN3"),
	PINCTRL_PIN(P_DSI_DP1, "P_DSI_DP1"),
	PINCTRL_PIN(P_DSI_DN1, "P_DSI_DN1"),
	PINCTRL_PIN(P_DSI_CP, "P_DSI_CP"),
	PINCTRL_PIN(P_DSI_CN, "P_DSI_CN"),
	PINCTRL_PIN(P_DSI_DP0, "P_DSI_DP0"),
	PINCTRL_PIN(P_DSI_DN0, "P_DSI_DN0"),
	PINCTRL_PIN(P_DSI_DP2, "P_DSI_DP2"),
	PINCTRL_PIN(P_DSI_DN2, "P_DSI_DN2"),
	PINCTRL_PIN(P_SENSOR0_PCLK, "P_SENSOR0_PCLK"),
	PINCTRL_PIN(P_CSI1_DN0, "P_CSI1_DN0"),
	PINCTRL_PIN(P_CSI1_DP0, "P_CSI1_DP0"),
	PINCTRL_PIN(P_CSI1_DN1, "P_CSI1_DN1"),
	PINCTRL_PIN(P_CSI1_DP1, "P_CSI1_DP1"),
	PINCTRL_PIN(P_CSI1_CN, "P_CSI1_CN"),
	PINCTRL_PIN(P_CSI1_CP, "P_CSI1_CP"),
	PINCTRL_PIN(P_SENSOR0_CKOUT, "P_SENSOR0_CKOUT"),
	PINCTRL_PIN(P_NAND0_D0, "P_NAND0_D0"),
	PINCTRL_PIN(P_NAND0_D1, "P_NAND0_D1"),
	PINCTRL_PIN(P_NAND0_D2, "P_NAND0_D2"),
	PINCTRL_PIN(P_NAND0_D3, "P_NAND0_D3"),
	PINCTRL_PIN(P_NAND0_D4, "P_NAND0_D4"),
	PINCTRL_PIN(P_NAND0_D5, "P_NAND0_D5"),
	PINCTRL_PIN(P_NAND0_D6, "P_NAND0_D6"),
	PINCTRL_PIN(P_NAND0_D7, "P_NAND0_D7"),
	PINCTRL_PIN(P_NAND0_DQS, "P_NAND0_DQS"),
	PINCTRL_PIN(P_NAND0_DQSN, "P_NAND0_DQSN"),
	PINCTRL_PIN(P_NAND0_ALE, "P_NAND0_ALE"),
	PINCTRL_PIN(P_NAND0_CLE, "P_NAND0_CLE"),
	PINCTRL_PIN(P_NAND0_CEB0, "P_NAND0_CEB0"),
	PINCTRL_PIN(P_NAND0_CEB1, "P_NAND0_CEB1"),
	PINCTRL_PIN(P_NAND0_CEB2, "P_NAND0_CEB2"),
	PINCTRL_PIN(P_NAND0_CEB3, "P_NAND0_CEB3"),
	PINCTRL_PIN(P_NAND1_D0, "P_NAND1_D0"),
	PINCTRL_PIN(P_NAND1_D1, "P_NAND1_D1"),
	PINCTRL_PIN(P_NAND1_D2, "P_NAND1_D2"),
	PINCTRL_PIN(P_NAND1_D3, "P_NAND1_D3"),
	PINCTRL_PIN(P_NAND1_D4, "P_NAND1_D4"),
	PINCTRL_PIN(P_NAND1_D5, "P_NAND1_D5"),
	PINCTRL_PIN(P_NAND1_D6, "P_NAND1_D6"),
	PINCTRL_PIN(P_NAND1_D7, "P_NAND1_D7"),
	PINCTRL_PIN(P_NAND1_DQS, "P_NAND1_DQS"),
	PINCTRL_PIN(P_NAND1_DQSN, "P_NAND1_DQSN"),
	PINCTRL_PIN(P_NAND1_ALE, "P_NAND1_ALE"),
	PINCTRL_PIN(P_NAND1_CLE, "P_NAND1_CLE"),
	PINCTRL_PIN(P_NAND1_CEB0, "P_NAND1_CEB0"),
	PINCTRL_PIN(P_NAND1_CEB1, "P_NAND1_CEB1"),
	PINCTRL_PIN(P_NAND1_CEB2, "P_NAND1_CEB2"),
	PINCTRL_PIN(P_NAND1_CEB3, "P_NAND1_CEB3"),
	PINCTRL_PIN(P_SGPIO0, "P_SGPIO0"),
	PINCTRL_PIN(P_SGPIO1, "P_SGPIO1"),
	PINCTRL_PIN(P_SGPIO2, "P_SGPIO2"),
	PINCTRL_PIN(P_SGPIO3, "P_SGPIO3"),
};

int s900_num_pads = ARRAY_SIZE(s900_pads);

/*****MFP group data****************************/
enum s900_mux {
	S900_MUX_ERAM,
	S900_MUX_ETH_RMII,
	S900_MUX_ETH_SMII,
	S900_MUX_SPI0,
	S900_MUX_SPI1,
	S900_MUX_SPI2,
	S900_MUX_SPI3,
	S900_MUX_SENS0,
	S900_MUX_UART0,
	S900_MUX_UART1,
	S900_MUX_UART2,
	S900_MUX_UART3,
	S900_MUX_UART4,
	S900_MUX_UART5,
	S900_MUX_UART6,
	S900_MUX_I2S0,
	S900_MUX_I2S1,
	S900_MUX_PCM0,
	S900_MUX_PCM1,
	S900_MUX_JTAG,
	S900_MUX_PWM0,
	S900_MUX_PWM1,
	S900_MUX_PWM2,
	S900_MUX_PWM3,
	S900_MUX_PWM4,
	S900_MUX_PWM5,
	S900_MUX_SD0,
	S900_MUX_SD1,
	S900_MUX_SD2,
	S900_MUX_SD3,
	S900_MUX_I2C0,
	S900_MUX_I2C1,
	S900_MUX_I2C2,
	S900_MUX_I2C3,
	S900_MUX_I2C4,
	S900_MUX_I2C5,
	S900_MUX_LVDS,
	S900_MUX_USB20,
	S900_MUX_USB30,
	S900_MUX_GPU,
	S900_MUX_MIPI_CSI0,
	S900_MUX_MIPI_CSI1,
	S900_MUX_MIPI_DSI,
	S900_MUX_NAND0,
	S900_MUX_NAND1,
	S900_MUX_SPDIF,
	S900_MUX_SIRQ0,
	S900_MUX_SIRQ1,
	S900_MUX_SIRQ2,
	S900_MUX_AUX_START,

	S900_MUX_MAX,
	S900_MUX_RESERVED,
};

/*
** mfp0_22
*/
static unsigned int  owl_mfp0_22_pads[] = {
	P_LVDS_OAP,
	P_LVDS_OAN,
};

static unsigned int  owl_mfp0_22_funcs[] = {
	S900_MUX_ERAM,
	S900_MUX_UART4,
};

/*
** mfp0_21_20
*/
static unsigned int  owl_mfp0_21_20_eth_mdc_pads[] = {
	P_ETH_MDC,
};

static unsigned int  owl_mfp0_21_20_eth_mdc_funcs[] = {
	S900_MUX_ETH_RMII,
	S900_MUX_PWM2,
	S900_MUX_UART2,
	S900_MUX_RESERVED,
};

static unsigned int  owl_mfp0_21_20_eth_mdio_pads[] = {
	P_ETH_MDIO,
};

static unsigned int  owl_mfp0_21_20_eth_mdio_funcs[] = {
	S900_MUX_ETH_RMII,
	S900_MUX_PWM3,
	S900_MUX_UART2,
	S900_MUX_RESERVED,
};

/*
** mfp0_19
*/
static unsigned int  owl_mfp0_19_sirq0_pads[] = {
	P_SIRQ0,
};

static unsigned int  owl_mfp0_19_sirq0_funcs[] = {
	S900_MUX_SIRQ0,
	S900_MUX_PWM0,
};

static unsigned int  owl_mfp0_19_sirq1_pads[] = {
	P_SIRQ1,
};

static unsigned int  owl_mfp0_19_sirq1_funcs[] = {
	S900_MUX_SIRQ1,
	S900_MUX_PWM1,
};

/*
** mfp0_18_16
*/
static unsigned int  owl_mfp0_18_16_eth_txd0_pads[] = {
	P_ETH_TXD0,
};

static unsigned int  owl_mfp0_18_16_eth_txd0_funcs[] = {
	S900_MUX_ETH_RMII,
	S900_MUX_ETH_SMII,
	S900_MUX_SPI2,
	S900_MUX_UART6,
	S900_MUX_SENS0,
	S900_MUX_PWM0,
};

static unsigned int  owl_mfp0_18_16_eth_txd1_pads[] = {
	P_ETH_TXD1,
};

static unsigned int  owl_mfp0_18_16_eth_txd1_funcs[] = {
	S900_MUX_ETH_RMII,
	S900_MUX_ETH_SMII,
	S900_MUX_SPI2,
	S900_MUX_UART6,
	S900_MUX_SENS0,
	S900_MUX_PWM1,
};

/*
** mfp0_15_13
*/
static unsigned int  owl_mfp0_15_13_eth_txen_pads[] = {
	P_ETH_TXEN,
};

static unsigned int  owl_mfp0_15_13_eth_txen_funcs[] = {
	S900_MUX_ETH_RMII,
	S900_MUX_UART2,
	S900_MUX_SPI3,
	S900_MUX_RESERVED,
	S900_MUX_RESERVED,
	S900_MUX_PWM2,
	S900_MUX_SENS0,
};

static unsigned int  owl_mfp0_15_13_eth_rxer_pads[] = {
	P_ETH_RXER,
};

static unsigned int  owl_mfp0_15_13_eth_rxer_funcs[] = {
	S900_MUX_ETH_RMII,
	S900_MUX_UART2,
	S900_MUX_SPI3,
	S900_MUX_RESERVED,
	S900_MUX_RESERVED,
	S900_MUX_PWM3,
	S900_MUX_SENS0,
};

/*
** mfp0_12_11
*/
static unsigned int  owl_mfp0_12_11_pads[] = {
	P_ETH_CRS_DV,
};

static unsigned int  owl_mfp0_12_11_funcs[] = {
	S900_MUX_ETH_RMII,
	S900_MUX_ETH_SMII,
	S900_MUX_SPI2,
	S900_MUX_UART4,
};

/*
** mfp0_10_8
*/
static unsigned int  owl_mfp0_10_8_eth_rxd1_pads[] = {
	P_ETH_RXD1,
};

static unsigned int  owl_mfp0_10_8_eth_rxd1_funcs[] = {
	S900_MUX_ETH_RMII,
	S900_MUX_UART2,
	S900_MUX_SPI3,
	S900_MUX_RESERVED,
	S900_MUX_UART5,
	S900_MUX_PWM0,
	S900_MUX_SENS0,
};

static unsigned int  owl_mfp0_10_8_eth_rxd0_pads[] = {
	P_ETH_RXD0,
};

static unsigned int  owl_mfp0_10_8_eth_rxd0_funcs[] = {
	S900_MUX_ETH_RMII,
	S900_MUX_UART2,
	S900_MUX_SPI3,
	S900_MUX_RESERVED,
	S900_MUX_UART5,
	S900_MUX_PWM1,
	S900_MUX_SENS0,
};

/*
** mfp0_7_6
*/
static unsigned int  owl_mfp0_7_6_pads[] = {
	P_ETH_REF_CLK,
};

static unsigned int  owl_mfp0_7_6_funcs[] = {
	S900_MUX_ETH_RMII,
	S900_MUX_UART4,
	S900_MUX_SPI2,
	S900_MUX_RESERVED,
};

/*
** mfp0_5
*/
static unsigned int  owl_mfp0_5_i2s_d0_pads[] = {
	P_I2S_D0,
};

static unsigned int  owl_mfp0_5_i2s_d0_funcs[] = {
	S900_MUX_I2S0,
	S900_MUX_PCM0,
};

static unsigned int  owl_mfp0_5_i2s_d1_pads[] = {
	P_I2S_D1,
};

static unsigned int  owl_mfp0_5_i2s_d1_funcs[] = {
	S900_MUX_I2S1,
	S900_MUX_PCM0,
};

/*
** mfp0_4_3
*/
static unsigned int  owl_mfp0_4_3_pads[] = {
	P_I2S_LRCLK0,
	P_I2S_MCLK0,
};

static unsigned int  owl_mfp0_4_3_funcs[] = {
	S900_MUX_I2S0,
	S900_MUX_PCM0,
	S900_MUX_PCM1,
	S900_MUX_RESERVED,
};

/*
** mfp0_2
*/
static unsigned int  owl_mfp0_2_i2s0_pads[] = {
	P_I2S_BCLK0,
};

static unsigned int  owl_mfp0_2_i2s0_funcs[] = {
	S900_MUX_I2S0,
	S900_MUX_PCM0,
};

static unsigned int  owl_mfp0_2_i2s1_pads[] = {
	P_I2S_BCLK1,
};

static unsigned int  owl_mfp0_2_i2s1_funcs[] = {
	S900_MUX_I2S1,
	S900_MUX_PCM0,
};

/*
** mfp0_1_0
*/
static unsigned int  owl_mfp0_1_0_pcm1_in_out_pads[] = {
	P_PCM1_IN,
	P_PCM1_OUT,
};

static unsigned int  owl_mfp0_1_0_pcm1_in_out_funcs[] = {
	S900_MUX_PCM1,
	S900_MUX_SPI1,
	S900_MUX_I2C3,
	S900_MUX_UART4,
};

static unsigned int  owl_mfp0_1_0_pcm1_clk_pads[] = {
	P_PCM1_CLK,
};

static unsigned int  owl_mfp0_1_0_pcm1_clk_funcs[] = {
	S900_MUX_PCM1,
	S900_MUX_SPI1,
	S900_MUX_PWM4,
	S900_MUX_UART4,
};

static unsigned int  owl_mfp0_1_0_pcm1_sync_pads[] = {
	P_PCM1_SYNC,
};

static unsigned int  owl_mfp0_1_0_pcm1_sync_funcs[] = {
	S900_MUX_PCM1,
	S900_MUX_SPI1,
	S900_MUX_PWM5,
	S900_MUX_UART4,
};


/*
** mfp1_31_29
*/
static unsigned int  owl_mfp1_31_29_eram_a5_pads[] = {
	P_ERAM_A5,
};

static unsigned int  owl_mfp1_31_29_eram_a5_funcs[] = {
	S900_MUX_UART4,
	S900_MUX_JTAG,
	S900_MUX_ERAM,
	S900_MUX_PWM0,
	S900_MUX_RESERVED,
	S900_MUX_SENS0,
};

static unsigned int  owl_mfp1_31_29_eram_a6_pads[] = {
	P_ERAM_A6,
};

static unsigned int  owl_mfp1_31_29_eram_a6_funcs[] = {
	S900_MUX_UART4,
	S900_MUX_JTAG,
	S900_MUX_ERAM,
	S900_MUX_PWM1,
	S900_MUX_RESERVED,
	S900_MUX_SENS0,
};

static unsigned int  owl_mfp1_31_29_eram_a7_pads[] = {
	P_ERAM_A7,
};

static unsigned int  owl_mfp1_31_29_eram_a7_funcs[] = {
	S900_MUX_RESERVED,
	S900_MUX_JTAG,
	S900_MUX_ERAM,
	S900_MUX_RESERVED,
	S900_MUX_RESERVED,
	S900_MUX_SENS0,
};

/*
** mfp1_28_26
*/
static unsigned int  owl_mfp1_28_26_eram_a8_pads[] = {
	P_ERAM_A8,
};

static unsigned int  owl_mfp1_28_26_eram_a8_funcs[] = {
	S900_MUX_RESERVED,
	S900_MUX_JTAG,
	S900_MUX_ERAM,
	S900_MUX_PWM1,
	S900_MUX_RESERVED,
	S900_MUX_SENS0,
};

static unsigned int  owl_mfp1_28_26_eram_a9_pads[] = {
	P_ERAM_A9,
};

static unsigned int  owl_mfp1_28_26_eram_a9_funcs[] = {
	S900_MUX_USB20,
	S900_MUX_UART5,
	S900_MUX_ERAM,
	S900_MUX_PWM2,
	S900_MUX_RESERVED,
	S900_MUX_SENS0,
};

static unsigned int  owl_mfp1_28_26_eram_a10_pads[] = {
	P_ERAM_A10,
};

static unsigned int  owl_mfp1_28_26_eram_a10_funcs[] = {
	S900_MUX_USB30,
	S900_MUX_JTAG,
	S900_MUX_ERAM,
	S900_MUX_PWM3,
	S900_MUX_RESERVED,
	S900_MUX_SENS0,
	S900_MUX_RESERVED,
	S900_MUX_RESERVED,
};

/*
** mfp1_25_23
*/
static unsigned int  owl_mfp1_25_23_pads[] = {
	P_ERAM_A11,
};

static unsigned int  owl_mfp1_25_23_funcs[] = {
	S900_MUX_RESERVED,
	S900_MUX_RESERVED,
	S900_MUX_ERAM,
	S900_MUX_PWM2,
	S900_MUX_UART5,
	S900_MUX_RESERVED,
	S900_MUX_SENS0,
	S900_MUX_RESERVED,
};

/*
** mfp1_22_lvds_o
*/
static unsigned int  owl_mfp1_22_lvds_oep_odn_pads[] = {
	P_LVDS_OEP,
	P_LVDS_OEN,
	P_LVDS_ODP,
	P_LVDS_ODN,
};

static unsigned int  owl_mfp1_22_lvds_oep_odn_funcs[] = {
	S900_MUX_LVDS,
	S900_MUX_UART2,
};

static unsigned int  owl_mfp1_22_lvds_ocp_obn_pads[] = {
	P_LVDS_OCP,
	P_LVDS_OCN,
	P_LVDS_OBP,
	P_LVDS_OBN,
};

static unsigned int  owl_mfp1_22_lvds_ocp_obn_funcs[] = {
	S900_MUX_LVDS,
	S900_MUX_PCM1,
};

static unsigned int  owl_mfp1_22_lvds_oap_oan_pads[] = {
	P_LVDS_OAP,
	P_LVDS_OAN,
};

static unsigned int  owl_mfp1_22_lvds_oap_oan_funcs[] = {
	S900_MUX_LVDS,
	S900_MUX_ERAM,
};

/*
** mfp1_21_lvds_e
*/
static unsigned int  owl_mfp1_21_lvds_e_pads[] = {
	P_LVDS_EEP,
	P_LVDS_EEN,
	P_LVDS_EDP,
	P_LVDS_EDN,
	P_LVDS_ECP,
	P_LVDS_ECN,
	P_LVDS_EBP,
	P_LVDS_EBN,
	P_LVDS_EAP,
	P_LVDS_EAN,
};

static unsigned int  owl_mfp1_21_lvds_e_funcs[] = {
	S900_MUX_LVDS,
	S900_MUX_ERAM,
};

/*
** mfp1_5_4
*/
static unsigned int  owl_mfp1_5_4_pads[] = {
	P_SPI0_SCLK,
	P_SPI0_MOSI,
};

static unsigned int  owl_mfp1_5_4_funcs[] = {
	S900_MUX_SPI0,
	S900_MUX_ERAM,
	S900_MUX_I2C3,
	S900_MUX_PCM0,
};

/*
** mfp1_3_1
*/
static unsigned int  owl_mfp1_3_1_spi0_ss_pads[] = {
	P_SPI0_SS,
};

static unsigned int  owl_mfp1_3_1_spi0_ss_funcs[] = {
	S900_MUX_SPI0,
	S900_MUX_ERAM,
	S900_MUX_I2S1,
	S900_MUX_PCM1,
	S900_MUX_PCM0,
	S900_MUX_PWM4,
};

static unsigned int  owl_mfp1_3_1_spi0_miso_pads[] = {
	P_SPI0_MISO,
};

static unsigned int  owl_mfp1_3_1_spi0_miso_funcs[] = {
	S900_MUX_SPI0,
	S900_MUX_ERAM,
	S900_MUX_I2S1,
	S900_MUX_PCM1,
	S900_MUX_PCM0,
	S900_MUX_PWM5,
};


/*
** mfp2_23
*/
static unsigned int  owl_mfp2_23_pads[] = {
	P_UART2_RTSB,
};

static unsigned int  owl_mfp2_23_funcs[] = {
	S900_MUX_UART2,
	S900_MUX_UART0,
};

/*
** mfp2_22
*/
static unsigned int  owl_mfp2_22_pads[] = {
	P_UART2_CTSB,
};

static unsigned int  owl_mfp2_22_funcs[] = {
	S900_MUX_UART2,
	S900_MUX_UART0,
};

/*
** mfp2_21
*/
static unsigned int  owl_mfp2_21_pads[] = {
	P_UART3_RTSB,
};

static unsigned int  owl_mfp2_21_funcs[] = {
	S900_MUX_UART3,
	S900_MUX_UART5,
};

/*
** mfp2_20
*/
static unsigned int  owl_mfp2_20_pads[] = {
	P_UART3_CTSB,
};

static unsigned int  owl_mfp2_20_funcs[] = {
	S900_MUX_UART3,
	S900_MUX_UART5,
};

/*
** mfp2_19_17
*/
static unsigned int  owl_mfp2_19_17_pads[] = {
	P_SD0_D0,
};

static unsigned int  owl_mfp2_19_17_funcs[] = {
	S900_MUX_SD0,
	S900_MUX_ERAM,
	S900_MUX_RESERVED,
	S900_MUX_JTAG,
	S900_MUX_UART2,
	S900_MUX_UART5,
	S900_MUX_GPU,
};

/*
** mfp2_16_14
*/
static unsigned int  owl_mfp2_16_14_pads[] = {
	P_SD0_D1,
};

static unsigned int  owl_mfp2_16_14_funcs[] = {
	S900_MUX_SD0,
	S900_MUX_ERAM,
	S900_MUX_GPU,
	S900_MUX_RESERVED,
	S900_MUX_UART2,
	S900_MUX_UART5,
};

/*
** mfp2_13_11
*/
static unsigned int  owl_mfp2_13_11_pads[] = {
	P_SD0_D2,
	P_SD0_D3,
};

static unsigned int  owl_mfp2_13_11_funcs[] = {
	S900_MUX_SD0,
	S900_MUX_ERAM,
	S900_MUX_RESERVED,
	S900_MUX_JTAG,
	S900_MUX_UART2,
	S900_MUX_UART1,
	S900_MUX_GPU,
};

/*
** mfp2_10_9
*/
static unsigned int  owl_mfp2_10_9_pads[] = {
	P_SD1_D0,
	P_SD1_D1,
	P_SD1_D2,
	P_SD1_D3,
};

static unsigned int  owl_mfp2_10_9_funcs[] = {
	S900_MUX_SD1,
	S900_MUX_ERAM,
};

/*
** mfp2_8_7
*/
static unsigned int  owl_mfp2_8_7_pads[] = {
	P_SD0_CMD,
};

static unsigned int  owl_mfp2_8_7_funcs[] = {
	S900_MUX_SD0,
	S900_MUX_ERAM,
	S900_MUX_GPU,
	S900_MUX_JTAG,
};

/*
** mfp2_6_5
*/
static unsigned int  owl_mfp2_6_5_pads[] = {
	P_SD0_CLK,
};

static unsigned int  owl_mfp2_6_5_funcs[] = {
	S900_MUX_SD0,
	S900_MUX_ERAM,
	S900_MUX_JTAG,
	S900_MUX_GPU,
};

/*
** mfp2_4_3
*/
static unsigned int  owl_mfp2_4_3_pads[] = {
	P_SD1_CMD,
	P_SD1_CLK,
};

static unsigned int  owl_mfp2_4_3_funcs[] = {
	S900_MUX_SD1,
	S900_MUX_ERAM,
};

/*
** mfp2_2_0
*/
static unsigned int  owl_mfp2_2_0_pads[] = {
	P_UART0_RX,
};

static unsigned int  owl_mfp2_2_0_funcs[] = {
	S900_MUX_UART0,
	S900_MUX_UART2,
	S900_MUX_SPI1,
	S900_MUX_I2C5,
	S900_MUX_PCM1,
	S900_MUX_I2S1,
};


/*
** mfp3_27
*/
static unsigned int  owl_mfp3_27_pads[] = {
	P_NAND0_D0,
	P_NAND0_D1,
	P_NAND0_D2,
	P_NAND0_D3,
	P_NAND0_D4,
	P_NAND0_D5,
	P_NAND0_D6,
	P_NAND0_D7,
	P_NAND0_DQSN,
	P_NAND0_CEB3,
};

static unsigned int  owl_mfp3_27_funcs[] = {
	S900_MUX_NAND0,
	S900_MUX_SD2,
};


/*
** mfp3_21_19
*/
static unsigned int  owl_mfp3_21_19_pads[] = {
	P_UART0_TX,
};

static unsigned int  owl_mfp3_21_19_funcs[] = {
	S900_MUX_UART0,
	S900_MUX_UART2,
	S900_MUX_SPI1,
	S900_MUX_I2C5,
	S900_MUX_SPDIF,
	S900_MUX_PCM1,
	S900_MUX_I2S1,
};

/*
** mfp3_18_16
*/
static unsigned int  owl_mfp3_18_16_pads[] = {
	P_I2C0_SCLK,
	P_I2C0_SDATA,
};

static unsigned int  owl_mfp3_18_16_funcs[] = {
	S900_MUX_I2C0,
	S900_MUX_UART2,
	S900_MUX_I2C1,
	S900_MUX_UART1,
	S900_MUX_SPI1,
};

/*
** mfp3_15
*/
static unsigned int  owl_mfp3_15_pads[] = {
	P_CSI0_CN,
	P_CSI0_CP,
};

static unsigned int  owl_mfp3_15_funcs[] = {
	S900_MUX_SENS0,
	S900_MUX_SENS0,
};

/*
** mfp3_14
*/
static unsigned int  owl_mfp3_14_pads[] = {
	P_CSI0_DN0,
	P_CSI0_DP0,
	P_CSI0_DN1,
	P_CSI0_DP1,
	P_CSI0_CN,
	P_CSI0_CP,
	P_CSI0_DP2,
	P_CSI0_DN2,
	P_CSI0_DN3,
	P_CSI0_DP3,
};

static unsigned int  owl_mfp3_14_funcs[] = {
	S900_MUX_MIPI_CSI0,
	S900_MUX_SENS0,
};

/*
** mfp3_13
*/
static unsigned int  owl_mfp3_13_pads[] = {
	P_CSI1_DN0,
	P_CSI1_DP0,
	P_CSI1_DN1,
	P_CSI1_DP1,
	P_CSI1_CN,
	P_CSI1_CP,
};

static unsigned int  owl_mfp3_13_funcs[] = {
	S900_MUX_MIPI_CSI1,
	S900_MUX_SENS0,
};


/*
** mfp3_12_dsi
*/
static unsigned int  owl_mfp3_12_dsi_dp3_dn1_pads[] = {
	P_DSI_DP3,
	P_DSI_DN2,
	P_DSI_DP1,
	P_DSI_DN1,
};

static unsigned int  owl_mfp3_12_dsi_dp3_dn1_funcs[] = {
	S900_MUX_MIPI_DSI,
	S900_MUX_UART2,
};

static unsigned int  owl_mfp3_12_dsi_cp_dn0_pads[] = {
	P_DSI_CP,
	P_DSI_CN,
	P_DSI_DP0,
	P_DSI_DN0,
};

static unsigned int  owl_mfp3_12_dsi_cp_dn0_funcs[] = {
	S900_MUX_MIPI_DSI,
	S900_MUX_PCM1,
};

static unsigned int  owl_mfp3_12_dsi_dp2_dn2_pads[] = {
	P_DSI_DP2,
	P_DSI_DN2,
};

static unsigned int  owl_mfp3_12_dsi_dp2_dn2_funcs[] = {
	S900_MUX_MIPI_DSI,
	S900_MUX_UART4,
};

/*
** mfp3_11
*/
static unsigned int  owl_mfp3_11_pads[] = {
	P_NAND1_D0,
	P_NAND1_D1,
	P_NAND1_D2,
	P_NAND1_D3,
	P_NAND1_D4,
	P_NAND1_D5,
	P_NAND1_D6,
	P_NAND1_D7,
	P_NAND1_DQSN,
	P_NAND1_CEB1,
};

static unsigned int  owl_mfp3_11_funcs[] = {
	S900_MUX_NAND1,
	S900_MUX_SD3,
};

/*
** mfp3_10
*/
static unsigned int  owl_mfp3_10_nand1_ceb3_pads[] = {
	P_NAND1_CEB3,
};

static unsigned int  owl_mfp3_10_nand1_ceb3_funcs[] = {
	S900_MUX_NAND1,
	S900_MUX_PWM0,
};

static unsigned int  owl_mfp3_10_nand1_ceb0_pads[] = {
	P_NAND1_CEB0,
};

static unsigned int  owl_mfp3_10_nand1_ceb0_funcs[] = {
	S900_MUX_NAND1,
	S900_MUX_PWM1,
};

/*
** mfp3_9
*/
static unsigned int  owl_mfp3_9_pads[] = {
	P_CSI1_DN0,
	P_CSI1_DP0,
};

static unsigned int  owl_mfp3_9_funcs[] = {
	S900_MUX_SENS0,
	S900_MUX_SENS0,
};

/*
** mfp3_8
*/
static unsigned int  owl_mfp3_8_pads[] = {
	P_UART4_RX,
	P_UART4_TX,
};

static unsigned int  owl_mfp3_8_funcs[] = {
	S900_MUX_UART4,
	S900_MUX_I2C4,
};


/*****End MFP group data****************************/

/*****PADDRV group data****************************/

/* PAD_DRV0 */

static unsigned int  owl_paddrv0_31_30_pads[] = {
	P_SGPIO3,
};

static unsigned int  owl_paddrv0_29_28_pads[] = {
	P_SGPIO2,
};

static unsigned int  owl_paddrv0_27_26_pads[] = {
	P_SGPIO1,
};

static unsigned int  owl_paddrv0_25_24_pads[] = {
	P_SGPIO0,
};

static unsigned int  owl_paddrv0_23_22_pads[] = {
	P_ETH_TXD0,
	P_ETH_TXD1,
};

static unsigned int  owl_paddrv0_21_20_pads[] = {
	P_ETH_TXEN,
	P_ETH_RXER,
};

static unsigned int  owl_paddrv0_19_18_pads[] = {
	P_ETH_CRS_DV,
};

static unsigned int  owl_paddrv0_17_16_pads[] = {
	P_ETH_RXD1,
	P_ETH_RXD0,
};

static unsigned int  owl_paddrv0_15_14_pads[] = {
	P_ETH_REF_CLK,
};

static unsigned int  owl_paddrv0_13_12_pads[] = {
	P_ETH_MDC,
	P_ETH_MDIO,
};

static unsigned int  owl_paddrv0_11_10_pads[] = {
	P_SIRQ0,
	P_SIRQ1,
};

static unsigned int  owl_paddrv0_9_8_pads[] = {
	P_SIRQ2,
};

static unsigned int  owl_paddrv0_7_6_pads[] = {
	P_I2S_D0,
	P_I2S_D1,
};

static unsigned int  owl_paddrv0_5_4_pads[] = {
	P_I2S_LRCLK0,
	P_I2S_MCLK0,
};

static unsigned int  owl_paddrv0_3_2_pads[] = {
	P_I2S_BCLK0,
	P_I2S_BCLK1,
	P_I2S_LRCLK1,
	P_I2S_MCLK1,
};

static unsigned int  owl_paddrv0_1_0_pads[] = {
	P_PCM1_IN,
	P_PCM1_CLK,
	P_PCM1_SYNC,
	P_PCM1_OUT,
};

/* PAD_DRV1 */

static unsigned int  owl_paddrv1_29_28_pads[] = {
	P_LVDS_OAP,
	P_LVDS_OAN,
};

static unsigned int  owl_paddrv1_27_26_pads[] = {
	P_LVDS_OEP,
	P_LVDS_OEN,
	P_LVDS_ODP,
	P_LVDS_ODN,
};

static unsigned int  owl_paddrv1_25_24_pads[] = {
	P_LVDS_OCP,
	P_LVDS_OCN,
	P_LVDS_OBP,
	P_LVDS_OBN,
};

static unsigned int  owl_paddrv1_23_22_pads[] = {
	P_LVDS_EEP,
	P_LVDS_EEN,
	P_LVDS_EDP,
	P_LVDS_EDN,
	P_LVDS_ECP,
	P_LVDS_ECN,
	P_LVDS_EBP,
	P_LVDS_EBN,
};

static unsigned int  owl_paddrv1_21_20_pads[] = {
	P_SD0_D3,
	P_SD0_D2,
	P_SD0_D1,
	P_SD0_D0,
};
static unsigned int  owl_paddrv1_19_18_pads[] = {
	P_SD1_D3,
	P_SD1_D2,
	P_SD1_D1,
	P_SD1_D0,
};

static unsigned int  owl_paddrv1_17_16_pads[] = {
	P_SD0_CLK,
	P_SD0_CMD,
	P_SD1_CLK,
	P_SD1_CMD,
};

static unsigned int  owl_paddrv1_15_14_pads[] = {
	P_SPI0_SCLK,
	P_SPI0_MOSI,
};

static unsigned int  owl_paddrv1_13_12_pads[] = {
	P_SPI0_SS,
	P_SPI0_MISO,
};

static unsigned int  owl_paddrv1_11_10_pads[] = {
	P_UART0_RX,
	P_UART0_TX,
};

static unsigned int  owl_paddrv1_9_8_pads[] = {
	P_UART4_RX,
	P_UART4_TX,
};

static unsigned int  owl_paddrv1_7_6_pads[] = {
	P_UART2_RX,
	P_UART2_TX,
	P_UART2_RTSB,
	P_UART2_CTSB,
};

static unsigned int  owl_paddrv1_5_4_pads[] = {
	P_UART3_RX,
	P_UART3_TX,
	P_UART3_RTSB,
	P_UART3_CTSB,
};

/* PAD_DRV2 */

static unsigned int  owl_paddrv2_31_30_pads[] = {
	P_I2C0_SCLK,
	P_I2C0_SDATA,
};

static unsigned int  owl_paddrv2_29_28_pads[] = {
	P_I2C1_SCLK,
	P_I2C1_SDATA,
};

static unsigned int  owl_paddrv2_27_26_pads[] = {
	P_I2C2_SCLK,
	P_I2C2_SDATA,
};

static unsigned int  owl_paddrv2_21_20_pads[] = {
	P_SENSOR0_PCLK,
	P_SENSOR0_CKOUT,
};

/*****End PADDRV group data****************************/

#define MUX_PG(group_name, mfpctl_regn, mfpctl_sft, mfpctl_w)		\
	{								\
		.name = #group_name,					\
		.pads = owl_##group_name##_pads,			\
		.padcnt = ARRAY_SIZE(owl_##group_name##_pads),		\
		.funcs = owl_##group_name##_funcs,			\
		.nfuncs = ARRAY_SIZE(owl_##group_name##_funcs),		\
		.mfpctl_reg = MFP_CTL##mfpctl_regn,			\
		.mfpctl_shift = mfpctl_sft,				\
		.mfpctl_width = mfpctl_w,				\
		.paddrv_reg = -1,					\
	}

#define MUX_PG_DUMMY(group_name)					\
	{								\
		.name = #group_name,					\
		.pads = owl_##group_name##_pads,			\
		.padcnt = ARRAY_SIZE(owl_##group_name##_pads),		\
		.funcs = owl_##group_name##_funcs,			\
		.nfuncs = ARRAY_SIZE(owl_##group_name##_funcs),		\
		.mfpctl_reg = -1,					\
		.paddrv_reg = -1,					\
	}

#define PADDRV_PG(group_name, paddrv_regn, paddrv_sft)			\
	{								\
		.name = #group_name,					\
		.pads = owl_##group_name##_pads,			\
		.padcnt = ARRAY_SIZE(owl_##group_name##_pads),		\
		.paddrv_reg = PAD_DRV##paddrv_regn,			\
		.paddrv_shift = paddrv_sft,				\
		.paddrv_width = 2,					\
		.mfpctl_reg = -1,					\
	}

/*
**
** all pinctrl groups of S900 board
**
*/
const struct owl_group s900_groups[] = {
	MUX_PG(mfp0_22, 0, 22, 1),
	MUX_PG(mfp0_21_20_eth_mdc, 0, 20, 2),
	MUX_PG(mfp0_21_20_eth_mdio, 0, 20, 2),
	MUX_PG(mfp0_19_sirq0, 0, 19, 1),
	MUX_PG(mfp0_19_sirq1, 0, 19, 1),
	MUX_PG(mfp0_18_16_eth_txd0, 0, 16, 3),
	MUX_PG(mfp0_18_16_eth_txd1, 0, 16, 3),
	MUX_PG(mfp0_15_13_eth_txen, 0, 13, 3),
	MUX_PG(mfp0_15_13_eth_rxer, 0, 13, 3),
	MUX_PG(mfp0_12_11, 0, 11, 2),
	MUX_PG(mfp0_10_8_eth_rxd1, 0, 8, 3),
	MUX_PG(mfp0_10_8_eth_rxd0, 0, 8, 3),
	MUX_PG(mfp0_7_6, 0, 6, 2),
	MUX_PG(mfp0_5_i2s_d0, 0, 5, 1),
	MUX_PG(mfp0_5_i2s_d1, 0, 5, 1),
	MUX_PG(mfp0_4_3, 0, 3, 2),
	MUX_PG(mfp0_2_i2s0, 0, 2, 1),
	MUX_PG(mfp0_2_i2s1, 0, 2, 1),
	MUX_PG(mfp0_1_0_pcm1_in_out, 0, 0, 2),
	MUX_PG(mfp0_1_0_pcm1_clk, 0, 0, 2),
	MUX_PG(mfp0_1_0_pcm1_sync, 0, 0, 2),
	MUX_PG(mfp1_31_29_eram_a5, 1, 29, 3),
	MUX_PG(mfp1_31_29_eram_a6, 1, 29, 3),
	MUX_PG(mfp1_31_29_eram_a7, 1, 29, 3),
	MUX_PG(mfp1_28_26_eram_a8, 1, 26, 3),
	MUX_PG(mfp1_28_26_eram_a9, 1, 26, 3),
	MUX_PG(mfp1_28_26_eram_a10, 1, 26, 3),
	MUX_PG(mfp1_25_23, 1, 23, 3),
	MUX_PG(mfp1_22_lvds_oep_odn, 1, 22, 1),
	MUX_PG(mfp1_22_lvds_ocp_obn, 1, 22, 1),
	MUX_PG(mfp1_22_lvds_oap_oan, 1, 22, 1),
	MUX_PG(mfp1_21_lvds_e, 1, 21, 1),
	MUX_PG(mfp1_5_4, 1, 4, 2),
	MUX_PG(mfp1_3_1_spi0_ss, 1, 1, 3),
	MUX_PG(mfp1_3_1_spi0_miso, 1, 1, 3),
	MUX_PG(mfp2_23, 2, 23, 1),
	MUX_PG(mfp2_22, 2, 22, 1),
	MUX_PG(mfp2_21, 2, 21, 1),
	MUX_PG(mfp2_20, 2, 20, 1),
	MUX_PG(mfp2_19_17, 2, 17, 3),
	MUX_PG(mfp2_16_14, 2, 14, 3),
	MUX_PG(mfp2_13_11, 2, 11, 3),
	MUX_PG(mfp2_10_9, 2, 9, 2),
	MUX_PG(mfp2_8_7, 2, 7, 2),
	MUX_PG(mfp2_6_5, 2, 5, 2),
	MUX_PG(mfp2_4_3, 2, 3, 2),
	MUX_PG(mfp2_2_0, 2, 0, 3),
	MUX_PG(mfp3_27, 3, 27, 1),
	MUX_PG(mfp3_21_19, 3, 19, 3),
	MUX_PG(mfp3_18_16, 3, 16, 3),
	MUX_PG(mfp3_15, 3, 15, 1),
	MUX_PG(mfp3_14, 3, 14, 1),
	MUX_PG(mfp3_13, 3, 13, 1),
	MUX_PG(mfp3_12_dsi_dp3_dn1, 3, 12, 1),
	MUX_PG(mfp3_12_dsi_cp_dn0, 3, 12, 1),
	MUX_PG(mfp3_12_dsi_dp2_dn2, 3, 12, 1),
	MUX_PG(mfp3_11, 3, 11, 1),
	MUX_PG(mfp3_10_nand1_ceb3, 3, 10, 1),
	MUX_PG(mfp3_10_nand1_ceb0, 3, 10, 1),
	MUX_PG(mfp3_9, 3, 9, 1),
	MUX_PG(mfp3_8, 3, 8, 1),

	PADDRV_PG(paddrv0_31_30, 0, 30),
	PADDRV_PG(paddrv0_29_28, 0, 28),
	PADDRV_PG(paddrv0_27_26, 0, 26),
	PADDRV_PG(paddrv0_25_24, 0, 24),
	PADDRV_PG(paddrv0_23_22, 0, 22),
	PADDRV_PG(paddrv0_21_20, 0, 20),
	PADDRV_PG(paddrv0_19_18, 0, 18),
	PADDRV_PG(paddrv0_17_16, 0, 16),
	PADDRV_PG(paddrv0_15_14, 0, 14),
	PADDRV_PG(paddrv0_13_12, 0, 12),
	PADDRV_PG(paddrv0_11_10, 0, 10),
	PADDRV_PG(paddrv0_9_8, 0, 8),
	PADDRV_PG(paddrv0_7_6, 0, 6),
	PADDRV_PG(paddrv0_5_4, 0, 4),
	PADDRV_PG(paddrv0_3_2, 0, 2),
	PADDRV_PG(paddrv0_1_0, 0, 0),
	PADDRV_PG(paddrv1_29_28, 1, 28),
	PADDRV_PG(paddrv1_27_26, 1, 26),
	PADDRV_PG(paddrv1_25_24, 1, 24),
	PADDRV_PG(paddrv1_23_22, 1, 22),
	PADDRV_PG(paddrv1_21_20, 1, 20),
	PADDRV_PG(paddrv1_19_18, 1, 18),
	PADDRV_PG(paddrv1_17_16, 1, 16),
	PADDRV_PG(paddrv1_15_14, 1, 14),
	PADDRV_PG(paddrv1_13_12, 1, 12),
	PADDRV_PG(paddrv1_11_10, 1, 10),
	PADDRV_PG(paddrv1_9_8, 1, 8),
	PADDRV_PG(paddrv1_7_6, 1, 6),
	PADDRV_PG(paddrv1_5_4, 1, 4),
	PADDRV_PG(paddrv2_31_30, 2, 30),
	PADDRV_PG(paddrv2_29_28, 2, 28),
	PADDRV_PG(paddrv2_27_26, 2, 26),
	PADDRV_PG(paddrv2_21_20, 2, 20),
};

int s900_num_groups = ARRAY_SIZE(s900_groups);

static const char * const eram_groups[] = {
	"mfp0_22",
	"mfp1_31_29_eram_a5",
	"mfp1_31_29_eram_a6",
	"mfp1_31_29_eram_a7",
	"mfp1_28_26_eram_a8",
	"mfp1_28_26_eram_a9",
	"mfp1_28_26_eram_a10",
	"mfp1_25_23",
	"mfp1_22_lvds_oap_oan",
	"mfp1_21_lvds_e",
	"mfp1_5_4",
	"mfp1_3_1_spi0_ss",
	"mfp1_3_1_spi0_miso",
	"mfp2_19_17",
	"mfp2_16_14",
	"mfp2_13_11",
	"mfp2_10_9",
	"mfp2_8_7",
	"mfp2_6_5",
	"mfp2_4_3",
};

static const char * const eth_rmii_groups[] = {
	"mfp0_21_20_eth_mdc",
	"mfp0_21_20_eth_mdio",
	"mfp0_18_16_eth_txd0",
	"mfp0_18_16_eth_txd1",
	"mfp0_15_13_eth_txen",
	"mfp0_15_13_eth_rxer",
	"mfp0_12_11",
	"mfp0_10_8_eth_rxd1",
	"mfp0_10_8_eth_rxd0",
	"mfp0_7_6",
	"eth_smi_dummy",
};

static const char * const eth_smii_groups[] = {
	"mfp0_18_16_eth_txd0",
	"mfp0_18_16_eth_txd1",
	"mfp0_12_11",
	"eth_smi_dummy",
};

static const char * const spi0_groups[] = {
	"mfp1_5_4",
	"mfp1_3_1_spi0_ss",
	"mfp1_3_1_spi0_miso",
	"mfp1_5_4",
	"mfp1_3_1_spi0_ss",
	"mfp1_3_1_spi0_miso",
};

static const char * const spi1_groups[] = {
	"mfp0_1_0_pcm1_in_out",
	"mfp0_1_0_pcm1_clk",
	"mfp0_1_0_pcm1_sync",
	"mfp2_2_0",
	"mfp3_21_19",
	"mfp3_18_16",
};

static const char * const spi2_groups[] = {
	"mfp0_18_16_eth_txd0",
	"mfp0_18_16_eth_txd1",
	"mfp0_12_11",
	"mfp0_7_6",
};

static const char * const spi3_groups[] = {
	"mfp0_15_13_eth_txen",
	"mfp0_15_13_eth_rxer",
};

static const char * const sens0_groups[] = {
	"mfp0_18_16_eth_txd0",
	"mfp0_18_16_eth_txd1",
	"mfp0_15_13_eth_txen",
	"mfp0_15_13_eth_rxer",
	"mfp0_10_8_eth_rxd1",
	"mfp0_10_8_eth_rxd0",
	"mfp1_31_29_eram_a5",
	"mfp1_31_29_eram_a6",
	"mfp1_31_29_eram_a7",
	"mfp1_28_26_eram_a8",
	"mfp1_28_26_eram_a9",
	"mfp3_15_pads",
	"mfp3_14_pads",
	"mfp3_13_pads",
	"mfp3_9_pads",
};

static const char * const uart0_groups[] = {
	"mfp2_23",
	"mfp2_22",
	"mfp2_2_0",
	"mfp3_21_19",
};

static const char * const uart1_groups[] = {
	"mfp2_13_11",
	"mfp3_18_16",
};

static const char * const uart2_groups[] = {
	"mfp0_21_20_eth_mdc",
	"mfp0_21_20_eth_mdio",
	"mfp0_15_13_eth_txen",
	"mfp0_15_13_eth_rxer",
	"mfp0_10_8_eth_rxd1",
	"mfp0_10_8_eth_rxd0",
	"mfp1_22_lvds_oep_odn",
	"mfp2_23",
	"mfp2_22",
	"mfp2_19_17",
	"mfp2_16_14",
	"mfp2_13_11",
	"mfp2_2_0",
	"mfp3_21_19_pads",
	"mfp3_18_16_pads",
	"mfp3_12_dsi_dp3_dn1",
	"uart2_dummy"
};

static const char * const uart3_groups[] = {
	"mfp2_21",
	"mfp2_20",
	"uart3_dummy"
};

static const char * const uart4_groups[] = {
	"mfp0_22",
	"mfp0_12_11",
	"mfp0_7_6",
	"mfp0_1_0_pcm1_in_out",
	"mfp0_1_0_pcm1_clk",
	"mfp0_1_0_pcm1_sync",
	"mfp1_31_29_eram_a5",
	"mfp1_31_29_eram_a6",
	"mfp3_12_dsi_dp2_dn2",
	"mfp3_8_pads",
	"uart4_dummy"
};

static const char * const uart5_groups[] = {
	"mfp0_10_8_eth_rxd1",
	"mfp0_10_8_eth_rxd0",
	"mfp1_28_26_eram_a9",
	"mfp1_25_23",
	"mfp2_21",
	"mfp2_20",
	"mfp2_19_17",
	"mfp2_16_14",
};

static const char * const uart6_groups[] = {
	"mfp0_18_16_eth_txd0",
	"mfp0_18_16_eth_txd1",
};

static const char * const i2s0_groups[] = {
	"mfp0_5_i2s_d0",
	"mfp0_4_3",
	"mfp0_2_i2s0",
	"i2s0_dummy",
};

static const char * const i2s1_groups[] = {
	"mfp0_5_i2s_d1",
	"mfp0_2_i2s1",
	"mfp1_3_1_spi0_ss"
	"mfp1_3_1_spi0_miso"
	"mfp2_2_0",
	"mfp3_21_19",
	"i2s1_dummy",
};

static const char * const pcm0_groups[] = {
	"_mfp0_5_i2s_d0",
	"_mfp0_5_i2s_d1",
	"_mfp0_4_3",
	"_mfp0_2_i2s0",
	"_mfp0_2_i2s1",
	"_mfp1_5_4",
	"_mfp1_3_1_spi0_ss",
	"_mfp1_3_1_spi0_miso",
};

static const char * const pcm1_groups[] = {
	"mfp0_4_3",
	"mfp0_1_0_pcm1_in_out",
	"mfp0_1_0_pcm1_clk",
	"mfp0_1_0_pcm1_sync",
	"mfp1_22_lvds_oep_odn",
	"mfp1_3_1_spi0_ss",
	"mfp1_3_1_spi0_miso",
	"mfp2_2_0",
	"mfp3_21_19",
	"mfp3_12_dsi_cp_dn0",
	"pcm1_dummy",
};

static const char * const jtag_groups[] = {
	"mfp1_31_29_eram_a5",
	"mfp1_31_29_eram_a6",
	"mfp1_31_29_eram_a7",
	"mfp1_28_26_eram_a8",
	"mfp1_28_26_eram_a10",
	"mfp1_28_26_eram_a10",
	"mfp2_13_11",
	"mfp2_8_7",
	"mfp2_6_5",
};

static const char * const pwm0_groups[] = {
	"owl_mfp0_19_sirq0",
	"owl_mfp0_18_16_eth_txd0",
	"owl_mfp0_10_8_eth_rxd1",
	"owl_mfp1_31_29_eram_a5",
	"owl_mfp3_10_nand1_ceb3",
};

static const char * const pwm1_groups[] = {
	"mfp0_19_sirq1",
	"mfp0_18_16_eth_txd1",
	"mfp0_10_8_eth_rxd0",
	"mfp1_31_29_eram_a6",
	"mfp1_28_26_eram_a8",
	"mfp3_10_nand1_ceb0",
};

static const char * const pwm2_groups[] = {
	"mfp0_21_20_eth_mdc",
	"mfp0_15_13_eth_txen",
	"mfp1_28_26_eram_a9",
	"mfp1_25_23",
};

static const char * const pwm3_groups[] = {
	"mfp0_21_20_eth_mdio",
	"mfp0_15_13_eth_rxer",
	"mfp1_28_26_eram_a10",
};

static const char * const pwm4_groups[] = {
	"mfp0_1_0_pcm1_clk",
	"mfp1_3_1_spi0_ss",
};

static const char * const pwm5_groups[] = {
	"mfp0_1_0_pcm1_sync",
	"mfp1_3_1_spi0_miso",
};

static const char * const sd0_groups[] = {
	"mfp2_19_17",
	"mfp2_16_14",
	"mfp2_13_11",
	"mfp2_8_7",
	"mfp2_6_5",
};

static const char * const sd1_groups[] = {
	"mfp2_10_9",
	"mfp2_4_3",
	"sd1_dummy",
};

static const char * const sd2_groups[] = {
	"mfp3_27",
};

static const char * const sd3_groups[] = {
	"mfp3_11",
};

static const char * const i2c0_groups[] = {
	"mfp3_18_16",
};

static const char * const i2c1_groups[] = {
	"mfp3_18_16",
	"i2c1_dummy"
};

static const char * const i2c2_groups[] = {
	"i2c2_dummy"
};

static const char * const i2c3_groups[] = {
	"mfp0_1_0",
	"mfp1_5_4",
};

static const char * const i2c4_groups[] = {
	"mfp3_8",
};

static const char * const i2c5_groups[] = {
	"mfp2_2_0",
	"mfp3_21_19",
};


static const char * const lvds_groups[] = {
	"mfp1_22_lvds_oep_odn",
	"mfp1_22_lvds_ocp_obn",
	"mfp1_22_lvds_oap_oan",
	"mfp1_21_lvds_e",
};

static const char * const usb20_groups[] = {
	"mfp1_28_26_eram_a9",
};

static const char * const usb30_groups[] = {
	"mfp1_28_26_eram_a10",
};

static const char * const gpu_groups[] = {
	"mfp2_19_17",
	"mfp2_16_14",
	"mfp2_13_11",
	"mfp2_8_7",
	"mfp2_6_5",
};

static const char * const mipi_csi0_groups[] = {
	"mfp3_14",
};

static const char * const mipi_csi1_groups[] = {
	"mfp3_13",
};

static const char * const mipi_dsi_groups[] = {
	"mfp3_12_dsi_dp3_dn1",
	"mfp3_12_dsi_cp_dn0",
	"mfp3_12_dsi_dp2_dn2",
	"mipi_dsi_dummy",
};

static const char * const nand0_groups[] = {
	"mfp3_27",
	"nand0_dummy",
};

static const char * const nand1_groups[] = {
	"mfp3_11",
	"mfp3_10_nand1_ceb3",
	"mfp3_10_nand1_ceb0",
	"nand1_dummy",
};

static const char * const spdif_groups[] = {
	"mfp3_21_19",
};

static const char * const lens_groups[] = {
	"mfp3_11_10",
	"mfp3_9_7",
	"mfp3_6_4",
	"mfp3_3_2",
	"mfp3_1_0",
};

static const char * const sirq0_groups[] = {
	"mfp0_19_sirq0",
	"sirq0_dummy",
};

static const char * const sirq1_groups[] = {
	"mfp0_19_sirq1",
	"sirq1_dummy",
};

static const char * const sirq2_groups[] = {
	"sirq2_dummy",
};

#define FUNCTION(fname)					\
	{						\
		.name = #fname,				\
		.groups = fname##_groups,		\
		.ngroups = ARRAY_SIZE(fname##_groups),	\
	}

const struct owl_pinmux_func s900_functions[] = {
	[S900_MUX_ERAM] = FUNCTION(eram),
	[S900_MUX_ETH_RMII] = FUNCTION(eth_rmii),
	[S900_MUX_ETH_SMII] = FUNCTION(eth_smii),
	[S900_MUX_SPI0] = FUNCTION(spi0),
	[S900_MUX_SPI1] = FUNCTION(spi1),
	[S900_MUX_SPI2] = FUNCTION(spi2),
	[S900_MUX_SPI3] = FUNCTION(spi3),
	[S900_MUX_SENS0] = FUNCTION(sens0),
	[S900_MUX_UART0] = FUNCTION(uart0),
	[S900_MUX_UART1] = FUNCTION(uart1),
	[S900_MUX_UART2] = FUNCTION(uart2),
	[S900_MUX_UART3] = FUNCTION(uart3),
	[S900_MUX_UART4] = FUNCTION(uart4),
	[S900_MUX_UART5] = FUNCTION(uart5),
	[S900_MUX_UART6] = FUNCTION(uart6),
	[S900_MUX_I2S0] = FUNCTION(i2s0),
	[S900_MUX_I2S1] = FUNCTION(i2s1),
	[S900_MUX_PCM0] = FUNCTION(pcm0),
	[S900_MUX_PCM1] = FUNCTION(pcm1),
	[S900_MUX_JTAG] = FUNCTION(jtag),
	[S900_MUX_PWM0] = FUNCTION(pwm0),
	[S900_MUX_PWM1] = FUNCTION(pwm1),
	[S900_MUX_PWM2] = FUNCTION(pwm2),
	[S900_MUX_PWM3] = FUNCTION(pwm3),
	[S900_MUX_PWM4] = FUNCTION(pwm4),
	[S900_MUX_PWM5] = FUNCTION(pwm5),
	[S900_MUX_SD0] = FUNCTION(sd0),
	[S900_MUX_SD1] = FUNCTION(sd1),
	[S900_MUX_SD2] = FUNCTION(sd2),
	[S900_MUX_SD3] = FUNCTION(sd3),
	[S900_MUX_I2C0] = FUNCTION(i2c0),
	[S900_MUX_I2C1] = FUNCTION(i2c1),
	[S900_MUX_I2C2] = FUNCTION(i2c2),
	[S900_MUX_I2C3] = FUNCTION(i2c3),
	[S900_MUX_I2C4] = FUNCTION(i2c4),
	[S900_MUX_I2C5] = FUNCTION(i2c5),
	[S900_MUX_LVDS] = FUNCTION(lvds),
	[S900_MUX_USB30] = FUNCTION(usb30),
	[S900_MUX_USB20] = FUNCTION(usb20),
	[S900_MUX_GPU] = FUNCTION(gpu),
	[S900_MUX_MIPI_CSI0] = FUNCTION(mipi_csi0),
	[S900_MUX_MIPI_CSI1] = FUNCTION(mipi_csi1),
	[S900_MUX_MIPI_DSI] = FUNCTION(mipi_dsi),
	[S900_MUX_NAND0] = FUNCTION(nand0),
	[S900_MUX_NAND1] = FUNCTION(nand1),
	[S900_MUX_SPDIF] = FUNCTION(spdif),
	[S900_MUX_SIRQ0] = FUNCTION(sirq0),
	[S900_MUX_SIRQ1] = FUNCTION(sirq1),
	[S900_MUX_SIRQ2] = FUNCTION(sirq2),
};

int s900_num_functions = ARRAY_SIZE(s900_functions);

#define SCHIMMT_CONF(pad_name, reg_n, sft)				\
	{								\
		.schimtt_funcs = pad_name##_schimtt_funcs,		\
		.num_schimtt_funcs =					\
			ARRAY_SIZE(pad_name##_schimtt_funcs),		\
		.reg = PAD_ST##reg_n,					\
		.shift = sft,						\
	}

#define PAD_SCHIMMT_CONF(pad_name, reg_n, sft)				\
	struct owl_pinconf_schimtt pad_name##_schimmt_conf = {		\
		.schimtt_funcs = pad_name##_schimtt_funcs,		\
		.num_schimtt_funcs =					\
			ARRAY_SIZE(pad_name##_schimtt_funcs),		\
		.reg = PAD_ST##reg_n,					\
		.shift = sft,						\
	}

/*
 * TODO: Pad schemit config is not implemented yet
 */

/* PAD PULL UP/DOWN CONFIGURES */
#define PULL_CONF(reg_n, sft, w, pup, pdn)				\
	{								\
		.reg = PAD_PULLCTL##reg_n,				\
		.shift = sft,						\
		.width = w,						\
		.pullup = pup,						\
		.pulldown = pdn,					\
	}

#define PAD_PULL_CONF(pad_name, reg_num,				\
			shift, width, pull_up, pull_down)		\
	struct owl_pinconf_reg_pull pad_name##_pull_conf		\
		= PULL_CONF(reg_num, shift, width, pull_up, pull_down)

/* PAD_PULLCTL0 */
static PAD_PULL_CONF(P_ETH_RXER, 0, 18, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SIRQ0, 0, 16, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SIRQ1, 0, 14, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SIRQ2, 0, 12, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_I2C0_SDATA, 0, 10, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_I2C0_SCLK, 0, 8, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_ERAM_A5, 0, 6, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_ERAM_A6, 0, 4, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_ERAM_A7, 0, 2, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_ERAM_A10, 0, 0, 2, 0x1, 0x2);

/*PAD_PULLCTL1*/
static PAD_PULL_CONF(P_PCM1_IN, 1, 30, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_PCM1_OUT, 1, 28, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SD0_D0, 1, 26, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SD0_D1, 1, 24, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SD0_D2, 1, 22, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SD0_D3, 1, 20, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SD0_CMD, 1, 18, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SD0_CLK, 1, 16, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SD1_CMD, 1, 14, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SD1_D0, 1, 12, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SD1_D1, 1, 10, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SD1_D2, 1, 8, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SD1_D3, 1, 6, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_UART0_RX, 1, 4, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_UART0_TX, 1, 2, 2, 0x1, 0x2);

/*PAD_PULLCTL2*/
static PAD_PULL_CONF(P_I2C2_SDATA, 2, 26, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_I2C2_SCLK, 2, 24, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SPI0_SCLK, 2, 22, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SPI0_MOSI, 2, 20, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_I2C1_SDATA, 2, 18, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_I2C1_SCLK, 2, 16, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_NAND0_D0, 2, 15, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND0_D1, 2, 15, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND0_D2, 2, 15, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND0_D3, 2, 15, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND0_D4, 2, 15, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND0_D5, 2, 15, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND0_D6, 2, 15, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND0_D7, 2, 15, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND0_DQSN, 2, 14, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND0_DQS, 2, 13, 1, 0x0, 0x1);
static PAD_PULL_CONF(P_NAND1_D0, 2, 12, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND1_D1, 2, 12, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND1_D2, 2, 12, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND1_D3, 2, 12, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND1_D4, 2, 12, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND1_D5, 2, 12, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND1_D6, 2, 12, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND1_D7, 2, 12, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND1_DQSN, 2, 11, 1, 0x1, 0x0);
static PAD_PULL_CONF(P_NAND1_DQS, 2, 10, 1, 0x0, 0x1);
static PAD_PULL_CONF(P_SGPIO2, 2, 8, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_SGPIO3, 2, 6, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_UART4_RX, 2, 4, 2, 0x1, 0x2);
static PAD_PULL_CONF(P_UART4_TX, 2, 2, 2, 0x1, 0x2);

/********PAD INFOS*****************************/
#define PAD_TO_GPIO(padnum)						\
		((padnum < NUM_GPIOS) ? padnum : -1)

#define PAD_INFO(name)							\
	{								\
		.pad = name,						\
		.gpio = PAD_TO_GPIO(name),				\
		.schimtt = NULL,					\
		.pull = NULL,						\
	}

#define PAD_INFO_SCHIMTT(name)						\
	{								\
		.pad = name,						\
		.gpio = PAD_TO_GPIO(name),				\
		.schimtt = &name##_schimmt_conf,			\
		.pull = NULL,						\
	}

#define PAD_INFO_PULL(name)						\
	{								\
		.pad = name,						\
		.gpio = PAD_TO_GPIO(name),				\
		.schimtt = NULL,					\
		.pull = &name##_pull_conf,				\
	}

#define PAD_INFO_SCHIMTT_PULL(name)					\
	{								\
		.pad = name,						\
		.gpio = PAD_TO_GPIO(name),				\
		.schimtt = &name##_schimmt_conf,			\
		.pull = &name##_pull_conf,				\
	}

/* Pad info table for the pinmux subsystem */
struct owl_pinconf_pad_info s900_pad_tab[NUM_PADS] = {
	[P_ETH_TXD0] = PAD_INFO(P_ETH_TXD0),
	[P_ETH_TXD1] = PAD_INFO(P_ETH_TXD1),
	[P_ETH_TXEN] = PAD_INFO(P_ETH_TXEN),
	[P_ETH_RXER] = PAD_INFO_PULL(P_ETH_RXER),
	[P_ETH_CRS_DV] = PAD_INFO(P_ETH_CRS_DV),
	[P_ETH_RXD1] = PAD_INFO(P_ETH_RXD1),
	[P_ETH_RXD0] = PAD_INFO(P_ETH_RXD0),
	[P_ETH_REF_CLK] = PAD_INFO(P_ETH_REF_CLK),
	[P_ETH_MDC] = PAD_INFO(P_ETH_MDC),
	[P_ETH_MDIO] = PAD_INFO(P_ETH_MDIO),
	[P_SIRQ0] = PAD_INFO_PULL(P_SIRQ0),
	[P_SIRQ1] = PAD_INFO_PULL(P_SIRQ1),
	[P_SIRQ2] = PAD_INFO_PULL(P_SIRQ2),
	[P_I2S_D0] = PAD_INFO(P_I2S_D0),
	[P_I2S_BCLK0] = PAD_INFO(P_I2S_BCLK0),
	[P_I2S_LRCLK0] = PAD_INFO(P_I2S_LRCLK0),
	[P_I2S_MCLK0] = PAD_INFO(P_I2S_MCLK0),
	[P_I2S_D1] = PAD_INFO(P_I2S_D1),
	[P_I2S_BCLK1] = PAD_INFO(P_I2S_BCLK1),
	[P_I2S_LRCLK1] = PAD_INFO(P_I2S_LRCLK1),
	[P_I2S_MCLK1] = PAD_INFO(P_I2S_MCLK1),
	[P_PCM1_IN] = PAD_INFO_PULL(P_PCM1_IN),
	[P_PCM1_CLK] = PAD_INFO(P_PCM1_CLK),
	[P_PCM1_SYNC] = PAD_INFO(P_PCM1_SYNC),
	[P_PCM1_OUT] = PAD_INFO_PULL(P_PCM1_OUT),
	[P_ERAM_A5] = PAD_INFO_PULL(P_ERAM_A5),
	[P_ERAM_A6] = PAD_INFO_PULL(P_ERAM_A6),
	[P_ERAM_A7] = PAD_INFO_PULL(P_ERAM_A7),
	[P_ERAM_A8] = PAD_INFO(P_ERAM_A8),
	[P_ERAM_A9] = PAD_INFO(P_ERAM_A9),
	[P_ERAM_A10] = PAD_INFO_PULL(P_ERAM_A10),
	[P_ERAM_A11] = PAD_INFO(P_ERAM_A11),
	[P_LVDS_OEP] = PAD_INFO(P_LVDS_OEP),
	[P_LVDS_OEN] = PAD_INFO(P_LVDS_OEN),
	[P_LVDS_ODP] = PAD_INFO(P_LVDS_ODP),
	[P_LVDS_ODN] = PAD_INFO(P_LVDS_ODN),
	[P_LVDS_OCP] = PAD_INFO(P_LVDS_OCP),
	[P_LVDS_OCN] = PAD_INFO(P_LVDS_OCN),
	[P_LVDS_OBP] = PAD_INFO(P_LVDS_OBP),
	[P_LVDS_OBN] = PAD_INFO(P_LVDS_OBN),
	[P_LVDS_OAP] = PAD_INFO(P_LVDS_OAP),
	[P_LVDS_OAN] = PAD_INFO(P_LVDS_OAN),
	[P_LVDS_EEP] = PAD_INFO(P_LVDS_EEP),
	[P_LVDS_EEN] = PAD_INFO(P_LVDS_EEN),
	[P_LVDS_EDP] = PAD_INFO(P_LVDS_EDP),
	[P_LVDS_EDN] = PAD_INFO(P_LVDS_EDN),
	[P_LVDS_ECP] = PAD_INFO(P_LVDS_ECP),
	[P_LVDS_ECN] = PAD_INFO(P_LVDS_ECN),
	[P_LVDS_EBP] = PAD_INFO(P_LVDS_EBP),
	[P_LVDS_EBN] = PAD_INFO(P_LVDS_EBN),
	[P_LVDS_EAP] = PAD_INFO(P_LVDS_EAP),
	[P_LVDS_EAN] = PAD_INFO(P_LVDS_EAN),
	[P_SD0_D0] = PAD_INFO_PULL(P_SD0_D0),
	[P_SD0_D1] = PAD_INFO_PULL(P_SD0_D1),
	[P_SD0_D2] = PAD_INFO_PULL(P_SD0_D2),
	[P_SD0_D3] = PAD_INFO_PULL(P_SD0_D3),
	[P_SD1_D0] = PAD_INFO_PULL(P_SD1_D0),
	[P_SD1_D1] = PAD_INFO_PULL(P_SD1_D1),
	[P_SD1_D2] = PAD_INFO_PULL(P_SD1_D2),
	[P_SD1_D3] = PAD_INFO_PULL(P_SD1_D3),
	[P_SD0_CMD] = PAD_INFO_PULL(P_SD0_CMD),
	[P_SD0_CLK] = PAD_INFO_PULL(P_SD0_CLK),
	[P_SD1_CMD] = PAD_INFO_PULL(P_SD1_CMD),
	[P_SD1_CLK] = PAD_INFO(P_SD1_CLK),
	[P_SPI0_SCLK] = PAD_INFO_PULL(P_SPI0_SCLK),
	[P_SPI0_SS] = PAD_INFO(P_SPI0_SS),
	[P_SPI0_MISO] = PAD_INFO(P_SPI0_MISO),
	[P_SPI0_MOSI] = PAD_INFO_PULL(P_SPI0_MOSI),
	[P_UART0_RX] = PAD_INFO_PULL(P_UART0_RX),
	[P_UART0_TX] = PAD_INFO_PULL(P_UART0_TX),
	[P_UART2_RX] = PAD_INFO(P_UART2_RX),
	[P_UART2_TX] = PAD_INFO(P_UART2_TX),
	[P_UART2_RTSB] = PAD_INFO(P_UART2_RTSB),
	[P_UART2_CTSB] = PAD_INFO(P_UART2_CTSB),
	[P_UART3_RX] = PAD_INFO(P_UART3_RX),
	[P_UART2_TX] = PAD_INFO(P_UART2_TX),
	[P_UART2_RTSB] = PAD_INFO(P_UART2_RTSB),
	[P_UART2_CTSB] = PAD_INFO(P_UART2_CTSB),
	[P_UART4_RX] = PAD_INFO_PULL(P_UART4_RX),
	[P_UART4_TX] = PAD_INFO_PULL(P_UART4_TX),
	[P_I2C0_SCLK] = PAD_INFO_PULL(P_I2C0_SCLK),
	[P_I2C0_SDATA] = PAD_INFO_PULL(P_I2C0_SDATA),
	[P_I2C1_SCLK] = PAD_INFO_PULL(P_I2C1_SCLK),
	[P_I2C1_SDATA] = PAD_INFO_PULL(P_I2C1_SDATA),
	[P_I2C2_SCLK] = PAD_INFO_PULL(P_I2C2_SCLK),
	[P_I2C2_SDATA] = PAD_INFO_PULL(P_I2C2_SDATA),
	[P_CSI0_DN0] = PAD_INFO(P_CSI0_DN0),
	[P_CSI0_DP0] = PAD_INFO(P_CSI0_DP0),
	[P_CSI0_DN1] = PAD_INFO(P_CSI0_DN1),
	[P_CSI0_DP1] = PAD_INFO(P_CSI0_DP1),
	[P_CSI0_CN] = PAD_INFO(P_CSI0_CN),
	[P_CSI0_CP] = PAD_INFO(P_CSI0_CP),
	[P_CSI0_DN2] = PAD_INFO(P_CSI0_DN2),
	[P_CSI0_DP2] = PAD_INFO(P_CSI0_DP2),
	[P_CSI0_DN3] = PAD_INFO(P_CSI0_DN3),
	[P_CSI0_DP3] = PAD_INFO(P_CSI0_DN0),
	[P_DSI_DP3] = PAD_INFO(P_DSI_DP3),
	[P_DSI_DN3] = PAD_INFO(P_DSI_DN3),
	[P_DSI_DP1] = PAD_INFO(P_DSI_DP1),
	[P_DSI_DN1] = PAD_INFO(P_DSI_DN1),
	[P_DSI_CP] = PAD_INFO(P_DSI_CP),
	[P_DSI_CN] = PAD_INFO(P_DSI_CN),
	[P_DSI_DP0] = PAD_INFO(P_DSI_DP0),
	[P_DSI_DN0] = PAD_INFO(P_DSI_DN0),
	[P_DSI_DP2] = PAD_INFO(P_DSI_DP2),
	[P_DSI_DN2] = PAD_INFO(P_DSI_DN2),
	[P_SENSOR0_PCLK] = PAD_INFO(P_SENSOR0_PCLK),
	[P_CSI1_DN0] = PAD_INFO(P_CSI1_DN0),
	[P_CSI1_DP0] = PAD_INFO(P_CSI1_DP0),
	[P_CSI1_DN1] = PAD_INFO(P_CSI1_DN1),
	[P_CSI1_DP1] = PAD_INFO(P_CSI1_DP1),
	[P_CSI1_CN] = PAD_INFO(P_CSI1_CN),
	[P_CSI1_CP] = PAD_INFO(P_CSI1_CP),
	[P_SENSOR0_CKOUT] = PAD_INFO(P_SENSOR0_CKOUT),
	[P_NAND0_D0] = PAD_INFO_PULL(P_NAND0_D0),
	[P_NAND0_D1] = PAD_INFO_PULL(P_NAND0_D1),
	[P_NAND0_D2] = PAD_INFO_PULL(P_NAND0_D2),
	[P_NAND0_D3] = PAD_INFO_PULL(P_NAND0_D3),
	[P_NAND0_D4] = PAD_INFO_PULL(P_NAND0_D4),
	[P_NAND0_D5] = PAD_INFO_PULL(P_NAND0_D5),
	[P_NAND0_D6] = PAD_INFO_PULL(P_NAND0_D6),
	[P_NAND0_D7] = PAD_INFO_PULL(P_NAND0_D7),
	[P_NAND0_DQS] = PAD_INFO_PULL(P_NAND0_DQS),
	[P_NAND0_DQSN] = PAD_INFO_PULL(P_NAND0_DQSN),
	[P_NAND0_ALE] = PAD_INFO(P_NAND0_ALE),
	[P_NAND0_CLE] = PAD_INFO(P_NAND0_CLE),
	[P_NAND0_CEB0] = PAD_INFO(P_NAND0_CEB0),
	[P_NAND0_CEB1] = PAD_INFO(P_NAND0_CEB1),
	[P_NAND0_CEB2] = PAD_INFO(P_NAND0_CEB2),
	[P_NAND0_CEB3] = PAD_INFO(P_NAND0_CEB3),
	[P_NAND1_D0] = PAD_INFO_PULL(P_NAND1_D0),
	[P_NAND1_D1] = PAD_INFO_PULL(P_NAND1_D1),
	[P_NAND1_D2] = PAD_INFO_PULL(P_NAND1_D2),
	[P_NAND1_D3] = PAD_INFO_PULL(P_NAND1_D3),
	[P_NAND1_D4] = PAD_INFO_PULL(P_NAND1_D4),
	[P_NAND1_D5] = PAD_INFO_PULL(P_NAND1_D5),
	[P_NAND1_D6] = PAD_INFO_PULL(P_NAND1_D6),
	[P_NAND1_D7] = PAD_INFO_PULL(P_NAND1_D7),
	[P_NAND1_DQS] = PAD_INFO_PULL(P_NAND1_DQS),
	[P_NAND1_DQSN] = PAD_INFO_PULL(P_NAND1_DQSN),
	[P_NAND1_ALE] = PAD_INFO(P_NAND1_ALE),
	[P_NAND1_CLE] = PAD_INFO(P_NAND1_CLE),
	[P_NAND1_CEB0] = PAD_INFO(P_NAND1_CEB0),
	[P_NAND1_CEB1] = PAD_INFO(P_NAND1_CEB1),
	[P_NAND1_CEB2] = PAD_INFO(P_NAND1_CEB2),
	[P_NAND1_CEB3] = PAD_INFO(P_NAND1_CEB3),
	[P_SGPIO0] = PAD_INFO(P_SGPIO0),
	[P_SGPIO1] = PAD_INFO(P_SGPIO1),
	[P_SGPIO2] = PAD_INFO_PULL(P_SGPIO2),
	[P_SGPIO3] = PAD_INFO_PULL(P_SGPIO3),
};

static struct pinctrl_gpio_range owl_gpio_ranges[] = {
	{
		.name = "s900-pinctrl-gpio",
		.id = 0,
		.base = 0,
		.pin_base = 0,
		.npins = NUM_GPIOS,
	},
};

static struct owl_pinctrl_soc_info s900_pinctrl_info = {
	.gpio_ranges = owl_gpio_ranges,
	.gpio_num_ranges = ARRAY_SIZE(owl_gpio_ranges),
	.padinfo = s900_pad_tab,
	.pins = (const struct pinctrl_pin_desc *)s900_pads,
	.npins = ARRAY_SIZE(s900_pads),
	.functions = s900_functions,
	.nfunctions = ARRAY_SIZE(s900_functions),
	.groups = s900_groups,
	.ngroups = ARRAY_SIZE(s900_groups),
};

static int s900_pinctrl_probe(struct platform_device *pdev)
{
	return owl_pinctrl_probe(pdev, &s900_pinctrl_info);
}

static int s900_pinctrl_remove(struct platform_device *pdev)
{
	return owl_pinctrl_remove(pdev);
}

static struct of_device_id s900_pinctrl_of_match[] = {
	{ .compatible = "actions,s900-pinctrl", },
	{}
};
MODULE_DEVICE_TABLE(of, s900_pinctrl_of_match);

static struct platform_driver s900_pinctrl_driver = {
	.probe = s900_pinctrl_probe,
	.remove = s900_pinctrl_remove,
	.driver = {
		.name = "pinctrl-s900",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(s900_pinctrl_of_match),
	},
};

static int __init s900_pinctrl_init(void)
{
	return platform_driver_register(&s900_pinctrl_driver);
}
arch_initcall(s900_pinctrl_init);

static void __exit s900_pinctrl_exit(void)
{
	platform_driver_unregister(&s900_pinctrl_driver);
}

module_exit(s900_pinctrl_exit);
MODULE_AUTHOR("Actions Semi Inc.");
MODULE_DESCRIPTION("Pin control driver for Actions S900 SoC");
MODULE_LICENSE("GPL");
