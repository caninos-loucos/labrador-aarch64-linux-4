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
#include "aotg.h"

static int handle_setup_packet(struct aotg_hcd *acthcd, struct aotg_queue *q);
static void handle_hcep0_in(struct aotg_hcd *acthcd);
static void handle_hcep0_out(struct aotg_hcd *acthcd);

typedef void (*aotg_hub_symbol_func_t)(int);
aotg_hub_symbol_func_t aotg_hub_notify_func;

static void aotg_hub_notify_hcd_exit(int state)
{
	static int is_first_call = 1;

	if (is_first_call) {
		is_first_call = 0;
		aotg_hub_notify_func = (aotg_hub_symbol_func_t)kallsyms_lookup_name("aotg_hub_notify_exit");
	}
	if (aotg_hub_notify_func)
		aotg_hub_notify_func(state);

	return;
}

static ulong get_fifo_addr(struct aotg_hcd *acthcd, int size)
{
	int i, j;
	ulong addr = 0;
	int mul = size / ALLOC_FIFO_UNIT;
	int max_unit = AOTG_MAX_FIFO_SIZE/ALLOC_FIFO_UNIT;
	int find_next = 0;

	if (mul == 0)
		mul = 1;

	for (i = 2; i < max_unit;) {
		if (acthcd->fifo_map[i] != 0) {
			i++;
			continue; /*find first unused addr*/
		}

		for (j = i; j < max_unit; j++) {
			if ((j - i + 1) == mul)
				break;

			if (acthcd->fifo_map[j]) {
				i = j;
				find_next = 1;
				break;
			}
		}

		if (j == max_unit) {
			break;
		} else if (find_next) {
			find_next = 0;
			continue;
		} else {
			int k;
			for (k = i; k <= j; k++)
				acthcd->fifo_map[k] = (1 << 31) | (i * 64);

			addr = i * ALLOC_FIFO_UNIT;
			break;
		}
	}

	return addr;
}

static void release_fifo_addr(struct aotg_hcd *acthcd, ulong addr)
{
	int i;

	for (i = addr/ALLOC_FIFO_UNIT; i < AOTG_MAX_FIFO_SIZE/ALLOC_FIFO_UNIT; i++) {
		if ((acthcd->fifo_map[i] & 0x7FFFFFFF) == addr)
			acthcd->fifo_map[i] = 0;
		else
			break;
	}
	return;
}

static struct aotg_queue *aotg_hcd_get_queue(struct aotg_hcd *acthcd,
	struct urb *urb, unsigned mem_flags)
{
	int i;
	int empty_idx = -1;
	struct aotg_queue *q = NULL;

	for (i = 0; i < AOTG_QUEUE_POOL_CNT; i++) {
		if (acthcd->queue_pool[i] != NULL) {
			if (acthcd->queue_pool[i]->in_using == 0) {
				q = acthcd->queue_pool[i];
				break;
			}
		} else {
			if (empty_idx < 0)
				empty_idx = i;
		}
	}
	if (i == AOTG_QUEUE_POOL_CNT) {
		q = kzalloc(sizeof(*q), GFP_ATOMIC);
		if (unlikely(!q)) {
			dev_err(acthcd->dev, "aotg_hcd_get_queue failed\n");
			return NULL;
		}
		if ((empty_idx >= 0) && (empty_idx < AOTG_QUEUE_POOL_CNT))
			acthcd->queue_pool[empty_idx] = q;
	}

	memset(q, 0, sizeof(*q));
	q->length = 0;
	q->td.trb_vaddr = NULL;
	INIT_LIST_HEAD(&q->enqueue_list);
	INIT_LIST_HEAD(&q->dequeue_list);
	INIT_LIST_HEAD(&q->finished_list);

	q->in_using = 1;
	return q;
}

void aotg_hcd_release_queue(struct aotg_hcd *acthcd, struct aotg_queue *q)
{
	int i;

	/* release all */
	if (q == NULL) {
		for (i = 0; i < AOTG_QUEUE_POOL_CNT; i++) {
			if (acthcd->queue_pool[i] != NULL) {
				kfree(acthcd->queue_pool[i]);
				acthcd->queue_pool[i] = NULL;
			}
		}
		return;
	}

	q->td.trb_vaddr = NULL;

	for (i = 0; i < AOTG_QUEUE_POOL_CNT; i++) {
		if (acthcd->queue_pool[i] == q) {
			acthcd->queue_pool[i]->in_using = 0;
			return;
		}
	}

	kfree(q);
	return;
}

static inline int is_epfifo_busy(struct aotg_hcep *ep, int is_in)
{
	if (is_in)
		return (EPCS_BUSY & readb(ep->reg_hcepcs)) == 0;
	else
		return (EPCS_BUSY & readb(ep->reg_hcepcs)) != 0;
}

static inline void ep_setup(struct aotg_hcep *ep, u8 type, u8 buftype)
{
	ep->buftype = buftype;
	writeb(type | buftype, ep->reg_hcepcon);
}

static inline void pio_irq_disable(struct aotg_hcd *acthcd, u8 mask)
{
	u8 is_out = mask & USB_HCD_OUT_MASK;
	u8 ep_num = mask & 0x0f;

	if (is_out)
		usb_clearbitsw(1 << ep_num, acthcd->base + HCOUTxIEN0);
	else
		usb_clearbitsw(1 << ep_num, acthcd->base + HCINxIEN0);

	return;
}

static inline void pio_irq_enable(struct aotg_hcd *acthcd, u8 mask)
{
	u8 is_out = mask & USB_HCD_OUT_MASK;
	u8 ep_num = mask & 0x0f;

	if (is_out)
		usb_setbitsw(1 << ep_num, acthcd->base + HCOUTxIEN0);
	else
		usb_setbitsw(1 << ep_num, acthcd->base + HCINxIEN0);

	return;
}

static inline void pio_irq_clear(struct aotg_hcd *acthcd, u8 mask)
{
	u8 is_out = mask & USB_HCD_OUT_MASK;
	u8 ep_num = mask & 0x0f;

	if (is_out)
		writew(1 << ep_num, acthcd->base + HCOUTxIRQ0);
	else
		writew(1 << ep_num, acthcd->base + HCINxIRQ0);

	return;
}

static inline void ep_enable(struct aotg_hcep *ep)
{
	usb_setbitsb(0x80, ep->reg_hcepcon);
}

static inline void ep_disable(struct aotg_hcep *ep)
{
	usb_clearbitsb(0x80, ep->reg_hcepcon);
}

static inline void aotg_sofirq_clr(struct aotg_hcd *acthcd)
{
	usb_setbitsb((1 << 1), acthcd->base + USBIRQ);
}

static inline void aotg_sofirq_on(struct aotg_hcd *acthcd)
{
	usb_setbitsb((1 << 1), acthcd->base + USBIEN);
}

static inline void aotg_sofirq_off(struct aotg_hcd *acthcd)
{
	usb_clearbitsb(1 << 1, acthcd->base + USBIEN);
}

static inline int get_subbuffer_count(u8 buftype)
{
	int count = 0;

	switch (buftype) {
	case EPCON_BUF_SINGLE:
		count = 1;
		break;
	case EPCON_BUF_DOUBLE:
		count = 2;
		break;
	case EPCON_BUF_TRIPLE:
		count = 3;
		break;
	case EPCON_BUF_QUAD:
		count = 4;
		break;
	}

	return count;
}

static inline void aotg_config_hub_addr(struct urb *urb, struct aotg_hcep *ep)
{
	if (ep->has_hub) {
		if (urb->dev->speed == USB_SPEED_HIGH) {
			writeb(usb_pipedevice(urb->pipe), ep->reg_hcep_dev_addr);
			writeb(urb->dev->portnum, ep->reg_hcep_port);
		} else {
			writeb((0x80 | usb_pipedevice(urb->pipe)), ep->reg_hcep_dev_addr);
			if (urb->dev->speed == USB_SPEED_LOW)
				writeb(0x80 | urb->dev->portnum, ep->reg_hcep_port);
			else
				writeb(urb->dev->portnum, ep->reg_hcep_port);
		}
	} else {
		writeb(usb_pipedevice(urb->pipe), ep->reg_hcep_dev_addr);
		writeb(urb->dev->portnum, ep->reg_hcep_port);
	}
}

static void aotg_start_ring_transfer(struct aotg_hcd *acthcd,
	struct aotg_hcep *ep, struct urb *urb)
{
	u32 addr;
	struct aotg_trb *trb;
	struct aotg_ring *ring = ep->ring;

	aotg_config_hub_addr(urb, ep);
	if (usb_pipetype(urb->pipe) == PIPE_INTERRUPT) {
		writeb(ep->interval, ep->reg_hcep_interval);
		if (ring->is_out) {
			trb = ring->dequeue_trb;
			trb->hw_buf_ptr = urb->transfer_dma;
			trb->hw_buf_len = urb->transfer_buffer_length;
		}

	}
	ep_enable(ep);
	addr = ring_trb_virt_to_dma(ring, ring->dequeue_trb);
	aotg_start_ring(ring, addr);
}

static int aotg_hcep_config(struct aotg_hcd *acthcd,
	struct aotg_hcep *ep,
	u8 type, u8 buftype, int is_out)
{
	int index = 0;
	ulong addr = 0;
	int get_ep = 0;
	int subbuffer_count;

	subbuffer_count = get_subbuffer_count(buftype);
	if (0 == subbuffer_count) {
		dev_err(acthcd->dev, "error buftype: %02X, %s, %d\n",
			buftype, __func__, __LINE__);
		return -EPIPE;
	}

	if (is_out) {
		for (index = 1; index < MAX_EP_NUM; index++) {
			if (acthcd->outep[index] == NULL) {
				ep->is_out = 1;
				ep->index = index;
				ep->mask = (u8) (USB_HCD_OUT_MASK | index);
				acthcd->outep[index] = ep;
				get_ep = 1;
				break;
			}
		}
	} else {
		for (index = 1; index < MAX_EP_NUM; index++) {
			if (acthcd->inep[index] == NULL) {
				ep->is_out = 0;
				ep->index = index;
				ep->mask = (u8) index;
				acthcd->inep[index] = ep;
				get_ep = 1;
				break;
			}
		}
	}

	if (!get_ep) {
		dev_err(acthcd->dev, "%s: no more available space for ep\n", __func__);
		return -ENOSPC;
	}
	
	addr = get_fifo_addr(acthcd, subbuffer_count * MAX_PACKET(ep->maxpacket));
	if (addr == 0) {
		dev_err(acthcd->dev, "buffer configuration overload!! addr: %08X, subbuffer_count: %d, ep->maxpacket: %u\n",
			(u32)addr, subbuffer_count, MAX_PACKET(ep->maxpacket));
		if (is_out)
			acthcd->outep[ep->index] = NULL;
		else
			acthcd->inep[ep->index] = NULL;

		return -ENOSPC;
	} else {
		ep->fifo_addr = addr;
	}

	ep->reg_hcepcon = get_hcepcon_reg(is_out,
		acthcd->base + HCOUT1CON,
		acthcd->base + HCIN1CON,
		ep->index);
	ep->reg_hcepcs = get_hcepcs_reg(is_out,
		acthcd->base + HCOUT1CS,
		acthcd->base + HCIN1CS,
		ep->index);
	ep->reg_hcepbc = get_hcepbc_reg(is_out,
		acthcd->base + HCOUT1BCL,
		acthcd->base + HCIN1BCL,
		ep->index);
	ep->reg_hcepctrl = get_hcepctrl_reg(is_out,
		acthcd->base + HCOUT1CTRL,
		acthcd->base + HCIN1CTRL,
		ep->index);
	ep->reg_hcmaxpck = get_hcepmaxpck_reg(is_out,
		acthcd->base + HCOUT1MAXPCKL,
		acthcd->base + HCIN1MAXPCKL,
		ep->index);
	ep->reg_hcepaddr = get_hcepaddr_reg(is_out,
		acthcd->base + HCOUT1STADDR,
		acthcd->base + HCIN1STADDR,
		ep->index);
	ep->reg_hcep_dev_addr = get_hcep_dev_addr_reg(is_out,
		acthcd->base + HCOUT1ADDR,
		acthcd->base + HCIN1ADDR,
		ep->index);
	ep->reg_hcep_port = get_hcep_port_reg(is_out,
		acthcd->base + HCOUT1PORT,
		acthcd->base + HCIN1PORT,
		ep->index);
	ep->reg_hcep_splitcs = get_hcep_splitcs_reg(is_out,
		acthcd->base + HCOUT1SPILITCS,
		acthcd->base + HCIN1SPILITCS,
		ep->index);

	if (!is_out) {
		ep->reg_hcerr = acthcd->base + HCIN0ERR + ep->index * 0x4;
		ep->reg_hcep_interval = acthcd->base + HCEP0BINTERVAL + ep->index * 0x8;
	} else {
		ep->reg_hcerr = acthcd->base + HCOUT0ERR + ep->index * 0x4;
		ep->reg_hcep_interval = acthcd->base + HCOUT1BINTERVAL + (ep->index - 1) * 0x8;
	}

#ifdef DEBUG_EP_CONFIG
	dev_info(acthcd->dev, "== ep->index: %d, is_out: %d, fifo addr: %08X\n", ep->index, is_out, (u32)addr);
	dev_info(acthcd->dev, "== reg_hcepcon: %08lX, reg_hcepcs: %08lX, reg_hcepbc: %08lX, reg_hcepctrl: %08lX, reg_hcmaxpck: %08lX, ep->reg_hcepaddr: %08lX\n",
			ep->reg_hcepcon,
			ep->reg_hcepcs,
			ep->reg_hcepbc,
			ep->reg_hcepctrl,
			ep->reg_hcmaxpck,
			ep->reg_hcepaddr);
#endif

	pio_irq_disable(acthcd, ep->mask);
	pio_irq_clear(acthcd, ep->mask);

	ep_disable(ep);

	/*allocate buffer address of ep fifo */
	writel(addr, ep->reg_hcepaddr);
	writew(ep->maxpacket, ep->reg_hcmaxpck);
	ep_setup(ep, type, buftype);	/*ep setup */

	/*reset this ep */
	usb_settoggle(ep->udev, ep->epnum, is_out, 0);
	aotg_hcep_reset(acthcd, ep->mask, ENDPRST_FIFORST | ENDPRST_TOGRST);
	writeb(ep->epnum, ep->reg_hcepctrl);
	return 0;
}

static int aotg_hcep_set_split_micro_frame(struct aotg_hcd *acthcd, struct aotg_hcep *ep)
{
	static const u8 split_val[] = {0x31, 0x42, 0x53, 0x64, 0x75, 0x17, 0x20};
	int i, index;
	u8 set_val, rd_val;

	for (i = 0; i < sizeof(split_val); i++) {
		set_val = split_val[i];

		for (index = 0; index < MAX_EP_NUM; index++) {
			if (acthcd->inep[index] != NULL) {
				rd_val = acthcd->inep[index]->reg_hcep_splitcs_val;

				if ((0 == rd_val) || (set_val != rd_val))
					continue;

				if (set_val == rd_val)
					set_val = 0;
				break;
			}
		}
		if (set_val == 0)
			continue;

		for (index = 0; index < MAX_EP_NUM; index++) {
			if (acthcd->outep[index] != NULL) {
				rd_val = acthcd->outep[index]->reg_hcep_splitcs_val;

				if ((0 == rd_val) || (set_val != rd_val))
					continue;

				if (set_val == rd_val)
					set_val = 0;
				break;
			}
		}

		if (set_val != 0)
			break;
	}

	if (set_val != 0) {
		ep->reg_hcep_splitcs_val = set_val;
		writeb(set_val, ep->reg_hcep_splitcs);
		pr_info("====reg_hcep_splitcs_val:%x, index:%d\n", set_val, ep->index);
	}
	return 0;
}

static void finish_request(struct aotg_hcd *acthcd,
	struct aotg_queue *q, int status)
{
	struct urb *urb = q->urb;

	if (unlikely((acthcd == NULL) || (q == NULL) || (urb == NULL))) {
		WARN_ON(1);
		return;
	}

	q->status = status;
	if (list_empty(&q->finished_list))
		list_add_tail(&q->finished_list, &acthcd->hcd_finished_list);
	else
		ACT_HCD_ERR

	tasklet_hi_schedule(&acthcd->urb_tasklet);
	return;
}

static void tasklet_finish_request(struct aotg_hcd *acthcd,
	struct aotg_queue *q, int status)
{
	struct urb *urb = q->urb;
	struct aotg_hcep *ep = q->ep;

	if (unlikely((acthcd == NULL) || (q == NULL) || (urb == NULL))) {
		WARN_ON(1);
		return;
	}

	if ((q != NULL) && (ep != NULL)) {
		if (ep->q == NULL) {
			ACT_HCD_ERR
		} else {
			if (ep->q == q)
				ep->q = NULL;
		}
	} else {
		ACT_HCD_ERR
		return;
	}

	if (status == 0)
		q->err_count = 0;

	if (usb_pipetype(urb->pipe) == PIPE_CONTROL) {
		if ((acthcd->active_ep0 != NULL) && (acthcd->active_ep0 == q->ep)) {
			if (acthcd->active_ep0->q == NULL)
				acthcd->active_ep0 = NULL;
			else
				ACT_HCD_ERR
		} else {
			ACT_HCD_ERR
		}
	}


	aotg_hcd_release_queue(acthcd, q);

	ep->urb_endque_cnt++;
	return;
}

static inline void handle_status(struct aotg_hcd *acthcd,
	struct aotg_hcep *ep, int is_out)
{
	/*status always DATA1,set 1 to ep0 toggle */
	writeb(EP0CS_HCSETTOOGLE, acthcd->base + EP0CS);

	if (is_out)
		writeb(0, acthcd->base + HCIN0BC);	/*recv 0 packet*/
	else
		writeb(0, acthcd->base + HCOUT0BC);	/*send 0 packet*/
}

static void write_hcep0_fifo(struct aotg_hcd *acthcd,
	struct aotg_hcep *ep, struct urb *urb)
{
	u32 *buf;
	int length, count;
	void __iomem *addr = acthcd->base + EP0INDATA_W0;

	if (!(readb(acthcd->base + EP0CS) & EP0CS_HCOUTBSY)) {
		buf = (u32 *) (urb->transfer_buffer + urb->actual_length);
		prefetch(buf);

		/* how big will this packet be? */
		length = min((int)ep->maxpacket, (int)urb->transfer_buffer_length - (int)urb->actual_length);

		count = length >> 2;	/*wirte in DWORD */
		if (length & 0x3)
			count++;

		while (likely(count--)) {
			writel(*buf, addr);
			buf++;
			addr += 4;
		}

		ep->length = length;
		writeb(length, acthcd->base + HCOUT0BC);
		usb_dotoggle(urb->dev, usb_pipeendpoint(urb->pipe), 1);
	} else {
		dev_err(acthcd->dev, "<CTRL>OUT data is not ready\n");
	}
}

static void read_hcep0_fifo(struct aotg_hcd *acthcd,
	struct aotg_hcep *ep, struct urb *urb)
{
	u8 *buf;
	unsigned overflag, is_short, shorterr, is_last;
	unsigned length, count;
	struct usb_device *udev;
	void __iomem *addr = acthcd->base + EP0OUTDATA_W0;	/*HCEP0INDAT0;*/
	unsigned bufferspace;

	overflag = 0;
	is_short = 0;
	shorterr = 0;
	is_last = 0;
	udev = ep->udev;

	if (readb(acthcd->base + EP0CS) & EP0CS_HCINBSY) {
		dev_err(acthcd->dev, "<CTRL>IN data is not ready\n");
		return;
	} else {
		usb_dotoggle(udev, ep->epnum, 0);
		buf = urb->transfer_buffer + urb->actual_length;
		bufferspace = urb->transfer_buffer_length - urb->actual_length;
		/*prefetch(buf);*/

		length = count = readb(acthcd->base + HCIN0BC);
		if (length > bufferspace) {
			count = bufferspace;
			urb->status = -EOVERFLOW;
			overflag = 1;
		}

		urb->actual_length += count;
		while (count--) {
			*buf++ = readb(addr);
#if 0
			buf--;
			pr_info("ep0in:%x, cnt:%d\n", (unsigned int)*buf, count);
			buf++;
#endif
			addr++;
		}

		if (urb->actual_length >= urb->transfer_buffer_length) {
			ep->nextpid = USB_PID_ACK;
			is_last = 1;
			handle_status(acthcd, ep, 0);
		} else if (length < ep->maxpacket) {
			is_short = 1;
			is_last = 1;
			if (urb->transfer_flags & URB_SHORT_NOT_OK) {
				urb->status = -EREMOTEIO;
				shorterr = 1;
			}
			ep->nextpid = USB_PID_ACK;
			handle_status(acthcd, ep, 0);
		} else {
			writeb(0, acthcd->base + HCIN0BC);
		}
	}
}

static int handle_setup_packet(struct aotg_hcd *acthcd, struct aotg_queue *q)
{
	struct urb *urb = q->urb;
	struct aotg_hcep *ep = q->ep;
	u32 *buf;
	void __iomem *addr = acthcd->base + EP0INDATA_W0;
	int i = 0;

#ifdef DEBUG_SETUP_DATA
	u16 w_value, w_index, w_length;
	struct usb_ctrlrequest *ctrlreq;

	ctrlreq = (struct usb_ctrlrequest *)urb->setup_packet;
	w_value = le16_to_cpu(ctrlreq->wValue);
	w_index = le16_to_cpu(ctrlreq->wIndex);
	w_length = le16_to_cpu(ctrlreq->wLength);
	dev_info(acthcd->dev, "<CTRL>SETUP stage  %02x.%02x V%04x I%04x L%04x\n ",
		ctrlreq->bRequestType, ctrlreq->bRequest, w_value, w_index,
		w_length);
#endif
	if ((q->is_xfer_start) || (ep->q)) {
		ACT_HCD_DBG
		pr_info("q->is_xfer_start:%d\n", q->is_xfer_start);
		return 0;
	}
	if (unlikely(!HC_IS_RUNNING(aotg_to_hcd(acthcd)->state))) {
		ACT_HCD_DBG
		return -ESHUTDOWN;
	}
	if (acthcd->active_ep0 != NULL) {
		ACT_HCD_ERR
		return -EBUSY;
	}

	writeb(ep->epnum, acthcd->base + HCEP0CTRL);
	writeb((u8)ep->maxpacket, acthcd->base + HCIN0MAXPCK);

	acthcd->active_ep0 = ep;
	ep->q = q;
	q->is_xfer_start = 1;
	usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), 1, 1);
	ep->nextpid = USB_PID_SETUP;
	buf = (u32 *)urb->setup_packet;

	/*initialize the setup stage */
	writeb(EP0CS_HCSET, acthcd->base + EP0CS);
	while (readb(acthcd->base + EP0CS) & EP0CS_HCOUTBSY) {
		writeb(EP0CS_HCSET, acthcd->base + EP0CS);
		i++;
		if (i > 2000000) {
			pr_err("handle_setup timeout!\n");
			break;
		}
	}

	if (!(readb(acthcd->base + EP0CS) & EP0CS_HCOUTBSY)) {
		/*fill the setup data in fifo */
		writel(*buf, addr);
		addr += 4;
		buf++;
		writel(*buf, addr);
		writeb(8, acthcd->base + HCOUT0BC);
	} else {
		dev_warn(acthcd->dev, "setup ep busy!!!!!!!\n");
	}

	return 0;
}

static void handle_hcep0_out(struct aotg_hcd *acthcd)
{
	struct aotg_hcep *ep;
	struct urb *urb;
	struct usb_device *udev;
	struct aotg_queue *q;

	ep = acthcd->active_ep0;

	if (unlikely(!ep)) {
		ACT_HCD_ERR
		return;
	}
	q = ep->q;
	if (q == NULL) {
		ACT_HCD_ERR
		return;
	}

	urb = q->urb;
	udev = ep->udev;

	switch (ep->nextpid) {
	case USB_PID_SETUP:
		if (urb->transfer_buffer_length == urb->actual_length) {
			ep->nextpid = USB_PID_ACK;
			handle_status(acthcd, ep, 1);	/*no-data transfer */
		} else if (usb_pipeout(urb->pipe)) {
			usb_settoggle(udev, 0, 1, 1);
			ep->nextpid = USB_PID_OUT;
			write_hcep0_fifo(acthcd, ep, urb);
		} else {
			usb_settoggle(udev, 0, 0, 1);
			ep->nextpid = USB_PID_IN;
			writeb(0, acthcd->base + HCIN0BC);
		}
		break;
	case USB_PID_OUT:
		urb->actual_length += ep->length;
		usb_dotoggle(udev, ep->epnum, 1);
		if (urb->actual_length >= urb->transfer_buffer_length) {
			ep->nextpid = USB_PID_ACK;
			handle_status(acthcd, ep, 1);	/*control write transfer */
		} else {
			ep->nextpid = USB_PID_OUT;
			write_hcep0_fifo(acthcd, ep, urb);
		}
		break;
	case USB_PID_ACK:
		finish_request(acthcd, q, 0);
		break;
	default:
		dev_err(acthcd->dev, "<CTRL>ep0 out ,odd pid %d, %s, %d\n",
			ep->nextpid, __func__, __LINE__);
	}
}

static void handle_hcep0_in(struct aotg_hcd *acthcd)
{
	struct aotg_hcep *ep;
	struct urb *urb;
	struct usb_device *udev;
	struct aotg_queue *q;

	ep = acthcd->active_ep0;
	if (unlikely(!ep)) {
		return;
	}
	q = ep->q;
	if (q == NULL) {
		ACT_HCD_ERR
		return;
	}

	urb = q->urb;
	udev = ep->udev;

	switch (ep->nextpid) {
	case USB_PID_IN:
		read_hcep0_fifo(acthcd, ep, urb);
		break;
	case USB_PID_ACK:
		finish_request(acthcd, q, 0);
		break;
	default:
		dev_err(acthcd->dev, "<CTRL>ep0 out ,odd pid %d\n", ep->nextpid);
	}
}

static void aotg_hcd_err_handle(struct aotg_hcd *acthcd, u32 irqvector,
	int ep_num, int is_in)
{
	struct urb *urb;
	struct aotg_queue *q;
	struct aotg_hcep *ep = NULL;
	struct aotg_ring *ring = NULL;
	struct aotg_td *td = NULL;
	int status = -EOVERFLOW;
	u8 err_val = 0;
	u8 err_type = 0;
	u8 reset = 0;
	struct usb_hcd *hcd = aotg_to_hcd(acthcd);

	printk(KERN_DEBUG"hcd ep err ep_num:%d, is_in:%d\n", ep_num, is_in);
	
	if (is_in)
		writew(1 << ep_num, acthcd->base + HCINxERRIRQ0);
	else
		writew(1 << ep_num, acthcd->base + HCOUTxERRIRQ0);
	if (ep_num == 0) {
		ep = acthcd->active_ep0;
		if (ep == NULL) {
			ACT_HCD_ERR
			return;
		}
		q = ep->q;
		if (is_in)
			ep->reg_hcerr = acthcd->base + HCIN0ERR;
		else
			ep->reg_hcerr = acthcd->base + HCOUT0ERR;
	} else {
		if (is_in)
			ep = acthcd->inep[ep_num];
		else
			ep = acthcd->outep[ep_num];
		if (ep == NULL) {
			ACT_HCD_ERR
			pr_warn("is_in:%d, ep_num:%d\n", is_in, ep_num);
			return;
		}
		ring = ep->ring;
		if (!ring) {
			ACT_HCD_ERR
			return;
		}
		td = list_first_entry_or_null(&ep->enring_td_list, struct aotg_td, enring_list);
		if (!td) {
			aotg_stop_ring(ring);
			ACT_HCD_ERR
			return;
		}
	}

	err_val = readb(ep->reg_hcerr);
	err_type = err_val & HCINxERR_TYPE_MASK;
	
	printk(KERN_DEBUG"err_type:%x\n", err_type>>2);
	
	switch (err_type) {
	case HCINxERR_NO_ERR:
	case HCINxERR_OVER_RUN:
		status = -EOVERFLOW;
		break;
	case HCINxERR_UNDER_RUN:
		status = -EREMOTEIO;
		break;
	case HCINxERR_STALL:
		status = -EPIPE;
		break;
	case HCINxERR_TIMEOUT:
		status = -ETIMEDOUT;
		break;
	case HCINxERR_CRC_ERR:
	case HCINxERR_TOG_ERR:
	case HCINxERR_PID_ERR:
		status = -EPROTO;
		break;
	/*case HCINxERR_SPLIET:*/
	default:
		printk(KERN_DEBUG"err_val:0x%x, err_type:%d\n", err_val, err_type);
		if (is_in) {
			printk(KERN_DEBUG"HCINEP%dSPILITCS:0x%x\n", ep_num,
				readb(acthcd->base + ep_num * 8 + HCEP0SPILITCS));
		} else {
			printk(KERN_DEBUG"HCOUTEP%dSPILITCS:0x%x\n", ep_num,
				readb(acthcd->base + (ep_num - 1) * 8 + HCOUT1SPILITCS));
		}
		status = -EPIPE;
		break;
	}

	if (!(acthcd->port & USB_PORT_STAT_ENABLE)
		|| (acthcd->port & (USB_PORT_STAT_C_CONNECTION << 16))
		|| (acthcd->hcd_exiting != 0)
		|| (acthcd->inserted == 0)
		|| !HC_IS_RUNNING(hcd->state)) {
		dev_err(acthcd->dev, "usbport, dead, port:%x, hcd_exiting:%d\n", acthcd->port, acthcd->hcd_exiting);
		status = -ENODEV;
	}

	if (ep->index == 0) {
		q = ep->q;
		urb = q->urb;
		if ((status == -EPIPE) || (status == -ENODEV))
			writeb(HCINxERR_RESEND, ep->reg_hcerr);
		finish_request(acthcd, q, status);
		dev_info(acthcd->dev, "%s ep %d error [0x%02X] error type [0x%02X], reset it...\n",
			usb_pipeout(urb->pipe) ? "HC OUT" : "HC IN", ep->index, err_val, (err_val>>2)&0x7);
	} else {
		if ((status != -EPIPE) && (status != -ENODEV)) {
			printk(KERN_DEBUG"td->err_count:%d,ep_errcount:%d\n", td->err_count, ep->error_count);
			td->err_count++;

			if ((td->err_count < MAX_ERROR_COUNT) && (ep->error_count < 3)) {
				writeb(HCINxERR_RESEND, ep->reg_hcerr);
				return;
			}
		}

		if (status == -ETIMEDOUT || status == -EPIPE)
			ep->error_count++;

		reset = ENDPRST_FIFORST | ENDPRST_TOGRST;
		ep_disable(ep);
		if (is_in)
			aotg_hcep_reset(acthcd, ep->mask, reset);
		else
			aotg_hcep_reset(acthcd, ep->mask | USB_HCD_OUT_MASK, reset);

		/*if (usb_pipeout(urb->pipe)) {
			aotg_hcep_reset(acthcd, ep->mask | USB_HCD_OUT_MASK, reset);
		} else {
			aotg_hcep_reset(acthcd, ep->mask, reset);
		}*/

		aotg_stop_ring(ring);
		urb = td->urb;
		if (ep->type == PIPE_INTERRUPT)
			dequeue_intr_td(ring, td);
		else
			dequeue_td(ring, td, TD_IN_FINISH);

		if (urb) {
			usb_hcd_unlink_urb_from_ep(hcd, urb);
			usb_hcd_giveback_urb(hcd, urb, status);
		} else {
			pr_err("urb not exist!\n");
		}

		/*
		 * after, need to rewrite port_num, dev_addr when using hub ?
		 */
		/*if ((urb) && (!list_empty(&ep->enring_td_list)) &&
				!is_ring_running(ring)) {
			ACT_HCD_DBG
			ep_enable(ep);
			addr = ring_trb_virt_to_dma(ring, ring->dequeue_trb);
			aotg_start_ring(ring, addr);
		}*/
		dev_info(acthcd->dev, "%s ep %d error [0x%02X] error type [0x%02X], reset it...\n",
			is_in ? "HC IN" : "HC OUT", ep->index, err_val, (err_val>>2)&0x7);
	}

	return;
}

static void handle_suspend(struct aotg_hcd *acthcd)
{
	usb_clearbitsb(SUSPEND_IRQIEN, acthcd->base + USBEIEN);
	usb_setbitsb(SUSPEND_IRQIEN, acthcd->base + USBEIRQ);
	
	aotg_sofirq_clr(acthcd);
	aotg_sofirq_on(acthcd);
}

static void handle_sof(struct aotg_hcd *acthcd)
{
	struct usb_hcd *hcd = aotg_to_hcd(acthcd);
	
	aotg_sofirq_clr(acthcd);
	aotg_sofirq_off(acthcd);
	
	if (HC_IS_SUSPENDED(hcd->state)) {
		usb_hcd_resume_root_hub(hcd);
	}

	usb_hcd_poll_rh_status(hcd);
}

void aotg_hcd_abort_urb(struct aotg_hcd *acthcd)
{
	int cnt;
	struct aotg_hcep *ep;
	struct urb *urb;
	struct aotg_ring *ring;
	struct aotg_td *td;
	unsigned long flags;
	struct usb_hcd *hcd = aotg_to_hcd(acthcd);

	spin_lock_irqsave(&acthcd->lock, flags);
	/*ep = acthcd->active_ep0;
	if (ep && ep->q) {
		q = ep->q;
		urb = q->urb;
		q->status = -ENODEV;
		//pr_warn("%s in ep 0\n",__func__);
		aotg_hcd_release_queue(acthcd, q);
		usb_hcd_unlink_urb_from_ep(hcd, urb);
		spin_unlock(&acthcd->lock);
		usb_hcd_giveback_urb(hcd, urb, -ENODEV);
		spin_lock(&acthcd->lock);
	}*/

	/* Stop DMA first */
	for (cnt = 1; cnt < MAX_EP_NUM; cnt++) {
		ep = acthcd->inep[cnt];
		ring = ep ? ep->ring : NULL;
		if (ep && ring)
			aotg_stop_ring(ring);
	}

	for (cnt = 1; cnt < MAX_EP_NUM; cnt++) {
		ep = acthcd->outep[cnt];
		ring = ep ? ep->ring : NULL;
		if (ep && ring)
			aotg_stop_ring(ring);
	}

	for (cnt = 1; cnt < MAX_EP_NUM; cnt++) {
		ep = acthcd->inep[cnt];
		if (ep) {
			ring = ep->ring;
			td = list_first_entry_or_null(&ep->enring_td_list, struct aotg_td, enring_list);
			if (!td)
				continue;
			urb = td->urb;
			if (!urb)
				continue;
			if (ep->type == PIPE_INTERRUPT)
				dequeue_intr_td(ring, td);
			else
				dequeue_td(ring, td, TD_IN_FINISH);
			usb_hcd_unlink_urb_from_ep(hcd, urb);
			spin_unlock(&acthcd->lock);
			usb_hcd_giveback_urb(hcd, urb, -ENODEV);
			spin_lock(&acthcd->lock);
		}
	}

	for (cnt = 1; cnt < MAX_EP_NUM; cnt++) {
		ep = acthcd->outep[cnt];
		if (ep) {
			ring = ep->ring;
			td = list_first_entry_or_null(&ep->enring_td_list, struct aotg_td, enring_list);
			if (!td)
				continue;
			urb = td->urb;
			if (!urb)
				continue;
			if (ep->type == PIPE_INTERRUPT)
				dequeue_intr_td(ring, td);
			else
				dequeue_td(ring, td, TD_IN_FINISH);

			usb_hcd_unlink_urb_from_ep(hcd, urb);
			spin_unlock(&acthcd->lock);
			usb_hcd_giveback_urb(hcd, urb, -ENODEV);
			spin_lock(&acthcd->lock);
		}
	}
	spin_unlock_irqrestore(&acthcd->lock, flags);
}

static irqreturn_t aotg_hub_irq(struct usb_hcd *hcd)
{
	struct platform_device *pdev;
	unsigned int port_no;
	u32 irqvector;
	struct aotg_hcd *acthcd = hcd_to_aotg(hcd);
	u8 eirq_mask = readb(acthcd->base + USBEIEN);
	u8 eirq_pending = readb(acthcd->base + USBEIRQ);
	u8 otg_state;

	/* take cate to use lock, because in irq -> dma_handler -> finish_request ->
	 * usb_hcd_giveback_urb -> urb->complete(), it maybe call enqueue and get spin_lock again.
	 */

	pdev = to_platform_device(hcd->self.controller);
	port_no = pdev->id & 0xff;


	if ((eirq_pending & SUSPEND_IRQIEN) && (eirq_mask & SUSPEND_IRQIEN)) {
		handle_suspend(acthcd);
		return IRQ_HANDLED;
	}
	
	if (eirq_pending & USBEIRQ_USBIRQ)
	{
		irqvector = (u32)readb(acthcd->base + IVECT);
		writeb(eirq_mask & USBEIRQ_USBIRQ, acthcd->base + USBEIRQ);
		
		switch (irqvector) {
		case UIV_IDLE:
		case UIV_SRPDET:
		case UIV_LOCSOF:
		case UIV_VBUSERR:
		case UIV_PERIPH:
		
		//when device is inserted: port_no:1 OTG IRQ, OTGSTATE: 0x03, USBIRQ:0x00
		
		//when device is removed:  port_no:1 OTG IRQ, OTGSTATE: 0x02, USBIRQ:0x02
		
		
		
			if (readb(acthcd->base + OTGIRQ) & (0x1<<2))
			{
				writeb(0x1<<2, acthcd->base + OTGIRQ);
				otg_state = readb(acthcd->base + OTGSTATE);
				
				pr_info("port_no:%d OTG IRQ, OTGSTATE: 0x%02X, USBIRQ:0x%02X\n",
					port_no, otg_state,
					readb(acthcd->base + USBIRQ));

				if (otg_state == AOTG_STATE_A_SUSPEND) {
					return IRQ_HANDLED;
				}
				
				//if (otg_state == AOTG_STATE_A_WAIT_BCON) {
				//	return IRQ_HANDLED;
				//}
				
				acthcd->put_aout_msg = 0;
				
				if (acthcd->discon_happened) {
					hrtimer_start(&acthcd->hotplug_timer, ktime_set(0, 500*NSEC_PER_MSEC), HRTIMER_MODE_REL);
				}
				else
				{
					acthcd->discon_happened = 1;
					hrtimer_start(&acthcd->hotplug_timer, ktime_set(0, 10*NSEC_PER_MSEC), HRTIMER_MODE_REL);
				}
			}
			else {
				pr_info("port_no:%d error OTG irq! OTGIRQ: 0x%02X\n",
					port_no, readb(acthcd->base + OTGIRQ));
			}
			break;
			
		case UIV_SOF:
			writeb(USBIRQ_SOF, acthcd->base + USBIRQ);
			
			if (acthcd->bus_remote_wakeup) {
				acthcd->bus_remote_wakeup = 0;
				acthcd->port |= (USB_PORT_STAT_C_SUSPEND<<16);
				acthcd->port &= ~USB_PORT_STAT_C_SUSPEND;
			}
			
			handle_sof(acthcd);
			break;
			
		case UIV_USBRESET:
		
			
			if (acthcd->port & (USB_PORT_STAT_POWER | USB_PORT_STAT_CONNECTION))
			{
				acthcd->speed = USB_SPEED_FULL;	/*FS is the default */
				acthcd->port |= (USB_PORT_STAT_C_RESET << 16);
				acthcd->port &= ~USB_PORT_STAT_RESET;

				/*clear usb reset irq */
				writeb(USBIRQ_URES, acthcd->base + USBIRQ);

				/*reset all ep-in */
				aotg_hcep_reset(acthcd, USB_HCD_IN_MASK,
						ENDPRST_FIFORST | ENDPRST_TOGRST);
				/*reset all ep-out */
				aotg_hcep_reset(acthcd, USB_HCD_OUT_MASK,
						ENDPRST_FIFORST | ENDPRST_TOGRST);

				acthcd->port |= USB_PORT_STAT_ENABLE;
				acthcd->rhstate = AOTG_RH_ENABLE;
				
				/*now root port is enabled fully */
				if (readb(acthcd->base + USBCS) & USBCS_HFMODE)
				{
					acthcd->speed = USB_SPEED_HIGH;
					acthcd->port |= USB_PORT_STAT_HIGH_SPEED;
					writeb(USBIRQ_HS, acthcd->base + USBIRQ);
					
					dev_info(acthcd->dev, "USB device is HS\n");
				}
				else if (readb(acthcd->base + USBCS) & USBCS_LSMODE)
				{
					acthcd->speed = USB_SPEED_LOW;
					acthcd->port |= USB_PORT_STAT_LOW_SPEED;
					
					dev_info(acthcd->dev, "USB device is LS\n");
				}
				else {
					acthcd->speed = USB_SPEED_FULL;
					
					dev_info(acthcd->dev, "USB device is FS\n");
				}

				
				writew(0xffff, acthcd->base + HCINxERRIRQ0);
				writew(0xffff, acthcd->base + HCOUTxERRIRQ0);

				writew(0xffff, acthcd->base + HCINxIRQ0);
				writew(0xffff, acthcd->base + HCOUTxIRQ0);
				writew(0xffff, acthcd->base + HCINxERRIEN0);
				writew(0xffff, acthcd->base + HCOUTxERRIEN0);

			}
			break;
			
			
			
		case UIV_EP0IN:
			writew(0x1, acthcd->base + HCOUTxIRQ0); //clear hcep0out irq
			handle_hcep0_out(acthcd);
			break;
			
		case UIV_EP0OUT:
			writew(0x1, acthcd->base + HCINxIRQ0); //clear hcep0in irq
			handle_hcep0_in(acthcd);
			break;
			
		case UIV_EP1IN:
			writew(0x1 << 1, acthcd->base + HCOUTxIRQ0); //clear hcep1out irq
			break;
			
		case UIV_EP1OUT:
			writew(0x1 << 1, acthcd->base + HCINxIRQ0); //clear hcep1in irq
			break;
			
		case UIV_EP2IN:
			writew(0x1 << 2, acthcd->base + HCOUTxIRQ0); //clear hcep2out irq
			break;
			
		case UIV_EP2OUT:
			writeb(0x1 << 2, acthcd->base + HCINxIRQ0); //clear hcep2in irq
			break;
			
			
		default:
			if ((irqvector >= UIV_HCOUT0ERR) && (irqvector <= UIV_HCOUT15ERR)) {
				printk(KERN_DEBUG"irqvector:%d, 0x%x\n", irqvector, irqvector);
				aotg_hcd_err_handle(acthcd, irqvector, (irqvector - UIV_HCOUT0ERR), 0);
				break;
			}
			if ((irqvector >= UIV_HCIN0ERR) && (irqvector <= UIV_HCIN15ERR)) {
				printk(KERN_DEBUG"irqvector:%d, 0x%x\n", irqvector, irqvector);
				aotg_hcd_err_handle(acthcd, irqvector, (irqvector - UIV_HCIN0ERR), 1);
				break;
			}
			dev_err(acthcd->dev, "error interrupt, pls check it! irqvector: 0x%02X\n", (u8)irqvector);
			return IRQ_NONE;
		}
	}

	/*clear all surprise interrupt*/
	aotg_clear_all_overflow_irq(acthcd);
	aotg_clear_all_shortpkt_irq(acthcd);
	aotg_clear_all_zeropkt_irq(acthcd);
	aotg_clear_all_hcoutdma_irq(acthcd);
	aotg_ring_irq_handler(acthcd);
	return IRQ_HANDLED;
}

enum hrtimer_restart aotg_hub_hotplug_timer(struct hrtimer *hrtimer)
{
	struct aotg_hcd *acthcd = container_of (hrtimer, struct aotg_hcd, hotplug_timer);
	struct usb_hcd *hcd = aotg_to_hcd(acthcd);
	struct platform_device *pdev;
	unsigned int port_no;
	unsigned long flags;
	int connect_changed = 0;

	if (acthcd->hcd_exiting != 0) {
		return HRTIMER_NORESTART;
	}
	
	disable_irq(acthcd->uhc_irq);
	spin_lock_irqsave(&acthcd->lock, flags);

	if (acthcd->put_aout_msg != 0)
	{
		pdev = to_platform_device(hcd->self.controller);
		port_no = pdev->id & 0xff;
		acthcd->put_aout_msg = 0;
		spin_unlock_irqrestore(&acthcd->lock, flags);
		enable_irq(acthcd->uhc_irq);
		aotg_hub_notify_hcd_exit(0);
		return HRTIMER_NORESTART;
	}

	if ((readb(acthcd->base + OTGSTATE) == AOTG_STATE_A_HOST) && (acthcd->discon_happened == 0)) {
		if (!acthcd->inserted) {
			acthcd->port |= (USB_PORT_STAT_C_CONNECTION << 16);
			/*set port status bit,and indicate the present of  a device */
			acthcd->port |= USB_PORT_STAT_CONNECTION;
			acthcd->rhstate = AOTG_RH_ATTACHED;
			acthcd->inserted = 1;
			connect_changed = 1;
		}
	} else {
		if (acthcd->inserted) {
			acthcd->port &= ~(USB_PORT_STAT_CONNECTION |
					  USB_PORT_STAT_ENABLE |
					  USB_PORT_STAT_LOW_SPEED |
					  USB_PORT_STAT_HIGH_SPEED | USB_PORT_STAT_SUSPEND);
			acthcd->port |= (USB_PORT_STAT_C_CONNECTION << 16);
			acthcd->rhstate = AOTG_RH_NOATTACHED;
			acthcd->inserted = 0;
			connect_changed = 1;
		}
		if (acthcd->discon_happened == 1) {
			acthcd->discon_happened = 0;

			if (readb(acthcd->base + OTGSTATE) == AOTG_STATE_A_HOST)
				hrtimer_start(&acthcd->hotplug_timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		}
	}

	dev_info(acthcd->dev, "connection changed: %d, acthcd->inserted: %d\n",
		connect_changed, acthcd->inserted);
	if (connect_changed) {
		if (HC_IS_SUSPENDED(hcd->state))
			usb_hcd_resume_root_hub(hcd);
		usb_hcd_poll_rh_status(hcd);
	}

	if ((acthcd->inserted == 0) && (connect_changed == 1) &&
		(readb(acthcd->base + OTGSTATE) != AOTG_STATE_A_HOST)) {
		acthcd->put_aout_msg = 1;
		hrtimer_start(&acthcd->hotplug_timer, ktime_set(2, 200*NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}
	acthcd->suspend_request_pend = 0;

	spin_unlock_irqrestore(&acthcd->lock, flags);
	enable_irq(acthcd->uhc_irq);
	return HRTIMER_NORESTART;
}


static inline int aotg_print_ep_timeout(struct aotg_hcep *ep)
{
	int ret = 0;

	if (ep == NULL)
		return ret;

	if (ep->q != NULL) {
		if (ep->q->timeout == 0)
			return ret;

		if (time_after(jiffies, ep->q->timeout)) {
			ret = 1;
			pr_err("ep->index:%x ep->mask:%x\n", ep->index, ep->mask);
			pr_err("timeout:0x%x!\n", (unsigned int)ep->q->timeout);
			ep->q->timeout = jiffies + HZ;
		}
	}
	return ret;
}


void aotg_check_trb_timer(struct timer_list *t)
{
	unsigned long flags;
	struct aotg_hcep *ep;
	int i;
	
	struct aotg_hcd *acthcd = container_of(t, struct aotg_hcd, check_trb_timer);

	
	if (acthcd->hcd_exiting != 0) {
		ACT_HCD_DBG
		return;
	}

	spin_lock_irqsave(&acthcd->lock, flags);
	if (acthcd->check_trb_mutex) {
		mod_timer(&acthcd->check_trb_timer, jiffies + msecs_to_jiffies(1));
		spin_unlock_irqrestore(&acthcd->lock, flags);
		return;
	}

	acthcd->check_trb_mutex = 1;
	for (i = 1; i < MAX_EP_NUM; i++) {
		ep = acthcd->inep[i];
		if (ep && (ep->ring) && (ep->ring->type == PIPE_BULK))
				handle_ring_dma_tx(acthcd, i);
	}

	for (i = 1; i < MAX_EP_NUM; i++) {
		ep = acthcd->outep[i];
		if (ep && (ep->ring) && (ep->ring->type == PIPE_BULK))
			handle_ring_dma_tx(acthcd, i | AOTG_DMA_OUT_PREFIX);
	}

	mod_timer(&acthcd->check_trb_timer, jiffies + msecs_to_jiffies(3));

	acthcd->check_trb_mutex = 0;
	spin_unlock_irqrestore(&acthcd->lock, flags);
	return;
}

void aotg_hub_trans_wait_timer(struct timer_list *t)
{
	unsigned long flags;
	struct aotg_hcep *ep;
	int i, ret;
	
	
	
	struct aotg_hcd *acthcd = container_of(t, struct aotg_hcd, trans_wait_timer);

	
	if (acthcd->hcd_exiting != 0) {
		ACT_HCD_DBG
		return;
	}

	disable_irq(acthcd->uhc_irq);
	spin_lock_irqsave(&acthcd->lock, flags);

	ep = acthcd->active_ep0;
	ret = aotg_print_ep_timeout(ep);

	for (i = 1; i < MAX_EP_NUM; i++) {
		ep = acthcd->inep[i];
		ret |= aotg_print_ep_timeout(ep);
	}
	for (i = 1; i < MAX_EP_NUM; i++) {
		ep = acthcd->outep[i];
		if (ep == NULL)
			continue;

		ret |= aotg_print_ep_timeout(ep);

		if (ep->fifo_busy) {
			if ((ep->fifo_busy > 80) && (ep->fifo_busy % 80 == 0))
				pr_info("ep->fifo_busy:%d\n", ep->fifo_busy);

			if (ret == 0) {
				tasklet_hi_schedule(&acthcd->urb_tasklet);
				break;
			}
		}
	}

	if (ret != 0)
		tasklet_hi_schedule(&acthcd->urb_tasklet);

	mod_timer(&acthcd->trans_wait_timer, jiffies + msecs_to_jiffies(500));

	spin_unlock_irqrestore(&acthcd->lock, flags);
	enable_irq(acthcd->uhc_irq);
	return;
}

static inline int start_transfer(struct aotg_hcd *acthcd,
	struct aotg_queue *q, struct aotg_hcep *ep)
{
	struct urb *urb = q->urb;
	int retval = 0;

	ep->urb_enque_cnt++;
	q->length = urb->transfer_buffer_length;

	/* do with hub connected. */
	if (ep->has_hub) {
		if (urb->dev->speed == USB_SPEED_HIGH) {
			writeb(usb_pipedevice(urb->pipe), ep->reg_hcep_dev_addr);
			writeb(urb->dev->portnum, ep->reg_hcep_port);
		} else {
			writeb((0x80 | usb_pipedevice(urb->pipe)), ep->reg_hcep_dev_addr);
			if (urb->dev->speed == USB_SPEED_LOW)
				writeb(0x80 | urb->dev->portnum, ep->reg_hcep_port);
			else
				writeb(urb->dev->portnum, ep->reg_hcep_port);
		}
		
	} else {
		writeb(usb_pipedevice(urb->pipe), ep->reg_hcep_dev_addr);
		writeb(urb->dev->portnum, ep->reg_hcep_port);
	}

	switch (usb_pipetype(urb->pipe)) {
	case PIPE_CONTROL:
		q->timeout = jiffies + HZ/2;
		retval = handle_setup_packet(acthcd, q);
		break;

	default:
		pr_err("%s err, check it pls!\n", __func__);
	}

	return retval;
}

static struct aotg_hcep	*aotg_hcep_alloc(struct usb_hcd *hcd, struct urb *urb)
{
	struct aotg_hcep *ep = NULL;
	int pipe = urb->pipe;
	int is_out = usb_pipeout(pipe);
	int type = usb_pipetype(pipe);
	int i, retval = 0;
	struct aotg_hcd *acthcd = hcd_to_aotg(hcd);
	u8 think_time;

	ep = kzalloc(sizeof(*ep), GFP_ATOMIC);
	if (NULL == ep)	{
		dev_err(acthcd->dev, "alloc	ep failed\n");
		retval = -ENOMEM;
		goto exit;
	}

	ep->udev =urb->dev; //can't use [usb_get_dev()], or will  memLeak when usbhHcd exit
	ep->epnum = usb_pipeendpoint(pipe);
	ep->maxpacket = usb_maxpacket(ep->udev, urb->pipe, is_out);
	ep->type = type;
	ep->urb_enque_cnt = 0;
	ep->urb_endque_cnt = 0;
	ep->urb_stop_stran_cnt = 0;
	ep->urb_unlinked_cnt = 0;
#ifdef USBH_DEBUG
	dev_info(acthcd->dev, "ep->epnum: %d, ep->maxpacket : %d, ep->type : %d\n", ep->epnum, ep->maxpacket, ep->type);
#endif
	ep->length = 0;
	usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), is_out, 0);

	if (urb->dev->parent) {
		if (urb->dev->tt) {
			/* calculate in ns. */
			think_time = (urb->dev->tt->think_time / 666);
			pr_info("think_time:%d\n", think_time);
			if (think_time <= 0)
				think_time = 1;
			else if (think_time > 4)
				think_time = 4;
			think_time = think_time * 20;
			writeb(think_time, acthcd->base + HCTRAINTERVAL);
			pr_info("think_time:0x%x\n", readb(acthcd->base + HCTRAINTERVAL));
			/*pr_info("urb->dev->tt->hub:%p\n", urb->dev->tt->hub);*/
		}

		if ((urb->dev->parent->parent) && (urb->dev->parent != hcd->self.root_hub)) {
			ep->has_hub = 1;
			ep->hub_addr = 0x7f & readb(acthcd->base + FNADDR);
		} else {
			ep->has_hub = 0;
		}
	}

	switch (type) {
	case PIPE_CONTROL:
		ep->reg_hcep_dev_addr = acthcd->base + HCEP0ADDR;
		ep->reg_hcep_port = acthcd->base + HCEP0PORT;
		ep->reg_hcep_splitcs = acthcd->base + HCEP0SPILITCS;

		for (i = 0; i < MAX_EP_NUM; i++) {
			if (acthcd->ep0[i] == NULL) {
				ep->ep0_index = i;
				acthcd->ep0[i] = ep;
				break;
			}
		}
		if (i == MAX_EP_NUM)
			ACT_HCD_ERR

		ep->index = 0;
		ep->mask = 0;
		usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), 1, 0);
		usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), 0, 0);

		if (acthcd->active_ep0 == NULL) {
			/*writeb(ep->epnum, acthcd->base + HCEP0CTRL);
			writeb((u8)ep->maxpacket, acthcd->base + HCIN0MAXPCK);
			writeb((u8)ep->maxpacket, acthcd->base + HCOUT0MAXPCK);*/
			usb_setbitsw(1, acthcd->base + HCOUTxIEN0);
			usb_setbitsw(1, acthcd->base + HCINxIEN0);
			writew(1, acthcd->base + HCOUTxIRQ0);
			writew(1, acthcd->base + HCINxIRQ0);

			if (ep->has_hub)
				usb_setbitsb(0x80, acthcd->base + FNADDR);
			else
				writeb(usb_pipedevice(urb->pipe), acthcd->base + FNADDR);
			dev_info(acthcd->dev, "device addr: 0x%08x\n", readb(acthcd->base + FNADDR));
		} else {
			ACT_HCD_ERR
		}
		break;

	case PIPE_BULK:
		retval = aotg_hcep_config(acthcd, ep, EPCON_TYPE_BULK, EPCON_BUF_SINGLE, is_out);
		if (retval < 0) {
			dev_err(acthcd->dev, "PIPE_BULK, retval: %d\n", retval);
			kfree(ep);
			goto exit;
		}
		break;

	case PIPE_INTERRUPT:
		retval = aotg_hcep_config(acthcd, ep, EPCON_TYPE_INT, EPCON_BUF_SINGLE, is_out);
		if (retval < 0) {
			dev_err(acthcd->dev, "PIPE_INTERRUPT, retval: %d\n", retval);
			kfree(ep);
			goto exit;
		}
		ep->interval = urb->ep->desc.bInterval;
		writeb(ep->interval, ep->reg_hcep_interval);
		/*pr_info("urb->interval: %d\n", urb->interval);
		pr_info("urb->ep->desc.bInterval: %d, reg_interval:0x%x\n",
		urb->ep->desc.bInterval, readb(ep->reg_hcep_interval));*/

		break;

	case PIPE_ISOCHRONOUS:
		retval = aotg_hcep_config(acthcd, ep, EPCON_TYPE_ISO, EPCON_BUF_SINGLE, is_out);
		ep->iso_packets = (urb->ep->desc.wMaxPacketSize >> 11) & 3;
		ep->interval = urb->ep->desc.bInterval;
		writeb(ep->interval, ep->reg_hcep_interval);
		usb_setb(ep->iso_packets << 4, ep->reg_hcepcon);
		pr_info("iso_packets:%d, bInterval:%d, urb_interval:%d, reg_con:0x%x\n",
					ep->iso_packets, ep->interval, urb->interval, readb(ep->reg_hcepcon));
		break;

	default:
		dev_err(acthcd->dev, "not support type, type: %d\n", type);
		retval = -ENODEV;
		kfree(ep);
		goto exit;
	}

	if ((ep->udev->speed != USB_SPEED_HIGH) && ep->has_hub &&
		(type == PIPE_INTERRUPT)) {
		aotg_hcep_set_split_micro_frame(acthcd, ep);
	}
	ep->hep = urb->ep;
	urb->ep->hcpriv = ep;
	return ep;

exit:
	return NULL;
}

static int aotg_hub_urb_enqueue(struct usb_hcd *hcd, struct urb *urb,
	unsigned mem_flags)
{
	struct aotg_hcd *acthcd = hcd_to_aotg(hcd);
	struct aotg_queue *q = NULL;
	unsigned long flags;
	struct aotg_hcep *ep = NULL;
	struct aotg_td *td, *next;
	int pipe = urb->pipe;
	int type = usb_pipetype(pipe);
	int retval = 0;

	if (acthcd == NULL) {
		pr_info("aotg_hcd device had been removed...\n");
		return -EIO;
	}

	if (acthcd->hcd_exiting != 0) {
		dev_dbg(acthcd->dev, "aotg hcd exiting! type:%d\n", type);
		return -ENODEV;
	}

	if (!(acthcd->port & USB_PORT_STAT_ENABLE)
		|| (acthcd->port & (USB_PORT_STAT_C_CONNECTION << 16))
		|| (acthcd->hcd_exiting != 0)
		|| (acthcd->inserted == 0)
		|| !HC_IS_RUNNING(hcd->state)) {
		dev_err(acthcd->dev, "usbport dead or disable\n");
		return -ENODEV;
	}

	spin_lock_irqsave(&acthcd->lock, flags);

	ep = urb->ep->hcpriv;
	if ((unlikely(!urb->ep->enabled)) || (likely(ep) &&
		unlikely(ep->error_count > 3))) {
		pr_err("ep %d had been stopped!\n", ep->epnum);
		retval = -ENOENT;
		goto exit0;
	}

	retval = usb_hcd_link_urb_to_ep(hcd, urb);
	if (retval) {
		dev_err(acthcd->dev, "<QUEUE> usb_hcd_link_urb_to_ep error!! retval:0x%x\n", retval);
		goto exit0;
	}

	if (likely(urb->ep->hcpriv)) {
		ep = (struct aotg_hcep *)urb->ep->hcpriv;
	} else {
		ep = aotg_hcep_alloc(hcd, urb);
		if (NULL == ep) {
			dev_err(acthcd->dev, "<QUEUE> alloc ep failed\n");
			retval = -ENOMEM;
			goto exit1;
		}

		if (!usb_pipecontrol(pipe)) {
			if (usb_pipeint(pipe))
				ep->ring = aotg_alloc_ring(acthcd, ep, INTR_TRBS, GFP_ATOMIC);
			else
				ep->ring = aotg_alloc_ring(acthcd, ep, NUM_TRBS, GFP_ATOMIC);
			if (!ep->ring) {
				dev_err(acthcd->dev, "alloc td_ring failed\n");
				retval = -ENOMEM;
				goto exit1;
			}
			INIT_LIST_HEAD(&ep->queue_td_list);
			INIT_LIST_HEAD(&ep->enring_td_list);
			INIT_LIST_HEAD(&ep->dering_td_list);

			/*enable_overflow_irq(acthcd, ep);*/
		}
		urb->ep->hcpriv	= ep;

	}

	urb->hcpriv = hcd;

	if (type == PIPE_CONTROL) {
		q = aotg_hcd_get_queue(acthcd, urb, mem_flags);
		if (unlikely(!q)) {
			dev_err(acthcd->dev, "<QUEUE>  alloc dma queue failed\n");
			spin_unlock_irqrestore(&acthcd->lock, flags);
			return -ENOMEM;
		}

		q->ep = ep;
		q->urb = urb;
		list_add_tail(&q->enqueue_list, &acthcd->hcd_enqueue_list);

	} else if (type == PIPE_BULK) {
		td = aotg_alloc_td(mem_flags);
		if (!td) {
			dev_err(acthcd->dev, "alloc td failed\n");
			retval = -ENOMEM;
			goto exit1;
		}
		td->urb = urb;

		ep->urb_enque_cnt++;

		if (list_empty(&ep->queue_td_list)) {
			retval = aotg_ring_enqueue_td(acthcd, ep->ring, td);
			if (retval) {
				list_add_tail(&td->queue_list, &ep->queue_td_list);
				goto out;
			}

			list_add_tail(&td->enring_list, &ep->enring_td_list);
			ep->ring->enring_cnt++;
		} else {
			list_add_tail(&td->queue_list, &ep->queue_td_list);
		}

		if (!list_empty(&ep->enring_td_list) &&
			!is_ring_running(ep->ring)) {
			aotg_start_ring_transfer(acthcd, ep, urb);
		}

	} else if (type == PIPE_INTERRUPT) {
		if (unlikely(ep->ring->intr_inited == 0)) {
			retval = aotg_ring_enqueue_intr_td(acthcd, ep->ring, ep, urb, GFP_ATOMIC);
			if (retval) {
				pr_err("%s, intr urb enqueue err!\n", __func__);
				goto exit1;
			}
			ep->ring->intr_started = 0;
		}
		ep->urb_enque_cnt++;
		list_for_each_entry_safe(td, next, &ep->enring_td_list, enring_list) {
			if (td->urb) {
				continue;
			} else {
				td->urb = urb;
				break;
			}
		}

		if (unlikely(ep->ring->enqueue_trb->hw_buf_len != urb->transfer_buffer_length)) {
			aotg_intr_chg_buf_len(acthcd, ep->ring, urb->transfer_buffer_length);
			pr_debug("WARNNING:interrupt urb length changed......\n");
		}

		if (ep->ring->intr_started == 0) {
			ep->ring->intr_started = 1;
			aotg_start_ring_transfer(acthcd, ep, urb);
		}

		if (!is_ring_running(ep->ring)) {	/*trb overflow or no urb*/
			if (ep->is_out) {
				aotg_start_ring_transfer(acthcd, ep, urb);
			} else {
				if (aotg_intr_get_finish_trb(ep->ring) == 0) {
					ep->ring->ring_stopped = 0;
					aotg_reorder_intr_td(ep);
					ep_enable(ep);
					mb();
					writel(DMACTRL_DMACS, ep->ring->reg_dmactrl);
				} else {
					ep->ring->ring_stopped = 1;
				}
			}
		}

	} else {	/* type == PIPE_ISOCHRONOUS*/
		td = aotg_alloc_td(mem_flags);
		if (!td) {
			dev_err(acthcd->dev, "alloc td failed\n");
			retval = -ENOMEM;
			goto exit1;
		}
		td->urb = urb;
		ep->urb_enque_cnt++;

		if (list_empty(&ep->queue_td_list)) {
			retval = aotg_ring_enqueue_isoc_td(acthcd, ep->ring, td);
			if (retval) {
				list_add_tail(&td->queue_list, &ep->queue_td_list);
				goto out;
			}

			list_add_tail(&td->enring_list, &ep->enring_td_list);
			ep->ring->enring_cnt++;
		} else {
			list_add_tail(&td->queue_list, &ep->queue_td_list);
		}

		if (!list_empty(&ep->enring_td_list) && !is_ring_running(ep->ring)) {
			// no need to reorder!
			//if (ep->ring->dequeue_trb != ep->ring->first_trb)
				//aotg_reorder_iso_td(acthcd, ep->ring);
			aotg_start_ring_transfer(acthcd, ep, urb);
		}
	}
out:
	spin_unlock_irqrestore(&acthcd->lock, flags);
	tasklet_hi_schedule(&acthcd->urb_tasklet);
	return retval;
exit1:
	usb_hcd_unlink_urb_from_ep(hcd, urb);
exit0:
	pr_err("never goto here, need to just\n");
	if (unlikely(retval < 0) && ep) {
		if (type == PIPE_CONTROL)	{
			ACT_HCD_ERR
			if (ep)
				ep->q = NULL;
			if (q)
				aotg_hcd_release_queue(acthcd, q);
		} else {
			writel(DMACTRL_DMACC, ep->ring->reg_dmactrl);
			ep_disable(ep);
		}
	}
	spin_unlock_irqrestore(&acthcd->lock, flags);
	return retval;
}

static int aotg_hub_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
	struct aotg_hcd *acthcd = hcd_to_aotg(hcd);
	struct aotg_hcep *ep;
	struct aotg_queue *q = NULL, *next, *tmp;
	struct aotg_ring *ring;
	struct aotg_td *td, *next_td;
	unsigned long flags;
	int retval = 0;

	if (acthcd == NULL) {
		pr_err("aotg_hcd device had been removed...\n");
		return -EIO;
	}

	spin_lock_irqsave(&acthcd->lock, flags);

	retval = usb_hcd_check_unlink_urb(hcd, urb, status);
	if (retval) {
		pr_debug("%s, retval:%d, urb not submitted or unlinked\n",
			__func__, retval);
		goto dequeue_out;
	}

	ep = (struct aotg_hcep *)urb->ep->hcpriv;
	if (ep == NULL) {
		ACT_HCD_ERR
		retval = -EINVAL;
		goto dequeue_out;
	}

	if (!usb_pipecontrol(urb->pipe)) {
		ep->urb_unlinked_cnt++;
		ring = ep->ring;

		if (usb_pipeint(urb->pipe)) {
			list_for_each_entry_safe(td, next_td, &ep->enring_td_list, enring_list) {
				if (urb == td->urb) {
					retval = aotg_ring_dequeue_intr_td(acthcd, ep, ring, td);
					goto de_bulk;
				}
			}
			pr_err("%s, intr dequeue err\n", __func__);
		}

		list_for_each_entry_safe(td, next_td, &ep->queue_td_list, queue_list) {
			if (urb == td->urb) {
				retval = aotg_ring_dequeue_td(acthcd, ring, td, TD_IN_QUEUE);
				goto de_bulk;
			}
		}

		list_for_each_entry_safe(td, next_td, &ep->enring_td_list, enring_list) {
			mb();
			if (urb == td->urb) {
				retval = aotg_ring_dequeue_td(acthcd, ring, td, TD_IN_RING);
				ep->urb_stop_stran_cnt++;
				goto de_bulk;
			}
		}

		retval = -EINVAL;
		goto dequeue_out;

de_bulk:
		usb_hcd_unlink_urb_from_ep(hcd, urb);
		spin_unlock(&acthcd->lock);
		usb_hcd_giveback_urb(hcd, urb, status);
		spin_lock(&acthcd->lock);

		spin_unlock_irqrestore(&acthcd->lock, flags);
		return retval;
	}

	q = ep->q;

	/* ep->mask currently equal to q->dma_no. */
	if (q && (q->urb == urb)) {
		writeb(EP0CS_HCSET, acthcd->base + EP0CS);

		/* maybe finished in tasklet_finish_request. */
		if (!list_empty(&q->finished_list)) {
			if (q->finished_list.next != LIST_POISON1)
				list_del(&q->finished_list);
		}

		if (q->is_xfer_start) {
			ep->urb_stop_stran_cnt++;
			q->is_xfer_start = 0;
		}
	} else {
		q = NULL;
		list_for_each_entry_safe(tmp, next, &acthcd->hcd_enqueue_list, enqueue_list) {
			if (tmp->urb == urb) {
				list_del(&tmp->enqueue_list);
				q = tmp;
				ep = q->ep;
				if (ep->q == q)
					ACT_HCD_DBG
				break;
			}
		}
	}

	if (likely(q)) {
		q->status = status;
		list_add_tail(&q->dequeue_list, &acthcd->hcd_dequeue_list);
		spin_unlock_irqrestore(&acthcd->lock, flags);
		tasklet_schedule(&acthcd->urb_tasklet);
		return retval;
	} else {
		/*ACT_HCD_ERR*/
		pr_err("dequeue's urb not find in enqueue_list!\n");
	}

dequeue_out:
	spin_unlock_irqrestore(&acthcd->lock, flags);
	return retval;
}

void urb_tasklet_func(unsigned long data)
{
	struct aotg_hcd *acthcd = (struct aotg_hcd *)data;
	struct aotg_queue *q, *next;
	struct aotg_hcep *ep;
	struct urb *urb;
	struct aotg_ring *ring;
	struct aotg_td *td;
	unsigned long flags;
	int status;
	struct usb_hcd *hcd = aotg_to_hcd(acthcd);
	int cnt = 0;

	do {
		status = (int)spin_is_locked(&acthcd->tasklet_lock);
		if (status) {
			acthcd->tasklet_retry = 1;
			pr_warn("locked, urb retry later!\n");
			return;
		}
		cnt++;
		/* sometimes tasklet_lock is unlocked, but spin_trylock still will be failed,
		 * maybe caused by the instruction of strexeq in spin_trylock,it will return failed
		 * if other cpu is accessing the nearby address of &acthcd->tasklet_lock.
		 */
		status = spin_trylock(&acthcd->tasklet_lock);
		if ((!status) && (cnt > 10)) {
			acthcd->tasklet_retry = 1;
			pr_warn("urb retry later!\n");
			return;
		}
	} while (status == 0);

	/*disable_irq_nosync(acthcd->uhc_irq);*/
	disable_irq(acthcd->uhc_irq);
	spin_lock_irqsave(&acthcd->lock, flags);

	for (cnt = 1; cnt < MAX_EP_NUM; cnt++) {
		ep = acthcd->inep[cnt];
		if (ep && (ep->type == PIPE_INTERRUPT)) {
			ring = ep->ring;
			if (ring->ring_stopped) {
				td = list_first_entry_or_null(&ep->enring_td_list, struct aotg_td, enring_list);
				if (!td)
					continue;
				urb = td->urb;
				if (!urb)
					continue;
				intr_finish_td(acthcd, ring, td);
			}
		}
	}
	/* do dequeue task. */
DO_DEQUEUE_TASK:
	urb = NULL;
	list_for_each_entry_safe(q, next, &acthcd->hcd_dequeue_list, dequeue_list) {
		if (q->status < 0) {
			urb = q->urb;
			ep = q->ep;
			if (ep) {
				ep->urb_unlinked_cnt++;
				/*ep->q = NULL;*/
			}
			list_del(&q->dequeue_list);
			status = q->status;
			tasklet_finish_request(acthcd, q, status);
			hcd = bus_to_hcd(urb->dev->bus);
			break;
		} else {
			ACT_HCD_ERR
		}
	}
	if (urb != NULL) {
		usb_hcd_unlink_urb_from_ep(hcd, urb);
		spin_unlock_irqrestore(&acthcd->lock, flags);
		/* in usb_hcd_giveback_urb, complete function may call new urb_enqueue. */
		usb_hcd_giveback_urb(hcd, urb, status);
		spin_lock_irqsave(&acthcd->lock, flags);
		goto DO_DEQUEUE_TASK;
	}

	/* do finished task. */
DO_FINISH_TASK:
	urb = NULL;
	list_for_each_entry_safe(q, next, &acthcd->hcd_finished_list, finished_list) {
		if (q->finished_list.next != LIST_POISON1) {
			list_del(&q->finished_list);
		} else {
			ACT_HCD_ERR
			break;
		}
		status = q->status;
		tasklet_finish_request(acthcd, q, status);

		hcd = aotg_to_hcd(acthcd);
		urb = q->urb;
		ep = q->ep;
		if (urb != NULL)
			break;
	}
	if (urb != NULL) {
		usb_hcd_unlink_urb_from_ep(hcd, urb);

		spin_unlock_irqrestore(&acthcd->lock, flags);

		/* in usb_hcd_giveback_urb, complete function may call new urb_enqueue. */
		usb_hcd_giveback_urb(hcd, urb, status);

		spin_lock_irqsave(&acthcd->lock, flags);
		goto DO_FINISH_TASK;
	}

	/*DO_ENQUEUE_TASK:*/
	/* do enqueue task. */
	/* start transfer directly, don't care setup appearing in bulkout. */
	q = list_first_entry_or_null(&acthcd->hcd_enqueue_list, struct aotg_queue, enqueue_list);
	if (q && (q->urb)) {
		urb = q->urb;
		ep = q->ep;

		if ((acthcd->active_ep0 != NULL) && (acthcd->active_ep0->q != NULL)) {
			acthcd->ep0_block_cnt++;
			if ((acthcd->ep0_block_cnt % 5) == 0) {
				/*ACT_HCD_DBG*/
				pr_info("cnt:%d\n", acthcd->ep0_block_cnt);
				acthcd->ep0_block_cnt = 0;
				spin_unlock_irqrestore(&acthcd->lock, flags);
				enable_irq(acthcd->uhc_irq);
				spin_unlock(&acthcd->tasklet_lock);
				aotg_hub_urb_dequeue(hcd, acthcd->active_ep0->q->urb, -ETIMEDOUT);
				return;
			}
			/*ACT_HCD_DBG*/
			goto exit;
		} else {
			acthcd->ep0_block_cnt = 0;
		}

		list_del(&q->enqueue_list);
		status = start_transfer(acthcd, q, ep);

		if (unlikely(status < 0)) {
			ACT_HCD_ERR
			hcd = bus_to_hcd(urb->dev->bus);
			aotg_hcd_release_queue(acthcd, q);

			usb_hcd_unlink_urb_from_ep(hcd, urb);
			spin_unlock_irqrestore(&acthcd->lock, flags);
			usb_hcd_giveback_urb(hcd, urb, status);
			spin_lock_irqsave(&acthcd->lock, flags);
		}
	}

	if (acthcd->tasklet_retry != 0) {
		acthcd->tasklet_retry = 0;
		goto DO_DEQUEUE_TASK;
	}
exit:
	spin_unlock_irqrestore(&acthcd->lock, flags);
	enable_irq(acthcd->uhc_irq);
	spin_unlock(&acthcd->tasklet_lock);
	return;
}

static void aotg_hub_endpoint_disable(struct usb_hcd *hcd,
	struct usb_host_endpoint *hep)
{
	int i;
	int index;
	unsigned long flags;
	struct aotg_hcd *acthcd = hcd_to_aotg(hcd);
	
	
	struct aotg_hcep *ep = hep->hcpriv;

	if (!ep)
		return;

	if (in_irq())
		disable_irq_nosync(acthcd->uhc_irq);
	else
		disable_irq(acthcd->uhc_irq);

	spin_lock_irqsave(&acthcd->lock, flags);

	index = ep->index;
	if (index == 0) {
		acthcd->ep0[ep->ep0_index] = NULL;
		if (acthcd->active_ep0 == ep)
			acthcd->active_ep0 = NULL;

		for (i = 0; i < MAX_EP_NUM; i++) {
			if (acthcd->ep0[i] != NULL)
				break;
		}
		if (i == MAX_EP_NUM) {
			usb_clearbitsw(1, acthcd->base + HCOUTxIEN0);
			usb_clearbitsw(1, acthcd->base + HCINxIEN0);
			writew(1, acthcd->base + HCOUTxIRQ0);
			writew(1, acthcd->base + HCINxIRQ0);
		}
	} else {
		ep_disable(ep);
		if (ep->mask & USB_HCD_OUT_MASK)
			acthcd->outep[index] = NULL;
		else
			acthcd->inep[index] = NULL;
		release_fifo_addr(acthcd, ep->fifo_addr);
	}

	hep->hcpriv = NULL;
	
	if (ep->ring)
	{
		aotg_stop_ring(ep->ring);
	}
	
	spin_unlock_irqrestore(&acthcd->lock, flags);
	
	if (ep->ring)
	{
		pr_info("%s\n", __func__);
		
		if (ep->ring->type == PIPE_INTERRUPT) {
			pr_info("%s, ep%d dma buf free\n", __func__, ep->index);
			aotg_intr_dma_buf_free(acthcd, ep->ring);
		}
		
		aotg_free_ring(acthcd, ep->ring);
	}

	dev_info(acthcd->dev, "<EP DISABLE> ep%d index %d from ep [%s]\n",
			ep->epnum, index,
			ep->mask & USB_HCD_OUT_MASK ? "out" : "in");

	enable_irq(acthcd->uhc_irq);
	
	kfree(ep);
	return;
}

static int aotg_hcd_get_frame(struct usb_hcd *hcd)
{
	struct timespec64 ts;
	
	ktime_get_real_ts64(&ts);
	
	return ts.tv_nsec / 1000000;
}

static int aotg_hub_status_data(struct usb_hcd *hcd, char *buf)
{
	struct aotg_hcd *acthcd;
	unsigned long flags;
	int retval = 0;

	acthcd = hcd_to_aotg(hcd);
	local_irq_save(flags);
	if (!HC_IS_RUNNING(hcd->state))
		goto done;

	if ((acthcd->port & AOTG_PORT_C_MASK) != 0) {
		*buf = (1 << 1);
		HUB_DEBUG("<HUB STATUS>port status %08x has changes\n", acthcd->port);
		retval = 1;
	}
done:
	local_irq_restore(flags);
	return retval;
}

static inline void port_reset(struct aotg_hcd *acthcd)
{
	/*portrst & 55ms */
	writeb(0x1<<6 | 0x1<<5, acthcd->base + HCPORTCTRL);
}

static void port_power(struct aotg_hcd *acthcd, int is_on)
{
	struct usb_hcd *hcd = aotg_to_hcd(acthcd);

	/* hub is inactive unless the port is powered */
	if (is_on) {
		hcd->self.controller->power.power_state = PMSG_ON;
		dev_dbg(acthcd->dev, "<USB> power on\n");
	} else {
		hcd->self.controller->power.power_state = PMSG_SUSPEND;
		dev_dbg(acthcd->dev, "<USB> power off\n");
	}
}

static int aotg_hcd_start(struct usb_hcd *hcd)
{
	struct aotg_hcd *acthcd = hcd_to_aotg(hcd);
	int retval = 0;
	unsigned long flags;

	dev_info(acthcd->dev, "<HCD> start\n");

	local_irq_save(flags);
	hcd->state = HC_STATE_RUNNING;
	hcd->uses_new_polling = 1;
	local_irq_restore(flags);

	return retval;
}

static void aotg_hcd_stop(struct usb_hcd *hcd)
{
	struct aotg_hcd *acthcd = hcd_to_aotg(hcd);
	unsigned long flags;

	dev_info(acthcd->dev, "<HCD> stop\n");

	local_irq_save(flags);
	hcd->state = HC_STATE_HALT;
	acthcd->port = 0;
	acthcd->rhstate = AOTG_RH_POWEROFF;
	local_irq_restore(flags);
	return;
}

static inline void aotg_hub_descriptor(struct usb_hub_descriptor *desc)
{
	memset(desc, 0, sizeof(*desc));
	desc->bDescriptorType = 0x29;
	desc->bDescLength = 9;
	desc->wHubCharacteristics = (__force __u16)
		(__constant_cpu_to_le16(0x0001));
	desc->bNbrPorts = 1;
}

static int aotg_hub_control(struct usb_hcd *hcd,
	u16 typeReq, u16 wValue, u16 wIndex, char *buf, u16 wLength)
{
	struct aotg_hcd *acthcd = hcd_to_aotg(hcd);
	unsigned long flags;
	int retval = 0;

	if (in_irq())
		disable_irq_nosync(acthcd->uhc_irq);
	else
		disable_irq(acthcd->uhc_irq);

	spin_lock_irqsave(&acthcd->lock, flags);

	if (!HC_IS_RUNNING(hcd->state)) {
		dev_err(acthcd->dev, "<HUB_CONTROL> hc state is not HC_STATE_RUNNING\n");
		spin_unlock_irqrestore(&acthcd->lock, flags);
		enable_irq(acthcd->uhc_irq);
		return -EPERM;
	}
	HUB_DEBUG("<HUB_CONTROL> typeReq:%x, wValue:%04x, wIndex: %04x, wLength: %04x\n", typeReq, wValue, wIndex, wLength);

	switch (typeReq) {
	case ClearHubFeature:
		HUB_DEBUG("<HUB_CONTROL> ClearHubFeature, wValue:%04x, wIndex: %04x, wLength: %04x\n",
		     wValue, wIndex, wLength);
		break;
	case ClearPortFeature:
		HUB_DEBUG("<HUB_CONTROL> ClearPortFeature, wValue:%04x, wIndex: %04x, wLength: %04x\n",
			wValue, wIndex, wLength);

		if (wIndex != 1 || wLength != 0)
			goto hub_error;
		HUB_DEBUG("<HUB_CONTROL> before clear operation,the port status is %08x\n", acthcd->port);
		switch (wValue) {
		case USB_PORT_FEAT_ENABLE:
			acthcd->port &= ~(USB_PORT_STAT_ENABLE
					| USB_PORT_STAT_LOW_SPEED
					| USB_PORT_STAT_HIGH_SPEED);
			acthcd->rhstate = AOTG_RH_DISABLE;
			if (acthcd->port & USB_PORT_STAT_POWER)
				port_power(acthcd, 0);
			break;
		case USB_PORT_FEAT_SUSPEND:
			HUB_DEBUG("<HUB_CONTROL>clear suspend feature\n");
			/*port_resume(acthcd);*/
			acthcd->port &= ~(1 << wValue);
			break;
		case USB_PORT_FEAT_POWER:
			acthcd->port = 0;
			acthcd->rhstate = AOTG_RH_POWEROFF;
			port_power(acthcd, 0);
			break;
		case USB_PORT_FEAT_C_ENABLE:
		case USB_PORT_FEAT_C_SUSPEND:
		case USB_PORT_FEAT_C_CONNECTION:
		case USB_PORT_FEAT_C_OVER_CURRENT:
		case USB_PORT_FEAT_C_RESET:
			acthcd->port &= ~(1 << wValue);
			break;
		default:
			goto hub_error;
		}
		HUB_DEBUG("<HUB_CONTROL> after clear operation,the port status is %08x\n", acthcd->port);
		break;
	case GetHubDescriptor:
		HUB_DEBUG("<HUB_CONTROL> GetHubDescriptor, wValue:%04x, wIndex: %04x, wLength: %04x\n",
		     wValue, wIndex, wLength);
		aotg_hub_descriptor((struct usb_hub_descriptor *)buf);
		break;
	case GetHubStatus:
		HUB_DEBUG("<HUB_CONTROL> GetHubStatus, wValue:%04x, wIndex: %04x, wLength: %04x\n",
			wValue, wIndex, wLength);

		*(__le32 *)buf = __constant_cpu_to_le32(0);
		break;
	case GetPortStatus:
		HUB_DEBUG("<HUB_CONTROL> GetPortStatus, wValue:%04x, wIndex: %04x, wLength: %04x\n",
			wValue, wIndex, wLength);

		if (wIndex != 1)
			goto hub_error;
		*(__le32 *)buf = cpu_to_le32(acthcd->port);
		HUB_DEBUG("<HUB_CONTROL> the port status is %08x\n", acthcd->port);
		break;
	case SetHubFeature:
		HUB_DEBUG("<HUB_CONTROL> SetHubFeature, wValue: %04x,wIndex: %04x, wLength: %04x\n",
			wValue, wIndex, wLength);
		goto hub_error;
		break;
	case SetPortFeature:
		HUB_DEBUG("<HUB_CONTROL> SetPortFeature, wValue:%04x, wIndex: %04x, wLength: %04x\n",
			wValue, wIndex, wLength);

		switch (wValue)
		{
		case USB_PORT_FEAT_POWER:
			if (unlikely(acthcd->port & USB_PORT_STAT_POWER))
				break;
			acthcd->port |= (1 << wValue);
			acthcd->rhstate = AOTG_RH_POWERED;
			port_power(acthcd, 1);
			break;
			
		case USB_PORT_FEAT_RESET:
			if (acthcd->hcd_exiting) {
				retval = -ENODEV;
				break;
			}
			
			dev_info(acthcd->dev, "hub_control: set port reset\n");
			port_reset(acthcd);
			
			/* if it's already enabled, disable */
			acthcd->port &= ~(USB_PORT_STAT_ENABLE
				| USB_PORT_STAT_LOW_SPEED
				| USB_PORT_STAT_HIGH_SPEED);
			
			acthcd->port |= (1 << wValue);
			mdelay(2);
			
			acthcd->rhstate = AOTG_RH_RESET;
			usb_setbitsb(USBIEN_URES, acthcd->base + USBIEN);
			usb_setbitsb(USBEIRQ_USBIEN, acthcd->base + USBEIEN);
			break;
			
		case USB_PORT_FEAT_SUSPEND:
			/*acthcd->port |= USB_PORT_FEAT_SUSPEND;*/
			acthcd->port |= (1 << wValue);
			acthcd->rhstate = AOTG_RH_SUSPEND;
			/*port_suspend(acthcd);*/
			break;
		default:
			if (acthcd->port & USB_PORT_STAT_POWER)
				acthcd->port |= (1 << wValue);
		}
		break;
	default:
hub_error:
		retval = -EPIPE;
		dev_err(acthcd->dev, "<HUB_CONTROL> hub control error\n");
		break;

	}
	spin_unlock_irqrestore(&acthcd->lock, flags);
	enable_irq(acthcd->uhc_irq);

	return retval;
}

void aotg_hcd_init(struct usb_hcd *hcd)
{
	struct aotg_hcd *acthcd = hcd_to_aotg(hcd);
	int i;

	/*init software state */
	spin_lock_init(&acthcd->lock);
	spin_lock_init(&acthcd->tasklet_lock);
	acthcd->tasklet_retry = 0;
	
	acthcd->port = 0;
	acthcd->rhstate = AOTG_RH_POWEROFF;
	acthcd->inserted = 0;
	
	INIT_LIST_HEAD(&acthcd->hcd_enqueue_list);
	INIT_LIST_HEAD(&acthcd->hcd_dequeue_list);
	INIT_LIST_HEAD(&acthcd->hcd_finished_list);
	
	tasklet_init(&acthcd->urb_tasklet, urb_tasklet_func, (unsigned long)acthcd);
	
	acthcd->active_ep0 = NULL;
	
	for (i = 0; i < MAX_EP_NUM; i++)
	{
		acthcd->ep0[i] = NULL;
		acthcd->inep[i] = NULL;
		acthcd->outep[i] = NULL;
	}
	
	acthcd->fifo_map[0] = 1<<31;
	acthcd->fifo_map[1] = 1<<31 | 64;
	
	for (i = 2; i < 64; i++) {
		acthcd->fifo_map[i] = 0;
	}
	
	acthcd->put_aout_msg = 0;
	acthcd->discon_happened = 0;
	acthcd->uhc_irq = 0;
	
	for (i = 0; i < AOTG_QUEUE_POOL_CNT; i++) {
		acthcd->queue_pool[i] = NULL;
	}
}

void aotg_hcd_exit(struct usb_hcd *hcd)
{
	struct aotg_hcd *acthcd = hcd_to_aotg(hcd);
	struct aotg_hcep *ep;
	int i;
	
	
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
}



static int caninos_map_urb(struct usb_hcd *hcd, struct urb *urb, gfp_t flags)
{
	if (usb_pipetype(urb->pipe) == PIPE_INTERRUPT && usb_pipein(urb->pipe)) {
		return 0;
	}
	return usb_hcd_map_urb_for_dma(hcd, urb, flags);
}

static void caninos_unmap_urb(struct usb_hcd *hcd, struct urb *urb)
{
	if (usb_pipetype(urb->pipe) == PIPE_INTERRUPT && usb_pipein(urb->pipe)) {
		return;
	}
	usb_hcd_unmap_urb_for_dma(hcd, urb);
}

void caninos_hcd_fill_ops(struct hc_driver *driver)
{
	/* generic hardware linkage */
	driver->irq = aotg_hub_irq;
	driver->flags = HCD_USB2 | HCD_MEMORY;
	
	/* basic lifecycle operations */
	driver->start = aotg_hcd_start;
	driver->stop = aotg_hcd_stop;
	
	/* managing i/o requests and associated device resources */
	driver->urb_enqueue = aotg_hub_urb_enqueue;
	driver->urb_dequeue = aotg_hub_urb_dequeue;
	driver->map_urb_for_dma = caninos_map_urb;
	driver->unmap_urb_for_dma = caninos_unmap_urb;
	driver->endpoint_disable = aotg_hub_endpoint_disable;
	
	/* periodic schedule support */
	driver->get_frame_number = aotg_hcd_get_frame;
	
	/* root hub support */
	driver->hub_status_data = aotg_hub_status_data;
	driver->hub_control = aotg_hub_control;
	driver->bus_suspend = NULL;
	driver->bus_resume = NULL;
};

