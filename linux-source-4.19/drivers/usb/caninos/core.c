#include <linux/module.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include "core.h"
#include "hcd.h"
#include "ep.h"

#define CONFIG_CANINOS_RESET_TIMEOUT 300000

struct kmem_cache *td_cache;

static int caninos_dev_reset(struct caninos_ctrl *ctrl)
{
	int timeout = CONFIG_CANINOS_RESET_TIMEOUT;
	struct device *dev = ctrl->dev;

	reset_control_assert(ctrl->rst);
	udelay(1);
	reset_control_deassert(ctrl->rst);
	
	do {
		if ((readb(ctrl->base + USBERESET) & USBERES_USBRESET) == 0) {
			break;
		}
		else {
			udelay(1);
		}
	} while (--timeout > 0);
	
	if (!(readb(ctrl->base + USBERESET) & USBERES_USBRESET)) {
		dev_info(dev, "reset OK\n");
	}
	else
	{
		dev_err(dev, "reset failed: 0x%x\n", readb(ctrl->base + USBERESET));
		return -EBUSY;
	}
	return 0;
}

static void caninos_set_phy(void __iomem *base, u8 reg, u8 value)
{
	u8 addrlow,	addrhigh;
	int	time = 1;
	
	addrlow	= reg & 0x0f;
	addrhigh = (reg >> 4) & 0x0f;
	
	/* write vstatus: */
	writeb(value, base + VDSTATE);
	mb();
	
	/* write vcontrol: */
	writeb(addrlow | 0x10, base + VDCTRL);
	udelay(time);  /* the vload period should > 33.3ns */
	writeb(addrlow & 0xf, base + VDCTRL);
	
	udelay(time);
	mb();
	
	writeb(addrhigh | 0x10, base + VDCTRL);
	udelay(time);
	writeb(addrhigh & 0x0f, base + VDCTRL);
	udelay(time);
	writeb(addrhigh | 0x10, base + VDCTRL);
	udelay(time);
}

static void caninos_configure_phy(struct caninos_ctrl *ctrl)
{
	caninos_set_phy(ctrl->base, 0xf4, 0xbb);
	caninos_set_phy(ctrl->base, 0xe1, 0xcf);
	caninos_set_phy(ctrl->base, 0xf4, 0x9b);
	caninos_set_phy(ctrl->base, 0xe6, 0xcc);
	caninos_set_phy(ctrl->base, 0xf4, 0xbb);
	caninos_set_phy(ctrl->base, 0xe2, 0x2);
	caninos_set_phy(ctrl->base, 0xe2, 0x16);
	caninos_set_phy(ctrl->base, 0xf4, 0x9b);
	caninos_set_phy(ctrl->base, 0xe7, 0xa1);
	caninos_set_phy(ctrl->base, 0xf4, 0xbb);
	caninos_set_phy(ctrl->base, 0xe0, 0x21);
	caninos_set_phy(ctrl->base, 0xe0, 0x25);
	caninos_set_phy(ctrl->base, 0xf4, 0x9b);
	caninos_set_phy(ctrl->base, 0xe4, 0xa6);
	caninos_set_phy(ctrl->base, 0xf0, 0xfc);
}



static void caninos_clk_enable(struct caninos_ctrl *ctrl, int enable)
{
	if (enable)
	{
		clk_prepare_enable(ctrl->clk_usbh_phy);
		clk_prepare_enable(ctrl->clk_usbh_pllen);
	}
	else
	{
		clk_disable_unprepare(ctrl->clk_usbh_pllen);
		clk_disable_unprepare(ctrl->clk_usbh_phy);
	}
}

static int caninos_power_enable(struct caninos_ctrl *ctrl, int enable)
{
	return 0; /* not implemented */
}

/**
 * Configure External Control and Status Resgister && Timer Resgisters
 *
 * For USBECS, we care about Soft idpin and Soft Vbus that control the
 * OTG FSM.
 * Bit name:         idpin Enable    idpin    Vbus Enable   Vbus
 * Legacy Mode:       bit 26        bit 27       bit 24       bit 25
 * New Mode (500):    bit 26        bit 27       bit 24       bit 25
 * New Mode (700):    bit 26        bit 27       bit 24       bit 25
 * New Mode (900):    bit 26        bit 27       bit 24       bit 25
 */
static void caninos_configure_ecs(struct caninos_ctrl *ctrl)
{
	/* Set DMA Chain Mode */
	writel(0x1, ctrl->base + HCDMABCKDOOR);
	
	/* Set idpin: 0; Set Soft Vbus: high */
	writel(0x37000000 | (0x3 << 4), ctrl->usbecs);
	udelay(100);
	
	caninos_configure_phy(ctrl);
	
	/***** TA_BCON_COUNT *****/
	writeb(0x0, ctrl->base + TA_BCON_COUNT); /* 110ms */
	
	/* set TA_SUSPEND_BDIS timeout never generate */
	usb_writeb(0xff, ctrl->base + TAAIDLBDIS);
	
	/* set TA_AIDL_BDIS timeout never generate */
	usb_writeb(0xff, ctrl->base + TAWAITBCON);
	
	/* set TA_WAIT_BCON timeout never generate */
	usb_writeb(0x28, ctrl->base + TBVBUSDISPLS);
	usb_setbitsb(0x1 << 7, ctrl->base + TAWAITBCON);
	
	usb_writew(0x1000, ctrl->base + VBUSDBCTIMERL);
}

void caninos_host_exit(struct caninos_ctrl *ctrl)
{
	/* OTG state is a_host */
	
	/* End the session: -> a_suspend */
	usb_clearbitsb(OTGCTRL_BUSREQ, ctrl->base + OTGCTRL);
	
	/* Set soft idpin: -> a_wait_vfall -> a_idle -> b_idle */
	usb_setbitsl(0x1 << 27, ctrl->usbecs);
	
	caninos_clk_enable(ctrl, 0);
	caninos_power_enable(ctrl, 0);
}

static struct of_device_id caninos_of_match[] = {
	{.compatible = "caninos,k7-usb2.0-0", .data = (void*)(0)},
	{.compatible = "caninos,k7-usb2.0-1", .data = (void*)(1)},
	{},
};

MODULE_DEVICE_TABLE(of, caninos_of_match);

static void caninos_hardware_init(struct caninos_ctrl *ctrl)
{
	caninos_power_enable(ctrl, 1);
	caninos_clk_enable(ctrl, 1);
	caninos_dev_reset(ctrl);
	caninos_configure_ecs(ctrl);
}

static void caninos_myhcd_init(struct caninos_ctrl *ctrl)
{
	struct caninos_hcd *myhcd = hcd_to_caninos(ctrl->hcd);
	int i;
	
	myhcd->hcd_exiting = false;
	myhcd->inserted = false;
	myhcd->port = 0;
	myhcd->rhstate = AOTG_RH_POWEROFF;
	myhcd->speed = USB_SPEED_HIGH;
	myhcd->ctrl = ctrl;
	myhcd->active_ep0 = NULL;
	myhcd->tasklet_retry = 0;
	
	myhcd->fifo_map[0] = (0x1 << 31);
	myhcd->fifo_map[1] = (0x1 << 31) | 64;
	
	for (i = 2; i < 64; i++) {
		myhcd->fifo_map[i] = 0;
	}
	
	for (i = 0; i < MAX_EP_NUM; i++)
	{
		myhcd->ep0[i] = NULL;
		myhcd->inep[i] = NULL;
		myhcd->outep[i] = NULL;
	}
	
	for (i = 0; i < AOTG_QUEUE_POOL_CNT; i++) {
		myhcd->queue_pool[i] = NULL;
	}
	
	spin_lock_init(&myhcd->lock);
	spin_lock_init(&myhcd->tasklet_lock);
	
	INIT_LIST_HEAD(&myhcd->hcd_enqueue_list);
	INIT_LIST_HEAD(&myhcd->hcd_dequeue_list);
	INIT_LIST_HEAD(&myhcd->hcd_finished_list);
	
	tasklet_init(&myhcd->urb_tasklet, urb_tasklet_func, (unsigned long)myhcd);
}

static int caninos_hcd_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	struct caninos_ctrl *ctrl;
	struct resource *res_mem;
	int retval;
	
	dev->coherent_dma_mask = DMA_BIT_MASK(32); 
	dev->dma_mask = &dev->coherent_dma_mask;
	
	ctrl = devm_kzalloc(dev, sizeof(struct caninos_ctrl), GFP_KERNEL);
	
	if (!ctrl)
	{
		dev_err(dev, "could not alloc driver private data.\n");
		return -ENOMEM;
	}
	
	ctrl->dev = dev;
	
	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res_mem)
	{
		dev_err(dev, "usb has no resource for mem.\n");
		return -ENODEV;
	}
	
	ctrl->irq = platform_get_irq(pdev, 0);
	
	if (ctrl->irq <= 0)
	{
		dev_err(dev, "usb has no resource for irq.\n");
		return -ENODEV;
	}
	
	of_id = of_match_node(caninos_of_match, dev->of_node);
	
	if (!of_id)
	{
		dev_err(dev, "could not match device type.\n");
		return -ENODEV;
	}
	
	ctrl->base = devm_ioremap(dev, res_mem->start, resource_size(res_mem));
	
	if (!ctrl->base)
	{
		dev_err(dev, "could not remap reg base.\n");
		return -ENODEV;
	}
	
	ctrl->id = (of_id->data == (void*)(1)) ? 1 : 0;
	pdev->id = ctrl->id;
	
	if (ctrl->id == 0) {
		ctrl->usbecs = devm_ioremap_nocache(dev, 0xE024C094, 4);
	}
	else {
		ctrl->usbecs = devm_ioremap_nocache(dev, 0xE024C098, 4);
	}
	
	if (!ctrl->usbecs)
	{
		dev_err(dev, "could not remap usbecs reg.\n");
		return -ENODEV;
	}
	
	if (ctrl->id == 0) {
		ctrl->rst = devm_reset_control_get(dev, "usb2h0");
	}
	else {
		ctrl->rst = devm_reset_control_get(dev, "usb2h1");
	}
	
	if (!ctrl->rst)
	{
		dev_err(dev, "could not get device reset control.\n");
		return -ENODEV;
	}
	
	if (ctrl->id == 0) {
		ctrl->clk_usbh_pllen = devm_clk_get(dev, "usbh0_pllen");
	}
	else {
		ctrl->clk_usbh_pllen = devm_clk_get(dev, "usbh1_pllen");
	}
	
	if (IS_ERR(ctrl->clk_usbh_pllen))
	{
		dev_err(dev, "unable to get usbh_pllen\n");
		return -EINVAL;
	}
	
	if (ctrl->id == 0) {
		ctrl->clk_usbh_phy = devm_clk_get(dev, "usbh0_phy");
	}
	else {
		ctrl->clk_usbh_phy = devm_clk_get(dev, "usbh1_phy");
	}
	
	if (IS_ERR(ctrl->clk_usbh_phy))
	{
		dev_err(dev, "unable to get usbh_phy\n");
		return -EINVAL;
	}
	
	ctrl->hcd = usb_create_hcd(&caninos_hc_driver, dev, dev_name(dev));
	
	if (!ctrl->hcd)
	{
		dev_err(dev, "usb create hcd failed\n");
		return -ENOMEM;
	}
	
	ctrl->hcd->rsrc_start = res_mem->start;
	ctrl->hcd->rsrc_len = resource_size(res_mem);
	ctrl->hcd->regs = ctrl->base;
	ctrl->hcd->self.sg_tablesize = 32;
	ctrl->hcd->has_tt = 1;
	ctrl->hcd->self.uses_pio_for_control = 1;
	
	caninos_myhcd_init(ctrl);
	
	caninos_hardware_init(ctrl);
	
	retval = usb_add_hcd(ctrl->hcd, ctrl->irq, IRQF_NOBALANCING | IRQF_SHARED);
	
	if (retval)
	{
		dev_err(dev, "usb add hcd failed\n");
		usb_put_hcd(ctrl->hcd);
		ctrl->hcd = NULL;
		return retval;
	}
	
	caninos_enable_irq(ctrl);
	
	return 0;
}

static int caninos_hcd_remove(struct platform_device *pdev)
{
	return 0;
}

struct platform_driver caninos_hcd_driver = {
	.probe  = caninos_hcd_probe,
	.remove = caninos_hcd_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name  = DRIVER_NAME,
		.of_match_table = caninos_of_match,
	},
};

static int __init caninos_init(void)
{
	td_cache = kmem_cache_create(DRIVER_NAME, sizeof(struct aotg_td), 0, 
	                             SLAB_HWCACHE_ALIGN | SLAB_PANIC, NULL);
	return platform_driver_register(&caninos_hcd_driver);
}

static void __exit caninos_exit(void)
{
	platform_driver_unregister(&caninos_hcd_driver);
	kmem_cache_destroy(td_cache);
}

module_init(caninos_init);
module_exit(caninos_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

