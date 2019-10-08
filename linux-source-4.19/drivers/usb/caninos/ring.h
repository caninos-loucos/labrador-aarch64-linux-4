#ifndef _RING_H_
#define _RING_H_

extern void aotg_free_ring(struct caninos_hcd *myhcd, struct aotg_ring *ring);

extern void aotg_stop_ring(struct aotg_ring *ring);

extern void dequeue_intr_td(struct aotg_ring *ring, struct aotg_td *td);

extern void dequeue_td
	(struct aotg_ring *ring, struct aotg_td *td, int dequeue_flag);
	
extern void aotg_intr_dma_buf_free
	(struct caninos_hcd *myhcd, struct aotg_ring *ring);

extern void aotg_ring_irq_handler(struct caninos_hcd *myhcd);

extern struct aotg_ring *aotg_alloc_ring
	(struct caninos_hcd *myhcd, struct aotg_hcep *ep,
	 unsigned int num_trbs, gfp_t mem_flags);

extern struct aotg_td *aotg_alloc_td(gfp_t mem_flags);

extern int aotg_ring_enqueue_td(struct caninos_hcd *myhcd,
	struct aotg_ring *ring, struct aotg_td *td);

extern int is_ring_running(struct aotg_ring *ring);

extern int aotg_ring_enqueue_intr_td
	(struct caninos_hcd *myhcd, struct aotg_ring *ring,
	struct aotg_hcep *ep, struct urb *urb, gfp_t mem_flags);

extern int aotg_intr_chg_buf_len
	(struct caninos_hcd *myhcd, struct aotg_ring *ring, int len);

extern int aotg_intr_get_finish_trb(struct aotg_ring *ring);

extern void aotg_reorder_intr_td(struct aotg_hcep *ep);

extern int aotg_ring_enqueue_isoc_td(struct caninos_hcd *myhcd,
	struct aotg_ring *ring, struct aotg_td *td);

extern int aotg_ring_dequeue_td(struct caninos_hcd *myhcd, struct aotg_ring *ring,
	struct aotg_td *td, int dequeue_flag);

extern int aotg_ring_dequeue_intr_td(struct caninos_hcd *myhcd, struct aotg_hcep *ep,
		struct aotg_ring *ring,	struct aotg_td *td);

extern void aotg_start_ring(struct aotg_ring *ring, u32 addr);

extern u32 ring_trb_virt_to_dma(struct aotg_ring *ring, struct aotg_trb *trb_vaddr);

extern int intr_finish_td(struct caninos_hcd *myhcd, struct aotg_ring *ring, struct aotg_td *td);

#endif

