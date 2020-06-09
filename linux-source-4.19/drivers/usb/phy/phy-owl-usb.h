/*
 * Actions OWL SoCs phy driver
 *
 * Copyright (c) 2015 Actions Semiconductor Co., ltd.
 * tangshaoqing <tangshaoqing@actions-semi.com>
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


#ifndef __PHY_OWL_USB_H
#define __PHY_OWL_USB_H


#include <linux/usb/phy.h>

enum {
	USB2PHY_R = 1,
	USB2PHY_C = 2
};

enum {
	USB3PHY_R = 1,
	USB3PHY_C = 2
};

struct owl_usbphy {
	struct usb_phy	phy;
	struct device	*dev;
	int phy_type;
	void __iomem	*regs;
};

#define phy_to_sphy(x)		container_of((x), struct owl_usbphy, phy)
#define USB3_PHY2_TX_CURRENT	(0x7)

extern int owl_dwc3_usb2phy_param_setup(int is_device_mode);
extern int owl_get_usb_hsdp(unsigned int *usb_hsdp);

extern bool usb3_support_runtime_pm(void);
extern bool usb3_need_double_check_inactive(void);
extern bool xhci_need_phy_config(void);
extern bool usb3_need_set_device_noattached(void);
extern bool usb_need_fix_vbus_reset(void);
#endif /* __PHY_OWL_USB_H */

