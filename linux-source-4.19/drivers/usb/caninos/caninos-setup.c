#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/delay.h>
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
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>

#include <asm/irq.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/suspend.h>

#include <linux/kallsyms.h>

#include "aotg_hcd.h"
#include "aotg_plat_data.h"
#include "aotg_hcd_debug.h"
#include "aotg_udc_debug.h"
#include "aotg_mon.h"
#include "aotg_udc.h"

static int aotg_initialized[2] = {0, 0};

extern struct mutex aotg_onoff_mutex;
extern struct aotg_plat_data aotg_data[2];





int aotg_device_init(int dev_id)
{
	struct device *dev = aotg_data[dev_id].dev;
	struct aotg_hcd *acthcd;
	struct usb_hcd *hcd;
	struct aotg_udc *udc;
	int ret = 0;
	
	mutex_lock(&aotg_onoff_mutex);
	
	/* check if already initialized */
	if (aotg_initialized[dev_id])
	{
		mutex_unlock(&aotg_onoff_mutex);
		return 0;
	}
	
	aotg_initialized[dev_id] = 1;
	
	hcd = usb_create_hcd(&act_hc_driver, dev, dev_name(dev));
	
	if (!hcd)
	{
		dev_err(dev, "<HCD_PROBE>usb create hcd failed\n");
		aotg_powergate_off(dev_id);
		mutex_unlock(&aotg_onoff_mutex);
		return -ENOMEM;
	}
	
	aotg_hcd_init(hcd, dev_id);
	
	hcd->rsrc_start = aotg_data[dev_id].rsrc_start;
	hcd->rsrc_len = aotg_data[dev_id].rsrc_len;
	
	acthcd = hcd_to_aotg(hcd);
	act_hcd_ptr[dev_id] = acthcd;
	acthcd->dev = dev;
	acthcd->base = aotg_data[dev_id].base;
	
	hcd->regs = acthcd->base;
	
	acthcd->hcd_exiting = 0;
	acthcd->uhc_irq = aotg_data[dev_id].irq;
	acthcd->id = dev_id;
	
	aotg_hardware_init(dev_id);
	
	hcd->self.sg_tablesize = 32;
	hcd->has_tt = 1;
	hcd->self.uses_pio_for_control = 1;
	
	hrtimer_init(&acthcd->hotplug_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	acthcd->hotplug_timer.function = aotg_hub_hotplug_timer;
	
	timer_setup(&acthcd->trans_wait_timer, aotg_hub_trans_wait_timer, 0);
	timer_setup(&acthcd->check_trb_timer, aotg_check_trb_timer, 0);
	
	ret = usb_add_hcd(hcd, acthcd->uhc_irq, 0);
	
	if (ret == 0)
	{
		aotg_enable_irq(acthcd);
		
		device_wakeup_enable(&hcd->self.root_hub->dev);
		
		dev_info(dev, "hcd controller initialized. OTGIRQ: 0x%02X, OTGSTATE: 0x%02X\n",
			readb(acthcd->base + OTGIRQ), readb(acthcd->base + OTGSTATE));
		
		writeb(readb(acthcd->base + USBEIRQ), acthcd->base + USBEIRQ);
		mutex_unlock(&aotg_onoff_mutex);
		return 0;
	}
	else
	{
		dev_err(dev, "%s:usb add hcd failed\n", __func__);
		
		hrtimer_cancel(&acthcd->hotplug_timer);
		del_timer_sync(&acthcd->trans_wait_timer);
		del_timer_sync(&acthcd->check_trb_timer);
		
		usb_put_hcd(hcd);
	}
	

	aotg_powergate_off(dev_id);
	mutex_unlock(&aotg_onoff_mutex);
	return ret;
}

int aotg_device_exit(int dev_id)
{
	struct aotg_hcd *acthcd;
	struct aotg_udc *udc;
	struct usb_hcd *hcd;
	struct aotg_hcep *ep;
	int i;
	
	mutex_lock(&aotg_onoff_mutex);
	
	if (!aotg_initialized[dev_id]) {
		pr_warn("aotg%d exit allready!\n", dev_id);
		mutex_unlock(&aotg_onoff_mutex);
		return -EINVAL;
	}
	
	aotg_initialized[dev_id] = 0;

	acthcd = act_hcd_ptr[dev_id];
	hcd = aotg_to_hcd(acthcd);
	
	usb_remove_hcd(hcd);
	
	act_hcd_ptr[dev_id] = NULL;
	
	aotg_disable_irq(acthcd);
	aotg_powergate_off(dev_id);
	acthcd->hcd_exiting = 1;
	pr_warn("usbh_cce%d had been poweroff...\n", dev_id);

	tasklet_kill(&acthcd->urb_tasklet);
	del_timer_sync(&acthcd->trans_wait_timer);
	del_timer_sync(&acthcd->check_trb_timer);
	hrtimer_cancel(&acthcd->hotplug_timer);
	
	aotg_hcd_release_queue(acthcd, NULL);

	for (i = 0; i < MAX_EP_NUM; i++)
	{
		ep = acthcd->ep0[i];
		if (ep)
		{
			ACT_HCD_DBG
			if (ep->q)
				ACT_HCD_DBG
			kfree(ep);
		}
	}
	
		for (i = 1; i < MAX_EP_NUM; i++) {
			ep = acthcd->inep[i];
			if (ep) {
				ACT_HCD_DBG
				if (ep->ring) {
					ACT_HCD_DBG
				}
				kfree(ep);
			}
		}
		for (i = 1; i < MAX_EP_NUM; i++) {
			ep = acthcd->outep[i];
			if (ep) {
				ACT_HCD_DBG
				if (ep->ring)
					ACT_HCD_DBG
				kfree(ep);
			}
		}

		usb_put_hcd(hcd);
		acthcd = NULL;
	
	aotg_mode[dev_id] = DEFAULT_MODE;

	mutex_unlock(&aotg_onoff_mutex);
	return 0;
}

