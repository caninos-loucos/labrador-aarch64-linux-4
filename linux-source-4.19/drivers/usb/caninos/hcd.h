#ifndef _HCD_H_
#define _HCD_H_

#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/otg.h>
#include <linux/usb/hcd.h>
#include <linux/slab.h>

#define MAX_EP_NUM 16
#define MAX_ERROR_COUNT	6

#define AOTG_MAX_FIFO_SIZE (1024*15 + 64*2)
#define ALLOC_FIFO_UNIT 64

#define NUM_TRBS (256)
#define RING_SIZE (NUM_TRBS * 16)

#define MY_OTG_PORT_C_MASK ((USB_PORT_STAT_C_CONNECTION \
	| USB_PORT_STAT_C_ENABLE \
	| USB_PORT_STAT_C_SUSPEND \
	| USB_PORT_STAT_C_OVERCURRENT \
	| USB_PORT_STAT_C_RESET) << 16)
	
enum aotg_rh_state {
	AOTG_RH_POWEROFF,
	AOTG_RH_POWERED,
	AOTG_RH_ATTACHED,
	AOTG_RH_NOATTACHED,
	AOTG_RH_RESET,
	AOTG_RH_ENABLE,
	AOTG_RH_DISABLE,
	AOTG_RH_SUSPEND,
	AOTG_RH_ERR
};

struct caninos_hcd
{
	u32 port; /* port status */
	
	bool hcd_exiting;
	bool inserted;
	
	volatile int tasklet_retry;
	
	enum aotg_rh_state rhstate;
	
	struct caninos_ctrl *ctrl;
	
	enum usb_device_speed speed;
	
	spinlock_t lock;
	spinlock_t tasklet_lock;
	
	/*
	when using hub, every usb device need a ep0 hcep data struct,
	but share the same hcd ep0.
	*/
	struct aotg_hcep *active_ep0;
	int ep0_block_cnt;
	struct aotg_hcep *ep0[MAX_EP_NUM];
	struct aotg_hcep *inep[MAX_EP_NUM]; /* 0 for reserved */
	struct aotg_hcep *outep[MAX_EP_NUM]; /* 0 for reserved */
	
	struct list_head hcd_enqueue_list;
	struct list_head hcd_dequeue_list;
	struct list_head hcd_finished_list;
	struct tasklet_struct urb_tasklet;
	
	ulong fifo_map[AOTG_MAX_FIFO_SIZE / ALLOC_FIFO_UNIT];
	
	#define AOTG_QUEUE_POOL_CNT	60
	struct aotg_queue *queue_pool[AOTG_QUEUE_POOL_CNT];
	
	bool check_trb_mutex;
};

extern struct hc_driver caninos_hc_driver;

static inline struct caninos_hcd *hcd_to_caninos(struct usb_hcd *hcd)
{
	return (struct caninos_hcd *) (hcd->hcd_priv);
}

static inline struct usb_hcd *caninos_to_hcd(struct caninos_hcd *myhcd)
{
	return myhcd->ctrl->hcd;
}

static inline void caninos_disable_irq(struct caninos_ctrl *ctrl)
{
	writeb(USBEIRQ_USBIEN, ctrl->base + USBEIRQ);
	usb_clearbitsb(USBEIRQ_USBIEN, ctrl->base + USBEIEN);
	usb_clearbitsb(OTGIEN_LOCSOF, ctrl->base + OTGIEN);
	usb_clearbitsb(OTGCTRL_BUSREQ, ctrl->base + OTGCTRL);
}

static inline void caninos_enable_irq(struct caninos_ctrl *ctrl)
{
	writeb(USBEIRQ_USBIEN, ctrl->base + USBEIRQ);
	usb_setbitsb(USBEIRQ_USBIEN, ctrl->base + USBEIEN);
	usb_setbitsb(OTGIEN_LOCSOF, ctrl->base + OTGIEN);
	usb_setbitsb(OTGCTRL_BUSREQ, ctrl->base + OTGCTRL);
}

static inline void caninos_clear_all_overflow_irq(struct usb_hcd *hcd)
{
	unsigned int irq_pend = 0;
	
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);
	
	irq_pend = readl(myhcd->ctrl->base + HCDMAxOVERFLOWIRQ);
	
	if (irq_pend) {
		writel(irq_pend, myhcd->ctrl->base + HCDMAxOVERFLOWIRQ);
	}
}

static inline void caninos_clear_all_shortpkt_irq(struct usb_hcd *hcd)
{
	unsigned int irq_pend = 0;
	
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);

	irq_pend = readw(myhcd->ctrl->base + HCINxSHORTPCKIRQ0);

	if (irq_pend) {
		writew(irq_pend, myhcd->ctrl->base + HCINxSHORTPCKIRQ0);
	}
}

static inline void caninos_clear_all_zeropkt_irq(struct usb_hcd *hcd)
{
	unsigned int irq_pend = 0;
	
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);

	irq_pend = readw(myhcd->ctrl->base + HCINxZEROPCKIEN0);

	if (irq_pend) {
		writew(irq_pend, myhcd->ctrl->base + HCINxZEROPCKIEN0);
	}
}

static inline void caninos_clear_all_hcoutdma_irq(struct usb_hcd *hcd)
{
	unsigned int irq_pend = 0;
	
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);

	irq_pend = readw(myhcd->ctrl->base + HCOUTxDMAIRQ0);

	if (irq_pend) {
		writew(irq_pend, myhcd->ctrl->base + HCOUTxDMAIRQ0);
	}
}

extern enum hrtimer_restart caninos_hub_hotplug_timer(struct hrtimer *hrtimer);

extern void urb_tasklet_func(unsigned long data);

#endif

