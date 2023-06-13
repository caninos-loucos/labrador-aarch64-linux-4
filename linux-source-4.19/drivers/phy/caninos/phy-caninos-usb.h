/*
 * Caninos SoCs phy driver
 *
 * Copyright (c) 2015 Actions Semiconductor Co., ltd.
 * tangshaoqing <tangshaoqing@actions-semi.com>
 * 
 * Copyright (c) 2023  LSI-TEC
 * Ana Clara Forcelli <ana.forcelli@lsitec.org.br>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PHY_CANINOS_USB_H
#define __PHY_CANINOS_USB_H

#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/io.h>
#include <linux/device.h>

struct caninos_usbphy {
	struct phy 		*phy;
	struct device	*dev;
	void __iomem	*regs;
};

#define phy_to_cphy(x)			container_of(x, struct caninos_usbphy, phy)
#define USB3_PHY2_TX_CURRENT	(0x7)

#endif /* __PHY_CANINOS_USB_H */

