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








enum aotg_mode_e aotg_mode[2];

int is_ls_device[2];


struct aotg_hcd *act_hcd_ptr[2];

static u64 aotg_dmamask = DMA_BIT_MASK(32);

struct aotg_plat_data aotg_data[2];

static void aotg_plat_data_fill(struct device *dev, int dev_id)
{
	aotg_data[dev_id].no_hs = 0;
	
	if (0 == dev_id)
	{
		aotg_data[0].usbecs = devm_ioremap_nocache(dev, 0xE024c094, 4);
	}
	else if (1 == dev_id)
	{
		aotg_data[1].usbecs = devm_ioremap_nocache(dev, 0xE024c098, 4);
	}
	else {
		BUG_ON(1);
	}
}

static int usb_current_calibrate(void)
{
	return 0x6;
}

static void aotg_DD_set_phy(void __iomem *base, u8 reg, u8 value)
{
	u8 addrlow, addrhigh;
	int time = 1;

	addrlow = reg & 0x0f;
	addrhigh = (reg >> 4) & 0x0f;

	/*write vstatus: */
	writeb(value, base + VDSTATUS);
	mb();

	/*write vcontrol: */
	writeb(addrlow | 0x10, base + VDCTRL);
	udelay(time); /*the vload period should > 33.3ns*/
	writeb(addrlow & 0x0f, base + VDCTRL);
	udelay(time);
	mb();
	writeb(addrlow | 0x10, base + VDCTRL);
	udelay(time);
	writeb(addrhigh | 0x10, base + VDCTRL);
	udelay(time);
	writeb(addrhigh & 0x0f, base + VDCTRL);
	udelay(time);
	writeb(addrhigh | 0x10, base + VDCTRL);
	udelay(time);
	return;
}


static void aotg_set_hcd_phy(int id)
{
	int value;
	
		aotg_DD_set_phy(aotg_data[id].base, 0xf4, 0xbb);
		aotg_DD_set_phy(aotg_data[id].base, 0xe1, 0xcf);
		aotg_DD_set_phy(aotg_data[id].base, 0xf4, 0x9b);
		aotg_DD_set_phy(aotg_data[id].base, 0xe6, 0xcc);
		aotg_DD_set_phy(aotg_data[id].base, 0xf4, 0xbb);
		aotg_DD_set_phy(aotg_data[id].base, 0xe2, 0x2);
		aotg_DD_set_phy(aotg_data[id].base, 0xe2, 0x16);
		aotg_DD_set_phy(aotg_data[id].base, 0xf4, 0x9b);
		aotg_DD_set_phy(aotg_data[id].base, 0xe7, 0xa1);
		aotg_DD_set_phy(aotg_data[id].base, 0xf4, 0xbb);
		aotg_DD_set_phy(aotg_data[id].base, 0xe0, 0x21);
		aotg_DD_set_phy(aotg_data[id].base, 0xe0, 0x25);
		aotg_DD_set_phy(aotg_data[id].base, 0xf4, 0x9b);

		value = usb_current_calibrate();
		dev_info(aotg_data[id].dev, "aotg[%d] CURRENT value: 0x%x\n",
			id, value);
		value |=  (0xa<<4);
		aotg_DD_set_phy(aotg_data[id].base, 0xe4, value);
		aotg_DD_set_phy(aotg_data[id].base, 0xf0, 0xfc);
		pr_info("PHY: Disable hysterisys mode\n");


	return;
}

void aotg_powergate_on(int id)
{
	pm_runtime_enable(aotg_data[id].dev);
	pm_runtime_get_sync(aotg_data[id].dev);

	clk_prepare_enable(aotg_data[id].clk_usbh_phy);

	clk_prepare_enable(aotg_data[id].clk_usbh_pllen);
}

void aotg_powergate_off(int id)
{
	clk_disable_unprepare(aotg_data[id].clk_usbh_pllen);
	clk_disable_unprepare(aotg_data[id].clk_usbh_phy);


	pm_runtime_put_sync(aotg_data[id].dev);
	pm_runtime_disable(aotg_data[id].dev);
}

int aotg_wait_reset(int id)
{
	int i = 0;
	while (((readb(aotg_data[id].base + USBERESET) & USBERES_USBRESET) != 0) && (i < 300000)) {
		i++;
		udelay(1);
	}

	if (!(readb(aotg_data[id].base + USBERESET) & USBERES_USBRESET)) {
		dev_info(aotg_data[id].dev, "usb reset OK: %x.\n", readb(aotg_data[id].base + USBERESET));
	} else {
		dev_err(aotg_data[id].dev, "usb reset ERROR: %x.\n", readb(aotg_data[id].base + USBERESET));
		return -EBUSY;
	}
	return 0;
}

void aotg_hardware_init(int id)
{
	u8 val8;
	unsigned long flags;
	struct aotg_plat_data *data = &aotg_data[id];

	local_irq_save(flags);
	/*aotg_hcd_controller_reset(acthcd->port_specific);*/
	aotg_powergate_on(id);
	aotg_wait_reset(id);
	/* fpga : new DMA mode */
	writel(0x1, data->base + HCDMABCKDOOR);

	usb_writel(0x37000000 | (0x3<<4), data->usbecs);

	local_irq_restore(flags);
	udelay(100);
	aotg_set_hcd_phy(id);
	local_irq_save(flags);

	/***** TA_BCON_COUNT *****/
	writeb(0x0, data->base + TA_BCON_COUNT);	/*110ms*/
	/*set TA_SUSPEND_BDIS timeout never generate */
	usb_writeb(0xff, data->base + TAAIDLBDIS);
	/*set TA_AIDL_BDIS timeout never generate */
	usb_writeb(0xff, data->base + TAWAITBCON);
	/*set TA_WAIT_BCON timeout never generate */
	usb_writeb(0x28, data->base + TBVBUSDISPLS);
	usb_setb(1 << 7, data->base + TAWAITBCON);

	usb_writew(0x1000, data->base + VBUSDBCTIMERL);

	val8 = readb(data->base + BKDOOR);
	if (data && data->no_hs)
		val8 |= (1 << 7);
	else
		val8 &= ~(1 << 7);

	if (is_ls_device[id])
		val8 |= (1<<7);
	writeb(val8, data->base + BKDOOR);
	
	mb();
	local_irq_restore(flags);
}






















static const char platform_drv_name[] = "aotg_hcd";
static struct workqueue_struct *start_mon_wq;
static struct delayed_work start_mon_wker;
struct kmem_cache *td_cache;
struct mutex aotg_onoff_mutex;

static void start_mon(struct work_struct *work)
{
	pr_info("usb start mon\n");
	aotg_uhost_mon_init(0);
	aotg_uhost_mon_init(1);
}

struct of_device_id aotg_of_match[] = {
	{.compatible = "caninos,k7-usb2.0-0"},
	{.compatible = "caninos,k7-usb2.0-1"},
	{},
};

MODULE_DEVICE_TABLE(of, aotg_of_match);

static int aotg_hcd_get_dts(struct platform_device *pdev)
{
	struct device_node *of_node = pdev->dev.of_node;
	enum of_gpio_flags flags;

	if (of_device_is_compatible(of_node, aotg_of_match[0].compatible)) {
		pdev->id = 0;
	} else if (of_device_is_compatible(of_node, aotg_of_match[1].compatible)) {
		pdev->id = 1;
	} else {
		dev_err(&pdev->dev, "compatible ic type failed\n");
	}
	
	return 0;
}

int aotg_probe(struct platform_device *pdev)
{
	struct resource *res_mem;
	int irq, retval;
	
	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res_mem)
	{
		dev_err(&pdev->dev, "<HCD_PROBE>usb has no resource for mem!\n");
		return -ENODEV;
	}
	
	irq = platform_get_irq(pdev, 0);
	
	if (irq <= 0)
	{
		dev_err(&pdev->dev, "<HCD_PROBE>usb has no resource for irq!\n");
		retval = -ENODEV;
		goto err1;
	}
	
	if (!request_mem_region(res_mem->start, res_mem->end - res_mem->start + 1, dev_name(&pdev->dev))) {
		dev_err(&pdev->dev, "<HCD_PROBE>request_mem_region failed\n");
		retval = -EBUSY;
		goto err1;
	}
	
	if (aotg_hcd_get_dts(pdev) < 0) {
		retval = -ENODEV;
		goto err1;
	}
	
	aotg_data[pdev->id].base = devm_ioremap(&pdev->dev, res_mem->start, res_mem->end - res_mem->start + 1);
	
	if (!aotg_data[pdev->id].base)
	{
		dev_err(&pdev->dev, "<HCD_PROBE>ioremap failed\n");
		retval = -ENOMEM;
		goto err1;
	}
	
	aotg_plat_data_fill(&pdev->dev, pdev->id);
	
	pdev->dev.dma_mask = &aotg_dmamask;
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	
	aotg_data[pdev->id].rsrc_start = res_mem->start;
	aotg_data[pdev->id].rsrc_len = res_mem->end - res_mem->start + 1;
	aotg_data[pdev->id].irq = irq;
	aotg_data[pdev->id].dev = &pdev->dev;
	
	device_init_wakeup(&pdev->dev, true);
	
	if (pdev->id)
	{
		aotg_data[pdev->id].clk_usbh_pllen = devm_clk_get(&pdev->dev, "usbh1_pllen");
	}
	else
	{
		aotg_data[pdev->id].clk_usbh_pllen = devm_clk_get(&pdev->dev, "usbh0_pllen");
	}
	
	if (IS_ERR(aotg_data[pdev->id].clk_usbh_pllen))
	{
		dev_err(&pdev->dev, "unable to get usbh_pllen\n");
		retval = -EINVAL;
		goto err1;
	}
	
	if (pdev->id) {
		aotg_data[pdev->id].clk_usbh_phy = devm_clk_get(&pdev->dev, "usbh1_phy");
	}
	else {
		aotg_data[pdev->id].clk_usbh_phy = devm_clk_get(&pdev->dev, "usbh0_phy");
	}
		
	if (IS_ERR(aotg_data[pdev->id].clk_usbh_phy))
	{
		dev_err(&pdev->dev, "unable to get usbh_phy\n");
		retval =  -EINVAL;
		goto err1;
	}
	
	pr_info("usb pdev->id:%x successfully probed\n", pdev->id);
	return 0;

err1:
	release_mem_region(res_mem->start, res_mem->end - res_mem->start + 1);
	return retval;
}

int aotg_remove(struct platform_device *pdev)
{
	aotg_uhost_mon_exit();
	release_mem_region(aotg_data[pdev->id].rsrc_start, aotg_data[pdev->id].rsrc_len);
	return 0;
}

struct platform_driver aotg_hcd_driver = {
	.probe = aotg_probe,
	.remove = aotg_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = platform_drv_name,
		.of_match_table = aotg_of_match,
	},
};

static int __init aotg_init(void)
{
	mutex_init(&aotg_onoff_mutex);
	
	td_cache = kmem_cache_create("aotg_hcd", sizeof(struct aotg_td), 0, SLAB_HWCACHE_ALIGN | SLAB_PANIC, NULL);
	
	platform_driver_register(&aotg_hcd_driver);
	
	start_mon_wq = create_singlethread_workqueue("aotg_start_mon_wq");
	
	INIT_DELAYED_WORK(&start_mon_wker, start_mon);
	
	queue_delayed_work(start_mon_wq, &start_mon_wker, msecs_to_jiffies(10000));
	
	return 0;
}

static void __exit aotg_exit(void)
{
	cancel_delayed_work_sync(&start_mon_wker);
	
	flush_workqueue(start_mon_wq);
	
	destroy_workqueue(start_mon_wq);
	
	platform_driver_unregister(&aotg_hcd_driver);
	
	kmem_cache_destroy(td_cache);
	return;
}

module_init(aotg_init);
module_exit(aotg_exit);

MODULE_DESCRIPTION("Actions OTG controller driver");
MODULE_LICENSE("GPL");

