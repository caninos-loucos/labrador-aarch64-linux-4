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

#include <linux/module.h>
#include <linux/of_dma.h>
#include <linux/property.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/ktime.h>
#include <linux/clk.h>
#include <linux/slab.h>

#include "dma.h"

#define DRIVER_NAME "caninos-dma"
#define DRIVER_DESC "Caninos Labrador DMA Controller Driver"

#define CANINOS_DMA_BUSWIDTHS \
        BIT(DMA_SLAVE_BUSWIDTH_4_BYTES) | BIT(DMA_SLAVE_BUSWIDTH_1_BYTE)

static void caninos_dma_free_lli(struct lab_dma *cd, struct lab_dma_lli *lli)
{
	list_del(&lli->node);
	dma_pool_free(cd->lli_pool, lli, lli->phys);
}

static void caninos_dma_free_txd(struct lab_dma *cd, struct lab_dma_txd *txd)
{
	struct lab_dma_lli *lli, *n;
	
	if (unlikely(!txd)) {
		return;
	}
	list_for_each_entry_safe(lli, n, &txd->lli_list, node) {
		caninos_dma_free_lli(cd, lli);
	}
	kfree(txd);
}

static void caninos_dma_desc_free(struct virt_dma_desc *vd)
{
	struct lab_dma *cd = to_lab_dma(vd->tx.chan->device);
	struct lab_dma_txd *txd = to_lab_txd(&vd->tx);

	caninos_dma_free_txd(cd, txd);
}

static inline void caninos_dma_put_pchan(struct lab_dma *cd,
                                         struct lab_dma_pchan *pchan)
{
	pchan->vchan = NULL;
}

static void caninos_dma_terminate_pchan(struct lab_dma *cd,
                                        struct lab_dma_pchan *pchan)
{
	unsigned long flags;
	u32 tmp;
	/* note: this is a forced termination, so we do not care about the data */
	
	/* stop dma operation */
	pchan_writel(pchan, 0U, DMAX_START);
	
	/* clear interrupt status */
	tmp = pchan_readl(pchan, DMAX_INT_STATUS);
	pchan_writel(pchan, tmp, DMAX_INT_STATUS);
	
	spin_lock_irqsave(&cd->lock, flags);
	
	/* disable channel interrupt */
	tmp = dma_readl(cd, DMA_IRQ_EN0) & ~(1U << pchan->id);
	dma_writel(cd, tmp, DMA_IRQ_EN0);
	
	/* clear interrupt pending flag */
	tmp = dma_readl(cd, DMA_IRQ_PD0);
	
	if (tmp & (1U << pchan->id)) {
		dma_writel(cd, (1U << pchan->id), DMA_IRQ_PD0);
	}
	
	spin_unlock_irqrestore(&cd->lock, flags);
}

static void caninos_dma_pchan_pause(struct lab_dma *cd, 
                                    struct lab_dma_pchan *pchan)
{
	u32 tmp;
	
	if(cd->devid == DEVID_K5_DMAC)
	{
		tmp = dma_readl(cd, DMA_DBGSEL) | (1U << (pchan->id + 16));
		dma_writel(cd, tmp, DMA_DBGSEL);
	}
	else {
		pchan_writel(pchan, 1U, DMAX_PAUSE_K7_K9);
	}
}

static void caninos_dma_pchan_resume(struct lab_dma *cd,
                                     struct lab_dma_pchan *pchan)
{
	u32 tmp;
	
	if(cd->devid == DEVID_K5_DMAC)
	{
		tmp = dma_readl(cd, DMA_DBGSEL) | ~(1U << (pchan->id + 16));
		dma_writel(cd, tmp, DMA_DBGSEL);
	}
	else {
		pchan_writel(pchan, 0U, DMAX_PAUSE_K7_K9);
	}
}

static int caninos_dma_pchan_busy_wait(struct lab_dma *cd,
                                       struct lab_dma_pchan *pchan)
{
	ktime_t timeout;
	u32 val;
	
	/* 50us should be enough */ 
	timeout = ktime_add_us(ktime_get(), 50);
	
	for (;;) {
		if(cd->devid == DEVID_K5_DMAC)
		{
			/* bus delay */
			dma_readl(cd, DMA_IDLE_STAT);
			dma_readl(cd, DMA_IDLE_STAT);
			dma_readl(cd, DMA_IDLE_STAT);
		}
		
		val = dma_readl(cd, DMA_IDLE_STAT);
		
		if (val & BIT(pchan->id)) {
			return 0;
		}
		if (ktime_compare(ktime_get(), timeout) > 0) {
			break;
		}
		
		cpu_relax();
	}
	return -ETIMEDOUT;
}

static int caninos_dma_start_next_txd(struct lab_dma_vchan *vchan)
{
	struct lab_dma *cd = to_lab_dma(vchan->vc.chan.device);
	struct virt_dma_desc *vd = vchan_next_desc(&vchan->vc);
	struct lab_dma_pchan *pchan = vchan->pchan;
	struct lab_dma_txd *txd = to_lab_txd(&vd->tx);
	struct lab_dma_lli *lli;
	unsigned long flags;
	u32 ctrla, ctrlb;
	
	list_del(&vd->node);
	vchan->txd = txd;
	
	/* wait for channel inactive */
	if (caninos_dma_pchan_busy_wait(cd, pchan) < 0) {
		caninos_dma_terminate_pchan(cd, pchan);
	}
	
	lli = list_first_entry(&txd->lli_list, struct lab_dma_lli, node);
	
	ctrla = DMA_LLC_SAV_LOAD_NEXT | DMA_LLC_DAV_LOAD_NEXT;
	
	if (txd->cyclic) {
		ctrlb = DMA_INTCTL_BLOCK;
	}
	else {
		ctrlb = DMA_INTCTL_SUPER_BLOCK;
	}
	
	pchan_writel(pchan, DMA_MODE_LME, DMAX_MODE);
	pchan_writel(pchan, ctrla, DMAX_LINKLIST_CTL);
	pchan_writel(pchan, lli->phys, DMAX_NEXT_DESCRIPTOR);
	pchan_writel(pchan, ctrlb, DMAX_INT_CTL);
	pchan_writel(pchan, pchan_readl(pchan, DMAX_INT_STATUS), DMAX_INT_STATUS);
	
	spin_lock_irqsave(&cd->lock, flags);
	
	/* enable channel interrupt */
	dma_writel(cd, dma_readl(cd, DMA_IRQ_EN0) | (1U << pchan->id), DMA_IRQ_EN0);
	
	spin_unlock_irqrestore(&cd->lock, flags);
	
	/* start dma operation */
	pchan_writel(pchan, 1U, DMAX_START);
	
	return 0;
}

static void caninos_dma_phy_reassign_start(struct lab_dma *cd,
                                           struct lab_dma_vchan *vchan,
                                           struct lab_dma_pchan *pchan)
{
	pchan->vchan = vchan;
	vchan->pchan = pchan;
	vchan->state = CANINOS_VCHAN_RUNNING;
	
	caninos_dma_start_next_txd(vchan);
}

static void caninos_dma_phy_free(struct lab_dma *cd,
                                 struct lab_dma_vchan *vchan)
{
	struct lab_dma_vchan *p, *next;

retry:
	next = NULL;
	
	/* Find a waiting virtual channel for the next transfer. */
	list_for_each_entry(p, &cd->dma.channels, vc.chan.device_node)
	{
		if (p->state == CANINOS_VCHAN_WAITING) {
			next = p;
			break;
		}
	}
	
	/* Ensure that the physical channel is stopped */
	caninos_dma_terminate_pchan(cd, vchan->pchan);
	
	if (next)
	{
		bool success;
		
		/*
		 * We know this isn't going to deadlock
		 * but lockdep probably doesn't.
		 */
		spin_lock(&next->vc.lock);
		
		/* Re-check the state now that we have the lock */
		success = (next->state == CANINOS_VCHAN_WAITING);
		
		if (success) {
			caninos_dma_phy_reassign_start(cd, next, vchan->pchan);
		}
		
		spin_unlock(&next->vc.lock);

		/* If the state changed, try to find another channel */
		if (!success) {
			goto retry;
		}
	}
	else
	{
		/* No more jobs, so free up the physical channel */
		caninos_dma_put_pchan(cd, vchan->pchan);
	}
	
	vchan->pchan = NULL;
	vchan->state = CANINOS_VCHAN_IDLE;
}

static struct lab_dma_pchan *caninos_dma_get_pchan(struct lab_dma *cd,
                                                   struct lab_dma_vchan *vchan)
{
	struct lab_dma_pchan *pchan = NULL;
	unsigned long flags;
	int i;
	
	for (i = 0; i < cd->nr_pchans; i++)
	{
		pchan = &cd->pchans[i];
		
		spin_lock_irqsave(&pchan->lock, flags);
		
		if (!pchan->vchan)
		{
			pchan->vchan = vchan;
			spin_unlock_irqrestore(&pchan->lock, flags);
			break;
		}
		
		spin_unlock_irqrestore(&pchan->lock, flags);
	}
	
	if (i == cd->nr_pchans)
	{
		/* No physical channel available */
		return NULL;
	}
	
	return pchan;
}

static struct lab_dma_lli *caninos_dma_alloc_lli(struct lab_dma *cd)
{
	struct lab_dma_lli *lli;
	dma_addr_t phys;
	
	lli = dma_pool_alloc(cd->lli_pool, GFP_ATOMIC, &phys);
	
	if (!lli) {
		return NULL;
	}
	
	memset(lli, 0, sizeof(*lli));
	
	INIT_LIST_HEAD(&lli->node);
	lli->phys = phys;
	return lli;
}

static struct lab_dma_lli *caninos_dma_add_lli
	(struct dma_chan *chan, struct lab_dma_txd *txd, struct lab_dma_lli *prev,
	 struct lab_dma_lli *next, bool close_cyclic)
{
	struct lab_dma *cd = to_lab_dma(chan->device);
	
	if (!close_cyclic) {
		list_add_tail(&next->node, &txd->lli_list);
	}
	if (!prev) {
		return next;
	}
	switch(cd->devid)
	{
	case DEVID_K7_DMAC:
		prev->hw.hw_s7.next_lli = next->phys;
		prev->hw.hw_s7.ctrla |= llc_hw_ctrla(DMA_MODE_LME, 0);
		break;
		
	case DEVID_K9_DMAC:
		prev->hw.hw_s9.next_lli = next->phys;
		prev->hw.hw_s9.ctrla |= llc_hw_ctrla(DMA_MODE_LME, 0);
		break;
		
	case DEVID_K5_DMAC:
		prev->hw.hw_s5.next_lli = next->phys;
		prev->hw.hw_s5.ctrla |= llc_hw_ctrla(DMA_MODE_LME, 0);
		break;
	}
	return next;
}

static int caninos_dma_cfg_lli
	(struct lab_dma_vchan *vchan, struct lab_dma_lli *lli,
	 dma_addr_t src, dma_addr_t dst, u32 len, enum dma_transfer_direction dir,
	 struct dma_slave_config *sconfig, bool is_cyclic)
{
	struct lab_dma *cd = to_lab_dma(vchan->vc.chan.device);
	u32 mode, ctrla, ctrlb;
	
	mode = DMA_MODE_PW(0);
	
	switch(dir)
	{
	case DMA_MEM_TO_MEM:
		mode |= DMA_MODE_TS(0) | DMA_MODE_ST_DCU | DMA_MODE_DT_DCU;
		mode |= DMA_MODE_SAM_INC | DMA_MODE_DAM_INC;
		break;
		
	case DMA_MEM_TO_DEV:
		mode |= DMA_MODE_TS(vchan->drq) | DMA_MODE_ST_DCU | DMA_MODE_DT_DEV;
		mode |= DMA_MODE_SAM_INC | DMA_MODE_DAM_CONST;
		
		/* for uart device */
		if (sconfig->dst_addr_width == DMA_SLAVE_BUSWIDTH_1_BYTE) {
			mode |= DMA_MODE_NDDBW_8BIT;
		}
		break;
		
	case DMA_DEV_TO_MEM:
		mode |= DMA_MODE_TS(vchan->drq) | DMA_MODE_ST_DEV | DMA_MODE_DT_DCU;
		mode |= DMA_MODE_SAM_CONST | DMA_MODE_DAM_INC;
		
		/* for uart device */
		if (sconfig->src_addr_width == DMA_SLAVE_BUSWIDTH_1_BYTE) {
			mode |= DMA_MODE_NDDBW_8BIT;
		}
		break;
		
	default:
		return -EINVAL;
	}
	
	if (is_cyclic) {
		ctrlb = llc_hw_ctrlb(DMA_INTCTL_BLOCK, cd->devid);
	}
	else {
		ctrlb = llc_hw_ctrlb(DMA_INTCTL_SUPER_BLOCK, cd->devid);
	}
	
	ctrla = llc_hw_ctrla(mode, DMA_LLC_SAV_LOAD_NEXT | DMA_LLC_DAV_LOAD_NEXT);
	
	/* frame count fixed as 1, and max frame length is 20bit */
	switch(cd->devid)
	{
	case DEVID_K7_DMAC:
		lli->hw.hw_s7.next_lli = 0;
		lli->hw.hw_s7.saddr = src;
		lli->hw.hw_s7.daddr = dst;
		lli->hw.hw_s7.fcnt = 1;
		lli->hw.hw_s7.flen = len;
		lli->hw.hw_s7.src_stride = 0;
		lli->hw.hw_s7.dst_stride = 0;
		lli->hw.hw_s7.ctrla = ctrla;
		lli->hw.hw_s7.ctrlb = ctrlb;
		break;
		
	case DEVID_K9_DMAC:
		lli->hw.hw_s9.next_lli = 0;
		lli->hw.hw_s9.saddr = src;
		lli->hw.hw_s9.daddr = dst;
		lli->hw.hw_s9.fcnt = 1;
		lli->hw.hw_s9.flen = len;
		lli->hw.hw_s9.src_stride = 0;
		lli->hw.hw_s9.dst_stride = 0;
		lli->hw.hw_s9.ctrla = ctrla;
		lli->hw.hw_s9.ctrlb = ctrlb;
		break;
		
	case DEVID_K5_DMAC:
		lli->hw.hw_s5.next_lli = 0;
		lli->hw.hw_s5.saddr = src;
		lli->hw.hw_s5.daddr = dst;
		lli->hw.hw_s5.fcnt = 1;
		lli->hw.hw_s5.flen = len;
		lli->hw.hw_s5.src_stride = 0;
		lli->hw.hw_s5.dst_stride = 0;
		lli->hw.hw_s5.ctrla = ctrla;
		lli->hw.hw_s5.ctrlb = ctrlb;
		break;
	
	default:
		return -EINVAL;
	}
	
	return 0;
}

static struct dma_async_tx_descriptor *caninos_device_prep_dma_cyclic
	(struct dma_chan *chan, dma_addr_t buf_addr, size_t buf_len,
	 size_t period_len, enum dma_transfer_direction dir, unsigned long flags)
{
	struct lab_dma *cd = to_lab_dma(chan->device);
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	struct dma_slave_config *sconfig = &vchan->cfg;
	struct lab_dma_lli *lli, *prev = NULL, *first = NULL;
	struct lab_dma_txd *txd;
	size_t idx, periods;
	dma_addr_t src = 0, dst = 0;
	int ret;
	
	if (unlikely(!period_len)) {
		return NULL;
	}
	
	periods = buf_len / period_len;
	
	txd = kzalloc(sizeof(*txd), GFP_NOWAIT);
	
	if (!txd) {
		return NULL;
	}
	
	INIT_LIST_HEAD(&txd->lli_list);
	txd->cyclic = true;
	
	for (idx = 0; idx < periods; idx++)
	{
		lli = caninos_dma_alloc_lli(cd);
		
		if (!lli) {
			dev_err(chan2dev(chan), "failed to alloc lli");
			caninos_dma_free_txd(cd, txd);
			return NULL;
		}
		
		if (dir == DMA_MEM_TO_DEV)
		{
			src = buf_addr + (period_len * idx);
			dst = sconfig->dst_addr;
		}
		else if (dir == DMA_DEV_TO_MEM)
		{
			src = sconfig->src_addr;
			dst = buf_addr + (period_len * idx);
		}
		
		ret = caninos_dma_cfg_lli(vchan, lli, src, dst, period_len, dir,
		                          sconfig, txd->cyclic);
		
		if (ret) {
			dev_err(chan2dev(chan), "failed to config lli");
			caninos_dma_free_txd(cd, txd);
			return NULL;
		}
		
		if (!first) {
			first = lli;
		}
		
		prev = caninos_dma_add_lli(chan, txd, prev, lli, false);
	}
	
	/* close the cyclic list */
	caninos_dma_add_lli(chan, txd, prev, first, true);
	
	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);
}

static struct dma_async_tx_descriptor *caninos_dma_prep_slave_sg
	(struct dma_chan *chan, struct scatterlist *sgl, unsigned int sg_len,
	 enum dma_transfer_direction dir, unsigned long flags, void *context)
{
	struct lab_dma *cd = to_lab_dma(chan->device);
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	struct dma_slave_config *sconfig = &vchan->cfg;
	struct lab_dma_txd *txd;
	struct lab_dma_lli *lli, *prev = NULL;
	struct scatterlist *sg;
	dma_addr_t addr, src = 0, dst = 0;
	size_t len;
	int i, ret;
	
	if (unlikely(!sgl || !sg_len)) {
		return NULL;
	}
	
	txd = kzalloc(sizeof(*txd), GFP_NOWAIT);
	
	if (!txd) {
		return NULL;
	}
	
	INIT_LIST_HEAD(&txd->lli_list);
	txd->cyclic = false;
	
	for_each_sg(sgl, sg, sg_len, i)
	{
		addr = sg_dma_address(sg);
		len = sg_dma_len(sg);
		
		if (len > CANINOS_DMA_FRAME_MAX_LENGTH) {
			caninos_dma_free_txd(cd, txd);
			return NULL;
		}
		
		lli = caninos_dma_alloc_lli(cd);
		
		if (!lli) {
			dev_err(chan2dev(chan), "failed to alloc lli");
			caninos_dma_free_txd(cd, txd);
			return NULL;
		}
		
		if (dir == DMA_MEM_TO_DEV)
		{
			src = addr;
			dst = sconfig->dst_addr;
		}
		else if (dir == DMA_DEV_TO_MEM)
		{
			src = sconfig->src_addr;
			dst = addr;
		}
		
		ret = caninos_dma_cfg_lli(vchan, lli, src, dst, len, dir,
		                          sconfig, txd->cyclic);
		
		if (ret) {
			dev_err(chan2dev(chan), "failed to config lli");
			caninos_dma_free_txd(cd, txd);
			return NULL;
		}
		
		prev = caninos_dma_add_lli(chan, txd, prev, lli, false);
	}
	
	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);
}

static struct dma_async_tx_descriptor *
	caninos_dma_prep_memcpy(struct dma_chan *chan, dma_addr_t dst,
	                        dma_addr_t src, size_t len, unsigned long flags)
{
	struct lab_dma *cd = to_lab_dma(chan->device);
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	struct lab_dma_lli *lli, *prev = NULL;
	struct dma_slave_config *sconfig = &vchan->cfg;
	struct lab_dma_txd *txd;
	size_t offset, bytes;
	int ret;
	
	if (!len) {
		return NULL;
	}
	
	txd = kzalloc(sizeof(*txd), GFP_NOWAIT);
	
	if (!txd) {
		return NULL;
	}
	
	INIT_LIST_HEAD(&txd->lli_list);
	txd->cyclic = false;
	
	for (offset = 0; offset < len; offset += bytes)
	{
		lli = caninos_dma_alloc_lli(cd);
		
		if (!lli) {
			dev_err(chan2dev(chan), "failed to alloc lli");
			caninos_dma_free_txd(cd, txd);
			return NULL;
		}
		
		bytes = min_t(size_t, (len - offset), CANINOS_DMA_FRAME_MAX_LENGTH);
		
		ret = caninos_dma_cfg_lli(vchan, lli, src + offset, dst + offset,
		                          bytes, DMA_MEM_TO_MEM, sconfig, txd->cyclic);
		
		if (ret) {
			dev_err(chan2dev(chan), "failed to config lli");
			caninos_dma_free_txd(cd, txd);
			return NULL;
		}
		
		prev = caninos_dma_add_lli(chan, txd, prev, lli, false);
	}
	
	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);
}

static int caninos_dma_alloc_chan_resources(struct dma_chan *chan)
{
	return 0;
}

static void caninos_dma_free_chan_resources(struct dma_chan *chan)
{
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	
	/* Ensure all queued descriptors are freed */
	vchan_free_chan_resources(&vchan->vc);
}

static inline void caninos_dma_free(struct lab_dma *cd)
{
	struct lab_dma_vchan *vchan = NULL;
	struct lab_dma_vchan *next;
	
	list_for_each_entry_safe(vchan, next, &cd->dma.channels,
	                         vc.chan.device_node)
	{
		list_del(&vchan->vc.chan.device_node);
		tasklet_kill(&vchan->vc.task);
	}
}

static int caninos_device_config(struct dma_chan *chan,
                                 struct dma_slave_config *config)
{
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	unsigned long flags;
	
	spin_lock_irqsave(&vchan->vc.lock, flags);
	memcpy(&vchan->cfg, (void *)config, sizeof(*config));
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
	return 0;
}

static int caninos_device_pause(struct dma_chan *chan)
{
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	struct lab_dma *cd = to_lab_dma(vchan->vc.chan.device);
	unsigned long flags;
	
	spin_lock_irqsave(&vchan->vc.lock, flags);
	
	if (!vchan->pchan && !vchan->txd) {
		spin_unlock_irqrestore(&vchan->vc.lock, flags);
		return 0;
	}
	
	spin_lock(&cd->lock);
	caninos_dma_pchan_pause(cd, vchan->pchan);
	vchan->state = CANINOS_VCHAN_PAUSED;
	spin_unlock(&cd->lock);
	
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
	return 0;
}

static int caninos_device_resume(struct dma_chan *chan)
{
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	struct lab_dma *cd = to_lab_dma(vchan->vc.chan.device);
	unsigned long flags;
	
	spin_lock_irqsave(&vchan->vc.lock, flags);
	
	if (!vchan->pchan && !vchan->txd) {
		spin_unlock_irqrestore(&vchan->vc.lock, flags);
		return 0;
	}
	
	spin_lock(&cd->lock);
	caninos_dma_pchan_resume(cd, vchan->pchan);
	vchan->state = CANINOS_VCHAN_RUNNING;
	spin_unlock(&cd->lock);
	
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
	return 0;
}

static inline unsigned long caninos_dma_clear_missed_irqs(struct lab_dma *cd)
{
	unsigned long global_irq_pending, chan_irq_pending, pending, tmp, i;
	struct lab_dma_pchan *pchan;
	
	for (pending = 0, i = 0; i < cd->nr_pchans; i++)
	{
		pchan = &cd->pchans[i];
		
		chan_irq_pending  = pchan_readl(pchan, DMAX_INT_CTL);
		chan_irq_pending &= pchan_readl(pchan, DMAX_INT_STATUS);
		
		/* dummy read to ensure DMA_IRQ_PD0 value is updated */
		dma_readl(cd, DMA_IRQ_PD0);
		
		global_irq_pending = dma_readl(cd, DMA_IRQ_PD0);
		
		if (chan_irq_pending && !(global_irq_pending & BIT(i)))
		{
			tmp = pchan_readl(pchan, DMAX_INT_STATUS);
			pchan_writel(pchan, tmp, DMAX_INT_STATUS);
			pending |= BIT(i);
		}
	}
	return pending;
}

static irqreturn_t caninos_dma_interrupt(int irq, void *data)
{
	struct lab_dma *cd = (struct lab_dma*) data;
	struct lab_dma_pchan *pchan;
	struct lab_dma_vchan *vchan;
	unsigned long pending, flags, tmp;
	int i;
	
	spin_lock_irqsave(&cd->lock, flags);
	
	pending = dma_readl(cd, DMA_IRQ_PD0);
	
	/* clear IRQ status for each pchan */
	for_each_set_bit(i, &pending, cd->nr_pchans) {
		pchan = &cd->pchans[i];
		tmp = pchan_readl(pchan, DMAX_INT_STATUS);
		pchan_writel(pchan, tmp, DMAX_INT_STATUS);
	}
	
	dma_writel(cd, pending, DMA_IRQ_PD0);
	
	/* check and clear missed IRQs */
	if(cd->devid != DEVID_K7_DMAC) {
		pending |= caninos_dma_clear_missed_irqs(cd);
	}
	
	spin_unlock_irqrestore(&cd->lock, flags);
	
	for_each_set_bit(i, &pending, cd->nr_pchans)
	{
		struct lab_dma_txd *txd;
		
		pchan = &cd->pchans[i];
		vchan = pchan->vchan;
		
		if (!vchan) {
			continue;
		}
		
		spin_lock_irqsave(&vchan->vc.lock, flags);
		
		txd = vchan->txd;
		
		if (txd)
		{
			if (txd->cyclic) {
				vchan_cyclic_callback(&txd->vd);
			}
			else
			{
				vchan->txd = NULL;
				vchan_cookie_complete(&txd->vd);
				
				if (vchan_next_desc(&vchan->vc)) {
					caninos_dma_start_next_txd(vchan);
				}
				else {
					caninos_dma_phy_free(cd, vchan);
				}
			}
		}
		
		spin_unlock_irqrestore(&vchan->vc.lock, flags);
	}
	
	return IRQ_HANDLED;
}

static int caninos_device_terminate_all(struct dma_chan *chan)
{
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	struct lab_dma *cd = to_lab_dma(vchan->vc.chan.device);
	unsigned long flags;
	LIST_HEAD(head);
	
	spin_lock_irqsave(&vchan->vc.lock, flags);
	
	if (!vchan->pchan && !vchan->txd) {
		spin_unlock_irqrestore(&vchan->vc.lock, flags);
		return 0;
	}
	
	vchan->state = CANINOS_VCHAN_IDLE;
	
	if (vchan->pchan) {
		caninos_dma_phy_free(cd, vchan);
	}
	
	if (vchan->txd) {
		caninos_dma_desc_free(&vchan->txd->vd);
		vchan->txd = NULL;
	}
	
	vchan_get_all_descriptors(&vchan->vc, &head);
	vchan_dma_desc_free_list(&vchan->vc, &head);
	vchan->vc.cyclic = NULL;
	
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
	
	return 0;
}

static inline void caninos_dma_phy_alloc_and_start(struct lab_dma_vchan *vchan)
{
	struct lab_dma *cd = to_lab_dma(vchan->vc.chan.device);
	struct lab_dma_pchan *pchan;
	
	pchan = caninos_dma_get_pchan(cd, vchan);
	
	if (!pchan) {
		vchan->state = CANINOS_VCHAN_WAITING;
		return;
	}
	
	vchan->pchan = pchan;
	vchan->state = CANINOS_VCHAN_RUNNING;
	
	caninos_dma_start_next_txd(vchan);
}

static void caninos_dma_issue_pending(struct dma_chan *chan)
{
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	unsigned long flags;
	
	spin_lock_irqsave(&vchan->vc.lock, flags);
	
	if (vchan_issue_pending(&vchan->vc)) {
		if (!vchan->pchan && vchan->state != CANINOS_VCHAN_WAITING) {
			caninos_dma_phy_alloc_and_start(vchan);
		}
	}
	
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
}

static size_t caninos_dma_getbytes_chan(struct lab_dma_vchan *vchan)
{
	struct lab_dma *cd = to_lab_dma(vchan->vc.chan.device);
	struct lab_dma_pchan *pchan = vchan->pchan;
	struct lab_dma_txd *txd = vchan->txd;
	struct lab_dma_lli *lli;
	unsigned long next_phys;
	bool start_counting;
	size_t bytes;
	int i = 0;
	
	if (unlikely(!pchan || !txd)) {
		return 0;
	}
	
	/* get remain count of current channel */
	bytes = pchan_readl(pchan, DMAX_REMAIN_CNT);
	
	/* check if in linked list mode */
	if ((pchan_readl(pchan, DMAX_MODE) & DMA_MODE_LME) != DMA_MODE_LME) {
		return bytes;
	}
	
	next_phys = pchan_readl(pchan, DMAX_NEXT_DESCRIPTOR);
	start_counting = false;
	
	/* get the linked list chain remain count */
	list_for_each_entry(lli, &txd->lli_list, node)
	{
		if (lli->phys == next_phys)
		{
			if (i == 0) {
				break;
			}
			start_counting = true;
		}
		if (start_counting) {
			bytes += lli_get_frame_len(lli, cd->devid);
		}
		i++;
	}
	return bytes;
}

static enum dma_status caninos_dma_tx_status(struct dma_chan *chan, 
                                             dma_cookie_t cookie,
                                             struct dma_tx_state *state)
{
	struct lab_dma *cd = to_lab_dma(chan->device);
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	struct lab_dma_lli *lli;
	struct virt_dma_desc *vd;
	struct lab_dma_txd *txd;
	enum dma_status ret;
	unsigned long flags;
	size_t bytes = 0;
	
	ret = dma_cookie_status(chan, cookie, state);
	
	if (ret == DMA_COMPLETE) {
		return ret;
	}
	
	if (!state)
	{
		if (vchan->state == CANINOS_VCHAN_PAUSED) {
			ret = DMA_PAUSED;
		}
		return ret;
	}
	
	spin_lock_irqsave(&vchan->vc.lock, flags);
	
	ret = dma_cookie_status(chan, cookie, state);
	
	if (ret != DMA_COMPLETE)
	{
		vd = vchan_find_desc(&vchan->vc, cookie);
		
		if (vd)
		{
			txd = to_lab_txd(&vd->tx);
			
			list_for_each_entry(lli, &txd->lli_list, node) {
				bytes += lli_get_frame_len(lli, cd->devid);
			}
		}
		else {
			bytes = caninos_dma_getbytes_chan(vchan);
		}
	}
	
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
	
	dma_set_residue(state, bytes);
	
	if (vchan->state == CANINOS_VCHAN_PAUSED && ret == DMA_IN_PROGRESS) {
		ret = DMA_PAUSED;
	}
	
	return ret;
}

struct caninos_dma_of_filter_args
{
	struct lab_dma *cd;
	unsigned int drq;
};

static bool caninos_dma_of_filter(struct dma_chan *chan, void *param)
{
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	struct caninos_dma_of_filter_args *fargs = param;
	
	/* Ensure the device matches our channel */
	if (chan->device != &fargs->cd->dma) {
		return false;
	}
	
	vchan->drq = fargs->drq;
	return true;
}

static struct dma_chan *caninos_of_dma_xlate(struct of_phandle_args *dma_spec,
                                             struct of_dma *ofdma)
{
	struct lab_dma *cd = ofdma->of_dma_data;
	unsigned int drq = dma_spec->args[0];
	struct caninos_dma_of_filter_args fargs;
	dma_cap_mask_t cap;
	
	if (drq > CANINOS_MAX_DRQ) {
		return NULL;
	}
	
	fargs.drq = drq;
	fargs.cd = cd;
	
	dma_cap_zero(cap);
	dma_cap_set(DMA_SLAVE, cap);
	
	return dma_request_channel(cap, caninos_dma_of_filter, &fargs);
}

#ifdef CONFIG_PM_SLEEP
static int caninos_dma_suspend(struct device *dev)
{
	return -EBUSY;
}

static int caninos_dma_resume(struct device *dev)
{
	return 0;
}
#else
#define caninos_dma_suspend NULL
#define caninos_dma_resume NULL
#endif

static int caninos_dma_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct lab_dma *cd;
	int ret, i;
	
	cd = devm_kzalloc(&pdev->dev, sizeof(*cd), GFP_KERNEL);
	
	if (!cd) {
		dev_err(&pdev->dev, "could not allocate main data structure.\n");
		return -ENOMEM;
	}
	
	cd->dev = &pdev->dev;
	
	cd->irq = platform_get_irq(pdev, 0);
	
	if (cd->irq < 0)
	{
		dev_err(cd->dev, "could not get irq resource.\n");
		return cd->irq;
	}
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cd->base = devm_ioremap_resource(cd->dev, res);
	
	if (IS_ERR(cd->base))
	{
		dev_err(cd->dev, "could not map memory resource.\n");
		return PTR_ERR(cd->base);
	}
	
	cd->clk = devm_clk_get(cd->dev, NULL);
	
	if (IS_ERR(cd->clk))
	{
		dev_err(cd->dev, "could not get clock resource.\n");
		return PTR_ERR(cd->clk);
	}
	
	cd->devid = (enum caninos_dmac_id) of_device_get_match_data(cd->dev);
	
	switch(cd->devid)
	{
	case DEVID_K9_DMAC:
		cd->nr_pchans = 12;
		cd->nr_vchans = 48;
		break;
		
	case DEVID_K7_DMAC:
		cd->nr_pchans = 10;
		cd->nr_vchans = 48;
		break;
		
	case DEVID_K5_DMAC:
		cd->nr_pchans = 12;
		cd->nr_vchans = 48;
		break;
		
	default:
		dev_err(cd->dev, "unrecognized hardware type.\n");
		return -ENODEV;
	}
	
	/* Set DMA mask to 32 bits */
	if (!cd->dev->dma_mask)
	{
		cd->dev->coherent_dma_mask = DMA_BIT_MASK(32);
		cd->dev->dma_mask = &(cd->dev->coherent_dma_mask);
	}
	
	platform_set_drvdata(pdev, cd);
	spin_lock_init(&cd->lock);
	
	/* Set capabilities */
	dma_cap_set(DMA_MEMCPY, cd->dma.cap_mask);
	dma_cap_set(DMA_SLAVE, cd->dma.cap_mask);
	dma_cap_set(DMA_CYCLIC, cd->dma.cap_mask);
	
	cd->dma.chancnt = cd->nr_vchans;
	cd->dma.max_burst = CANINOS_DMA_FRAME_MAX_LENGTH;
	cd->dma.src_addr_widths = CANINOS_DMA_BUSWIDTHS;
	cd->dma.dst_addr_widths = CANINOS_DMA_BUSWIDTHS;
	cd->dma.directions = BIT(DMA_MEM_TO_MEM);
	cd->dma.directions |= BIT(DMA_MEM_TO_DEV) | BIT(DMA_DEV_TO_MEM);
	cd->dma.residue_granularity = DMA_RESIDUE_GRANULARITY_BURST;
	cd->dma.copy_align = DMAENGINE_ALIGN_4_BYTES;
	
	cd->dma.dev = cd->dev;
	cd->dma.device_alloc_chan_resources = caninos_dma_alloc_chan_resources;
	cd->dma.device_free_chan_resources = caninos_dma_free_chan_resources;
	cd->dma.device_tx_status = caninos_dma_tx_status;
	cd->dma.device_issue_pending = caninos_dma_issue_pending;
	cd->dma.device_prep_dma_memcpy = caninos_dma_prep_memcpy;
	cd->dma.device_terminate_all = caninos_device_terminate_all;
	cd->dma.device_pause = caninos_device_pause;
	cd->dma.device_resume = caninos_device_resume;
	cd->dma.device_config = caninos_device_config;
	cd->dma.device_prep_slave_sg = caninos_dma_prep_slave_sg;
	cd->dma.device_prep_dma_cyclic = caninos_device_prep_dma_cyclic;
	
	cd->dma.dev->dma_parms = &cd->dma_parms;
	dma_set_max_seg_size(cd->dev, CANINOS_DMA_FRAME_MAX_LENGTH);
	
	INIT_LIST_HEAD(&cd->dma.channels);
	
	/* Request IRQ */
	ret = devm_request_irq(cd->dev, cd->irq, caninos_dma_interrupt,
	                       IRQF_SHARED, dev_name(cd->dev), cd);
	
	if (ret)
	{
		dev_err(cd->dev, "could not request irq.\n");
		return ret;
	}
	
	/* Init physical channels */
	cd->pchans = devm_kcalloc(cd->dev, cd->nr_pchans,
	                          sizeof(struct lab_dma_pchan), GFP_KERNEL);
	
	if (!cd->pchans)
	{
		dev_err(cd->dev, "could not allocate physical channel data.\n");
		return -ENOMEM;
	}
	
	for (i = 0; i < cd->nr_pchans; i++)
	{
		struct lab_dma_pchan *pchan = &(cd->pchans[i]);
		
		pchan->id = i;
		pchan->base = cd->base + DMA_CHAN_BASE(i);
		spin_lock_init(&pchan->lock);
	}
	
	/* Init virtual channels */
	cd->vchans = devm_kcalloc(cd->dev, cd->nr_vchans,
	                          sizeof(struct lab_dma_vchan), GFP_KERNEL);
	
	if (!cd->vchans)
	{
		dev_err(cd->dev, "could not allocate virtual channel data.\n");
		return -ENOMEM;
	}
	
	for (i = 0; i < cd->nr_vchans; i++)
	{
		struct lab_dma_vchan *vchan = &(cd->vchans[i]);
		
		vchan->vc.desc_free = caninos_dma_desc_free;
		vchan_init(&vchan->vc, &cd->dma);
		vchan->drq = CANINOS_INV_DRQ;
		vchan->state = CANINOS_VCHAN_IDLE;
	}
	
	/* Create a pool of consistent memory blocks for hardware descriptors */
	cd->lli_pool = dma_pool_create(dev_name(cd->dev), cd->dev,
	                               sizeof(struct lab_dma_lli), 8, 0);
	if (!cd->lli_pool)
	{
		dev_err(cd->dev, "could not create dma pool.\n");
		return -ENOMEM;
	}
	
	/* Init hardware */
	pm_runtime_dont_use_autosuspend(cd->dev);
	pm_runtime_mark_last_busy(cd->dev);
	pm_runtime_set_active(cd->dev);
	pm_runtime_enable(cd->dev);
	pm_runtime_get(cd->dev);
	
	clk_prepare_enable(cd->clk);
	
	/* Mask interrupts */
	dma_writel(cd, DMA_IRQ_EN0, 0x0);
	
	/* Setup QOS */
	if (cd->devid == DEVID_K5_DMAC) {
		dma_writel(cd, DMA_NIC_QOS, 0xf0);
	}
	
	/* Register DMA controller device */
	ret = dma_async_device_register(&cd->dma);
	
	if (ret)
	{
		clk_disable_unprepare(cd->clk);
		pm_runtime_put(cd->dev);
		pm_runtime_disable(cd->dev);
		
		dma_pool_destroy(cd->lli_pool);
		
		dev_err(cd->dev, "could not register dma async device\n");
		return ret;
	}
	
	/* Device-tree DMA controller registration */
	ret = of_dma_controller_register(cd->dev->of_node,
	                                 caninos_of_dma_xlate, cd);
	if (ret)
	{
		dma_async_device_unregister(&cd->dma);
		
		clk_disable_unprepare(cd->clk);
		pm_runtime_put(cd->dev);
		pm_runtime_disable(cd->dev);
		
		dma_pool_destroy(cd->lli_pool);
		dev_err(cd->dev, "could not register OF dma controller\n");
		return ret;
	}
	
	dev_info(cd->dev, "probe finished\n");
	return 0;
}

static int caninos_dma_remove(struct platform_device *pdev)
{
	struct lab_dma *cd = platform_get_drvdata(pdev);
	
	of_dma_controller_free(pdev->dev.of_node);
	dma_async_device_unregister(&cd->dma);
	
	dma_writel(cd, DMA_IRQ_EN0, 0x0);
	caninos_dma_free(cd);
	
	clk_disable_unprepare(cd->clk);
	
	pm_runtime_put(cd->dev);
	pm_runtime_disable(cd->dev);
	
	dma_pool_destroy(cd->lli_pool);
	
	return 0;
}

static const struct of_device_id caninos_dma_match[] = {
	{ .compatible = "caninos,k9-dma", .data = (void *)DEVID_K9_DMAC,},
	{ .compatible = "caninos,k7-dma", .data = (void *)DEVID_K7_DMAC,},
	{ .compatible = "caninos,k5-dma", .data = (void *)DEVID_K5_DMAC,},
	{ /* sentinel */ },
};

static SIMPLE_DEV_PM_OPS(caninos_dma_pm_ops,
                         caninos_dma_suspend, caninos_dma_resume);

MODULE_DEVICE_TABLE(of, caninos_dma_match);

static struct platform_driver caninos_dma_driver = {
	.probe	= caninos_dma_probe,
	.remove	= caninos_dma_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(caninos_dma_match),
		.pm = &caninos_dma_pm_ops,
	},
};

static int caninos_dma_init(void)
{
	return platform_driver_register(&caninos_dma_driver);
}
subsys_initcall(caninos_dma_init);

static void __exit caninos_dma_exit(void)
{
	platform_driver_unregister(&caninos_dma_driver);
}
module_exit(caninos_dma_exit);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
