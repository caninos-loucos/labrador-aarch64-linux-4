/*
 * Caninos Labrador DMA
 *
 * Copyright (c) 2022 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2019 LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2012 Actions Semi Inc.
 * Author: Actions Semi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CANINOS_DMA_H_
#define _CANINOS_DMA_H_

#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include "../virt-dma.h"
#include "regs.h"

#include <dt-bindings/dma/caninos-dma.h>

struct lab_dma;
struct lab_dma_pchan;
struct lab_dma_vchan;

#define CANINOS_DMA_FRAME_MAX_LENGTH 0xffff0

/* extract the bit field to new shift */
#define BIT_FIELD(val, width, shift, newshift)	\
	((((val) >> (shift)) & (((1u) << (width)) - 1)) << newshift)

/**
 * struct caninos_dma_lli_hw - hardware link list for dma transfer
 */
struct caninos_lli_hw_k9 {
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
} __attribute__((packed, aligned(4)));

struct caninos_lli_hw_k7 {
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
} __attribute__((packed, aligned(4)));

struct caninos_lli_hw_k5 {
	u32	next_lli;	/* physical address of the next link list */
	u32	saddr;		/* source physical address */
	u32	daddr;		/* destination physical address */
	u32	flen:20;	/* frame length */
	u32	fcnt:12;	/* frame count */
	u32	src_stride;	/* source stride */
	u32	dst_stride;	/* destination stride */
	u32	ctrla;		/* dma_mode and linklist ctrl */
	u32	ctrlb; 		/* interrupt control and acp attribute */
	u32	const_num;	/* data for constant fill */
} __attribute__((packed, aligned(4)));

/**
 * enum caninos_dmac_id - hardware type enum
 */
enum caninos_dmac_id {
	DEVID_K9_DMAC = 1,
	DEVID_K7_DMAC = 2,
	DEVID_K5_DMAC = 3,
};

/**
 * struct lab_dma_lli - link list for dma transfer
 */
struct lab_dma_lli {
	union {
		volatile struct caninos_lli_hw_k9 hw_s9;
		volatile struct caninos_lli_hw_k7 hw_s7;
		volatile struct caninos_lli_hw_k5 hw_s5;
	} hw;
	dma_addr_t phys;       /* physical address of hardware link list */
	struct list_head node; /* node for txd's lli_list */
};

/**
 * struct lab_dma_txd - wrapper for struct dma_async_tx_descriptor
 */
struct lab_dma_txd {
	struct virt_dma_desc vd;   /* virtual DMA descriptor */
	struct list_head lli_list; /* link list of children sg's */
};

/**
 * struct lab_dma_pchan - this structure wraps a DMA ENGINE channel
 */
struct lab_dma_vchan {
	struct virt_dma_chan vc;       /* wrappped virtual channel */
	struct lab_dma_pchan *pchan;   /* phys channel used by this channel */
	struct dma_slave_config cfg;   /* dma slave config */
	struct lab_dma_txd *txd;       /* active transaction on this channel */
	int	drq;                       /* physical DMA DRQ this channel is using */
};

/**
 * struct lab_dma_pchan - holder for the physical channels
 */
struct lab_dma_pchan { 
	u32	id;                      /* physical index to this channel */
	void __iomem *base;          /* virtual memory base for the dma channel */
	struct lab_dma_vchan *vchan; /* virtual channel being served now by pchan */
	bool paused;
};

/**
 * struct lab_dma - holder for the Caninos DMA controller
 */
struct lab_dma {
	struct dma_device dma;        /* dma engine for this instance */
	void __iomem *base;           /* register base for the DMA controller */
	struct clk *clk;              /* clock for the DMA controller */
	spinlock_t lock;              /* lock used for DMA controller registers */
	struct dma_pool *lli_pool;    /* a pool for the LLI descriptors */
	enum caninos_dmac_id	devid;        /* hardware type */
	u32 nr_pchans;       /* number of physical dma channels */
	struct lab_dma_pchan *pchans; /* array of data for the physical channels */
	u32 nr_vchans;       /* number of virtual channels */
	struct lab_dma_vchan *vchans; /* array of data for the virtual channels */
	int irq;
	struct device *dev;
	struct device_dma_parameters dma_parms;
};

struct caninos_dma_config {
	dma_addr_t src;
	dma_addr_t dst;
	u32 len;
	enum dma_transfer_direction dir;
};

static inline void pchan_writel(struct lab_dma_pchan *pchan, u32 val, u32 reg) {
	writel(val, pchan->base + reg);
}

static inline u32 pchan_readl(struct lab_dma_pchan *pchan, u32 reg) {
	return readl(pchan->base + reg);
}

static inline void dma_writel(struct lab_dma *od, u32 val, u32 reg) {
	writel(val, od->base + reg);
}

static inline u32 dma_readl(struct lab_dma *od, u32 reg) {
	return readl(od->base + reg);
}

static inline struct lab_dma *to_lab_dma(struct dma_device *dd) {
	return container_of(dd, struct lab_dma, dma);
}

static inline struct device *chan2dev(struct dma_chan *chan) {
	return &chan->dev->device;
}

static inline struct lab_dma_vchan *to_lab_vchan(struct dma_chan *chan) {
	return container_of(chan, struct lab_dma_vchan, vc.chan);
}

static inline struct lab_dma_txd *
	to_lab_txd(struct dma_async_tx_descriptor *tx) {
	return container_of(tx, struct lab_dma_txd, vd.tx);
}

static inline u32 llc_hw_ctrla(u32 mode, u32 llc_ctl)
{
	u32 ctl = BIT_FIELD(mode, 4, 28, 28) | BIT_FIELD(mode, 8, 16, 20);
	ctl |= BIT_FIELD(mode, 4, 8, 16) | BIT_FIELD(mode, 6, 0, 10);
	ctl |= BIT_FIELD(llc_ctl, 2, 10, 8) | BIT_FIELD(llc_ctl, 2, 8, 6);
	return ctl;
}

static inline u32 llc_hw_ctrlb(u32 int_ctl, enum caninos_dmac_id devid)
{
	switch(devid) {
	case DEVID_K7_DMAC:
		return BIT_FIELD(int_ctl, 7, 0, 6);
	case DEVID_K9_DMAC:
	case DEVID_K5_DMAC:
		return BIT_FIELD(int_ctl, 7, 0, 18);
	}
	return 0;
}

static inline u32 lli_get_frame_len(struct lab_dma_lli *lli,
                                    enum caninos_dmac_id devid)
{
	switch(devid) {
	case DEVID_K7_DMAC:
		return lli->hw.hw_s7.flen;
	case DEVID_K9_DMAC:
		return lli->hw.hw_s9.flen;
	case DEVID_K5_DMAC:
		return lli->hw.hw_s5.flen;
	}
	return 0;
}

#endif

