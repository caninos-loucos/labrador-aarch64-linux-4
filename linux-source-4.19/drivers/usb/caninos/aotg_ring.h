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

#ifndef __LINUX_USB_AOTG_RING_H__
#define __LINUX_USB_AOTG_RING_H__

void aotg_hcd_dump_trb(struct aotg_trb *trb);
void aotg_hcd_dump_td(struct aotg_td *td);
void enable_overflow_irq(struct aotg_hcd *acthcd, struct aotg_hcep *ep);
void disable_overflow_irq(struct aotg_hcd *acthcd, struct aotg_hcep *ep);
void clear_overflow_irq(struct aotg_hcd *acthcd, struct aotg_hcep *ep);
struct aotg_td *aotg_alloc_td(gfp_t mem_flags);
void aotg_release_td(struct aotg_td *td);
int is_ring_running(struct aotg_ring *ring);
void start_ring(struct aotg_ring *ring);
void stop_ring(struct aotg_ring *ring);
struct aotg_ring *aotg_alloc_ring(struct aotg_hcd *acthcd,
	struct aotg_hcep *ep, unsigned int num_trbs, gfp_t mem_flags);
void aotg_free_ring(struct aotg_hcd *acthcd, struct aotg_ring *ring);
dma_addr_t ring_trb_virt_to_dma(struct aotg_ring *ring,
	struct aotg_trb *trb_vaddr);
int ring_empty(struct aotg_ring *ring);
inline int is_room_on_ring(struct aotg_ring *ring, unsigned int num_trbs);
inline unsigned int	count_urb_need_trbs(struct urb *urb);
inline unsigned int	count_sg_urb_need_trbs(struct urb *urb);
void aotg_fill_trb(struct aotg_trb *trb, u32 dma_addr, u32 len, u32 token);
int aotg_sg_map_trb(struct aotg_trb *trb,
	struct scatterlist *sg, int len, u32 token);
void inc_enqueue_safe(struct aotg_ring *ring);
void enqueue_trb(struct aotg_ring *ring, u32 buf_ptr,
	u32 buf_len, u32 token);
int ring_enqueue_sg_td(struct aotg_hcd *acthcd,
				struct aotg_ring *ring, struct aotg_td *td);
int aotg_ring_enqueue_td(struct aotg_hcd *acthcd,
				struct aotg_ring *ring, struct aotg_td *td);
void inc_dequeue_safe(struct aotg_ring *ring);
void aotg_ring_dequeue_td(struct aotg_ring *ring, struct aotg_td *td);
unsigned int get_ring_irq(struct aotg_hcd *acthcd);
void clear_ring_irq(struct aotg_hcd *acthcd, unsigned int irq_mask);
int finish_td(struct aotg_ring *ring, struct aotg_td *td);
void handle_ring_dma_tx(struct aotg_hcd *acthcd, unsigned int irq_mask);
void aotg_ring_irq_handler(struct aotg_hcd *acthcd);

#endif
