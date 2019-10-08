#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_dma.h>
#include <linux/of_device.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/ktime.h>
#include "../virt-dma.h"

#define DRIVER_NAME "caninos-dma"
#define DRIVER_DESC "Caninos Labrador DMA Controller Driver"

#define OWL_DMA_FRAME_MAX_LENGTH	0xffff0

/* global register for dma controller */
#define DMA_IRQ_PD0			(0x00)
#define DMA_IRQ_PD1			(0x04)
#define DMA_IRQ_PD2			(0x08)
#define DMA_IRQ_PD3			(0x0C)
#define DMA_IRQ_EN0			(0x10)
#define DMA_IRQ_EN1			(0x14)
#define DMA_IRQ_EN2			(0x18)
#define DMA_IRQ_EN3			(0x1C)
#define DMA_SECURE_ACCESS_CTL		(0x20)
#define DMA_NIC_QOS			(0x24)
#define DMA_DBGSEL			(0x28)
#define DMA_IDLE_STAT			(0x2C)

/* channel register */
#define DMA_CHAN_BASE(i)		(0x100 + (i) * 0x100)
#define DMAX_MODE			(0x00)
#define DMAX_SOURCE			(0x04)
#define DMAX_DESTINATION		(0x08)
#define DMAX_FRAME_LEN			(0x0C)
#define DMAX_FRAME_CNT			(0x10)
#define DMAX_REMAIN_FRAME_CNT		(0x14)
#define DMAX_REMAIN_CNT			(0x18)
#define DMAX_SOURCE_STRIDE		(0x1C)
#define DMAX_DESTINATION_STRIDE		(0x20)
#define DMAX_START			(0x24)
#define DMAX_PAUSE			(0x28)
#define DMAX_CHAINED_CTL		(0x2C)
#define DMAX_CONSTANT			(0x30)
#define DMAX_LINKLIST_CTL		(0x34)
#define DMAX_NEXT_DESCRIPTOR		(0x38)
#define DMAX_CURRENT_DESCRIPTOR_NUM	(0x3C)
#define DMAX_INT_CTL			(0x40)
#define DMAX_INT_STATUS			(0x44)
#define DMAX_CURRENT_SOURCE_POINTER	(0x48)
#define DMAX_CURRENT_DESTINATION_POINTER	(0x4C)

/* DMAX_MODE */
#define DMA_MODE_TS(x)			(((x) & 0x3f) << 0)
#define DMA_MODE_ST(x)			(((x) & 0x3) << 8)
#define		DMA_MODE_ST_DEV			DMA_MODE_ST(0)
#define		DMA_MODE_ST_DCU			DMA_MODE_ST(2)
#define		DMA_MODE_ST_SRAM		DMA_MODE_ST(3)
#define DMA_MODE_DT(x)			(((x) & 0x3) << 10)
#define		DMA_MODE_DT_DEV			DMA_MODE_DT(0)
#define		DMA_MODE_DT_DCU			DMA_MODE_DT(2)
#define		DMA_MODE_DT_SRAM		DMA_MODE_DT(3)
#define DMA_MODE_SAM(x)			(((x) & 0x3) << 16)
#define		DMA_MODE_SAM_CONST		DMA_MODE_SAM(0)
#define		DMA_MODE_SAM_INC		DMA_MODE_SAM(1)
#define		DMA_MODE_SAM_STRIDE		DMA_MODE_SAM(2)
#define DMA_MODE_DAM(x)			(((x) & 0x3) << 18)
#define		DMA_MODE_DAM_CONST		DMA_MODE_DAM(0)
#define		DMA_MODE_DAM_INC		DMA_MODE_DAM(1)
#define		DMA_MODE_DAM_STRIDE		DMA_MODE_DAM(2)
#define DMA_MODE_PW(x)			(((x) & 0x7) << 20)
#define DMA_MODE_CB			(0x1 << 23)
#define DMA_MODE_NDDBW(x)		(((x) & 0x1) << 28)
#define		DMA_MODE_NDDBW_32BIT		DMA_MODE_NDDBW(0)
#define		DMA_MODE_NDDBW_8BIT		DMA_MODE_NDDBW(1)
#define DMA_MODE_CFE			(0x1 << 29)
#define DMA_MODE_LME			(0x1 << 30)
#define DMA_MODE_CME			(0x1 << 31)

/* DMAX_LINKLIST_CTL */
#define DMA_LLC_SAV(x)			(((x) & 0x3) << 8)
#define		DMA_LLC_SAV_INC			DMA_LLC_SAV(0)
#define		DMA_LLC_SAV_LOAD_NEXT		DMA_LLC_SAV(1)
#define		DMA_LLC_SAV_LOAD_PREV		DMA_LLC_SAV(2)
#define DMA_LLC_DAV(x)			(((x) & 0x3) << 10)
#define		DMA_LLC_DAV_INC			DMA_LLC_DAV(0)
#define		DMA_LLC_DAV_LOAD_NEXT		DMA_LLC_DAV(1)
#define		DMA_LLC_DAV_LOAD_PREV		DMA_LLC_DAV(2)
#define DMA_LLC_SUSPEND			(0x1 << 16)

/* DMAX_INT_CTL */
#define DMA_INTCTL_BLOCK		(0x1 << 0)
#define DMA_INTCTL_SUPER_BLOCK		(0x1 << 1)
#define DMA_INTCTL_FRAME		(0x1 << 2)
#define DMA_INTCTL_HALF_FRAME		(0x1 << 3)
#define DMA_INTCTL_LAST_FRAME		(0x1 << 4)

/* DMAX_INT_STATUS */
#define DMA_INTSTAT_BLOCK		(0x1 << 0)
#define DMA_INTSTAT_SUPER_BLOCK		(0x1 << 1)
#define DMA_INTSTAT_FRAME		(0x1 << 2)
#define DMA_INTSTAT_HALF_FRAME		(0x1 << 3)
#define DMA_INTSTAT_LAST_FRAME		(0x1 << 4)

#define DMA_IRQ_LINE_NR			(4)

/* extract the bit field to new shift */
#define BIT_FIELD(val, width, shift, newshift)	\
		((((val) >> (shift)) & (((1u) << (width)) - 1)) << newshift)

enum owl_dma_id {
	S900_DMA,
	S700_DMA,
};
/**
 * struct owl_dma_lli_hw - hardware link list for dma transfer
 * @next_lli: physical address of the next link list
 * @saddr: source physical address
 * @daddr: destination physical address
 * @flen: frame length
 * @fcnt: frame count
 * @src_stride: source stride
 * @dst_stride: destination stride
 * @ctrla: dma_mode and linklist ctrl config
 * @ctrlb: interrupt config
 * @const_num: data for constant fill
 */
struct owl_lli_hw_s900 {
	u32	next_lli;	/* physical address of the next link list */
	u32	saddr;		/* source physical address */
	u32	daddr;		/* destination physical address */
	u32	flen:20;	/* frame length */
	u32	fcnt:12;	/* frame count */
	u32	src_stride;	/* source stride */
	u32	dst_stride;	/* destination stride */
	u32	ctrla;		/* dma_mode and linklist ctrl */
	u32	ctrlb;		/* interrupt control */
	u32	const_num;	/* data for constant fill */
};
struct owl_lli_hw_s700 {

	u32	next_lli;	/* physical address of the next link list */
	u32	saddr;		/* source physical address */
	u32	daddr;		/* destination physical address */
	u32	flen;		/* frame length bit0-23 */
	u32	src_stride;	/* source stride */
	u32	dst_stride;	/* destination stride */
	u32	ctrla;		/* dma_mode and linklist ctrl */
	u32	fcnt:12;	/* frame count */
	u32	ctrlb:20;	/* interrupt control */
	u32	const_num;	/* data for constant fill */
};
/**
 * struct owl_dma_lli - link list for dma transfer
 * @hw: hardware link list
 * @phys: physical address of hardware link list
 * @node: node for txd's lli_list
 */
struct owl_dma_lli {
	union {
		struct owl_lli_hw_s900	hw_s9;
		struct owl_lli_hw_s700	hw_s7;
	} hw;
	dma_addr_t		phys;
	struct list_head	node;
};

/**
 * struct owl_dma_txd - wrapper for struct dma_async_tx_descriptor
 * @vd: virtual DMA descriptor
 * @lli_list: link list of children sg's
 * @done: this marks completed descriptors
 * @cyclic: indicate cyclic transfers
 */
struct owl_dma_txd {
	struct virt_dma_desc	vd;
	struct list_head	lli_list;
	bool			done;
	bool			cyclic;
};

/**
 * struct owl_dma_pchan - holder for the physical channels
 * @id: physical index to this channel
 * @base: virtual memory base for the dma channel
 * @lock: a lock to use when altering an instance of this struct
 * @vchan: the virtual channel currently being served by this physical
 * channel
 * @txd_issued: issued count of txd in this physical channel
 * @txd_callback: callback count after txd completed in this physical channel
 * @ts_issued: timestamp of txd issued
 * @ts_callback: timestamp of txd callback
 */
struct owl_dma_pchan {
	u32			id;
	void __iomem		*base;
	struct owl_dma_vchan	*vchan;

	spinlock_t		lock;

	unsigned long		txd_issued;
	unsigned long		txd_callback;

	ktime_t			ts_issued;
	ktime_t			ts_callback;
};

/**
 * struct owl_dma_chan_state - holds the OWL dma specific virtual channel
 * states
 * @OWL_DMA_CHAN_IDLE: the channel is idle
 * @OWL_DMA_CHAN_RUNNING: the channel has allocated a physical transport
 * channel and is running a transfer on it
 * @OWL_DMA_CHAN_PAUSED: the channel has allocated a physical transport
 * channel, but the transfer is currently paused
 * @OWL_DMA_CHAN_WAITING: the channel is waiting for a physical transport
 * channel to become available (only pertains to memcpy channels)
 */
enum owl_dma_chan_state {
	OWL_DMA_CHAN_IDLE,
	OWL_DMA_CHAN_RUNNING,
	OWL_DMA_CHAN_PAUSED,
	OWL_DMA_CHAN_WAITING,
};

/**
 * struct owl_dma_pchan - this structure wraps a DMA ENGINE channel
 * @vc: wrappped virtual channel
 * @pchan: the physical channel utilized by this channel, if there is one
 * @cfg: dma slave config
 * @at: active transaction on this channel
 * @state: whether the channel is idle, paused, running etc
 * @drq: the physical DMA DRQ which this channel is using
 * @txd_issued: issued count of txd in this physical channel
 * @txd_callback: callback count after txd completed in this physical channel
 * @ts_issued: timestamp of txd issued
 * @ts_callback: timestamp of txd callback
 */
struct owl_dma_vchan {
	struct virt_dma_chan	vc;
	struct owl_dma_pchan	*pchan;
	struct dma_slave_config	cfg;
	struct owl_dma_txd	*at;
	enum owl_dma_chan_state state;
	int			drq;

	long			txd_issued;
	long			txd_callback;

	ktime_t			ts_issued;
	ktime_t			ts_callback;
};

/**
 * struct owl_dma - holder for the OWL DMA controller
 * @dma: dma engine for this instance
 * @base: virtual memory base for the DMA controller
 * @clk: clock for the DMA controller
 * @lock: a lock to use when change DMA controller global register
 * @lli_pool: a pool for the LLI descriptors
 * @nr_pchans: the number of physical channels
 * @pchans: array of data for the physical channels
 * @nr_vchans: the number of physical channels
 * @vchans: array of data for the physical channels
 */
struct owl_dma {
	struct dma_device	dma;
	void __iomem		*base;
	struct clk *clk;
	spinlock_t		lock;
	struct dma_pool		*lli_pool;

	enum owl_dma_id	devid;
	/* physical dma channels */
	unsigned int		nr_pchans;
	struct owl_dma_pchan	*pchans;

	/* virtual dma channels */
	unsigned int		nr_vchans;
	struct owl_dma_vchan	*vchans;
};

/* for dma debug dump only */
static struct owl_dma *g_od;

static void pchan_writel(struct owl_dma_pchan *pchan, u32 data, u32 reg)
{
	writel(data, pchan->base + reg);
}

static u32 pchan_readl(struct owl_dma_pchan *pchan, u32 reg)
{
	return readl(pchan->base + reg);
}

static void dma_writel(struct owl_dma *od, u32 data, u32 reg)
{
	writel(data, od->base + reg);
}

static u32 dma_readl(struct owl_dma *od, u32 reg)
{
	return readl(od->base + reg);
}

static inline struct owl_dma *to_owl_dma(struct dma_device *dd)
{
	return container_of(dd, struct owl_dma, dma);
}

static struct device *chan2dev(struct dma_chan *chan)
{
	return &chan->dev->device;
}

static inline struct owl_dma_vchan *to_owl_vchan(struct dma_chan *chan)
{
	return container_of(chan, struct owl_dma_vchan, vc.chan);
}

static inline struct owl_dma_txd *
to_owl_txd(struct dma_async_tx_descriptor *tx)
{
	return container_of(tx, struct owl_dma_txd, vd.tx);
}

static inline void owl_dma_dump_com_regs(struct owl_dma *od)
{
	dev_info(od->dma.dev, "Common register:\n"
		"  irqpd:  0x%08x 0x%08x 0x%08x 0x%08x\n"
		"  irqen:  0x%08x 0x%08x 0x%08x 0x%08x\n"
		" secure:  0x%08x	  nic_qos: 0x%08x\n"
		" dbgsel:  0x%08x	idle_stat: 0x%08x\n",
		dma_readl(od, DMA_IRQ_PD0), dma_readl(od, DMA_IRQ_PD1),
		dma_readl(od, DMA_IRQ_PD2), dma_readl(od, DMA_IRQ_PD3),
		dma_readl(od, DMA_IRQ_EN0), dma_readl(od, DMA_IRQ_EN1),
		dma_readl(od, DMA_IRQ_EN2), dma_readl(od, DMA_IRQ_EN3),
		dma_readl(od, DMA_SECURE_ACCESS_CTL),
		dma_readl(od, DMA_NIC_QOS),
		dma_readl(od, DMA_DBGSEL),
		dma_readl(od, DMA_IDLE_STAT));
}

static inline void owl_dma_dump_chan_regs(struct owl_dma *od,
					  struct owl_dma_pchan *pchan)
{
	phys_addr_t reg = __virt_to_phys((unsigned long)pchan->base);

	dev_info(od->dma.dev, "Chan %d reg: %pa,devid=%d\n"
		"   mode:  0x%08x	llctl: 0x%08x\n"
		"	src:  0x%08x	 dest: 0x%08x\n"
		"   flen:  0x%08x	 fcnt: 0x%08x\n"
		"  start:  0x%08x	pause: 0x%08x\n"
		"  rlen:  0x%08x	rfcnt: 0x%08x\n"
		"  nextd:  0x%08x  descnum: 0x%08x\n"
		"int_ctl:  0x%08x  intstat: 0x%08x\n"
		"  s_ptr:  0x%08x	d_ptr: 0x%08x\n",
		pchan->id, &reg, od->devid,
		pchan_readl(pchan, DMAX_MODE),
		pchan_readl(pchan, DMAX_LINKLIST_CTL),
		pchan_readl(pchan, DMAX_SOURCE),
		pchan_readl(pchan, DMAX_DESTINATION),
		pchan_readl(pchan, DMAX_FRAME_LEN),
		pchan_readl(pchan, DMAX_FRAME_CNT),
		pchan_readl(pchan, DMAX_START),
		pchan_readl(pchan, DMAX_PAUSE),
		pchan_readl(pchan, DMAX_REMAIN_CNT),
		pchan_readl(pchan, DMAX_REMAIN_FRAME_CNT),
		pchan_readl(pchan, DMAX_NEXT_DESCRIPTOR),
		pchan_readl(pchan, DMAX_CURRENT_DESCRIPTOR_NUM),
		pchan_readl(pchan, DMAX_INT_CTL),
		pchan_readl(pchan, DMAX_INT_STATUS),
		pchan_readl(pchan, DMAX_CURRENT_SOURCE_POINTER),
		pchan_readl(pchan, DMAX_CURRENT_DESTINATION_POINTER));
}

static inline void owl_dma_dump_lli_s900(struct owl_dma_vchan *vchan,
				    struct owl_dma_lli *lli)
{
	phys_addr_t p_lli = lli->phys;
	struct owl_lli_hw_s900 *hw_s9;

	hw_s9 = (struct owl_lli_hw_s900 *)&lli->hw.hw_s9;
	dev_dbg(chan2dev(&vchan->vc.chan),
		"\n  s900_lli:   p 0x%llx   v 0x%p\n"
		"	  nl: 0x%08x   src: 0x%08x	dst: 0x%08x\n"
		"	flen: 0x%08x  fcnt: 0x%08x  ctrla: 0x%08x\n"
		"   ctrlb: 0x%08x  const_num: 0x%08x\n",
		p_lli, &lli->hw,
		hw_s9->next_lli, hw_s9->saddr, hw_s9->daddr,
		hw_s9->flen, hw_s9->fcnt, hw_s9->ctrla,
		hw_s9->ctrlb, hw_s9->const_num);
}
static inline void owl_dma_dump_lli_s700(struct owl_dma_vchan *vchan,
				    struct owl_dma_lli *lli)
{
	phys_addr_t p_lli = lli->phys;
	struct owl_lli_hw_s700 *hw_s7 = (struct owl_lli_hw_s700 *)&lli->hw;
	dev_dbg(chan2dev(&vchan->vc.chan),
		"\n  s700_lli:   p 0x%llx   v 0x%p\n"
		"	  nl: 0x%08x   src: 0x%08x	dst: 0x%08x\n"
		"	flen: 0x%08x  fcnt: 0x%08x  ctrla: 0x%08x\n"
		"   ctrlb: 0x%08x  const_num: 0x%08x\n",
		p_lli, &lli->hw,
		hw_s7->next_lli, hw_s7->saddr, hw_s7->daddr,
		hw_s7->flen, hw_s7->fcnt, hw_s7->ctrla,
		hw_s7->ctrlb, hw_s7->const_num);
}

static void owl_dma_dump(struct owl_dma *od)
{
	struct owl_dma_pchan *pchan;
	struct owl_dma_vchan *vchan;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&od->lock, flags);

	owl_dma_dump_com_regs(od);

	for (i = 0; i < od->nr_pchans; i++) {
		pchan = &od->pchans[i];
		vchan = pchan->vchan;

		pr_info("[pchan %d] issued %ld, callback %ld, last(%lld-%lld=%lld ns), vchan %p(drq:%d)\n",
			i, pchan->txd_issued, pchan->txd_callback,
			ktime_to_ns(pchan->ts_callback),
			ktime_to_ns(pchan->ts_issued),
			ktime_to_ns(ktime_sub(pchan->ts_callback, pchan->ts_issued)),
			vchan, vchan ? vchan->drq : 0);

		owl_dma_dump_chan_regs(od, pchan);
	}

	for (i = 0; i < od->nr_vchans; i++) {
		vchan = &od->vchans[i];
		pchan = vchan->pchan;

		pr_info("[vchan %d] drq %d, issued %ld, callback %ld, last(%lld-%lld=%lld ns), pchan %p(%d)\n",
			i, vchan->drq,
			vchan->txd_issued,
			vchan->txd_callback,
			ktime_to_ns(vchan->ts_callback),
			ktime_to_ns(vchan->ts_issued),
			ktime_to_ns(ktime_sub(vchan->ts_callback, vchan->ts_issued)),
			pchan, pchan ? pchan->id : 0);
	}

	spin_unlock_irqrestore(&od->lock, flags);
}

/*
 * dump all dma channels information for debug
 */
void owl_dma_debug_dump(void)
{
	owl_dma_dump(g_od);
}
EXPORT_SYMBOL(owl_dma_debug_dump);

static inline u32 llc_hw_ctrla(u32 mode, u32 llc_ctl)
{
	u32 ctl;

	ctl = BIT_FIELD(mode, 4, 28, 28)
		| BIT_FIELD(mode, 8, 16, 20)
		| BIT_FIELD(mode, 4, 8, 16)
		| BIT_FIELD(mode, 6, 0, 10)
		| BIT_FIELD(llc_ctl, 2, 10, 8)
		| BIT_FIELD(llc_ctl, 2, 8, 6);

	return ctl;
}

static inline u32 llc_hw_ctrlb_s900(u32 int_ctl)
{
	u32 ctl;

	ctl = BIT_FIELD(int_ctl, 7, 0, 18);
	return ctl;
}
static inline u32 llc_hw_ctrlb_s700(u32 int_ctl)
{
	u32 ctl;
	ctl = BIT_FIELD(int_ctl, 7, 0, 6);

	return ctl;
}

static void owl_dma_free_lli(struct owl_dma *od,
		struct owl_dma_lli *lli)
{
	list_del(&lli->node);
	dma_pool_free(od->lli_pool, lli, lli->phys);
}

static struct owl_dma_lli *owl_dma_alloc_lli(struct owl_dma *od)
{
	struct owl_dma_lli *lli;
	dma_addr_t phys;

	lli = dma_pool_alloc(od->lli_pool, GFP_ATOMIC, &phys);
	if (!lli)
		return NULL;

	memset(lli, 0, sizeof(*lli));

	INIT_LIST_HEAD(&lli->node);
	lli->phys = phys;

	return lli;
}

static struct owl_dma_lli *owl_dma_add_lli(struct owl_dma_txd *txd,
					   struct owl_dma_lli *prev,
					   struct owl_dma_lli *next,
					   bool is_cyclic,
					   enum owl_dma_id devid)
{
	if (!is_cyclic)
		list_add_tail(&next->node, &txd->lli_list);

	if (prev) {
		if (devid == S700_DMA) {
			prev->hw.hw_s7.next_lli = next->phys;
			prev->hw.hw_s7.ctrla |= llc_hw_ctrla(DMA_MODE_LME, 0);
		} else {
			prev->hw.hw_s9.next_lli = next->phys;
			prev->hw.hw_s9.ctrla |= llc_hw_ctrla(DMA_MODE_LME, 0);
		}
	}

	return next;
}
static inline void owl_dma_cfg_lli_s700(struct owl_lli_hw_s700 *hw_s7,
			dma_addr_t src,	dma_addr_t dst,
			bool is_cyclic, u32 mode, u32 len)
{
	hw_s7->next_lli = 0;   /* 1 link list by default */
	hw_s7->saddr = src;
	hw_s7->daddr = dst;
	/* frame count fixed as 1, and max frame length is 20bit */
	hw_s7->fcnt = 1;
	hw_s7->flen = len;
	hw_s7->src_stride = 0;
	hw_s7->dst_stride = 0;
	hw_s7->ctrla = llc_hw_ctrla(mode,
		DMA_LLC_SAV_LOAD_NEXT | DMA_LLC_DAV_LOAD_NEXT);

	if (is_cyclic)
		hw_s7->ctrlb = llc_hw_ctrlb_s700(DMA_INTCTL_BLOCK);
	else
		hw_s7->ctrlb = llc_hw_ctrlb_s700(DMA_INTCTL_SUPER_BLOCK);

}

static inline void owl_dma_cfg_lli_s900(struct owl_lli_hw_s900 *hw_s9,
			dma_addr_t src,	dma_addr_t dst,
			bool is_cyclic, u32 mode, u32 len)
{
	hw_s9->next_lli = 0;   /* 1 link list by default */
	hw_s9->saddr = src;
	hw_s9->daddr = dst;
	/* frame count fixed as 1, and max frame length is 20bit */
	hw_s9->fcnt = 1;
	hw_s9->flen = len;
	hw_s9->src_stride = 0;
	hw_s9->dst_stride = 0;
	hw_s9->ctrla = llc_hw_ctrla(mode,
		DMA_LLC_SAV_LOAD_NEXT | DMA_LLC_DAV_LOAD_NEXT);

	if (is_cyclic)
		hw_s9->ctrlb = llc_hw_ctrlb_s900(DMA_INTCTL_BLOCK);
	else
		hw_s9->ctrlb = llc_hw_ctrlb_s900(DMA_INTCTL_SUPER_BLOCK);

}

static inline int owl_dma_cfg_lli(struct owl_dma_vchan *vchan,
			struct owl_dma_lli *lli,
			dma_addr_t src,	dma_addr_t dst,
			u32 len, enum dma_transfer_direction dir,
			struct dma_slave_config *sconfig, bool is_cyclic)
{
	struct owl_dma *od = to_owl_dma(vchan->vc.chan.device);
	u32 mode;

	mode = DMA_MODE_PW(0);

	switch (dir) {
	case DMA_MEM_TO_MEM:
		mode |= DMA_MODE_TS(0) | DMA_MODE_ST_DCU | DMA_MODE_DT_DCU
		| DMA_MODE_SAM_INC | DMA_MODE_DAM_INC;

		break;
	case DMA_MEM_TO_DEV:
		mode |= DMA_MODE_TS(vchan->drq)
			| DMA_MODE_ST_DCU | DMA_MODE_DT_DEV
			| DMA_MODE_SAM_INC | DMA_MODE_DAM_CONST;

		/* for uart device */
		if (sconfig->dst_addr_width == DMA_SLAVE_BUSWIDTH_1_BYTE)
			mode |= DMA_MODE_NDDBW_8BIT;

		break;
	case DMA_DEV_TO_MEM:
		 mode |= DMA_MODE_TS(vchan->drq)
			| DMA_MODE_ST_DEV | DMA_MODE_DT_DCU
			| DMA_MODE_SAM_CONST | DMA_MODE_DAM_INC;

		/* for uart device */
		if (sconfig->src_addr_width == DMA_SLAVE_BUSWIDTH_1_BYTE)
			mode |= DMA_MODE_NDDBW_8BIT;

		break;
	default:
		return -EINVAL;
	}
	/* frame count fixed as 1, and max frame length is 20bit */
	if (od->devid == S700_DMA) {
		owl_dma_cfg_lli_s700(&lli->hw.hw_s7, src, dst,
					is_cyclic, mode, len);
		owl_dma_dump_lli_s700(vchan, lli);
	} else {
		owl_dma_cfg_lli_s900(&lli->hw.hw_s9, src, dst,
					is_cyclic, mode, len);
		owl_dma_dump_lli_s900(vchan, lli);
	}

	return 0;
}

/*
 * Allocate a physical channel for a virtual channel
 *
 * Try to locate a physical channel to be used for this transfer. If all
 * are taken return NULL and the requester will have to cope by using
 * some fallback PIO mode or retrying later.
 */
struct owl_dma_pchan *owl_dma_get_pchan(struct owl_dma *od,
					struct owl_dma_vchan *vchan)
{
	struct owl_dma_pchan *pchan;
	unsigned long flags;
	int i;
	
	for (i = 0; i < od->nr_pchans; i++)
	{
		pchan = &od->pchans[i];
		
		spin_lock_irqsave(&pchan->lock, flags);
		
		if (!pchan->vchan)
		{
			pchan->vchan = vchan;
			spin_unlock_irqrestore(&pchan->lock, flags);
			break;
		}
		
		spin_unlock_irqrestore(&pchan->lock, flags);
	}
	
	if (i == od->nr_pchans)
	{
		/* No physical channel available, cope with it */
		dev_dbg(od->dma.dev, "No physical channel available for vchan(drq:%d)\n",
			vchan->drq);
		return NULL;
	}

	return pchan;
}

/* Mark the physical channel as free.  Note, this write is atomic. */
static inline void owl_dma_put_pchan(struct owl_dma *od,
				     struct owl_dma_pchan *pchan)
{
	pchan->vchan = NULL;
}

/* Whether a certain channel is busy or not */
static int owl_dma_pchan_busy(struct owl_dma *od, struct owl_dma_pchan *pchan)
{
	unsigned int val;

	val = dma_readl(od, DMA_IDLE_STAT);
	return !(val & (1 << pchan->id));
}


/*
 * owl_dma_terminate_pchan() stops the channel and  clears any pending
 * interrupt status.  This should not be used for an on-going transfer,
 * but as a method of shutting down a channel (eg, when it's no longer used)
 * or terminating a transfer.
 */
static void owl_dma_terminate_pchan(struct owl_dma *od,
				    struct owl_dma_pchan *pchan)
{
	u32 irq_pd;
	
	pchan_writel(pchan, 0, DMAX_START);
	pchan_writel(pchan, pchan_readl(pchan, DMAX_INT_STATUS), DMAX_INT_STATUS);
	
	spin_lock(&od->lock);
	
	dma_writel(od, dma_readl(od, DMA_IRQ_EN0) & ~(1 << pchan->id), DMA_IRQ_EN0);

	irq_pd = dma_readl(od, DMA_IRQ_PD0);
	
	if (irq_pd & (1 << pchan->id)) {
		dev_warn(od->dma.dev,
			"warning: terminate pchan%d that still "
			"has pending irq (irq_pd:0x%x)\n",
			pchan->id, irq_pd);
		dma_writel(od, (1 << pchan->id), DMA_IRQ_PD0);
	}

	spin_unlock(&od->lock);
}

static void owl_dma_pause_pchan(struct owl_dma_pchan *pchan)
{
	pchan_writel(pchan, 1, DMAX_PAUSE);
}

static void owl_dma_resume_pchan(struct owl_dma_pchan *pchan)
{
	pchan_writel(pchan, 0, DMAX_PAUSE);
}

static int owl_dma_start_next_txd(struct owl_dma_vchan *vchan)
{
	struct owl_dma *od = to_owl_dma(vchan->vc.chan.device);
	struct virt_dma_desc *vd = vchan_next_desc(&vchan->vc);
	struct owl_dma_pchan *pchan = vchan->pchan;
	struct owl_dma_txd *txd = to_owl_txd(&vd->tx);
	struct owl_dma_lli *lli;
	u32 int_ctl;
	unsigned long flags;

	BUG_ON(pchan == NULL);

	list_del(&vd->node);

	vchan->at = txd;

	/* Wait for channel inactive */
	while (owl_dma_pchan_busy(od, pchan))
		cpu_relax();

	lli = list_first_entry(&txd->lli_list,
			struct owl_dma_lli, node);

	if (txd->cyclic)
		int_ctl = DMA_INTCTL_BLOCK;
	else
		int_ctl = DMA_INTCTL_SUPER_BLOCK;

	pchan_writel(pchan, DMA_MODE_LME, DMAX_MODE);
	pchan_writel(pchan, DMA_LLC_SAV_LOAD_NEXT | DMA_LLC_DAV_LOAD_NEXT,
		DMAX_LINKLIST_CTL);
	pchan_writel(pchan, lli->phys, DMAX_NEXT_DESCRIPTOR);
	pchan_writel(pchan, int_ctl, DMAX_INT_CTL);
	pchan_writel(pchan, pchan_readl(pchan, DMAX_INT_STATUS),
		DMAX_INT_STATUS);

	pchan->txd_issued++;
	vchan->txd_issued++;
	pchan->ts_issued = ktime_get();
	vchan->ts_issued = ktime_get();

	spin_lock_irqsave(&od->lock, flags);

	dma_writel(od, dma_readl(od, DMA_IRQ_EN0) | (1 << pchan->id),
		DMA_IRQ_EN0);

	dev_dbg(chan2dev(&vchan->vc.chan), "start pchan%d for vchan(drq:%d)\n",
		pchan->id, vchan->drq);

	spin_unlock_irqrestore(&od->lock, flags);

	pchan_writel(pchan, 0x1, DMAX_START);

	return 0;
}

static void owl_dma_phy_reassign_start(struct owl_dma *od,
			struct owl_dma_vchan *vchan,
			struct owl_dma_pchan *pchan)
{
	dev_dbg(chan2dev(&vchan->vc.chan), "reassigned pchan%d for vchan(drq:%d)\n",
		pchan->id, vchan->drq);

	/*
	 * We do this without taking the lock; we're really only concerned
	 * about whether this pointer is NULL or not, and we're guaranteed
	 * that this will only be called when it _already_ is non-NULL.
	 */
	pchan->vchan = vchan;
	vchan->pchan = pchan;
	vchan->state = OWL_DMA_CHAN_RUNNING;
	owl_dma_start_next_txd(vchan);
}

/*
 * Free a physical DMA channel, potentially reallocating it to another
 * virtual channel if we have any pending.
 */
static void owl_dma_phy_free(struct owl_dma *od, struct owl_dma_vchan *vchan)
{
	struct owl_dma_vchan *p, *next;

 retry:
	next = NULL;

	/* Find a waiting virtual channel for the next transfer. */
	list_for_each_entry(p, &od->dma.channels, vc.chan.device_node)
		if (p->state == OWL_DMA_CHAN_WAITING) {
			next = p;
			break;
		}

	/* Ensure that the physical channel is stopped */
	owl_dma_terminate_pchan(od, vchan->pchan);

	if (next) {
		bool success;

		/*
		 * Eww.  We know this isn't going to deadlock
		 * but lockdep probably doesn't.
		 */
		spin_lock(&next->vc.lock);
		/* Re-check the state now that we have the lock */
		success = next->state == OWL_DMA_CHAN_WAITING;
		if (success)
			owl_dma_phy_reassign_start(od, next, vchan->pchan);
		spin_unlock(&next->vc.lock);

		/* If the state changed, try to find another channel */
		if (!success)
			goto retry;
	} else {
		/* No more jobs, so free up the physical channel */
		owl_dma_put_pchan(od, vchan->pchan);
	}

	vchan->pchan = NULL;
	vchan->state = OWL_DMA_CHAN_IDLE;
}

static irqreturn_t owl_dma_interrupt(int irq, void *dev_id)
{
	struct owl_dma *od = dev_id;
	struct owl_dma_vchan *vchan;
	struct owl_dma_pchan *pchan;
	unsigned long pending;
	int i;
	unsigned int global_irq_pending, chan_irq_pending;

	spin_lock(&od->lock);

	pending = dma_readl(od, DMA_IRQ_PD0);

	/* clear IRQ pending */
	for_each_set_bit(i, &pending, od->nr_pchans) {
		pchan = &od->pchans[i];
		/* clear pending */
		pchan_writel(pchan, pchan_readl(pchan, DMAX_INT_STATUS),
			DMAX_INT_STATUS);
	}

	dma_writel(od, pending, DMA_IRQ_PD0);

	/* check missed IRQ pending */
	if (od->devid == S900_DMA) {
		for (i = 0; i < od->nr_pchans; i++) {
			pchan = &od->pchans[i];
			chan_irq_pending = pchan_readl(pchan, DMAX_INT_CTL) &
					   pchan_readl(pchan, DMAX_INT_STATUS);

			/* dummy read to ensure DMA_IRQ_PD0 value is updated */
			dma_readl(od, DMA_IRQ_PD0);
			global_irq_pending = dma_readl(od, DMA_IRQ_PD0);

			if (chan_irq_pending && !(global_irq_pending & BIT(i)))	{
				dev_dbg(od->dma.dev,
					"Warning: global IRQ pending(0x%x) does't "
					"match with channel%d IRQ pending(0x%x)\n",
					global_irq_pending, i, chan_irq_pending);

				/* clear pending */
				pchan_writel(pchan, pchan_readl(pchan, DMAX_INT_STATUS),
					DMAX_INT_STATUS);

				/* update global IRQ pending */
				pending |= BIT(i);
			}
		}
	}

	spin_unlock(&od->lock);

	for_each_set_bit(i, &pending, od->nr_pchans) {
		struct owl_dma_txd *txd;

		pchan = &od->pchans[i];
		pchan->txd_callback++;
		pchan->ts_callback = ktime_get();

		vchan = pchan->vchan;
		if (!vchan) {
			dev_warn(od->dma.dev, "No vchan attached on pchan%d\n",
				pchan->id);
			continue;
		}

		spin_lock(&vchan->vc.lock);

		txd = vchan->at;
		if (txd) {
			if (txd->cyclic)
				vchan_cyclic_callback(&txd->vd);
			else {
				vchan->at = NULL;
				txd->done = true;

#if 1
				/* for debug only */
				if (pchan_readl(pchan, DMAX_REMAIN_CNT)) {
					dev_warn(od->dma.dev,
						"%s: warning: terminate pchan%d that still "
						"busy(rlen %x)\n",
						__func__,
						pchan->id,
						pchan_readl(pchan, DMAX_REMAIN_CNT));
					owl_dma_dump(od);
					BUG();
				}
#endif

				vchan_cookie_complete(&txd->vd);

				/*
				 * And start the next descriptor (if any),
				 * otherwise free this channel.
				 */
				if (vchan_next_desc(&vchan->vc))
					owl_dma_start_next_txd(vchan);
				else
					owl_dma_phy_free(od, vchan);
			}
		}

		vchan->txd_callback++;
		vchan->ts_callback = ktime_get();
		spin_unlock(&vchan->vc.lock);
	}

	return IRQ_HANDLED;
}

static void owl_dma_free_txd(struct owl_dma *od, struct owl_dma_txd *txd)
{
	struct owl_dma_lli *lli, *_lli;

	if (unlikely(!txd))
		return;

	list_for_each_entry_safe(lli, _lli, &txd->lli_list, node) {
		owl_dma_free_lli(od, lli);
	}

	kfree(txd);
}

static void owl_dma_desc_free(struct virt_dma_desc *vd)
{
	struct owl_dma *od = to_owl_dma(vd->tx.chan->device);
	struct owl_dma_txd *txd = to_owl_txd(&vd->tx);

	owl_dma_free_txd(od, txd);
}

static int owl_dma_terminate_all(struct owl_dma_vchan *vchan)
{
	struct owl_dma *od = to_owl_dma(vchan->vc.chan.device);
	unsigned long flags;
	LIST_HEAD(head);

	dev_dbg(chan2dev(&vchan->vc.chan), "%s: vchan(drq:%d), pchan(id:%d)\n",
		__func__,
		vchan->drq,
		vchan->pchan ? vchan->pchan->id : -1);

	spin_lock_irqsave(&vchan->vc.lock, flags);
	if (!vchan->pchan && !vchan->at) {
		spin_unlock_irqrestore(&vchan->vc.lock, flags);
		return 0;
	}

	vchan->state = OWL_DMA_CHAN_IDLE;

	if (vchan->pchan) {
		/* Mark physical channel as free */
		owl_dma_phy_free(od, vchan);
	}

	/* Dequeue jobs and free LLIs */
	if (vchan->at) {
		owl_dma_desc_free(&vchan->at->vd);
		vchan->at = NULL;
	}

	/* Dequeue jobs not yet fired as well */
	vchan_get_all_descriptors(&vchan->vc, &head);
	vchan_dma_desc_free_list(&vchan->vc, &head);

	/*
	 * clear cyclic dma descriptor to avoid using invalid cyclic pointer
	 * in vchan_complete()
	 */
	vchan->vc.cyclic = NULL;

	spin_unlock_irqrestore(&vchan->vc.lock, flags);

	return 0;
}

static int owl_dma_pause(struct dma_chan *chan)
{
	struct owl_dma_vchan *vchan = to_owl_vchan(chan);
	unsigned long flags;

	/*
	 * Anything succeeds on channels with no physical allocation and
	 * no queued transfers.
	 */
	spin_lock_irqsave(&vchan->vc.lock, flags);
	if (!vchan->pchan && !vchan->at) {
		spin_unlock_irqrestore(&vchan->vc.lock, flags);
		return 0;
	}

	owl_dma_pause_pchan(vchan->pchan);
	vchan->state = OWL_DMA_CHAN_PAUSED;

	spin_unlock_irqrestore(&vchan->vc.lock, flags);

	return 0;
}

static int owl_dma_resume(struct dma_chan *chan)
{
	struct owl_dma_vchan *vchan = to_owl_vchan(chan);
	unsigned long flags;

	dev_dbg(chan2dev(chan), "vchan %p: resume\n", &vchan->vc);

	/*
	 * Anything succeeds on channels with no physical allocation and
	 * no queued transfers.
	 */
	spin_lock_irqsave(&vchan->vc.lock, flags);
	if (!vchan->pchan && !vchan->at) {
		spin_unlock_irqrestore(&vchan->vc.lock, flags);
		return 0;
	}

	owl_dma_resume_pchan(vchan->pchan);
	vchan->state = OWL_DMA_CHAN_RUNNING;

	spin_unlock_irqrestore(&vchan->vc.lock, flags);

	return 0;
}

static int caninos_device_pause(struct dma_chan *chan)
{
	owl_dma_pause(chan);
	return 0;
}

static int caninos_device_resume(struct dma_chan *chan)
{
	owl_dma_resume(chan);
	return 0;
}

static int caninos_device_terminate_all(struct dma_chan *chan)
{
	struct owl_dma_vchan *vchan = to_owl_vchan(chan);
	return owl_dma_terminate_all(vchan);
}

static int caninos_device_config(struct dma_chan *chan, struct dma_slave_config *config)
{
	struct owl_dma_vchan *vchan = to_owl_vchan(chan);
	memcpy(&vchan->cfg, (void *)config, sizeof(*config));
	return 0;
}

/* The channel should be paused when calling this */
static u32 owl_dma_getbytes_chan(struct owl_dma_vchan *vchan)
{
	struct owl_dma *od = to_owl_dma(vchan->vc.chan.device);
	struct owl_dma_pchan *pchan;
	struct owl_dma_txd *txd;
	struct owl_dma_lli *lli;
	unsigned int next_lli_phy;
	int i = 0, found = 0;
	size_t bytes;

	pchan = vchan->pchan;
	txd = vchan->at;

	if (!pchan || !txd)
		return 0;

	/* get remain count of current link list */
	bytes = pchan_readl(pchan, DMAX_REMAIN_CNT);

	/* the link list chain remain count */
	if (pchan_readl(pchan, DMAX_MODE) & DMA_MODE_LME) {
		next_lli_phy = pchan_readl(pchan, DMAX_NEXT_DESCRIPTOR);
		list_for_each_entry(lli, &txd->lli_list, node) {
			if (lli->phys == next_lli_phy) {
				/* if the next lli point to first lli, actually
				 * it is the last lli in txd in cyclic mode
				 */
				 if (i == 0)
					break;

				found = 1;
			}

			if (found) {
				if (od->devid == S700_DMA)
					bytes += lli->hw.hw_s7.flen;
				else
					bytes += lli->hw.hw_s9.flen;
			}

			i++;
		}
	}

	return bytes;
}

static enum dma_status owl_dma_tx_status(struct dma_chan *chan,
		dma_cookie_t cookie,
		struct dma_tx_state *state)
{
	struct owl_dma *od = to_owl_dma(chan->device);
	struct owl_dma_vchan *vchan = to_owl_vchan(chan);
	struct owl_dma_lli *lli;
	struct virt_dma_desc *vd;
	struct owl_dma_txd *txd;
	enum dma_status ret;
	unsigned long flags;
	size_t bytes = 0;

	ret = dma_cookie_status(chan, cookie, state);
	
	if (ret == DMA_COMPLETE) {
		return ret;
	}
	
	/*
	 * There's no point calculating the residue if there's
	 * no txstate to store the value.
	 */
	if (!state)
	{
		if (vchan->state == OWL_DMA_CHAN_PAUSED) {
			ret = DMA_PAUSED;
		}
		return ret;
	}

	spin_lock_irqsave(&vchan->vc.lock, flags);
	
	ret = dma_cookie_status(chan, cookie, state);
	
	if (ret != DMA_COMPLETE)
	{
		vd = vchan_find_desc(&vchan->vc, cookie);
		if (vd) {
			/* On the issued list, so hasn't been processed yet */
			txd = to_owl_txd(&vd->tx);
			list_for_each_entry(lli, &txd->lli_list, node) {
				if (od->devid == S700_DMA)
					bytes += lli->hw.hw_s7.flen;
				else
					bytes += lli->hw.hw_s9.flen;
			}
		} else {
			bytes = owl_dma_getbytes_chan(vchan);
		}
	}
	spin_unlock_irqrestore(&vchan->vc.lock, flags);

	/*
	 * This cookie not complete yet
	 * Get number of bytes left in the active transactions and queue
	 */
	dma_set_residue(state, bytes);

	if (vchan->state == OWL_DMA_CHAN_PAUSED && ret == DMA_IN_PROGRESS) {
		ret = DMA_PAUSED;
	}

	/* Whether waiting or running, we're in progress */
	return ret;
}

/*
 * Try to allocate a physical channel.  When successful, assign it to
 * this virtual channel, and initiate the next descriptor.  The
 * virtual channel lock must be held at this point.
 */
static void owl_dma_phy_alloc_and_start(struct owl_dma_vchan *vchan)
{
	struct owl_dma *od = to_owl_dma(vchan->vc.chan.device);
	struct owl_dma_pchan *pchan;

	pchan = owl_dma_get_pchan(od, vchan);
	if (!pchan) {
		dev_dbg(od->dma.dev, "no physical channel available for xfer on vchan(%d)\n",
			vchan->drq);
		vchan->state = OWL_DMA_CHAN_WAITING;
		return;
	}

	dev_dbg(od->dma.dev, "allocated pchan%d for vchan(drq:%d)\n",
		pchan->id, vchan->drq);

	vchan->pchan = pchan;
	vchan->state = OWL_DMA_CHAN_RUNNING;
	owl_dma_start_next_txd(vchan);
}

/*
 * Slave transactions callback to the slave device to allow
 * synchronization of slave DMA signals with the DMAC enable
 */
static void owl_dma_issue_pending(struct dma_chan *chan)
{
	struct owl_dma_vchan *vchan = to_owl_vchan(chan);
	unsigned long flags;

	spin_lock_irqsave(&vchan->vc.lock, flags);
	if (vchan_issue_pending(&vchan->vc)) {
		if (!vchan->pchan && vchan->state != OWL_DMA_CHAN_WAITING)
			owl_dma_phy_alloc_and_start(vchan);
	}
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
}

static struct dma_async_tx_descriptor *owl_dma_prep_memcpy(
		struct dma_chan *chan, dma_addr_t dst, dma_addr_t src,
		size_t len, unsigned long flags)
{
	struct owl_dma *od = to_owl_dma(chan->device);
	struct owl_dma_vchan *vchan = to_owl_vchan(chan);
	struct dma_slave_config *sconfig = &vchan->cfg;
	struct owl_dma_txd *txd;
	struct owl_dma_lli *lli, *prev = NULL;
	size_t offset, bytes;
	int ret;

	if (!len)
		return NULL;

	txd = kzalloc(sizeof(*txd), GFP_NOWAIT);
	if (!txd)
		return NULL;

	INIT_LIST_HEAD(&txd->lli_list);

	for (offset = 0; offset < len; offset += bytes) {
		lli = owl_dma_alloc_lli(od);
		if (!lli) {
			dev_warn(chan2dev(chan), "failed to get lli");
			goto err_txd_free;
		}

		bytes = min_t(size_t, len - offset, OWL_DMA_FRAME_MAX_LENGTH);

		ret = owl_dma_cfg_lli(vchan, lli, src + offset, dst + offset,
			bytes, DMA_MEM_TO_MEM, sconfig, txd->cyclic);
		if (ret) {
			dev_warn(chan2dev(chan), "failed to config lli");
			goto err_txd_free;
		}

		prev = owl_dma_add_lli(txd, prev, lli, false, od->devid);
	}

	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);

err_txd_free:
	owl_dma_free_txd(od, txd);
	return NULL;
}

static struct dma_async_tx_descriptor *owl_dma_prep_slave_sg(
		struct dma_chan *chan, struct scatterlist *sgl,
		unsigned int sg_len, enum dma_transfer_direction dir,
		unsigned long flags, void *context)
{
	struct owl_dma *od = to_owl_dma(chan->device);
	struct owl_dma_vchan *vchan = to_owl_vchan(chan);
	struct dma_slave_config *sconfig = &vchan->cfg;
	struct owl_dma_txd *txd;
	struct owl_dma_lli *lli, *prev = NULL;
	size_t len;
	int ret, i;
	dma_addr_t addr, src = 0, dst = 0;
	struct scatterlist *sg;

	if (unlikely(!sgl || !sg_len))
		return NULL;

	txd = kzalloc(sizeof(*txd), GFP_NOWAIT);
	if (!txd)
		return NULL;

	INIT_LIST_HEAD(&txd->lli_list);

	for_each_sg(sgl, sg, sg_len, i) {
		addr = sg_dma_address(sg);
		len = sg_dma_len(sg);

		if (len > OWL_DMA_FRAME_MAX_LENGTH) {
			dev_err(od->dma.dev,
				"reqeset line %d: maximum bytes for sg entry "
				"exceeded: %zu > %d\n",
				vchan->drq, len,
				OWL_DMA_FRAME_MAX_LENGTH);
			goto err_txd_free;
		}

		lli = owl_dma_alloc_lli(od);
		if (!lli) {
			dev_warn(chan2dev(chan), "failed to get lli");
			goto err_txd_free;
		}

		if (dir == DMA_MEM_TO_DEV) {
			src = addr;
			dst = sconfig->dst_addr;
		} else if (dir == DMA_DEV_TO_MEM) {
			src = sconfig->src_addr;
			dst = addr;
		}

		ret = owl_dma_cfg_lli(vchan, lli, src, dst, len, dir, sconfig,
				      txd->cyclic);
		if (ret) {
			dev_warn(chan2dev(chan), "failed to config lli");
			goto err_txd_free;
		}

		prev = owl_dma_add_lli(txd, prev, lli, false, od->devid);
	}

	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);

err_txd_free:
	owl_dma_free_txd(od, txd);
	return NULL;
}

/**
 * caninos_device_prep_dma_cyclic - prepare the cyclic DMA transfer
 * @chan: the DMA channel to prepare
 * @buf_addr: physical DMA address where the buffer starts
 * @buf_len: total number of bytes for the entire buffer
 * @period_len: number of bytes for each period
 * dir: transfer direction, to or from device
 */

struct dma_async_tx_descriptor *
caninos_device_prep_dma_cyclic(struct dma_chan *chan, dma_addr_t buf_addr,
		size_t buf_len, size_t period_len, enum dma_transfer_direction dir,
		unsigned long flags)
{
	struct owl_dma *od = to_owl_dma(chan->device);
	struct owl_dma_vchan *vchan = to_owl_vchan(chan);
	struct dma_slave_config *sconfig = &vchan->cfg;
	struct owl_dma_txd *txd;
	struct owl_dma_lli *lli, *prev = NULL, *first = NULL;
	int ret, i;
	dma_addr_t src = 0, dst = 0;
	unsigned int periods = buf_len / period_len;

	txd = kzalloc(sizeof(*txd), GFP_NOWAIT);
	if (!txd)
		return NULL;

	INIT_LIST_HEAD(&txd->lli_list);
	txd->cyclic = true;

	for (i = 0; i < periods; i++) {
		lli = owl_dma_alloc_lli(od);
		if (!lli) {
			dev_warn(chan2dev(chan), "failed to get lli");
			goto err_txd_free;
		}

		if (dir == DMA_MEM_TO_DEV) {
			src = buf_addr + (period_len * i);
			dst = sconfig->dst_addr;
		} else if (dir == DMA_DEV_TO_MEM) {
			src = sconfig->src_addr;
			dst = buf_addr + (period_len * i);
		}

		ret = owl_dma_cfg_lli(vchan, lli, src, dst, period_len,
			dir, sconfig, txd->cyclic);
		if (ret) {
			dev_warn(chan2dev(chan), "failed to config lli");
			goto err_txd_free;
		}

		if (!first)
			first = lli;

		prev = owl_dma_add_lli(txd, prev, lli, false, od->devid);
	}

	/* close the cyclic list */
	owl_dma_add_lli(txd, prev, first, true, od->devid);

	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);

err_txd_free:
	owl_dma_free_txd(od, txd);
	return NULL;
}

static int owl_dma_alloc_chan_resources(struct dma_chan *chan)
{
	return 0;
}

static void owl_dma_free_chan_resources(struct dma_chan *chan)
{
	struct owl_dma_vchan *vchan = to_owl_vchan(chan);

	/* Ensure all queued descriptors are freed */
	vchan_free_chan_resources(&vchan->vc);
}

static inline void owl_dma_free(struct owl_dma *od)
{
	struct owl_dma_vchan *vchan = NULL;
	struct owl_dma_vchan *next;

	list_for_each_entry_safe(vchan,
				 next, &od->dma.channels, vc.chan.device_node) {
		list_del(&vchan->vc.chan.device_node);
		tasklet_kill(&vchan->vc.task);
	}
}

struct owl_dma_of_filter_args {
	struct owl_dma *od;
	unsigned int drq;
};

static bool owl_dma_of_filter(struct dma_chan *chan, void *param)
{
	struct owl_dma_vchan *vchan = to_owl_vchan(chan);
	struct owl_dma_of_filter_args *fargs = param;

	/* Ensure the device matches our channel */
	if (chan->device != &fargs->od->dma)
			return false;

	vchan->drq = fargs->drq;

	return true;
}

static struct dma_chan *
owl_of_dma_simple_xlate(struct of_phandle_args *dma_spec,
		struct of_dma *ofdma)
{
	struct owl_dma *od = ofdma->of_dma_data;
	unsigned int drq = dma_spec->args[0];
	struct owl_dma_of_filter_args fargs = {
		.od = od,
	};
	dma_cap_mask_t cap;

	if (drq > od->nr_vchans)
		return NULL;

	fargs.drq = drq;
	dma_cap_zero(cap);
	dma_cap_set(DMA_SLAVE, cap);

	/*
	 * for linux3.14+
	 * dma_get_slave_channel(&(od->vchans[request].vc.chan));
	 */
	return dma_request_channel(cap, owl_dma_of_filter, &fargs);
}

static int owl_dma_debugfs_show(struct seq_file *s, void *unused)
{
	struct owl_dma *od = s->private;

	owl_dma_dump(od);

	return 0;
}

static int owl_dma_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, owl_dma_debugfs_show, inode->i_private);
}

static const struct file_operations owl_dma_debugfs_operations = {
	.open = owl_dma_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int owl_dma_register_dbgfs(struct owl_dma *od)
{
	(void) debugfs_create_file(dev_name(od->dma.dev),
			S_IFREG | S_IRUGO, NULL, od,
			&owl_dma_debugfs_operations);

	return 0;
}

static const struct of_device_id owl_dma_match[] = {
	{ .compatible = "caninos,k9-dma", .data = (void *)S900_DMA,},
	{ .compatible = "caninos,k7-dma", .data = (void *)S700_DMA,},
	{},
};

static int owl_dma_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct owl_dma *od;
	struct resource *res;
	int ret, i, irq, nr_channels, nr_requests;
	const struct of_device_id *of_id =
			of_match_device(owl_dma_match, &pdev->dev);
			
	od = devm_kzalloc(&pdev->dev, sizeof(*od), GFP_KERNEL);
	
	if (!od)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	od->base = devm_ioremap_resource(&pdev->dev, res);
	
	if (IS_ERR(od->base))
		return PTR_ERR(od->base);

	ret = of_property_read_u32(np, "dma-channels", &nr_channels);
	
	if (ret) {
		dev_err(&pdev->dev, "Can't get dma-channels.\n");
		return ret;
	}

	ret = of_property_read_u32(np, "dma-requests", &nr_requests);
	
	if (ret) {
		dev_err(&pdev->dev, "Can't get dma-requests.\n");
		return ret;
	}
	od->devid = (enum owl_dma_id)of_id->data;

	dev_info(&pdev->dev, "dma-channels %d, dma-requests %d\n", nr_channels, nr_requests);

	od->nr_pchans = nr_channels;
	od->nr_vchans = nr_requests;

	if (!pdev->dev.dma_mask) {
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	}

	platform_set_drvdata(pdev, od);
	spin_lock_init(&od->lock);

	dma_cap_set(DMA_MEMCPY, od->dma.cap_mask);
	dma_cap_set(DMA_SLAVE, od->dma.cap_mask);
	dma_cap_set(DMA_CYCLIC, od->dma.cap_mask);
	
	od->dma.dev = &pdev->dev;
	od->dma.device_alloc_chan_resources = owl_dma_alloc_chan_resources;
	od->dma.device_free_chan_resources = owl_dma_free_chan_resources;
	od->dma.device_tx_status = owl_dma_tx_status;
	od->dma.device_issue_pending = owl_dma_issue_pending;
	od->dma.device_prep_dma_memcpy = owl_dma_prep_memcpy;
	od->dma.device_prep_slave_sg = owl_dma_prep_slave_sg;
	
	od->dma.device_prep_dma_cyclic = caninos_device_prep_dma_cyclic;
	od->dma.device_pause = caninos_device_pause;
	od->dma.device_resume = caninos_device_resume;
	od->dma.device_terminate_all = caninos_device_terminate_all;
	od->dma.device_config = caninos_device_config;
	
	INIT_LIST_HEAD(&od->dma.channels);

	od->clk = devm_clk_get(&pdev->dev, NULL);
	
	if (IS_ERR(od->clk)) {
		dev_err(&pdev->dev, "Can't get clock");
		return PTR_ERR(od->clk);
	}

	irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, irq, owl_dma_interrupt, 0,
			dev_name(&pdev->dev), od);
	if (ret) {
		dev_err(&pdev->dev, "Can't request IRQ\n");
		return ret;
	}

	/* init physical channel */
	od->pchans = devm_kzalloc(&pdev->dev,
		od->nr_pchans * sizeof(struct owl_dma_pchan), GFP_KERNEL);
	
	if (od->pchans == NULL)
		return -ENOMEM;

	for (i = 0; i < od->nr_pchans; i++) {
		struct owl_dma_pchan *pchan = &od->pchans[i];

		pchan->id = i;
		pchan->base = od->base + DMA_CHAN_BASE(i);

		spin_lock_init(&pchan->lock);
	}

	/* init virtual channel */
	od->vchans = devm_kzalloc(&pdev->dev,
		od->nr_vchans * sizeof(struct owl_dma_vchan), GFP_KERNEL);
	if (od->vchans == NULL)
		return -ENOMEM;

	for (i = 0; i < od->nr_vchans; i++) {
		struct owl_dma_vchan *vchan = &od->vchans[i];

		vchan->vc.desc_free = owl_dma_desc_free;
		vchan_init(&vchan->vc, &od->dma);
		vchan->drq = -1;
		vchan->state = OWL_DMA_CHAN_IDLE;
	}

	/* Create a pool of consistent memory blocks for hardware descriptors */
	od->lli_pool = dma_pool_create(dev_name(od->dma.dev), od->dma.dev,
			 sizeof(struct owl_dma_lli), 8, 0);
	if (!od->lli_pool) {
		dev_err(&pdev->dev, "No memory for descriptors dma pool\n");
		return -ENOMEM;
	}

	clk_prepare_enable(od->clk);

	ret = dma_async_device_register(&od->dma);
	if (ret) {
		dev_err(&pdev->dev, "failed to register DMA engine device\n");
		goto err_pool_free;
	}

	/* Device-tree DMA controller registration */
	ret = of_dma_controller_register(pdev->dev.of_node,
			owl_of_dma_simple_xlate, od);
	if (ret) {
		dev_err(&pdev->dev, "of_dma_controller_register failed\n");
		goto err_dma_unregister;
	}

	owl_dma_register_dbgfs(od);

	g_od = od;

	return 0;

err_dma_unregister:
	dma_async_device_unregister(&od->dma);
err_pool_free:
	clk_disable_unprepare(od->clk);
	dma_pool_destroy(od->lli_pool);
	return ret;
}

static int owl_dma_remove(struct platform_device *pdev)
{
	struct owl_dma *od = platform_get_drvdata(pdev);

	of_dma_controller_free(pdev->dev.of_node);
	dma_async_device_unregister(&od->dma);

	/* mask all interrupts for this execution environment */
	dma_writel(od, 0x0, DMA_IRQ_EN0);
	owl_dma_free(od);

	clk_disable_unprepare(od->clk);

	return 0;
}


MODULE_DEVICE_TABLE(of, owl_dma_match);

static struct platform_driver owl_dma_driver = {
	.probe	= owl_dma_probe,
	.remove	= owl_dma_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(owl_dma_match),
	},
};

static int owl_dma_init(void)
{
	return platform_driver_register(&owl_dma_driver);
}
subsys_initcall(owl_dma_init);

static void __exit owl_dma_exit(void)
{
	platform_driver_unregister(&owl_dma_driver);
}
module_exit(owl_dma_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
