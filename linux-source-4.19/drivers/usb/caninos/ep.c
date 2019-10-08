#include "core.h"
#include "hcd.h"
#include "ep.h"

#define MAX_PACKET(x) ((x)&0x7FF)

static inline void handle_status
	(struct caninos_hcd *myhcd, struct aotg_hcep *ep, int is_out)
{
	/* status always DATA1,set 1 to ep0 toggle */
	writeb(EP0CS_HCSETTOOGLE, myhcd->ctrl->base + EP0CS);
	
	if (is_out) {
		writeb(0, myhcd->ctrl->base + HCIN0BC); /* recv 0 packet */
	}
	else {
		writeb(0, myhcd->ctrl->base + HCOUT0BC); /* send 0 packet */
	}
}

static void read_hcep0_fifo
	(struct caninos_hcd *myhcd, struct aotg_hcep *ep, struct urb *urb)
{
	unsigned overflag, is_short, shorterr, is_last;
	unsigned length, count;
	struct usb_device *udev;
	void __iomem *addr = myhcd->ctrl->base + EP0OUTDATA_W0; /* HCEP0INDAT0 */
	unsigned bufferspace;
	u8 *buf;
	
	overflag = 0;
	is_short = 0;
	shorterr = 0;
	is_last = 0;
	udev = ep->udev;
	
	if (readb(myhcd->ctrl->base + EP0CS) & EP0CS_HCINBSY) {
		return;
	}
	else
	{
		usb_dotoggle(udev, ep->epnum, 0);
		buf = urb->transfer_buffer + urb->actual_length;
		bufferspace = urb->transfer_buffer_length - urb->actual_length;
		
		length = count = readb(myhcd->ctrl->base + HCIN0BC);
		
		if (length > bufferspace)
		{
			count = bufferspace;
			urb->status = -EOVERFLOW;
			overflag = 1;
		}
		
		urb->actual_length += count;
		
		while (count--)
		{
			*buf++ = readb(addr);
			addr++;
		}
		
		if (urb->actual_length >= urb->transfer_buffer_length)
		{
			ep->nextpid = USB_PID_ACK;
			is_last = 1;
			handle_status(myhcd, ep, 0);
		}
		else if (length < ep->maxpacket)
		{
			is_short = 1;
			is_last = 1;
			
			if (urb->transfer_flags & URB_SHORT_NOT_OK)
			{
				urb->status = -EREMOTEIO;
				shorterr = 1;
			}
			
			ep->nextpid = USB_PID_ACK;
			handle_status(myhcd, ep, 0);
			
		}
		else {
			writeb(0, myhcd->ctrl->base + HCIN0BC);
		}
	}
}

void finish_request
	(struct caninos_hcd *myhcd, struct aotg_queue *q, int status)
{
	struct urb *urb = q->urb;
	
	if (unlikely((myhcd == NULL) || (q == NULL) || (urb == NULL))) {
		return;
	}
	
	q->status = status;
	
	if (list_empty(&q->finished_list)) {
		list_add_tail(&q->finished_list, &myhcd->hcd_finished_list);
	}
	
	tasklet_hi_schedule(&myhcd->urb_tasklet);
	return;
}

static void write_hcep0_fifo
	(struct caninos_hcd *myhcd, struct aotg_hcep *ep, struct urb *urb)
{
	u32 *buf;
	int length, count;
	void __iomem *addr = myhcd->ctrl->base + EP0INDATA_W0;
	
	if (!(readb(myhcd->ctrl->base + EP0CS) & EP0CS_HCOUTBSY))
	{
		buf = (u32 *) (urb->transfer_buffer + urb->actual_length);
		prefetch(buf);
		
		/* how big will this packet be? */
		length = min((int)ep->maxpacket, 
			(int)urb->transfer_buffer_length - (int)urb->actual_length);
		
		count = length >> 2; /* write in DWORD */
		
		if (length & 0x3) {
			count++;
		}
		
		while (likely(count--)) {
			writel(*buf, addr);
			buf++;
			addr += 4;
		}
		
		ep->length = length;
		
		writeb(length, myhcd->ctrl->base + HCOUT0BC);
		usb_dotoggle(urb->dev, usb_pipeendpoint(urb->pipe), 1);
	}
}

void handle_hcep0_out(struct caninos_hcd *myhcd)
{
	struct aotg_hcep *ep;
	struct urb *urb;
	struct usb_device *udev;
	struct aotg_queue *q;
	
	ep = myhcd->active_ep0;
	
	if (unlikely(!ep)) {
		return;
	}
	
	q = ep->q;
	
	if (q == NULL) {
		return;
	}
	
	urb = q->urb;
	udev = ep->udev;
	
	switch (ep->nextpid)
	{
	case USB_PID_SETUP:
		if (urb->transfer_buffer_length == urb->actual_length)
		{
			ep->nextpid = USB_PID_ACK;
			handle_status(myhcd, ep, 1);
		}
		else if (usb_pipeout(urb->pipe))
		{
			usb_settoggle(udev, 0, 1, 1);
			ep->nextpid = USB_PID_OUT;
			write_hcep0_fifo(myhcd, ep, urb);
		}
		else
		{
			usb_settoggle(udev, 0, 0, 1);
			ep->nextpid = USB_PID_IN;
			writeb(0, myhcd->ctrl->base + HCIN0BC);
		}
		break;
		
	case USB_PID_OUT:
		urb->actual_length += ep->length;
		
		usb_dotoggle(udev, ep->epnum, 1);
		
		if (urb->actual_length >= urb->transfer_buffer_length)
		{
			ep->nextpid = USB_PID_ACK;
			handle_status(myhcd, ep, 1);
		}
		else
		{
			ep->nextpid = USB_PID_OUT;
			write_hcep0_fifo(myhcd, ep, urb);
		}
		break;
		
	case USB_PID_ACK:
		finish_request(myhcd, q, 0);
		break;
	}
}

void handle_hcep0_in(struct caninos_hcd *myhcd)
{
	struct aotg_hcep *ep;
	struct urb *urb;
	struct usb_device *udev;
	struct aotg_queue *q;
	
	ep = myhcd->active_ep0;
	
	if (unlikely(!ep)) {
		return;
	}
	q = ep->q;
	
	if (q == NULL) {
		return;
	}
	
	urb = q->urb;
	udev = ep->udev;
	
	switch (ep->nextpid)
	{
	case USB_PID_IN:
		read_hcep0_fifo(myhcd, ep, urb);
		break;
		
	case USB_PID_ACK:
		finish_request(myhcd, q, 0);
		break;
	}
}

static inline int get_subbuffer_count(u8 buftype)
{
	int count = 0;
	
	switch (buftype)
	{
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

static ulong get_fifo_addr(struct caninos_hcd *myhcd, int size)
{
	int i, j;
	ulong addr = 0;
	int mul = size / ALLOC_FIFO_UNIT;
	int max_unit = AOTG_MAX_FIFO_SIZE/ALLOC_FIFO_UNIT;
	int find_next = 0;
	
	if (mul == 0) {
		mul = 1;
	}
	
	for (i = 2; i < max_unit;)
	{
		if (myhcd->fifo_map[i] != 0)
		{
			i++;
			continue; /*find first unused addr*/
		}
		
		for (j = i; j < max_unit; j++)
		{
			if ((j - i + 1) == mul) {
				break;
			}
			if (myhcd->fifo_map[j])
			{
				i = j;
				find_next = 1;
				break;
			}
		}
		if (j == max_unit) {
			break;
		}
		else if (find_next)
		{
			find_next = 0;
			continue;
		}
		else
		{
			int k;
			for (k = i; k <= j; k++) {
				myhcd->fifo_map[k] = (1 << 31) | (i * 64);
			}
			addr = i * ALLOC_FIFO_UNIT;
			break;
		}
	}
	return addr;
}

int caninos_hcep_config
	(struct caninos_hcd *myhcd, struct aotg_hcep *ep,
	u8 type, u8 buftype, int is_out)
{
	int index = 0;
	ulong addr = 0;
	int get_ep = 0;
	int subbuffer_count;
	
	subbuffer_count = get_subbuffer_count(buftype);
	
	if (0 == subbuffer_count) {
		return -EPIPE;
	}
	
	if (is_out)
	{
		for (index = 1; index < MAX_EP_NUM; index++)
		{
			if (myhcd->outep[index] == NULL)
			{
				ep->is_out = 1;
				ep->index = index;
				ep->mask = (u8) (USB_HCD_OUT_MASK | index);
				myhcd->outep[index] = ep;
				get_ep = 1;
				break;
			}
		}
	}
	else
	{
		for (index = 1; index < MAX_EP_NUM; index++)
		{
			if (myhcd->inep[index] == NULL)
			{
				ep->is_out = 0;
				ep->index = index;
				ep->mask = (u8) index;
				myhcd->inep[index] = ep;
				get_ep = 1;
				break;
			}
		}
	}
	
	if (!get_ep) {
		return -ENOSPC;
	}
	
	addr = get_fifo_addr(myhcd, subbuffer_count * MAX_PACKET(ep->maxpacket));
	
	if (addr == 0)
	{
		if (is_out) {
			myhcd->outep[ep->index] = NULL;
		}
		else {
			myhcd->inep[ep->index] = NULL;
		}
		return -ENOSPC;
	}
	else {
		ep->fifo_addr = addr;
	}
	
	ep->reg_hcepcon = get_hcepcon_reg(is_out,
		myhcd->ctrl->base + HCOUT1CON,
		myhcd->ctrl->base + HCIN1CON,
		ep->index);
	ep->reg_hcepcs = get_hcepcs_reg(is_out,
		myhcd->ctrl->base + HCOUT1CS,
		myhcd->ctrl->base + HCIN1CS,
		ep->index);
	ep->reg_hcepbc = get_hcepbc_reg(is_out,
		myhcd->ctrl->base + HCOUT1BCL,
		myhcd->ctrl->base + HCIN1BCL,
		ep->index);
	ep->reg_hcepctrl = get_hcepctrl_reg(is_out,
		myhcd->ctrl->base + HCOUT1CTRL,
		myhcd->ctrl->base + HCIN1CTRL,
		ep->index);
	ep->reg_hcmaxpck = get_hcepmaxpck_reg(is_out,
		myhcd->ctrl->base + HCOUT1MAXPCKL,
		myhcd->ctrl->base + HCIN1MAXPCKL,
		ep->index);
	ep->reg_hcepaddr = get_hcepaddr_reg(is_out,
		myhcd->ctrl->base + HCOUT1STADDR,
		myhcd->ctrl->base + HCIN1STADDR,
		ep->index);
	ep->reg_hcep_dev_addr = get_hcep_dev_addr_reg(is_out,
		myhcd->ctrl->base + HCOUT1ADDR,
		myhcd->ctrl->base + HCIN1ADDR,
		ep->index);
	ep->reg_hcep_port = get_hcep_port_reg(is_out,
		myhcd->ctrl->base + HCOUT1PORT,
		myhcd->ctrl->base + HCIN1PORT,
		ep->index);
	ep->reg_hcep_splitcs = get_hcep_splitcs_reg(is_out,
		myhcd->ctrl->base + HCOUT1SPILITCS,
		myhcd->ctrl->base + HCIN1SPILITCS,
		ep->index);
	
	if (!is_out)
	{
		ep->reg_hcerr = myhcd->ctrl->base + HCIN0ERR + ep->index * 0x4;
		ep->reg_hcep_interval =
			myhcd->ctrl->base + HCEP0BINTERVAL + ep->index * 0x8;
	}
	else {
		ep->reg_hcerr = myhcd->ctrl->base + HCOUT0ERR + ep->index * 0x4;
		ep->reg_hcep_interval =
			myhcd->ctrl->base + HCOUT1BINTERVAL + (ep->index - 1) * 0x8;
	}
	
	pio_irq_disable(myhcd, ep->mask);
	pio_irq_clear(myhcd, ep->mask);
	
	ep_disable(ep);
	
	/*allocate buffer address of ep fifo */
	writel(addr, ep->reg_hcepaddr);
	writew(ep->maxpacket, ep->reg_hcmaxpck);
	ep_setup(ep, type, buftype);	/*ep setup */
	
	/*reset this ep */
	usb_settoggle(ep->udev, ep->epnum, is_out, 0);
	caninos_hcep_reset
		(caninos_to_hcd(myhcd), ep->mask, ENDPRST_FIFORST | ENDPRST_TOGRST);
	writeb(ep->epnum, ep->reg_hcepctrl);
	return 0;
}

