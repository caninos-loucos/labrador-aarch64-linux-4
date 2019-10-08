#ifndef _EP_H_
#define _EP_H_

struct aotg_hcep;

#define  get_hcepcon_reg(dir , x , y , z)   ((dir ? x : y) + (z - 1)*8)
#define  get_hcepcs_reg(dir , x , y , z)   ((dir ? x : y) + (z - 1)*8)
#define  get_hcepctrl_reg(dir , x , y , z)   ((dir ? x : y) + (z - 1)*4)
#define  get_hcepbc_reg(dir , x , y , z)   ((dir ? x : y) + (z - 1)*8)
#define  get_hcepmaxpck_reg(dir , x , y , z)   ((dir ? x : y) + (z - 1)*2)
#define  get_hcepaddr_reg(dir , x , y , z)  ((dir ? x : y) + (z - 1)*4)
#define  get_hcep_dev_addr_reg(dir , x , y , z)  ((dir ? x : y) + (z - 1)*8)
#define  get_hcep_port_reg(dir , x , y , z)  ((dir ? x : y) + (z - 1)*8)
#define  get_hcep_splitcs_reg(dir , x , y , z)  ((dir ? x : y) + (z - 1)*8)

#define AOTG_DMA_OUT_PREFIX		0x10
#define AOTG_DMA_NUM_MASK		0xf
#define AOTG_IS_DMA_OUT(x)		((x) & AOTG_DMA_OUT_PREFIX)
#define AOTG_GET_DMA_NUM(x)		((x) & AOTG_DMA_NUM_MASK)

#define GET_DMALINKADDR_REG(dir, x, y, z) ((dir ? x : y) + (z - 1) * 0x10)
#define GET_CURADDR_REG(dir, x, y, z) ((dir ? x : y) + (z - 1) * 0x10)
#define GET_DMACTRL_REG(dir, x, y, z) ((dir ? x : y) + (z - 1) * 0x10)
#define GET_DMACOMPLETE_CNT_REG(dir, x, y, z) ((dir ? x : y) + (z - 1) * 0x10)

#define TRB_ITE	(1 << 11)
#define TRB_CHN	(1 << 10)
#define TRB_CSP	(1 << 9)
#define TRB_COF	(1 << 8)
#define TRB_ICE	(1 << 7)
#define TRB_IZE	(1 << 6)
#define TRB_ISE (1 << 5)
#define TRB_LT	(1 << 4)
#define AOTG_TRB_IOC	(1 << 3)
#define AOTG_TRB_IOZ	(1 << 2)
#define AOTG_TRB_IOS	(1 << 1)
#define TRB_OF	(1 << 0)

#define INTR_TRBS (10)

#define RING_IN_OF (0xFFFE)
#define RING_OUT_OF (0xFFFE0000)

#define TD_IN_FINISH (0)
#define TD_IN_RING  (0x1 << 0)
#define TD_IN_QUEUE (0x1 << 1)

struct aotg_trb {
	u32 hw_buf_ptr;
	u32 hw_buf_len;
	u32 hw_buf_remain;
	u32 hw_token;
};

struct aotg_ring
{
	unsigned is_running:1;
	unsigned is_out:1;
	unsigned intr_inited:1;
	unsigned intr_started:1;
	unsigned ring_stopped:1;
	int num_trbs;
	int type;
	u8 mask;
	void *priv;
	atomic_t num_trbs_free;
	
	int intr_mem_size;
	struct dma_pool *intr_dma_pool;
	u8 *intr_dma_buf_vaddr;
	dma_addr_t intr_dma_buf_phyaddr;
	char pool_name[32];
	
	struct aotg_trb *enqueue_trb;
	struct aotg_trb *dequeue_trb;
	struct aotg_trb *first_trb;
	struct aotg_trb *last_trb;
	struct aotg_trb *ring_trb;
	u32 trb_dma;
	
	unsigned int enring_cnt;
	unsigned int dering_cnt;
	
	volatile void __iomem *reg_dmalinkaddr;
	volatile void __iomem *reg_curaddr;
	volatile void __iomem *reg_dmactrl;
	volatile void __iomem *reg_dmacomplete_cnt;
};

struct aotg_td
{
	struct urb *urb;
	int num_trbs;
	u32 trb_dma;
	int err_count;
	struct aotg_trb *trb_vaddr;
	
	struct list_head queue_list;
	struct list_head enring_list;
	struct list_head dering_list;
	
	u8 *intr_mem_vaddr;
	dma_addr_t intr_men_phyaddr;
	int mem_size;
	
	unsigned cross_ring:1;
};

struct aotg_queue
{
	int in_using;
	struct aotg_hcep *ep;
	struct urb *urb;
	int dma_no;
	int is_xfer_start;
	int need_zero;
	
	struct list_head enqueue_list;
	struct list_head dequeue_list;
	struct list_head finished_list;
	int status;
	int length;
	
	struct aotg_td td;
	
	struct scatterlist *cur_sg;
	int err_count;
	unsigned long timeout;	/* jiffies + n. */
	
	/* fixing dma address unaligned to 4 Bytes. */
	u8 *dma_copy_buf;
	dma_addr_t dma_addr;
	
	/* for debug. */
	unsigned int seq_info;
} __attribute__ ((aligned(4)));

struct aotg_hcep
{
	struct usb_host_endpoint *hep;
	struct usb_device *udev;
	int index;
	/*
	just for ep0, when using hub,
	every usb device need a ep0 hcep data struct,
	but share the same hcd ep0.
	*/
	int ep0_index;
	int iso_packets;
	u32 maxpacket;
	u16 error_count;
	u16 length;
	u8 epnum;
	u8 nextpid;
	u8 mask;
	u8 type;
	u8 is_out;
	u8 buftype;
	u8 has_hub;
	u8 hub_addr;
	u8 reg_hcep_splitcs_val;
	
	void __iomem *reg_hcepcs;
	void __iomem *reg_hcepcon;
	void __iomem *reg_hcepctrl;
	void __iomem *reg_hcepbc;
	void __iomem *reg_hcmaxpck;
	void __iomem *reg_hcepaddr;
	void __iomem *reg_hcerr;
	void __iomem *reg_hcep_interval;
	void __iomem *reg_hcep_dev_addr;
	void __iomem *reg_hcep_port;
	void __iomem *reg_hcep_splitcs;
	
	unsigned int urb_enque_cnt;
	unsigned int urb_endque_cnt;
	unsigned int urb_stop_stran_cnt;
	unsigned int urb_unlinked_cnt;
	
	u32 dma_bytes;
	u16 interval;
	u16 load; /* one packet times in us. */
	
	u16 fifo_busy;
	
	ulong fifo_addr;
	struct aotg_queue *q;
	
	struct aotg_ring *ring;
	struct list_head queue_td_list;
	struct list_head enring_td_list;
	struct list_head dering_td_list;
};

static inline void caninos_hcep_reset
	(struct usb_hcd *hcd, u8 ep_mask, u8 type_mask)
{
	u8 val;
	
	struct caninos_hcd *myhcd = hcd_to_caninos(hcd);

	writeb(ep_mask, myhcd->ctrl->base + ENDPRST); /* select ep */
	val = ep_mask | type_mask;
	writeb(val, myhcd->ctrl->base + ENDPRST); /* reset ep */
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

static inline void ep_setup(struct aotg_hcep *ep, u8 type, u8 buftype)
{
	ep->buftype = buftype;
	writeb(type | buftype, ep->reg_hcepcon);
}

static inline void pio_irq_disable(struct caninos_hcd *myhcd, u8 mask)
{
	u8 is_out = mask & USB_HCD_OUT_MASK;
	u8 ep_num = mask & 0x0f;
	
	if (is_out) {
		usb_clearbitsw(1 << ep_num, myhcd->ctrl->base + HCOUTxIEN0);
	}
	else {
		usb_clearbitsw(1 << ep_num, myhcd->ctrl->base + HCINxIEN0);
	}
}

static inline void pio_irq_clear(struct caninos_hcd *myhcd, u8 mask)
{
	u8 is_out = mask & USB_HCD_OUT_MASK;
	u8 ep_num = mask & 0x0f;
	
	if (is_out) {
		writew(1 << ep_num, myhcd->ctrl->base + HCOUTxIRQ0);
	}
	else {
		writew(1 << ep_num, myhcd->ctrl->base + HCINxIRQ0);
	}
}

extern void finish_request
	(struct caninos_hcd *myhcd, struct aotg_queue *q, int status);

extern void handle_hcep0_out(struct caninos_hcd *myhcd);

extern void handle_hcep0_in(struct caninos_hcd *myhcd);

extern int caninos_hcep_config
	(struct caninos_hcd *myhcd, struct aotg_hcep *ep,
	u8 type, u8 buftype, int is_out);

#endif

