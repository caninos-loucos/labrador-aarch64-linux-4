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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/otg.h>
#include <linux/usb/hcd.h>
#include <asm/irq.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/suspend.h>

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>

#include "aotg_hcd.h"
#include "aotg_regs.h"
#include "aotg_plat_data.h"
#include "aotg.h"

int aotg_device_init(int dev_id);
int aotg_device_exit(int dev_id);

/* usbecs register. */
#define	USB2_ECS_VBUS_P0		10
#define	USB2_ECS_ID_P0			12
#define USB2_ECS_LS_P0_SHIFT	8
#define USB2_ECS_LS_P0_MASK		(0x3<<8)
#define USB2_ECS_DPPUEN_P0     3
#define USB2_ECS_DMPUEN_P0     2
#define USB2_ECS_DMPDDIS_P0    1
#define USB2_ECS_DPPDDIS_P0    0
#define USB2_ECS_SOFTIDEN_P0   (1<<26)
#define USB2_ECS_SOFTID_P0     27
#define USB2_ECS_SOFTVBUSEN_P0 (1<<24)
#define USB2_ECS_SOFTVBUS_P0   25
#define USB2_PLL_EN0           (1<<12)
#define USB2_PLL_EN1           (1<<13)

extern struct aotg_plat_data aotg_data[2];
extern int is_ls_device[2];

struct aotg_uhost_mon_t *aotg_uhost_mon[2];
void aotg_dev_plugout_msg(int id);

int usb2_set_dp_500k_15k(struct aotg_uhost_mon_t *umon, int enable_500k_up, int enable_15k_down)
{
	unsigned int val;

	val = readl(aotg_data[umon->id].usbecs) & (~((1 << USB2_ECS_DPPUEN_P0) |
			(1 << USB2_ECS_DMPUEN_P0) |
			(1 << USB2_ECS_DMPDDIS_P0) |
			(1 << USB2_ECS_DPPDDIS_P0)));

	if (enable_500k_up != 0)
		val |= (1 << USB2_ECS_DPPUEN_P0)|(1 << USB2_ECS_DMPUEN_P0);

	if (enable_15k_down == 0) {
		val |= (1 << USB2_ECS_DPPDDIS_P0)|(1 << USB2_ECS_DMPDDIS_P0);
	}

	writel(val, aotg_data[umon->id].usbecs);
	return 0;
}

/* return dp, dm state. */
static inline unsigned int usb_get_linestates(struct aotg_uhost_mon_t *umon)
{
	unsigned int state;
	state = ((readl(aotg_data[umon->id].usbecs) & USB2_ECS_LS_P0_MASK) >> USB2_ECS_LS_P0_SHIFT);
	return state;
}


static unsigned int usb_get_stat(struct aotg_uhost_mon_t *umon)
{
	unsigned int state;
	
	/*if not enble udc mode,only detect linestate */

		state = usb_get_linestates(umon);

	return state;
}

static  void usb_disable_soft_vbus_idpin(struct aotg_uhost_mon_t *umon)
{
	writel(readl(aotg_data[umon->id].usbecs) & (~(USB2_ECS_SOFTVBUSEN_P0 | USB2_ECS_SOFTIDEN_P0)),
		aotg_data[umon->id].usbecs);
}

/*
*	This monitor only detect usb plug in, the plug out action will be detected by controller itself.
*/
static void aotg_uhost_mon_timer(struct timer_list *t)
{
	struct aotg_uhost_mon_t *umon = container_of(t, struct aotg_uhost_mon_t, hotplug_timer);
	
	if (umon->det_step == 0)
	{
		/* before detect linestate ,pull down 15k*/
		usb2_set_dp_500k_15k(umon, 0, 1);
		umon->det_step++;
		mod_timer(&umon->hotplug_timer, jiffies + msecs_to_jiffies(100));
	}
	else if (umon->det_step == 1)
	{
		umon->det_step++;
		umon->state = usb_get_stat(umon);
		umon->old_state = umon->state;
		
		mod_timer(&umon->hotplug_timer, jiffies + msecs_to_jiffies(400));
		
		/*check linestate */
		if (umon->state == 0 ||  umon->state == 3) {
			umon->det_step = 0;
		}
	}
	else
	{
		umon->state = usb_get_stat(umon);
		
		if (umon->state == umon->old_state)
		{
			if (usb_get_linestates(umon) == 2) {
				is_ls_device[umon->id] = 1;
			}
			umon->aotg_det = 0;
			umon->det_step = 0;
			
			pr_info("usb device connected!");
			queue_delayed_work(umon->aotg_dev_onoff, &umon->aotg_dev_init, msecs_to_jiffies(1));
		}
		else
		{
			mod_timer(&umon->hotplug_timer, jiffies + msecs_to_jiffies(400));
			umon->aotg_det = 1;
			umon->det_step = 0;
		}
	}
}

static void aotg_dev_register(struct work_struct *w)
{
	struct aotg_uhost_mon_t *umon = container_of(w, struct aotg_uhost_mon_t, aotg_dev_init.work);
	pm_runtime_put_sync(aotg_data[umon->id].dev);
	pm_runtime_disable(aotg_data[umon->id].dev);
	
	aotg_mode[umon->id] = umon->det_mode;
	aotg_device_init(umon->id);
	return;
}

static void aotg_dev_unregister(struct work_struct *w)
{
	struct aotg_uhost_mon_t *umon = container_of(w, struct aotg_uhost_mon_t, aotg_dev_exit.work);
	int ret = 0;
	

	ret = aotg_device_exit(umon->id);
	
	if (ret < 0)
		return;
		
	usb_disable_soft_vbus_idpin(umon);
	
	umon->aotg_det = 1;
	umon->det_mode = HCD_MODE;
	pm_runtime_enable(aotg_data[umon->id].dev);
	pm_runtime_get_sync(aotg_data[umon->id].dev);
	is_ls_device[umon->id] = 0;
	mod_timer(&umon->hotplug_timer, jiffies + msecs_to_jiffies(100));
	return;
}

void aotg_dev_plugout_msg(int id)
{
	pr_debug("usb%d had been plugged out!\n", id);
	aotg_uhost_mon[id]->old_state = 0;
	queue_delayed_work(aotg_uhost_mon[id]->aotg_dev_onoff, &aotg_uhost_mon[id]->aotg_dev_exit, msecs_to_jiffies(500));
	return;
}

static struct aotg_uhost_mon_t *aotg_uhost_mon_alloc(void)
{
	struct aotg_uhost_mon_t *umon = NULL;
	
	umon = kzalloc(sizeof(*umon), GFP_KERNEL);
	
	if (!umon) {
		return NULL;
	}
	
	timer_setup(&umon->hotplug_timer, aotg_uhost_mon_timer, 0);
	
	INIT_DELAYED_WORK(&umon->aotg_dev_init, aotg_dev_register);
	INIT_DELAYED_WORK(&umon->aotg_dev_exit, aotg_dev_unregister);
	
	umon->aotg_det = 1;
	umon->det_mode = HCD_MODE;
	return umon;
}

void aotg_uhost_mon_init(int id)
{

	
	aotg_uhost_mon[id] = aotg_uhost_mon_alloc();
	aotg_uhost_mon[id]->id = id;
	
	if (id) {
		aotg_uhost_mon[id]->aotg_dev_onoff = create_singlethread_workqueue("aotg_dev1_onoff");
	}
	else {
		aotg_uhost_mon[id]->aotg_dev_onoff = create_singlethread_workqueue("aotg_dev0_onoff");
	}
	
	pm_runtime_enable(aotg_data[id].dev);
	pm_runtime_get_sync(aotg_data[id].dev);
	
	clk_prepare_enable(aotg_data[id].clk_usbh_pllen);
	
	pr_info("start mon %d ......\n", id);
	
	mod_timer(&aotg_uhost_mon[id]->hotplug_timer, jiffies + msecs_to_jiffies(HZ));
	return;
}

static int aotg_uhost_mon_free(int id)
{
	clk_disable_unprepare(aotg_data[id].clk_usbh_pllen);
	clk_disable_unprepare(aotg_data[id].clk_usbh_phy);
	
	pm_runtime_put_sync(aotg_data[id].dev);
	pm_runtime_disable(aotg_data[id].dev);
	
	if (aotg_uhost_mon[id]->aotg_dev_onoff)
	{
		cancel_delayed_work_sync(&aotg_uhost_mon[id]->aotg_dev_init);
		cancel_delayed_work_sync(&aotg_uhost_mon[id]->aotg_dev_exit);
		flush_workqueue(aotg_uhost_mon[id]->aotg_dev_onoff);
		destroy_workqueue(aotg_uhost_mon[id]->aotg_dev_onoff);
	}
	
	del_timer_sync(&aotg_uhost_mon[id]->hotplug_timer);
	
	kfree(aotg_uhost_mon[id]);
	return 0;
}

void aotg_uhost_mon_exit(void)
{
	if (aotg_uhost_mon[0])
		aotg_uhost_mon_free(0);
	if (aotg_uhost_mon[1])
		aotg_uhost_mon_free(1);
	return;
}

