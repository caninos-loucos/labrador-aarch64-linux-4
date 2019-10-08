#include "core.h"
#include "hcd.h"
#include "ep.h"
#include "ring.h"
#include "urb.h"

void caninos_hcd_release_queue(struct caninos_hcd *myhcd, struct aotg_queue *q)
{
	int i;
	
	/* release all */
	if (q == NULL)
	{
		for (i = 0; i < AOTG_QUEUE_POOL_CNT; i++)
		{
			if (myhcd->queue_pool[i] != NULL)
			{
				kfree(myhcd->queue_pool[i]);
				myhcd->queue_pool[i] = NULL;
			}
		}
		return;
	}
	
	q->td.trb_vaddr = NULL;
	
	for (i = 0; i < AOTG_QUEUE_POOL_CNT; i++)
	{
		if (myhcd->queue_pool[i] == q)
		{
			myhcd->queue_pool[i]->in_using = 0;
			return;
		}
	}
	
	kfree(q);
}

static struct aotg_queue *caninos_hcd_get_queue(struct caninos_hcd *myhcd,
	struct urb *urb, unsigned mem_flags)
{
	int i;
	int empty_idx = -1;
	struct aotg_queue *q = NULL;

	for (i = 0; i < AOTG_QUEUE_POOL_CNT; i++)
	{
		if (myhcd->queue_pool[i] != NULL)
		{
			if (myhcd->queue_pool[i]->in_using == 0)
			{
				q = myhcd->queue_pool[i];
				break;
			}
		}
		else
		{
			if (empty_idx < 0) {
				empty_idx = i;
			}
		}
	}
	if (i == AOTG_QUEUE_POOL_CNT)
	{
		q = kzalloc(sizeof(*q), GFP_ATOMIC);
		
		if (unlikely(!q)) {
			return NULL;
		}
		
		if ((empty_idx >= 0) && (empty_idx < AOTG_QUEUE_POOL_CNT)) {
			myhcd->queue_pool[empty_idx] = q;
		}
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

static inline void caninos_config_hub_addr(struct urb *urb, struct aotg_hcep *ep)
{
	if (ep->has_hub)
	{
		if (urb->dev->speed == USB_SPEED_HIGH)
		{
			writeb(usb_pipedevice(urb->pipe), ep->reg_hcep_dev_addr);
			writeb(urb->dev->portnum, ep->reg_hcep_port);
		}
		else
		{
			writeb((0x80 | usb_pipedevice(urb->pipe)), ep->reg_hcep_dev_addr);
			
			if (urb->dev->speed == USB_SPEED_LOW) {
				writeb(0x80 | urb->dev->portnum, ep->reg_hcep_port);
			}
			else {
				writeb(urb->dev->portnum, ep->reg_hcep_port);
			}
		}
	}
	else {
		writeb(usb_pipedevice(urb->pipe), ep->reg_hcep_dev_addr);
		writeb(urb->dev->portnum, ep->reg_hcep_port);
	}
}

static void caninos_start_ring_transfer
	(struct caninos_hcd *myhcd, struct aotg_hcep *ep, struct urb *urb)
{
	u32 addr;
	struct aotg_trb *trb;
	struct aotg_ring *ring = ep->ring;

	caninos_config_hub_addr(urb, ep);
	
	if (usb_pipetype(urb->pipe) == PIPE_INTERRUPT)
	{
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

static int caninos_hcep_set_split_micro_frame
	(struct caninos_hcd *myhcd, struct aotg_hcep *ep)
{
	static const u8 split_val[] = {0x31, 0x42, 0x53, 0x64, 0x75, 0x17, 0x20};
	int i, index;
	u8 set_val, rd_val;
	
	for (i = 0; i < sizeof(split_val); i++)
	{
		set_val = split_val[i];
		
		for (index = 0; index < MAX_EP_NUM; index++)
		{
			if (myhcd->inep[index] != NULL)
			{
				rd_val = myhcd->inep[index]->reg_hcep_splitcs_val;
				
				if ((0 == rd_val) || (set_val != rd_val)) {
					continue;
				}
				if (set_val == rd_val) {
					set_val = 0;
				}
				break;
			}
		}
		if (set_val == 0) {
			continue;
		}
		for (index = 0; index < MAX_EP_NUM; index++)
		{
			if (myhcd->outep[index] != NULL)
			{
				rd_val = myhcd->outep[index]->reg_hcep_splitcs_val;
				
				if ((0 == rd_val) || (set_val != rd_val)) {
					continue;
				}
				if (set_val == rd_val) {
					set_val = 0;
				}
				break;
			}
		}
		if (set_val != 0) {
			break;
		}
	}

	if (set_val != 0)
	{
		ep->reg_hcep_splitcs_val = set_val;
		writeb(set_val, ep->reg_hcep_splitcs);
	}
	return 0;
}

static struct aotg_hcep	*caninos_hcep_alloc(struct usb_hcd *hcd, struct urb *urb)
{
	struct aotg_hcep *ep = NULL;
	int pipe = urb->pipe;
	int is_out = usb_pipeout(pipe);
	int type = usb_pipetype(pipe);
	int i, retval = 0;
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);
	u8 think_time;
	
	ep = kzalloc(sizeof(*ep), GFP_ATOMIC);
	
	if (NULL == ep)
	{
		retval = -ENOMEM;
		goto exit;
	}
	
	ep->udev = urb->dev;
	ep->epnum = usb_pipeendpoint(pipe);
	ep->maxpacket = usb_maxpacket(ep->udev, urb->pipe, is_out);
	ep->type = type;
	ep->urb_enque_cnt = 0;
	ep->urb_endque_cnt = 0;
	ep->urb_stop_stran_cnt = 0;
	ep->urb_unlinked_cnt = 0;
	
	ep->length = 0;
	
	usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), is_out, 0);
	
	if (urb->dev->parent)
	{
		if (urb->dev->tt)
		{
			/* calculate in ns. */
			think_time = (urb->dev->tt->think_time / 666);
			
			if (think_time <= 0) {
				think_time = 1;
			}
			else if (think_time > 4) {
				think_time = 4;
			}
			
			think_time = think_time * 20;
			writeb(think_time, myhcd->ctrl->base + HCTRAINTERVAL);
		}
		
		if ((urb->dev->parent->parent) && (urb->dev->parent != hcd->self.root_hub))
		{
			ep->has_hub = 1;
			ep->hub_addr = 0x7f & readb(myhcd->ctrl->base + FNADDR);
		}
		else {
			ep->has_hub = 0;
		}
	}
	
	switch (type)
	{
	case PIPE_CONTROL:
		ep->reg_hcep_dev_addr = myhcd->ctrl->base + HCEP0ADDR;
		ep->reg_hcep_port = myhcd->ctrl->base + HCEP0PORT;
		ep->reg_hcep_splitcs = myhcd->ctrl->base + HCEP0SPILITCS;

		for (i = 0; i < MAX_EP_NUM; i++)
		{
			if (myhcd->ep0[i] == NULL)
			{
				ep->ep0_index = i;
				myhcd->ep0[i] = ep;
				break;
			}
		}
		
		ep->index = 0;
		ep->mask = 0;
		usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), 1, 0);
		usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), 0, 0);
		
		if (myhcd->active_ep0 == NULL)
		{
			usb_setbitsw(1, myhcd->ctrl->base + HCOUTxIEN0);
			usb_setbitsw(1, myhcd->ctrl->base + HCINxIEN0);
			writew(1, myhcd->ctrl->base + HCOUTxIRQ0);
			writew(1, myhcd->ctrl->base + HCINxIRQ0);
			
			if (ep->has_hub) {
				usb_setbitsb(0x80, myhcd->ctrl->base + FNADDR);
			}
			else {
				writeb(usb_pipedevice(urb->pipe), myhcd->ctrl->base + FNADDR);
			}
		}
		break;

	case PIPE_BULK:
		retval = caninos_hcep_config
			(myhcd, ep, EPCON_TYPE_BULK, EPCON_BUF_SINGLE, is_out);
		if (retval < 0) {
			kfree(ep);
			goto exit;
		}
		break;

	case PIPE_INTERRUPT:
		retval = caninos_hcep_config
			(myhcd, ep, EPCON_TYPE_INT, EPCON_BUF_SINGLE, is_out);
		if (retval < 0) {
			kfree(ep);
			goto exit;
		}
		ep->interval = urb->ep->desc.bInterval;
		writeb(ep->interval, ep->reg_hcep_interval);
		break;

	case PIPE_ISOCHRONOUS:
		retval = caninos_hcep_config
			(myhcd, ep, EPCON_TYPE_ISO, EPCON_BUF_SINGLE, is_out);
		ep->iso_packets = (urb->ep->desc.wMaxPacketSize >> 11) & 3;
		ep->interval = urb->ep->desc.bInterval;
		writeb(ep->interval, ep->reg_hcep_interval);
		usb_setbitsb(ep->iso_packets << 4, ep->reg_hcepcon);

		break;

	default:
		retval = -ENODEV;
		kfree(ep);
		goto exit;
	}

	if ((ep->udev->speed != USB_SPEED_HIGH) && ep->has_hub &&
		(type == PIPE_INTERRUPT)) {
		caninos_hcep_set_split_micro_frame(myhcd, ep);
	}
	ep->hep = urb->ep;
	urb->ep->hcpriv = ep;
	return ep;

exit:
	return NULL;
}

int caninos_hub_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);
	struct aotg_hcep *ep;
	struct aotg_queue *q = NULL, *next, *tmp;
	struct aotg_ring *ring;
	struct aotg_td *td, *next_td;
	unsigned long flags;
	int retval = 0;
	
	if (myhcd == NULL) {
		return -EIO;
	}
	
	spin_lock_irqsave(&myhcd->lock, flags);
	
	retval = usb_hcd_check_unlink_urb(hcd, urb, status);
	
	if (retval) {
		goto dequeue_out;
	}
	
	ep = (struct aotg_hcep *)urb->ep->hcpriv;
	
	if (ep == NULL)
	{
		retval = -EINVAL;
		goto dequeue_out;
	}
	
	if (!usb_pipecontrol(urb->pipe))
	{
		ep->urb_unlinked_cnt++;
		ring = ep->ring;
		
		if (usb_pipeint(urb->pipe))
		{
			list_for_each_entry_safe(td, next_td, &ep->enring_td_list, enring_list)
			{
				if (urb == td->urb)
				{
					retval = aotg_ring_dequeue_intr_td(myhcd, ep, ring, td);
					goto de_bulk;
				}
			}
		}
		
		list_for_each_entry_safe(td, next_td, &ep->queue_td_list, queue_list)
		{
			if (urb == td->urb)
			{
				retval = aotg_ring_dequeue_td(myhcd, ring, td, TD_IN_QUEUE);
				goto de_bulk;
			}
		}
		
		list_for_each_entry_safe(td, next_td, &ep->enring_td_list, enring_list)
		{
			mb();
			if (urb == td->urb)
			{
				retval = aotg_ring_dequeue_td(myhcd, ring, td, TD_IN_RING);
				ep->urb_stop_stran_cnt++;
				goto de_bulk;
			}
		}
		
		retval = -EINVAL;
		goto dequeue_out;

de_bulk:
		usb_hcd_unlink_urb_from_ep(hcd, urb);
		spin_unlock(&myhcd->lock);
		usb_hcd_giveback_urb(hcd, urb, status);
		spin_lock(&myhcd->lock);
		spin_unlock_irqrestore(&myhcd->lock, flags);
		return retval;
	}
	
	q = ep->q;
	
	/* ep->mask currently equal to q->dma_no. */
	if (q && (q->urb == urb))
	{
		writeb(EP0CS_HCSET, myhcd->ctrl->base + EP0CS);
		
		/* maybe finished in tasklet_finish_request. */
		if (!list_empty(&q->finished_list))
		{
			if (q->finished_list.next != LIST_POISON1) {
				list_del(&q->finished_list);
			}
		}
		
		if (q->is_xfer_start)
		{
			ep->urb_stop_stran_cnt++;
			q->is_xfer_start = 0;
		}
	}
	else
	{
		q = NULL;
		
		list_for_each_entry_safe
			(tmp, next, &myhcd->hcd_enqueue_list, enqueue_list)
		{
			if (tmp->urb == urb)
			{
				list_del(&tmp->enqueue_list);
				q = tmp;
				ep = q->ep;
				break;
			}
		}
	}
	
	if (likely(q))
	{
		q->status = status;
		list_add_tail(&q->dequeue_list, &myhcd->hcd_dequeue_list);
		spin_unlock_irqrestore(&myhcd->lock, flags);
		tasklet_schedule(&myhcd->urb_tasklet);
		return retval;
	}

dequeue_out:
	spin_unlock_irqrestore(&myhcd->lock, flags);
	return retval;
}

int caninos_hub_urb_enqueue
	(struct usb_hcd *hcd, struct urb *urb, unsigned mem_flags)
{
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);
	struct aotg_queue *q = NULL;
	unsigned long flags;
	struct aotg_hcep *ep = NULL;
	struct aotg_td *td, *next;
	int pipe = urb->pipe;
	int type = usb_pipetype(pipe);
	int retval = 0;
	
	if (myhcd == NULL) {
		return -EIO;
	}
	
	if (myhcd->hcd_exiting) {
		return -ENODEV;
	}
	
	if (!(myhcd->port & USB_PORT_STAT_ENABLE)
		|| (myhcd->port & (USB_PORT_STAT_C_CONNECTION << 16))
		|| (myhcd->hcd_exiting)
		|| (!myhcd->inserted)
		|| !HC_IS_RUNNING(hcd->state)) {
		return -ENODEV;
	}
	
	spin_lock_irqsave(&myhcd->lock, flags);
	
	ep = urb->ep->hcpriv;
	
	if ((unlikely(!urb->ep->enabled)) || (likely(ep) &&
		unlikely(ep->error_count > 3))) {
		retval = -ENOENT;
		goto exit0;
	}
	
	retval = usb_hcd_link_urb_to_ep(hcd, urb);
	
	if (retval) {
		goto exit0;
	}
	
	if (likely(urb->ep->hcpriv)) {
		ep = (struct aotg_hcep *)urb->ep->hcpriv;
	}
	else
	{
		ep = caninos_hcep_alloc(hcd, urb);
		
		if (NULL == ep)
		{
			retval = -ENOMEM;
			goto exit1;
		}
		
		if (!usb_pipecontrol(pipe))
		{
			if (usb_pipeint(pipe)) {
				ep->ring = aotg_alloc_ring(myhcd, ep, INTR_TRBS, GFP_ATOMIC);
			}
			else {
				ep->ring = aotg_alloc_ring(myhcd, ep, NUM_TRBS, GFP_ATOMIC);
			}
			
			if (!ep->ring)
			{
				retval = -ENOMEM;
				goto exit1;
			}
			
			INIT_LIST_HEAD(&ep->queue_td_list);
			INIT_LIST_HEAD(&ep->enring_td_list);
			INIT_LIST_HEAD(&ep->dering_td_list);
		}
		urb->ep->hcpriv	= ep;
	}
	
	urb->hcpriv = hcd;
	
	if (type == PIPE_CONTROL)
	{
		q = caninos_hcd_get_queue(myhcd, urb, mem_flags);
		
		if (unlikely(!q))
		{
			spin_unlock_irqrestore(&myhcd->lock, flags);
			return -ENOMEM;
		}
		
		q->ep = ep;
		q->urb = urb;
		
		list_add_tail(&q->enqueue_list, &myhcd->hcd_enqueue_list);
	}
	else if (type == PIPE_BULK)
	{
		td = aotg_alloc_td(mem_flags);
		
		if (!td) {
			retval = -ENOMEM;
			goto exit1;
		}
		td->urb = urb;

		ep->urb_enque_cnt++;

		if (list_empty(&ep->queue_td_list))
		{
			retval = aotg_ring_enqueue_td(myhcd, ep->ring, td);
			
			if (retval) {
				list_add_tail(&td->queue_list, &ep->queue_td_list);
				goto out;
			}
			
			list_add_tail(&td->enring_list, &ep->enring_td_list);
			ep->ring->enring_cnt++;
		}
		else {
			list_add_tail(&td->queue_list, &ep->queue_td_list);
		}
		
		if (!list_empty(&ep->enring_td_list) &&
			!is_ring_running(ep->ring)) {
			caninos_start_ring_transfer(myhcd, ep, urb);
		}
	}
	else if (type == PIPE_INTERRUPT)
	{
		if (unlikely(ep->ring->intr_inited == 0))
		{
			retval = aotg_ring_enqueue_intr_td
				(myhcd, ep->ring, ep, urb, GFP_ATOMIC);
			
			if (retval)
			{
				goto exit1;
			}
			
			ep->ring->intr_started = 0;
		}
		ep->urb_enque_cnt++;
		
		list_for_each_entry_safe(td, next, &ep->enring_td_list, enring_list)
		{
			if (td->urb) {
				continue;
			}
			else
			{
				td->urb = urb;
				break;
			}
		}
		
		if (unlikely(ep->ring->enqueue_trb->hw_buf_len != urb->transfer_buffer_length))
		{
			aotg_intr_chg_buf_len(myhcd, ep->ring, urb->transfer_buffer_length);
		}

		if (ep->ring->intr_started == 0)
		{
			ep->ring->intr_started = 1;
			caninos_start_ring_transfer(myhcd, ep, urb);
		}
		
		if (!is_ring_running(ep->ring))
		{
			if (ep->is_out) {
				caninos_start_ring_transfer(myhcd, ep, urb);
			}
			else
			{
				if (aotg_intr_get_finish_trb(ep->ring) == 0)
				{
					ep->ring->ring_stopped = 0;
					aotg_reorder_intr_td(ep);
					ep_enable(ep);
					mb();
					writel(DMACTRL_DMACS, ep->ring->reg_dmactrl);
				}
				else {
					ep->ring->ring_stopped = 1;
				}
			}
		}

	}
	else 
	{
		td = aotg_alloc_td(mem_flags);
		if (!td)
		{
			retval = -ENOMEM;
			goto exit1;
		}
		td->urb = urb;
		ep->urb_enque_cnt++;

		if (list_empty(&ep->queue_td_list))
		{
			retval = aotg_ring_enqueue_isoc_td(myhcd, ep->ring, td);
			
			if (retval)
			{
				list_add_tail(&td->queue_list, &ep->queue_td_list);
				goto out;
			}

			list_add_tail(&td->enring_list, &ep->enring_td_list);
			ep->ring->enring_cnt++;
		}
		else {
			list_add_tail(&td->queue_list, &ep->queue_td_list);
		}

		if (!list_empty(&ep->enring_td_list) && !is_ring_running(ep->ring))
		{
			caninos_start_ring_transfer(myhcd, ep, urb);
		}
	}
out:
	spin_unlock_irqrestore(&myhcd->lock, flags);
	tasklet_hi_schedule(&myhcd->urb_tasklet);
	return retval;
exit1:
	usb_hcd_unlink_urb_from_ep(hcd, urb);
exit0:
	if (unlikely(retval < 0) && ep)
	{
		if (type == PIPE_CONTROL)
		{
			if (ep) {
				ep->q = NULL;
			}
			if (q) {
				caninos_hcd_release_queue(myhcd, q);
			}
		}
		else
		{
			writel(DMACTRL_DMACC, ep->ring->reg_dmactrl);
			ep_disable(ep);
		}
	}
	spin_unlock_irqrestore(&myhcd->lock, flags);
	return retval;
}


