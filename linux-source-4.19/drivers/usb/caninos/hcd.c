#include "core.h"
#include "hcd.h"
#include "ep.h"
#include "ring.h"
#include "urb.h"

static inline void caninos_sofirq_clr(struct caninos_hcd *myhcd)
{
	usb_setbitsb((0x1 << 1), myhcd->ctrl->base + USBIRQ);
}

static inline void caninos_sofirq_on(struct caninos_hcd *myhcd)
{
	usb_setbitsb((0x1 << 1), myhcd->ctrl->base + USBIEN);
}

static inline void caninos_sofirq_off(struct caninos_hcd *myhcd)
{
	usb_clearbitsb(0x1 << 1, myhcd->ctrl->base + USBIEN);
}

static void handle_sof(struct caninos_hcd *myhcd)
{
	struct usb_hcd *hcd = caninos_to_hcd(myhcd);
	
	caninos_sofirq_clr(myhcd);
	caninos_sofirq_off(myhcd);
	
	if (HC_IS_SUSPENDED(hcd->state)) {
		usb_hcd_resume_root_hub(hcd);
	}
	
	usb_hcd_poll_rh_status(hcd);
}

static void handle_suspend(struct caninos_hcd *myhcd)
{
	usb_clearbitsb(SUSPEND_IRQIEN, myhcd->ctrl->base + USBEIEN);
	usb_setbitsb(SUSPEND_IRQIEN, myhcd->ctrl->base + USBEIRQ);
	
	caninos_sofirq_clr(myhcd);
	caninos_sofirq_on(myhcd);
}

static void caninos_hcd_err_handle
	(struct caninos_hcd *myhcd, u32 irqvector, int ep_num, int is_in)
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
	
	struct usb_hcd *hcd = caninos_to_hcd(myhcd);
	
	if (is_in) {
		writew(1 << ep_num, myhcd->ctrl->base + HCINxERRIRQ0);
	}
	else {
		writew(1 << ep_num, myhcd->ctrl->base + HCOUTxERRIRQ0);
	}
	
	if (ep_num == 0)
	{
		ep = myhcd->active_ep0;
		
		if (ep == NULL) {
			return;
		}
		
		q = ep->q;
		
		if (is_in) {
			ep->reg_hcerr = myhcd->ctrl->base + HCIN0ERR;
		}
		else {
			ep->reg_hcerr = myhcd->ctrl->base + HCOUT0ERR;
		}
	}
	else
	{
		if (is_in) {
			ep = myhcd->inep[ep_num];
		}
		else {
			ep = myhcd->outep[ep_num];
		}
		if (ep == NULL) {
			return;
		}
		
		ring = ep->ring;
		
		if (!ring) {
			return;
		}
		
		td = list_first_entry_or_null
			(&ep->enring_td_list, struct aotg_td, enring_list);
		
		if (!td)
		{
			aotg_stop_ring(ring);
			return;
		}
	}
	
	err_val = readb(ep->reg_hcerr);
	err_type = err_val & HCINxERR_TYPE_MASK;
	
	switch (err_type)
	{
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
	
	default:
		status = -EPIPE;
		break;
	}

	if (!(myhcd->port & USB_PORT_STAT_ENABLE)
		|| (myhcd->port & (USB_PORT_STAT_C_CONNECTION << 16))
		|| (myhcd->hcd_exiting)
		|| (!myhcd->inserted)
		|| !HC_IS_RUNNING(hcd->state)) {
		
		status = -ENODEV;
	}
	
	if (ep->index == 0)
	{
		q = ep->q;
		urb = q->urb;
		
		if ((status == -EPIPE) || (status == -ENODEV)) {
			writeb(HCINxERR_RESEND, ep->reg_hcerr);
		}
		finish_request(myhcd, q, status);
	}
	else
	{
		if ((status != -EPIPE) && (status != -ENODEV))
		{
			td->err_count++;

			if ((td->err_count < MAX_ERROR_COUNT) && (ep->error_count < 3)) {
				writeb(HCINxERR_RESEND, ep->reg_hcerr);
				return;
			}
		}
		
		if (status == -ETIMEDOUT || status == -EPIPE) {
			ep->error_count++;
		}
		
		reset = ENDPRST_FIFORST | ENDPRST_TOGRST;
		
		ep_disable(ep);
		
		if (is_in) {
			caninos_hcep_reset(hcd, ep->mask, reset);
		}
		else {
			caninos_hcep_reset(hcd, ep->mask | USB_HCD_OUT_MASK, reset);
		}
		
		aotg_stop_ring(ring);
		
		urb = td->urb;
		
		if (ep->type == PIPE_INTERRUPT) {
			dequeue_intr_td(ring, td);
		}
		else {
			dequeue_td(ring, td, TD_IN_FINISH);
		}
		
		if (urb)
		{
			usb_hcd_unlink_urb_from_ep(hcd, urb);
			usb_hcd_giveback_urb(hcd, urb, status);
		}
	}
}

static void caninos_hcd_abort_urb(struct caninos_hcd *myhcd)
{
	struct aotg_hcep *ep;
	struct urb *urb;
	struct aotg_ring *ring;
	struct aotg_td *td;
	unsigned long flags;
	int cnt;
	struct usb_hcd *hcd = caninos_to_hcd(myhcd);
	
	spin_lock_irqsave(&myhcd->lock, flags);
	
	/* Stop DMA first */
	for (cnt = 1; cnt < MAX_EP_NUM; cnt++)
	{
		ep = myhcd->inep[cnt];
		ring = ep ? ep->ring : NULL;
		
		if (ep && ring) {
			aotg_stop_ring(ring);
		}
	}

	for (cnt = 1; cnt < MAX_EP_NUM; cnt++)
	{
		ep = myhcd->outep[cnt];
		
		ring = ep ? ep->ring : NULL;
		
		if (ep && ring) {
			aotg_stop_ring(ring);
		}
	}

	for (cnt = 1; cnt < MAX_EP_NUM; cnt++)
	{
		ep = myhcd->inep[cnt];
		
		if (ep)
		{
			ring = ep->ring;
			
			td = list_first_entry_or_null
				(&ep->enring_td_list, struct aotg_td, enring_list);
			
			if (!td) {
				continue;
			}
			
			urb = td->urb;
			
			if (!urb) {
				continue;
			}
			
			if (ep->type == PIPE_INTERRUPT) {
				dequeue_intr_td(ring, td);
			}
			else {
				dequeue_td(ring, td, TD_IN_FINISH);
			}
			
			usb_hcd_unlink_urb_from_ep(hcd, urb);
			spin_unlock(&myhcd->lock);
			usb_hcd_giveback_urb(hcd, urb, -ENODEV);
			spin_lock(&myhcd->lock);
		}
	}
	
	for (cnt = 1; cnt < MAX_EP_NUM; cnt++)
	{
		ep = myhcd->outep[cnt];
		
		if (ep)
		{
			ring = ep->ring;
			
			td = list_first_entry_or_null
				(&ep->enring_td_list, struct aotg_td, enring_list);
			
			if (!td) {
				continue;
			}
			
			urb = td->urb;
			
			if (!urb) {
				continue;
			}
			
			if (ep->type == PIPE_INTERRUPT) {
				dequeue_intr_td(ring, td);
			}
			else {
				dequeue_td(ring, td, TD_IN_FINISH);
			}
			
			usb_hcd_unlink_urb_from_ep(hcd, urb);
			spin_unlock(&myhcd->lock);
			usb_hcd_giveback_urb(hcd, urb, -ENODEV);
			spin_lock(&myhcd->lock);
		}
	}
	spin_unlock_irqrestore(&myhcd->lock, flags);
}

static void caninos_device_state_handle(struct caninos_hcd *myhcd, u32 state)
{
	struct usb_hcd *hcd = caninos_to_hcd(myhcd);
	unsigned long flags;
	bool connect_changed;
	
	spin_lock_irqsave(&myhcd->lock, flags);
	
	connect_changed = false;
	
	if (state != AOTG_STATE_A_HOST)
	{
		if (myhcd->inserted)
		{
			myhcd->inserted = false;
			connect_changed = true;
			
			myhcd->port &= ~(USB_PORT_STAT_CONNECTION);
			myhcd->port &= ~(USB_PORT_STAT_ENABLE);
			myhcd->port &= ~(USB_PORT_STAT_LOW_SPEED);
			myhcd->port &= ~(USB_PORT_STAT_HIGH_SPEED);
			myhcd->port &= ~(USB_PORT_STAT_SUSPEND);
			
			myhcd->port |= (USB_PORT_STAT_C_CONNECTION << 16);
			myhcd->rhstate = AOTG_RH_NOATTACHED;
		}
	}
	else
	{
		if (!myhcd->inserted)
		{
			myhcd->inserted = true;
			connect_changed = true;
			
			myhcd->port |= (USB_PORT_STAT_C_CONNECTION << 16);
			myhcd->port |= USB_PORT_STAT_CONNECTION;
			myhcd->rhstate = AOTG_RH_ATTACHED;
		}
	}
	
	spin_unlock_irqrestore(&myhcd->lock, flags);
	
	if (connect_changed)
	{
		if (HC_IS_SUSPENDED(hcd->state)) {
			usb_hcd_resume_root_hub(hcd);
		}
		usb_hcd_poll_rh_status(hcd);
	}
}

static irqreturn_t caninos_hub_irq(struct usb_hcd *hcd)
{
	u8 eirq_mask, eirq_pending, otg_state;
	struct caninos_hcd *myhcd;
	u32 irqvector;
	
	
	
	myhcd = hcd_to_caninos(hcd);
	
	if (!myhcd) {
		return IRQ_NONE;
	}
	
	eirq_mask = readb(myhcd->ctrl->base + USBEIEN);
	eirq_pending = readb(myhcd->ctrl->base + USBEIRQ);
	
	if ((eirq_pending & SUSPEND_IRQIEN) && (eirq_mask & SUSPEND_IRQIEN))
	{
		handle_suspend(myhcd);
		return IRQ_HANDLED;
	}
	
	if (eirq_pending & USBEIRQ_USBIRQ)
	{
		irqvector = (u32)readb(myhcd->ctrl->base + IVECT);
		writeb(eirq_mask & USBEIRQ_USBIRQ, myhcd->ctrl->base + USBEIRQ);
		
		switch (irqvector)
		{
		case UIV_IDLE:
		case UIV_SRPDET:
		case UIV_LOCSOF:
		case UIV_VBUSERR:
		case UIV_PERIPH:
			if (readb(myhcd->ctrl->base + OTGIRQ) & OTGIRQ_LOCSOF)
			{
				writeb(OTGIRQ_LOCSOF, myhcd->ctrl->base + OTGIRQ);
				
				otg_state = readb(myhcd->ctrl->base + OTGSTATE);
				
				if (otg_state == AOTG_STATE_A_SUSPEND) {
					return IRQ_HANDLED;
				}
				
				if (otg_state == AOTG_STATE_A_WAIT_BCON)
				{
					caninos_disable_irq(myhcd->ctrl);
					myhcd->hcd_exiting = true;
					caninos_hcd_abort_urb(myhcd);
					return IRQ_HANDLED;
				}
				
				caninos_device_state_handle(myhcd, otg_state);
			}
			break;
			
		case UIV_SOF:
			writeb(USBIRQ_SOF, myhcd->ctrl->base + USBIRQ);
			handle_sof(myhcd);
			break;
			
		case UIV_USBRESET:
			if (myhcd->port & (USB_PORT_STAT_POWER | USB_PORT_STAT_CONNECTION))
			{
				myhcd->speed = USB_SPEED_FULL;
				myhcd->port |= (USB_PORT_STAT_C_RESET << 16);
				myhcd->port &= ~USB_PORT_STAT_RESET;
				
				/* clear usb reset irq */
				writeb(USBIRQ_URES, myhcd->ctrl->base + USBIRQ);
				
				/* reset all ep-in */
				caninos_hcep_reset(hcd, USB_HCD_IN_MASK,
				                   ENDPRST_FIFORST | ENDPRST_TOGRST);
				
				/* reset all ep-out */
				caninos_hcep_reset(hcd, USB_HCD_OUT_MASK,
				                   ENDPRST_FIFORST | ENDPRST_TOGRST);
				
				myhcd->port |= USB_PORT_STAT_ENABLE;
				myhcd->rhstate = AOTG_RH_ENABLE;
				
				/* now root port is fully enabled */
				if (readb(myhcd->ctrl->base + USBCS) & USBCS_HFMODE)
				{
					myhcd->speed = USB_SPEED_HIGH;
					myhcd->port |= USB_PORT_STAT_HIGH_SPEED;
					writeb(USBIRQ_HS, myhcd->ctrl->base + USBIRQ);
				}
				else if (readb(myhcd->ctrl->base + USBCS) & USBCS_LSMODE)
				{
					myhcd->speed = USB_SPEED_LOW;
					myhcd->port |= USB_PORT_STAT_LOW_SPEED;
				}
				else {
					myhcd->speed = USB_SPEED_FULL;
				}
				
				writew(0xffff, myhcd->ctrl->base + HCINxERRIRQ0);
				writew(0xffff, myhcd->ctrl->base + HCOUTxERRIRQ0);
				writew(0xffff, myhcd->ctrl->base + HCINxIRQ0);
				writew(0xffff, myhcd->ctrl->base + HCOUTxIRQ0);
				writew(0xffff, myhcd->ctrl->base + HCINxERRIEN0);
				writew(0xffff, myhcd->ctrl->base + HCOUTxERRIEN0);
			}
			break;
			
		case UIV_EP0IN:
			/* clear hcep0out irq */
			writew(1, myhcd->ctrl->base + HCOUTxIRQ0);
			handle_hcep0_out(myhcd);
			break;
			
		case UIV_EP0OUT:
			/* clear hcep0in irq */
			writew(1, myhcd->ctrl->base + HCINxIRQ0);
			handle_hcep0_in(myhcd);
			break;
			
		case UIV_EP1IN:
			/* clear hcep1out irq */
			writew(1<<1, myhcd->ctrl->base + HCOUTxIRQ0);
			break;
			
		case UIV_EP1OUT:
			/* clear hcep1in irq */
			writeb(1<<1, myhcd->ctrl->base + HCINxIRQ0); 
			break;
			
		case UIV_EP2IN:
			/* clear hcep2out irq */
			writew(1<<2, myhcd->ctrl->base + HCOUTxIRQ0);
			break;
			
		case UIV_EP2OUT:
			/* clear hcep2in irq */
			writeb(1<<2, myhcd->ctrl->base + HCINxIRQ0);
			break;
			
		default:
			if ((irqvector >= UIV_HCOUT0ERR) && (irqvector <= UIV_HCOUT15ERR))
			{
				caninos_hcd_err_handle
					(myhcd, irqvector, (irqvector - UIV_HCOUT0ERR), 0);
				break;
			}
			if ((irqvector >= UIV_HCIN0ERR) && (irqvector <= UIV_HCIN15ERR))
			{
				caninos_hcd_err_handle
					(myhcd, irqvector, (irqvector - UIV_HCIN0ERR), 1);
				break;
			}
			return IRQ_NONE;
		}
	}
	
	/* clear all surprise interrupts */
	caninos_clear_all_overflow_irq(hcd);
	caninos_clear_all_shortpkt_irq(hcd);
	caninos_clear_all_zeropkt_irq(hcd);
	caninos_clear_all_hcoutdma_irq(hcd);
	
	/* handle ring interrupts */
	aotg_ring_irq_handler(myhcd);
	
	return IRQ_HANDLED;
}

static inline void caninos_hub_descriptor(struct usb_hub_descriptor *desc)
{
	memset(desc, 0, sizeof(*desc));
	desc->bDescriptorType = 0x29;
	desc->bDescLength = 9;
	desc->wHubCharacteristics = (__force __u16)(__constant_cpu_to_le16(0x0001));
	desc->bNbrPorts = 1;
}

static inline void port_reset(struct caninos_hcd *myhcd)
{
	/* portrst & 55ms */
	writeb(0x1<<6 | 0x1<<5, myhcd->ctrl->base + HCPORTCTRL);
}

static inline void port_suspend(struct caninos_hcd *myhcd)
{
	usb_setbitsb(SUSPEND_IRQIEN, myhcd->ctrl->base + USBEIRQ);
	usb_setbitsb(SUSPEND_IRQIEN, myhcd->ctrl->base + USBEIEN);
	usb_clearbitsb(OTGCTRL_BUSREQ, myhcd->ctrl->base + OTGCTRL);
}

static inline void port_resume(struct caninos_hcd *myhcd)
{
	usb_setbitsb(OTGCTRL_BUSREQ, myhcd->ctrl->base + OTGCTRL);
}

static int set_port_feature(struct usb_hcd *hcd, u16 wValue)
{
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);
	
	switch (wValue)
	{
	case USB_PORT_FEAT_POWER:
		if (myhcd->port & USB_PORT_STAT_POWER) {
			break;
		}
		myhcd->port |= (0x1 << wValue);
		myhcd->rhstate = AOTG_RH_POWERED;
		hcd->self.controller->power.power_state = PMSG_ON;
		break;
			
	case USB_PORT_FEAT_RESET:
		if (myhcd->hcd_exiting) {
			return -ENODEV;
		}
		port_reset(myhcd);
		
		/* if it's already enabled, disable */
		myhcd->port &= ~(USB_PORT_STAT_ENABLE);
		myhcd->port &= ~(USB_PORT_STAT_LOW_SPEED);
		myhcd->port &= ~(USB_PORT_STAT_HIGH_SPEED);
		myhcd->port |= (0x1 << wValue);
		mdelay(2);
		myhcd->rhstate = AOTG_RH_RESET;
		
		/* enable reset irq */
		usb_setbitsb(USBIEN_URES, myhcd->ctrl->base + USBIEN);
		usb_setbitsb(USBEIRQ_USBIEN, myhcd->ctrl->base + USBEIEN);
		break;
		
	case USB_PORT_FEAT_SUSPEND:
		myhcd->port |= (0x1 << wValue);
		myhcd->rhstate = AOTG_RH_SUSPEND;
		break;
		
	default:
		if (myhcd->port & USB_PORT_STAT_POWER) {
			myhcd->port |= (0x1 << wValue);
		}
	}
	return 0;
}

static int clear_port_feature(struct usb_hcd *hcd, u16 wValue)
{
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);
	
	switch (wValue)
	{
	case USB_PORT_FEAT_ENABLE:
		myhcd->port &= ~(USB_PORT_STAT_ENABLE);
		myhcd->port &= ~(USB_PORT_STAT_LOW_SPEED);
		myhcd->port &= ~(USB_PORT_STAT_HIGH_SPEED);
		myhcd->rhstate = AOTG_RH_DISABLE;
		if (myhcd->port & USB_PORT_STAT_POWER) {
			hcd->self.controller->power.power_state = PMSG_SUSPEND;
		}
		break;
	
	case USB_PORT_FEAT_SUSPEND:
		myhcd->port &= ~(0x1 << wValue);
		break;
		
	case USB_PORT_FEAT_POWER:
		myhcd->port = 0;
		myhcd->rhstate = AOTG_RH_POWEROFF;
		hcd->self.controller->power.power_state = PMSG_SUSPEND;
		break;
		
	case USB_PORT_FEAT_C_ENABLE:
	case USB_PORT_FEAT_C_SUSPEND:
	case USB_PORT_FEAT_C_CONNECTION:
	case USB_PORT_FEAT_C_OVER_CURRENT:
	case USB_PORT_FEAT_C_RESET:
		myhcd->port &= ~(0x1 << wValue);
		break;
		
	default:
		return -EPERM;
	}
	return 0;
}

static int caninos_hub_control(struct usb_hcd *hcd,
	u16 typeReq, u16 wValue, u16 wIndex, char *buf, u16 wLength)
{
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);
	unsigned long flags;
	int retval = 0;
	
	
	
	if (in_irq()) {
		disable_irq_nosync(myhcd->ctrl->irq);
	}
	else {
		disable_irq(myhcd->ctrl->irq);
	}
	
	spin_lock_irqsave(&myhcd->lock, flags);
	
	if (!HC_IS_RUNNING(hcd->state))
	{
		spin_unlock_irqrestore(&myhcd->lock, flags);
		enable_irq(myhcd->ctrl->irq);
		return -EPERM;
	}
	
	switch (typeReq)
	{
	case ClearHubFeature:
		break;
	
	case ClearPortFeature:
		if (wIndex != 1 || wLength != 0) {
			retval = -EPIPE;
		}
		else {
			retval = clear_port_feature(hcd, wValue);
		}
		break;
	
	case GetHubDescriptor:
		caninos_hub_descriptor((struct usb_hub_descriptor *)buf);
		break;
	
	case GetHubStatus:
		*(__le32 *)buf = __constant_cpu_to_le32(0);
		break;
	
	case GetPortStatus:
		if (wIndex != 1) {
			retval = -EPIPE;
		}
		else {
			*(__le32 *)buf = cpu_to_le32(myhcd->port);
		}
		break;
		
	case SetHubFeature:
		retval = -EPIPE;
		break;
	
	case SetPortFeature:
		retval = set_port_feature(hcd, wValue);
		break;
		
	default:
		retval = -EPIPE;
		break;
	}
	
	spin_unlock_irqrestore(&myhcd->lock, flags);
	enable_irq(myhcd->ctrl->irq);
	return retval;
}

static int caninos_hub_status_data(struct usb_hcd *hcd, char *buf)
{
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);
	unsigned long flags;
	
	
	
	local_irq_save(flags);
	
	if (!HC_IS_RUNNING(hcd->state))
	{
		local_irq_restore(flags);
		return 0;
	}
	if ((myhcd->port & MY_OTG_PORT_C_MASK) != 0)
	{
		*buf = (0x1 << 1);
		local_irq_restore(flags);
		return 1;
	}
	
	local_irq_restore(flags);
	return 0;
}

static int caninos_hcd_start(struct usb_hcd *hcd)
{
	int retval = 0;
	unsigned long flags;
	
	local_irq_save(flags);
	
	hcd->state = HC_STATE_RUNNING;
	hcd->uses_new_polling = 1;
	
	local_irq_restore(flags);
	
	
	
	return retval;
}

static void caninos_hcd_stop(struct usb_hcd *hcd)
{
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);
	unsigned long flags;
	
	local_irq_save(flags);
	
	hcd->state = HC_STATE_HALT;
	myhcd->port = 0;
	myhcd->rhstate = AOTG_RH_POWEROFF;
	
	local_irq_restore(flags);
	
	
}

static int caninos_get_frame(struct usb_hcd *hcd)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	return tv.tv_usec / 1000;
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

static void release_fifo_addr(struct caninos_hcd *myhcd, ulong addr)
{
	int i;
	
	for (i = addr/ALLOC_FIFO_UNIT; i < AOTG_MAX_FIFO_SIZE/ALLOC_FIFO_UNIT; i++)
	{
		if ((myhcd->fifo_map[i] & 0x7FFFFFFF) == addr) {
			myhcd->fifo_map[i] = 0;
		}
		else {
			break;
		}
	}
}

static void caninos_hub_endpoint_disable
	(struct usb_hcd *hcd, struct usb_host_endpoint *hep)
{
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);
	struct aotg_hcep *ep = hep->hcpriv;
	unsigned long flags;
	int index, i;
	
	
	
	if (!ep) {
		return;
	}
	
	if (in_irq()) {
		disable_irq_nosync(myhcd->ctrl->irq);
	}
	else {
		disable_irq(myhcd->ctrl->irq);
	}
	
	spin_lock_irqsave(&myhcd->lock, flags);
	
	index = ep->index;
	
	if (index == 0)
	{
		myhcd->ep0[ep->ep0_index] = NULL;
		
		if (myhcd->active_ep0 == ep) {
			myhcd->active_ep0 = NULL;
		}
		
		for (i = 0; i < MAX_EP_NUM; i++) {
			if (myhcd->ep0[i] != NULL) {
				break;
			}
		}
		
		if (i == MAX_EP_NUM)
		{
			usb_clearbitsw(1, myhcd->ctrl->base + HCOUTxIEN0);
			usb_clearbitsw(1, myhcd->ctrl->base + HCINxIEN0);
			writew(1, myhcd->ctrl->base + HCOUTxIRQ0);
			writew(1, myhcd->ctrl->base + HCINxIRQ0);
		}
	}
	else
	{
		ep_disable(ep);
		
		if (ep->mask & USB_HCD_OUT_MASK) {
			myhcd->outep[index] = NULL;
		}
		else {
			myhcd->inep[index] = NULL;
		}
		release_fifo_addr(myhcd, ep->fifo_addr);
	}
	
	hep->hcpriv = NULL;
	
	if (ep->ring)
	{
		aotg_stop_ring(ep->ring);
		
		if (ep->ring->type == PIPE_INTERRUPT) {
			aotg_intr_dma_buf_free(myhcd, ep->ring);
		}
		
		aotg_free_ring(myhcd, ep->ring);
	}
	
	spin_unlock_irqrestore(&myhcd->lock, flags);
	enable_irq(myhcd->ctrl->irq);
	kfree(ep);
}

static void tasklet_finish_request(struct caninos_hcd *myhcd,
	struct aotg_queue *q, int status)
{
	struct urb *urb = q->urb;
	struct aotg_hcep *ep = q->ep;

	if (unlikely((myhcd == NULL) || (q == NULL) || (urb == NULL))) {
		return;
	}

	if ((q != NULL) && (ep != NULL)) {
		if (ep->q != NULL){
			if (ep->q == q)
				ep->q = NULL;
		}
	}
	else {
		return;
	}
	
	if (status == 0) {
		q->err_count = 0;
	}
	
	if (usb_pipetype(urb->pipe) == PIPE_CONTROL) {
		if ((myhcd->active_ep0 != NULL) && (myhcd->active_ep0 == q->ep)) {
			if (myhcd->active_ep0->q == NULL)
				myhcd->active_ep0 = NULL;
			
		}
	}
	
	caninos_hcd_release_queue(myhcd, q);
	ep->urb_endque_cnt++;
}

static int handle_setup_packet(struct caninos_hcd *myhcd, struct aotg_queue *q)
{
	struct urb *urb = q->urb;
	struct aotg_hcep *ep = q->ep;
	u32 *buf;
	void __iomem *addr = myhcd->ctrl->base + EP0INDATA_W0;
	int i = 0;
	
	if ((q->is_xfer_start) || (ep->q)) {
		return 0;
	}
	if (unlikely(!HC_IS_RUNNING(caninos_to_hcd(myhcd)->state))) {
		return -ESHUTDOWN;
	}
	
	if (myhcd->active_ep0 != NULL) {
		return -EBUSY;
	}
	
	writeb(ep->epnum, myhcd->ctrl->base + HCEP0CTRL);
	writeb((u8)ep->maxpacket, myhcd->ctrl->base + HCIN0MAXPCK);
	
	myhcd->active_ep0 = ep;
	ep->q = q;
	q->is_xfer_start = 1;
	usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), 1, 1);
	ep->nextpid = USB_PID_SETUP;
	buf = (u32 *)urb->setup_packet;
	
	/*initialize the setup stage */
	writeb(EP0CS_HCSET, myhcd->ctrl->base + EP0CS);
	
	while (readb(myhcd->ctrl->base + EP0CS) & EP0CS_HCOUTBSY)
	{
		writeb(EP0CS_HCSET, myhcd->ctrl->base + EP0CS);
		i++;
		if (i > 2000000) {
			break;
		}
	}
	
	if (!(readb(myhcd->ctrl->base + EP0CS) & EP0CS_HCOUTBSY))
	{
		/*fill the setup data in fifo */
		writel(*buf, addr);
		addr += 4;
		buf++;
		writel(*buf, addr);
		writeb(8, myhcd->ctrl->base + HCOUT0BC);
	}
	
	return 0;
}

static inline int start_transfer(struct caninos_hcd *myhcd,
	struct aotg_queue *q, struct aotg_hcep *ep)
{
	struct urb *urb = q->urb;
	int retval = 0;
	
	ep->urb_enque_cnt++;
	q->length = urb->transfer_buffer_length;
	
	/* do with hub connected. */
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
	
	switch (usb_pipetype(urb->pipe)) {
	case PIPE_CONTROL:
		q->timeout = jiffies + HZ/2;
		retval = handle_setup_packet(myhcd, q);
		break;

	default:
		break;
	}
	
	return retval;
}

void urb_tasklet_func(unsigned long data)
{
	struct caninos_hcd *myhcd = (struct caninos_hcd *)data;
	struct aotg_queue *q, *next;
	struct aotg_hcep *ep;
	struct urb *urb;
	struct aotg_ring *ring;
	struct aotg_td *td;
	unsigned long flags;
	
	int status;
	struct usb_hcd *hcd = caninos_to_hcd(myhcd);
	int cnt = 0;
	
	do
	{
		status = (int)spin_is_locked(&myhcd->tasklet_lock);
		
		if (status) {
			myhcd->tasklet_retry = 1;
			return;
		}
		
		cnt++;
		status = spin_trylock(&myhcd->tasklet_lock);
		
		if ((!status) && (cnt > 10))
		{
			myhcd->tasklet_retry = 1;
			return;
		}
		
	} while (status == 0);
	
	disable_irq_nosync(myhcd->ctrl->irq);
	
	spin_lock_irqsave(&myhcd->lock, flags);
	
	for (cnt = 1; cnt < MAX_EP_NUM; cnt++)
	{
		ep = myhcd->inep[cnt];
		if (ep && (ep->type == PIPE_INTERRUPT))
		{
			ring = ep->ring;
			
			if (ring->ring_stopped)
			{
				td = list_first_entry_or_null
					(&ep->enring_td_list, struct aotg_td, enring_list);
				if (!td) {
					continue;
				}
				urb = td->urb;
				if (!urb) {
					continue;
				}
				intr_finish_td(myhcd, ring, td);
			}
		}
	}
	/* do dequeue task. */
DO_DEQUEUE_TASK:
	urb = NULL;
	list_for_each_entry_safe(q, next, &myhcd->hcd_dequeue_list, dequeue_list)
	{
		if (q->status < 0)
		{
			urb = q->urb;
			ep = q->ep;
			if (ep) {
				ep->urb_unlinked_cnt++;
			}
			list_del(&q->dequeue_list);
			status = q->status;
			tasklet_finish_request(myhcd, q, status);
			hcd = bus_to_hcd(urb->dev->bus);
			break;
		}
	}
	
	if (urb != NULL)
	{
		usb_hcd_unlink_urb_from_ep(hcd, urb);
		spin_unlock_irqrestore(&myhcd->lock, flags);
		usb_hcd_giveback_urb(hcd, urb, status);
		spin_lock_irqsave(&myhcd->lock, flags);
		goto DO_DEQUEUE_TASK;
	}
	
	/* do finished task. */
DO_FINISH_TASK:
	urb = NULL;
	list_for_each_entry_safe(q, next, &myhcd->hcd_finished_list, finished_list)
	{
		if (q->finished_list.next != LIST_POISON1) {
			list_del(&q->finished_list);
		}
		else {
			break;
		}
		status = q->status;
		tasklet_finish_request(myhcd, q, status);
		
		hcd = caninos_to_hcd(myhcd);
		urb = q->urb;
		ep = q->ep;
		
		if (urb != NULL) {
			break;
		}
	}
	if (urb != NULL)
	{
		usb_hcd_unlink_urb_from_ep(hcd, urb);
		spin_unlock_irqrestore(&myhcd->lock, flags);
		
		usb_hcd_giveback_urb(hcd, urb, status);
		
		spin_lock_irqsave(&myhcd->lock, flags);
		goto DO_FINISH_TASK;
	}
	
	/* start transfer directly, don't care setup appearing in bulkout. */
	q = list_first_entry_or_null
		(&myhcd->hcd_enqueue_list, struct aotg_queue, enqueue_list);
	
	if (q && (q->urb))
	{
		urb = q->urb;
		ep = q->ep;
		
		if ((myhcd->active_ep0 != NULL) && (myhcd->active_ep0->q != NULL))
		{
			myhcd->ep0_block_cnt++;
			if ((myhcd->ep0_block_cnt % 5) == 0)
			{
				myhcd->ep0_block_cnt = 0;
				
				spin_unlock_irqrestore(&myhcd->lock, flags);
				enable_irq(myhcd->ctrl->irq);
				spin_unlock(&myhcd->tasklet_lock);
				
				caninos_hub_urb_dequeue
					(hcd, myhcd->active_ep0->q->urb, -ETIMEDOUT);
				return;
			}
			
			spin_unlock_irqrestore(&myhcd->lock, flags);
			enable_irq(myhcd->ctrl->irq);
			spin_unlock(&myhcd->tasklet_lock);
			return;
		}
		else {
			myhcd->ep0_block_cnt = 0;
		}
		
		list_del(&q->enqueue_list);
		status = start_transfer(myhcd, q, ep);
		
		if (unlikely(status < 0))
		{
			hcd = bus_to_hcd(urb->dev->bus);
			caninos_hcd_release_queue(myhcd, q);
			
			usb_hcd_unlink_urb_from_ep(hcd, urb);
			spin_unlock_irqrestore(&myhcd->lock, flags);
			usb_hcd_giveback_urb(hcd, urb, status);
			spin_lock_irqsave(&myhcd->lock, flags);
		}
	}
	
	if (myhcd->tasklet_retry != 0)
	{
		myhcd->tasklet_retry = 0;
		goto DO_DEQUEUE_TASK;
	}
	
	spin_unlock_irqrestore(&myhcd->lock, flags);
	enable_irq(myhcd->ctrl->irq);
	spin_unlock(&myhcd->tasklet_lock);
}

struct hc_driver caninos_hc_driver = {
	.description = DRIVER_NAME,
	.product_desc = DRIVER_DESC,
	.hcd_priv_size = sizeof(struct caninos_hcd),
	
	.irq = caninos_hub_irq,
	.flags = HCD_USB2 | HCD_MEMORY,
	
	.start = caninos_hcd_start,
	.stop = caninos_hcd_stop,
	
	.get_frame_number = caninos_get_frame,
	
	.urb_enqueue = caninos_hub_urb_enqueue,
	.urb_dequeue = caninos_hub_urb_dequeue,
	
	.map_urb_for_dma = caninos_map_urb,
	.unmap_urb_for_dma = caninos_unmap_urb,
	
	.endpoint_disable = caninos_hub_endpoint_disable,
	
	.hub_status_data = caninos_hub_status_data,
	.hub_control = caninos_hub_control,
	
	.bus_suspend = NULL,
	.bus_resume = NULL,
};

