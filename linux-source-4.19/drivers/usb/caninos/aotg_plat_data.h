/*
 * Actions OWL SoCs usb2.0 controller driver
 *
 * Copyright (c) 2015 Actions Semiconductor Co., ltd.
 * dengtaiping <dengtaiping@actions-semi.com>
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

#ifndef __AOTG_PLAT_DATA_H__
#define __AOTG_PLAT_DATA_H__


struct aotg_plat_data {
	void __iomem *usbecs;
	/*void __iomem *usbpll;
	u32 usbpll_bits;
	void __iomem *devrst;
	u32 devrst_bits;
	void __iomem *sps_pg_ctl;
	u32 pg_ctl_bits;
	void __iomem *sps_pg_ack;
	u32 pg_ack_bits;*/
	int no_hs;

	struct clk *clk_usbh_pllen;
	struct clk *clk_usbh_phy;
	struct clk *clk_usbh_cce;
	int irq;
	struct device *dev;
	void __iomem *base;
	resource_size_t		rsrc_start;
	resource_size_t		rsrc_len;
};

enum aotg_mode_e {
	DEFAULT_MODE,
	HCD_MODE,
	UDC_MODE,
};

enum ic_type_e {
	ERR_TYPE = 0,
	S700,
	S900,
};

struct aotg_uhost_mon_t {
	int id;
	/*struct aotg_plat_data data;*/

	struct timer_list hotplug_timer;

	struct workqueue_struct *aotg_dev_onoff;
	struct delayed_work aotg_dev_init;
	struct delayed_work aotg_dev_exit;


	unsigned int aotg_det;

	/* dp, dm state. */
	unsigned int old_state;
	unsigned int state;
	unsigned int det_step;
	enum aotg_mode_e det_mode;
};

int aotg0_device_init(int power_only);
void aotg0_device_exit(int power_only);

int aotg1_device_init(int power_only);
void aotg1_device_exit(int power_only);
void aotg_hub_unregister(int dev_id);
int aotg_hub_register(int dev_id);

#endif
