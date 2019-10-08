#ifndef _URB_H_
#define _URB_H_

extern int caninos_hub_urb_enqueue
	(struct usb_hcd *hcd, struct urb *urb, unsigned mem_flags);

extern int caninos_hub_urb_dequeue
	(struct usb_hcd *hcd, struct urb *urb, int status);

extern void caninos_hcd_release_queue
	(struct caninos_hcd *myhcd, struct aotg_queue *q);

#endif

