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
#include "aotg_hcd_debug.h"
#include "aotg_regs.h"
#include "aotg.h"


#define HCD_DRIVER_DESC "Caninos USB Host Controller Driver"
#define HCD_DRIVER_NAME "hcd-caninos"

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

static struct kmem_cache *td_cache = NULL;

static struct aotg_plat_data aotg_data[] = {
	[0] = {.irq = -1, .id = 0},
	[1] = {.irq = -1, .id = 1},
};

struct aotg_td *aotg_alloc_td(gfp_t mem_flags)
{
	struct aotg_td *td;

	td = kmem_cache_alloc(td_cache, GFP_ATOMIC);
	if (!td)
		return NULL;
	memset(td, 0, sizeof(struct aotg_td));

	td->cross_ring = 0;
	td->err_count = 0;
	td->urb = NULL;
	INIT_LIST_HEAD(&td->queue_list);
	INIT_LIST_HEAD(&td->enring_list);
	INIT_LIST_HEAD(&td->dering_list);

	return td;
}

void aotg_release_td(struct aotg_td *td)
{
	if (!td)
		return;
	kmem_cache_free(td_cache, td);
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
}

static void aotg_set_hcd_phy(struct aotg_plat_data *pdata)
{
	aotg_DD_set_phy(pdata->base, 0xf4, 0xbb);
	aotg_DD_set_phy(pdata->base, 0xe1, 0xcf);
	aotg_DD_set_phy(pdata->base, 0xf4, 0x9b);
	aotg_DD_set_phy(pdata->base, 0xe6, 0xcc);
	aotg_DD_set_phy(pdata->base, 0xf4, 0xbb);
	aotg_DD_set_phy(pdata->base, 0xe2, 0x02);
	aotg_DD_set_phy(pdata->base, 0xe2, 0x16);
	aotg_DD_set_phy(pdata->base, 0xf4, 0x9b);
	aotg_DD_set_phy(pdata->base, 0xe7, 0xa1);
	aotg_DD_set_phy(pdata->base, 0xf4, 0xbb);
	aotg_DD_set_phy(pdata->base, 0xe0, 0x21);
	aotg_DD_set_phy(pdata->base, 0xe0, 0x25);
	aotg_DD_set_phy(pdata->base, 0xf4, 0x9b);
	aotg_DD_set_phy(pdata->base, 0xe4, 0xa6);
	aotg_DD_set_phy(pdata->base, 0xf0, 0xfc);
}

static void aotg_powergate_on(struct aotg_plat_data *pdata)
{
	pm_runtime_enable(pdata->dev);
	pm_runtime_get_sync(pdata->dev);

	clk_prepare_enable(pdata->clk_usbh_phy);
	clk_prepare_enable(pdata->clk_usbh_pllen);
}

static void aotg_powergate_off(struct aotg_plat_data *pdata)
{
	clk_disable_unprepare(pdata->clk_usbh_pllen);
	clk_disable_unprepare(pdata->clk_usbh_phy);
	
	pm_runtime_put_sync(pdata->dev);
	pm_runtime_disable(pdata->dev);
}

static int aotg_wait_reset(struct aotg_plat_data *pdata)
{
	int i = 0;
	while (((readb(pdata->base + USBERESET) & USBERES_USBRESET) != 0) && (i < 300000)) {
		i++;
		udelay(1);
	}
	if (!(readb(pdata->base + USBERESET) & USBERES_USBRESET)) {
		dev_info(pdata->dev, "usb reset OK: %x.\n", readb(pdata->base + USBERESET));
	} else {
		dev_err(pdata->dev, "usb reset ERROR: %x.\n", readb(pdata->base + USBERESET));
		return -EBUSY;
	}
	return 0;
}

static void aotg_hardware_init(struct aotg_plat_data *pdata)
{
	unsigned long flags;
	u8 val8;
	
	local_irq_save(flags);
	
	aotg_powergate_on(pdata);
	aotg_wait_reset(pdata);
	writel(0x1, pdata->base + HCDMABCKDOOR);
	usb_writel(0x37000000 | (0x3<<4), pdata->usbecs);
	
	local_irq_restore(flags);
	
	udelay(100);
	aotg_set_hcd_phy(pdata);
	
	local_irq_save(flags);
	
	writeb(0x0, pdata->base + TA_BCON_COUNT);
	usb_writeb(0xff, pdata->base + TAAIDLBDIS);
	usb_writeb(0xff, pdata->base + TAWAITBCON);
	usb_writeb(0x28, pdata->base + TBVBUSDISPLS);
	usb_setb(1 << 7, pdata->base + TAWAITBCON);
	usb_writew(0x1000, pdata->base + VBUSDBCTIMERL);
	
	val8 = readb(pdata->base + BKDOOR);
	val8 &= ~(1 << 7);
	//if (is_ls_device[id])
	//	val8 |= (1<<7);
	writeb(val8, pdata->base + BKDOOR);
	
	mb();
	local_irq_restore(flags);
}

static int caninos_hcd_probe(struct platform_device *pdev)
{
	struct aotg_plat_data *pdata;
	struct aotg_hcd *acthcd;
	struct resource *res;
	struct usb_hcd *hcd;
	int retval;
	
	dev_info(&pdev->dev, "caninos_hcd_probe() started\n");
	
	if (usb_disabled()) {
		dev_err(&pdev->dev, "usb is disabled, hcd probe aborted\n");
		return -ENODEV;
	}
	
	pdata = (struct aotg_plat_data *)of_device_get_match_data(&pdev->dev);
	
	if (!pdata) {
		dev_err(&pdev->dev, "could not get of device match data\n");
		return -ENODEV;
	}
	
	pdev->id = pdata->id;
	pdata->dev = &pdev->dev;
	
	retval = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	
	if (retval) {
		dev_err(&pdev->dev, "could not set dma mask, %d\n", retval);
		return retval;
	}
	
	pdata->irq = platform_get_irq(pdev, 0);
	
	if (pdata->irq <= 0) {
		dev_err(&pdev->dev, "could not get irq\n");
		return -ENODEV;
	}
	
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	pdata->base = devm_ioremap_resource(&pdev->dev, res);
	
	if (IS_ERR(pdata->base))
	{
		retval = PTR_ERR(pdata->base);
		dev_err(&pdev->dev, "failed to ioremap base resource, %d\n", retval);
		return retval;
	}
	
	pdata->rsrc_start = res->start;
	pdata->rsrc_len = resource_size(res);
	
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "usbecs");
	
	if (!res || resource_type(res) != IORESOURCE_MEM)
	{
		dev_err(&pdev->dev, "failed to get usbecs resource\n");
		return -EINVAL;
	}
	
	pdata->usbecs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	
	if (!pdata->usbecs)
	{
		dev_err(&pdev->dev, "failed to ioremap usbecs resource\n");
		return -ENOMEM;
	}
	
	pdata->clk_usbh_pllen = devm_clk_get(&pdev->dev, "pllen");
	
	if (IS_ERR(pdata->clk_usbh_pllen))
	{
		retval = PTR_ERR(pdata->clk_usbh_pllen);
		dev_err(&pdev->dev, "could not get pllen clk, %d\n", retval);
		return retval;
	}
	
	pdata->clk_usbh_phy = devm_clk_get(&pdev->dev, "phy");
	
	if (IS_ERR(pdata->clk_usbh_phy))
	{
		retval = PTR_ERR(pdata->clk_usbh_phy);
		dev_err(&pdev->dev, "could not get phy clk, %d\n", retval);
		return retval;
	}
	
	pdata->clk_usbh_cce = devm_clk_get(&pdev->dev, "cce");
	
	if (IS_ERR(pdata->clk_usbh_cce))
	{
		retval = PTR_ERR(pdata->clk_usbh_cce);
		dev_err(&pdev->dev, "could not get cce clk, %d\n", retval);
		return retval;
	}
	
	device_init_wakeup(&pdev->dev, true);
	
	hcd = usb_create_hcd(&act_hc_driver, &pdev->dev, dev_name(&pdev->dev));
	
	if (!hcd)
	{
		dev_err(&pdev->dev, "usb create hcd failed\n");
		return -ENOMEM;
	}
	
	platform_set_drvdata(pdev, hcd);
	aotg_hcd_init(hcd);
	
	hcd->regs = pdata->base;
	hcd->rsrc_start = pdata->rsrc_start;
	hcd->rsrc_len = pdata->rsrc_len;
	
	acthcd = hcd_to_aotg(hcd);
	acthcd->dev = &pdev->dev;
	acthcd->base = pdata->base;
	acthcd->hcd_exiting = 0;
	acthcd->uhc_irq = pdata->irq;
	acthcd->id = pdata->id;
	
	aotg_hardware_init(pdata);
	
	hcd->self.sg_tablesize = 32;
	hcd->has_tt = 1;
	hcd->self.uses_pio_for_control = 1;
	
	hrtimer_init(&acthcd->hotplug_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	acthcd->hotplug_timer.function = aotg_hub_hotplug_timer;
	
	timer_setup(&acthcd->trans_wait_timer, aotg_hub_trans_wait_timer, 0);
	timer_setup(&acthcd->check_trb_timer, aotg_check_trb_timer, 0);
	
	retval = usb_add_hcd(hcd, acthcd->uhc_irq, 0);
	
	if (retval)
	{
		//usb_remove_hcd(hcd);
		
		aotg_disable_irq(acthcd);
		aotg_powergate_off(pdata);
		
		acthcd->hcd_exiting = 1;
		
		tasklet_kill(&acthcd->urb_tasklet);
		del_timer_sync(&acthcd->trans_wait_timer);
		del_timer_sync(&acthcd->check_trb_timer);
		hrtimer_cancel(&acthcd->hotplug_timer);
	
		aotg_hcd_release_queue(acthcd, NULL);
		
		aotg_hcd_exit(hcd);
		usb_put_hcd(hcd);
		
		dev_info(&pdev->dev, "usb add hcd failed\n");
		return retval;
	}
	
	aotg_enable_irq(acthcd);
	device_wakeup_enable(&hcd->self.root_hub->dev);
	writeb(readb(acthcd->base + USBEIRQ), acthcd->base + USBEIRQ);
	dev_info(&pdev->dev, "caninos_hcd_probe() successfully finished\n");
	return 0;
}

static int caninos_hcd_remove(struct platform_device *pdev)
{
	return 0;
}

static int __maybe_unused caninos_hcd_pm_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused caninos_hcd_pm_resume(struct device *dev)
{
	return 0;
}

struct of_device_id caninos_hcd_dt_id[] = {
	{.compatible = "caninos,k7-usb2.0-0", .data = (void*)&aotg_data[0]},
	{.compatible = "caninos,k7-usb2.0-1", .data = (void*)&aotg_data[1]},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, caninos_hcd_dt_id);

static SIMPLE_DEV_PM_OPS(caninos_hcd_pm_ops, caninos_hcd_pm_suspend,
                         caninos_hcd_pm_resume);

struct platform_driver caninos_hcd_driver = {
	.driver = {
		.name = HCD_DRIVER_NAME,
		.of_match_table = caninos_hcd_dt_id,
		.owner = THIS_MODULE,
		.pm	= &caninos_hcd_pm_ops,
	},
	.probe = caninos_hcd_probe,
	.remove = caninos_hcd_remove,
	.shutdown = usb_hcd_platform_shutdown,
};

static int __init caninos_usb_module_init(void)
{
	td_cache = kmem_cache_create(HCD_DRIVER_NAME, sizeof(struct aotg_td), 0,
	                             SLAB_HWCACHE_ALIGN | SLAB_PANIC, NULL);
	platform_driver_register(&caninos_hcd_driver);
	return 0;
}

module_init(caninos_usb_module_init);

static void __exit caninos_usb_module_exit(void)
{
	platform_driver_unregister(&caninos_hcd_driver);
	kmem_cache_destroy(td_cache);
}

module_exit(caninos_usb_module_exit);

MODULE_DESCRIPTION(HCD_DRIVER_DESC);
MODULE_LICENSE("GPL");

