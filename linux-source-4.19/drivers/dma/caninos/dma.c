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
#include <linux/clk.h>
#include <linux/slab.h>

#include "dma.h"

#define DRIVER_NAME "caninos-dma"
#define DRIVER_DESC "Caninos Labrador DMA Controller Driver"

#define CANINOS_DMA_BUSWIDTHS \
        BIT(DMA_SLAVE_BUSWIDTH_4_BYTES) | BIT(DMA_SLAVE_BUSWIDTH_1_BYTE)

static void caninos_dma_free_txd(struct lab_dma *cd, struct lab_dma_txd *txd)
{
	struct lab_dma_lli *lli, *n;
	
	if (unlikely(!txd)) {
		return;
	}
	list_for_each_entry_safe(lli, n, &txd->lli_list, node) {
		list_del(&lli->node);
		dma_pool_free(cd->lli_pool, lli, lli->phys);
	}
	kfree(txd);
}

static struct lab_dma_lli *caninos_dma_alloc_lli
	(struct dma_chan *chan, struct caninos_dma_config *config)
{
	struct lab_dma *cd = to_lab_dma(chan->device);
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	struct dma_slave_config *sconfig = &vchan->cfg;
	struct lab_dma_lli *lli;
	u32 mode, ctrla, ctrlb;
	dma_addr_t phys;
	
	mode = DMA_MODE_PW(0);
	
	switch (config->dir)
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
		return ERR_PTR(-EINVAL);
	}
	
	if (config->len > CANINOS_DMA_FRAME_MAX_LENGTH) {
		return ERR_PTR(-EINVAL);
	}
	
	ctrla = llc_hw_ctrla(mode, DMA_LLC_SAV_LOAD_NEXT | DMA_LLC_DAV_LOAD_NEXT);
	ctrlb = llc_hw_ctrlb(DMA_INTCTL_SUPER_BLOCK, cd->devid);
	
	lli = dma_pool_alloc(cd->lli_pool, GFP_ATOMIC, &phys);
	
	if (!lli) {
		return ERR_PTR(-ENOMEM);
	}
	
	memset(lli, 0, sizeof(*lli));
	
	INIT_LIST_HEAD(&lli->node);
	
	lli->phys = phys;
	
	/* frame count fixed as 1, and max frame length is 20bit */
	switch(cd->devid)
	{
	case DEVID_K7_DMAC:
		lli->hw.hw_s7.next_lli = 0;
		lli->hw.hw_s7.saddr = config->src;
		lli->hw.hw_s7.daddr = config->dst;
		lli->hw.hw_s7.fcnt = 1;
		lli->hw.hw_s7.flen = config->len;
		lli->hw.hw_s7.src_stride = 0;
		lli->hw.hw_s7.dst_stride = 0;
		lli->hw.hw_s7.ctrla = ctrla;
		lli->hw.hw_s7.ctrlb = ctrlb;
		break;
		
	case DEVID_K9_DMAC:
		lli->hw.hw_s9.next_lli = 0;
		lli->hw.hw_s9.saddr = config->src;
		lli->hw.hw_s9.daddr = config->dst;
		lli->hw.hw_s9.fcnt = 1;
		lli->hw.hw_s9.flen = config->len;
		lli->hw.hw_s9.src_stride = 0;
		lli->hw.hw_s9.dst_stride = 0;
		lli->hw.hw_s9.ctrla = ctrla;
		lli->hw.hw_s9.ctrlb = ctrlb;
		break;
		
	case DEVID_K5_DMAC:
		lli->hw.hw_s5.next_lli = 0;
		lli->hw.hw_s5.saddr = config->src;
		lli->hw.hw_s5.daddr = config->dst;
		lli->hw.hw_s5.fcnt = 1;
		lli->hw.hw_s5.flen = config->len;
		lli->hw.hw_s5.src_stride = 0;
		lli->hw.hw_s5.dst_stride = 0;
		lli->hw.hw_s5.ctrla = ctrla;
		lli->hw.hw_s5.ctrlb = ctrlb;
		break;
	}
	return lli;
}

static struct lab_dma_lli *caninos_dma_add_lli
	(struct dma_chan *chan, struct lab_dma_txd *txd, struct lab_dma_lli *prev,
	 struct lab_dma_lli *next)
{
	struct lab_dma *cd = to_lab_dma(chan->device);
	
	list_add_tail(&next->node, &txd->lli_list);
	
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

static struct lab_dma_pchan *caninos_dma_get_pchan
	(struct lab_dma *cd, struct lab_dma_vchan *vchan)
{
	struct lab_dma_pchan *pchan = NULL;
	unsigned long flags;
	int i;
	
	for (i = 0; i < cd->nr_pchans; i++)
	{
		pchan = &cd->pchans[i];
		
		spin_lock_irqsave(&cd->lock, flags);
		
		if (!pchan->vchan)
		{
			pchan->vchan = vchan;
			spin_unlock_irqrestore(&cd->lock, flags);
			break;
		}
		
		spin_unlock_irqrestore(&cd->lock, flags);
	}
	
	return pchan;
}

static void caninos_dma_terminate_pchan(struct lab_dma *cd,
                                        struct lab_dma_pchan *pchan)
{
	unsigned long flags, timeout = 1000000, tmp;
	/* note: this is a forced termination, so we do not care about the data */
	
	spin_lock_irqsave(&cd->lock, flags);
	
	/* disable channel interrupt */
	dma_writel(cd, dma_readl(cd, DMA_IRQ_EN0) & ~BIT(pchan->id), DMA_IRQ_EN0);
	
	/* stop dma operation */
	pchan_writel(pchan, 0, DMAX_START);
	
	spin_unlock_irqrestore(&cd->lock, flags);
	
	/* wait for channel inactive */
	do {
		/* bus delay */
		dma_readl(cd, DMA_IDLE_STAT);
		dma_readl(cd, DMA_IDLE_STAT);
		dma_readl(cd, DMA_IDLE_STAT);
		
		tmp = dma_readl(cd, DMA_IDLE_STAT) & BIT(pchan->id); //high -> idle
		timeout--;
		
	} while (!tmp && timeout > 0);
	
	if (timeout == 0) {
		dev_err(cd->dev, "termination of channel %u timed out.\n", pchan->id);
	}
	
	spin_lock_irqsave(&cd->lock, flags);
	
	/* clear interrupt status */
	pchan_writel(pchan, pchan_readl(pchan, DMAX_INT_STATUS), DMAX_INT_STATUS);
	
	/* clear interrupt pending flag */
	dma_writel(cd, BIT(pchan->id), DMA_IRQ_PD0);
	
	pchan->vchan = NULL;
	
	spin_unlock_irqrestore(&cd->lock, flags);
}

static void caninos_dma_pchan_pause(struct lab_dma *cd, 
                                    struct lab_dma_pchan *pchan)
{
	u32 tmp;
	
	if(cd->devid == DEVID_K5_DMAC) {
		tmp = dma_readl(cd, DMA_DBGSEL);
		
		if (!(tmp & BIT(pchan->id + 16))) {
			dma_writel(cd, tmp | BIT(pchan->id + 16), DMA_DBGSEL);
		}
	}
	else {
		tmp = pchan_readl(pchan, DMAX_PAUSE_K7_K9);
		
		if (!(tmp & 0x1)) {
			pchan_writel(pchan, 0x1, DMAX_PAUSE_K7_K9);
		}
	}
	pchan->paused = true;
}

static void caninos_dma_pchan_resume(struct lab_dma *cd,
                                     struct lab_dma_pchan *pchan)
{
	u32 tmp;
	
	if(cd->devid == DEVID_K5_DMAC) {
		tmp = dma_readl(cd, DMA_DBGSEL);
		
		if (tmp & BIT(pchan->id + 16)) {
			dma_writel(cd, tmp & ~BIT(pchan->id + 16), DMA_DBGSEL);
		}
	}
	else {
		tmp = pchan_readl(pchan, DMAX_PAUSE_K7_K9);
		
		if (tmp & 0x1) {
			pchan_writel(pchan, 0x0, DMAX_PAUSE_K7_K9);
		}
	}
	pchan->paused = false;
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
	
	caninos_dma_terminate_pchan(cd, pchan);
	
	lli = list_first_entry(&txd->lli_list, struct lab_dma_lli, node);
	
	ctrla = DMA_LLC_SAV_LOAD_NEXT | DMA_LLC_DAV_LOAD_NEXT;
	ctrlb = DMA_INTCTL_SUPER_BLOCK;
	
	spin_lock_irqsave(&cd->lock, flags);
	
	pchan_writel(pchan, DMA_MODE_LME, DMAX_MODE);
	pchan_writel(pchan, ctrla, DMAX_LINKLIST_CTL);
	pchan_writel(pchan, lli->phys, DMAX_NEXT_DESCRIPTOR);
	pchan_writel(pchan, ctrlb, DMAX_INT_CTL);
	
	caninos_dma_pchan_resume(cd, pchan);
	
	/* disable channel interrupt */
	dma_writel(cd, dma_readl(cd, DMA_IRQ_EN0) & ~BIT(pchan->id), DMA_IRQ_EN0);
	
	/* clear interrupt status */
	pchan_writel(pchan, pchan_readl(pchan, DMAX_INT_STATUS), DMAX_INT_STATUS);
	
	/* clear interrupt pending flag */
	dma_writel(cd, BIT(pchan->id), DMA_IRQ_PD0);
	
	/* enable channel interrupt */
	dma_writel(cd, dma_readl(cd, DMA_IRQ_EN0) | BIT(pchan->id), DMA_IRQ_EN0);
	
	/* start dma operation */
	pchan_writel(pchan, 0x1, DMAX_START);
	
	spin_unlock_irqrestore(&cd->lock, flags);
	return 0;
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
	struct caninos_dma_config config;
	struct scatterlist *sg;
	int i, ret;
	
	if (unlikely(!sgl || !sg_len)) {
		return NULL;
	}
	
	txd = kzalloc(sizeof(*txd), GFP_NOWAIT);
	
	if (!txd) {
		return NULL;
	}
	
	INIT_LIST_HEAD(&txd->lli_list);
	
	for_each_sg(sgl, sg, sg_len, i)
	{
		config.dir = dir;
		config.len = sg_dma_len(sg);
		
		if (config.dir == DMA_MEM_TO_DEV)
		{
			config.src = sg_dma_address(sg);
			config.dst = sconfig->dst_addr;
		}
		else if (config.dir == DMA_DEV_TO_MEM)
		{
			config.src = sconfig->src_addr;
			config.dst = sg_dma_address(sg);
		}
		
		lli = caninos_dma_alloc_lli(chan, &config);
		
		if (IS_ERR(lli))
		{
			ret = (int) PTR_ERR(lli);
			dev_err(chan2dev(chan), "failed to alloc lli, ret=%d", ret);
			caninos_dma_free_txd(cd, txd);
			return NULL;
		}
		
		prev = caninos_dma_add_lli(chan, txd, prev, lli);
	}
	
	smp_wmb();
	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);
}

static struct dma_async_tx_descriptor *
	caninos_dma_prep_memcpy(struct dma_chan *chan, dma_addr_t dst,
	                        dma_addr_t src, size_t len, unsigned long flags)
{
	struct lab_dma *cd = to_lab_dma(chan->device);
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	struct lab_dma_lli *lli, *prev = NULL;
	struct caninos_dma_config config;
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
	
	for (offset = 0; offset < len; offset += bytes)
	{
		bytes = min_t(size_t, (len - offset), CANINOS_DMA_FRAME_MAX_LENGTH);
		
		config.src = src + offset;
		config.dst = dst + offset;
		config.len = bytes;
		config.dir = DMA_MEM_TO_MEM;
		
		lli = caninos_dma_alloc_lli(chan, &config);
		
		if (IS_ERR(lli))
		{
			ret = (int) PTR_ERR(lli);
			dev_err(chan2dev(chan), "failed to alloc lli, ret=%d", ret);
			caninos_dma_free_txd(cd, txd);
			return NULL;
		}
		
		prev = caninos_dma_add_lli(chan, txd, prev, lli);
	}
	
	return vchan_tx_prep(&vchan->vc, &txd->vd, flags);
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
	struct lab_dma_pchan *pchan;
	unsigned long flags;
	
	spin_lock_irqsave(&vchan->vc.lock, flags);
	
	pchan = vchan->pchan;
	
	if (pchan && vchan->txd) {
		spin_lock(&cd->lock);
		caninos_dma_pchan_pause(cd, pchan);
		spin_unlock(&cd->lock);
	}
	
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
	return 0;
}

static int caninos_device_resume(struct dma_chan *chan)
{
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	struct lab_dma *cd = to_lab_dma(vchan->vc.chan.device);
	struct lab_dma_pchan *pchan = vchan->pchan;
	unsigned long flags;
	
	spin_lock_irqsave(&vchan->vc.lock, flags);
	
	if (pchan && vchan->txd) {
		spin_lock(&cd->lock);
		caninos_dma_pchan_resume(cd, pchan);
		spin_unlock(&cd->lock);
	}
	
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
	return 0;
}

static void caninos_dma_phy_free(struct lab_dma *cd,
                                 struct lab_dma_vchan *vchan)
{
	/* Ensure that the physical channel is stopped */
	caninos_dma_terminate_pchan(cd, vchan->pchan);
	vchan->pchan = NULL;
	vchan->drq = -1;
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

static void caninos_dma_desc_free(struct virt_dma_desc *vd)
{
	struct lab_dma *cd = to_lab_dma(vd->tx.chan->device);
	struct lab_dma_txd *txd = to_lab_txd(&vd->tx);

	caninos_dma_free_txd(cd, txd);
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
	pending |= caninos_dma_clear_missed_irqs(cd);
	
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
			vchan->txd = NULL;
			vchan_cookie_complete(&txd->vd);
			
			if (vchan_next_desc(&vchan->vc)) {
				caninos_dma_start_next_txd(vchan);
			}
			else {
				caninos_dma_phy_free(cd, vchan);
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
	
	if (vchan->pchan) {
		caninos_dma_phy_free(cd, vchan);
	}
	
	if (vchan->txd) {
		caninos_dma_desc_free(&vchan->txd->vd);
		vchan->txd = NULL;
	}
	
	vchan_get_all_descriptors(&vchan->vc, &head);
	vchan_dma_desc_free_list(&vchan->vc, &head);
	
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
	
	return 0;
}

static inline void caninos_dma_phy_alloc_and_start(struct lab_dma_vchan *vchan)
{
	struct lab_dma *cd = to_lab_dma(vchan->vc.chan.device);
	struct lab_dma_pchan *pchan;
	
	pchan = caninos_dma_get_pchan(cd, vchan);
	
	if (!pchan) {
		return;
	}
	
	vchan->pchan = pchan;
	
	caninos_dma_start_next_txd(vchan);
}

static void caninos_dma_issue_pending(struct dma_chan *chan)
{
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	unsigned long flags;
	
	spin_lock_irqsave(&vchan->vc.lock, flags);
	
	if (vchan_issue_pending(&vchan->vc)) {
		if (!vchan->pchan) {
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
	bool start_counting;
	u32 next_phys;
	size_t bytes;
	
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
	
	lli = list_first_entry(&txd->lli_list, typeof(*lli), node);
	
	start_counting = false;
	
	/* get the linked list chain remain count */
	list_for_each_entry(lli, &txd->lli_list, node)
	{
		if (lli->phys == next_phys) {
			start_counting = true;
		}
		if (start_counting) {
			bytes += lli_get_frame_len(lli, cd->devid);
		}
		smp_rmb();
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
	
	spin_lock_irqsave(&vchan->vc.lock, flags);
	
	if (ret == DMA_IN_PROGRESS)
	{
		spin_lock(&cd->lock);
		
		if (vchan->pchan && vchan->txd && vchan->pchan->paused) {
			ret = DMA_PAUSED;
		}
		
		spin_unlock(&cd->lock);
	}
	
	if (!state) {
		spin_unlock_irqrestore(&vchan->vc.lock, flags);
		return ret;
	}
	
	vd = vchan_find_desc(&vchan->vc, cookie);
	
	if (vd)
	{
		txd = to_lab_txd(&vd->tx);
		list_for_each_entry(lli, &txd->lli_list, node) {
			bytes += lli_get_frame_len(lli, cd->devid);
		}
	}
	else
	{
		spin_lock(&cd->lock);
		bytes = caninos_dma_getbytes_chan(vchan);
		spin_unlock(&cd->lock);
	}
	
	spin_unlock_irqrestore(&vchan->vc.lock, flags);
	dma_set_residue(state, bytes);
	return ret;
}

static void caninos_dma_free_chan_resources(struct dma_chan *chan)
{
	struct lab_dma_vchan *vchan = to_lab_vchan(chan);
	
	/* Ensure all queued descriptors are freed */
	vchan_free_chan_resources(&vchan->vc);
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
	
	if (drq > DMA_DRQ_LIMIT) {
		return NULL;
	}
	
	fargs.drq = drq;
	fargs.cd = cd;
	
	dma_cap_zero(cap);
	dma_cap_set(DMA_SLAVE, cap);
	
	return dma_request_channel(cap, caninos_dma_of_filter, &fargs);
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

static inline void caninos_dma_disable_all_irqs(struct lab_dma *cd)
{
	dma_writel(cd, DMA_IRQ_EN0, 0x0);
	dma_writel(cd, DMA_IRQ_EN1, 0x0);
	dma_writel(cd, DMA_IRQ_EN2, 0x0);
	dma_writel(cd, DMA_IRQ_EN3, 0x0);
}

static inline void caninos_dma_clear_all_irqs(struct lab_dma *cd)
{
	dma_writel(cd, DMA_IRQ_PD0, 0xfff);
	dma_writel(cd, DMA_IRQ_PD1, 0xfff);
	dma_writel(cd, DMA_IRQ_PD2, 0xfff);
	dma_writel(cd, DMA_IRQ_PD3, 0xfff);
}

static int caninos_dma_hw_init(struct lab_dma *cd)
{
	int ret;
	
	pm_runtime_enable(cd->dev);
	pm_runtime_get(cd->dev);
	
	ret = clk_prepare_enable(cd->clk);
	
	if (ret) {
		pm_runtime_put(cd->dev);
		pm_runtime_disable(cd->dev);
		return ret;
	}
	
	/* Make sure all irqs are masked */
	caninos_dma_disable_all_irqs(cd);
	
	/* Clear all pendings irqs */
	caninos_dma_clear_all_irqs(cd);
	
	/* Request IRQ */
	ret = devm_request_irq(cd->dev, cd->irq, caninos_dma_interrupt,
	                       IRQF_SHARED, dev_name(cd->dev), cd);
	if (ret) {
		clk_disable_unprepare(cd->clk);
		pm_runtime_put(cd->dev);
		pm_runtime_disable(cd->dev);
		return ret;
	}
	
	/* Setup QOS */
	if (cd->devid == DEVID_K5_DMAC) {
		dma_writel(cd, DMA_NIC_QOS, 0xf0);
	}
	
	return 0;
}

static void caninos_dma_hw_turnoff(struct lab_dma *cd)
{
	caninos_dma_disable_all_irqs(cd);
	caninos_dma_clear_all_irqs(cd);
	
	devm_free_irq(cd->dma.dev, cd->irq, cd);
	
	clk_disable_unprepare(cd->clk);
	
	pm_runtime_put(cd->dev);
	pm_runtime_disable(cd->dev);
}

static int __maybe_unused caninos_dma_runtime_suspend(struct device *dev)
{
	//struct lab_dma *cd = dev_get_drvdata(dev);
	return 0;
}

static int __maybe_unused caninos_dma_runtime_resume(struct device *dev)
{
	//struct lab_dma *cd = dev_get_drvdata(dev);
	return 0;
}

static int parse_device_properties(struct lab_dma *cd)
{
	enum caninos_dmac_id devid;
	
	devid = (enum caninos_dmac_id) of_device_get_match_data(cd->dev);
	
	switch(devid)
	{
	case DEVID_K7_DMAC:
		cd->nr_pchans = 10;
		cd->nr_vchans = 48;
		break;
	case DEVID_K9_DMAC:
	case DEVID_K5_DMAC:
		cd->nr_pchans = 12;
		cd->nr_vchans = 48;
		break;
	default:
		dev_err(cd->dev, "unrecognized hardware type.\n");
		return -ENODEV;
	}
	
	cd->devid = devid;
	
	return 0;
}

static int caninos_dma_probe(struct platform_device *pdev)
{
	struct lab_dma *cd;
	struct resource *res;
	int ret, i;
	
	cd = devm_kzalloc(&pdev->dev, sizeof(*cd), GFP_KERNEL);
	
	if (!cd) {
		return -ENOMEM;
	}
	
	cd->dev = &pdev->dev;
	spin_lock_init(&cd->lock);
	
	cd->irq = platform_get_irq(pdev, 0);
	
	if (cd->irq < 0) {
		return cd->irq;
	}
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cd->base = devm_ioremap_resource(cd->dev, res);
	
	if (IS_ERR(cd->base)) {
		return PTR_ERR(cd->base);
	}
	
	cd->clk = devm_clk_get(cd->dev, NULL);
	
	if (IS_ERR(cd->clk)) {
		return PTR_ERR(cd->clk);
	}
	
	ret = parse_device_properties(cd);
	
	if (ret) {
		return ret;
	}
	
	cd->pchans = devm_kcalloc(cd->dev, cd->nr_pchans,
	                          sizeof(struct lab_dma_pchan), GFP_KERNEL);
	
	if (!cd->pchans) {
		return -ENOMEM;
	}
	
	cd->vchans = devm_kcalloc(cd->dev, cd->nr_vchans,
	                          sizeof(struct lab_dma_vchan), GFP_KERNEL);
	
	if (!cd->vchans) {
		return -ENOMEM;
	}
	
	INIT_LIST_HEAD(&cd->dma.channels);
	
	/* Init physical channels */
	for (i = 0; i < cd->nr_pchans; i++)
	{
		struct lab_dma_pchan *pchan = &cd->pchans[i];
		
		pchan->id = i;
		pchan->base = cd->base + DMA_CHAN_BASE(i);
		pchan->paused = false;
	}
	
	/* Init virtual channels */
	for (i = 0; i < cd->nr_vchans; i++)
	{
		struct lab_dma_vchan *vchan = &cd->vchans[i];
		
		vchan->drq = -1;
		vchan->vc.desc_free = caninos_dma_desc_free;
		vchan_init(&vchan->vc, &cd->dma);
	}
	
	/* Set capabilities */
	dma_cap_set(DMA_MEMCPY, cd->dma.cap_mask);
	dma_cap_set(DMA_SLAVE, cd->dma.cap_mask);
	
	/* DMA capabilities */
	cd->dma.chancnt = cd->nr_vchans;
	cd->dma.max_burst = CANINOS_DMA_FRAME_MAX_LENGTH;
	cd->dma.src_addr_widths = CANINOS_DMA_BUSWIDTHS;
	cd->dma.dst_addr_widths = CANINOS_DMA_BUSWIDTHS;
	cd->dma.directions = BIT(DMA_MEM_TO_MEM);
	cd->dma.directions |= BIT(DMA_MEM_TO_DEV) | BIT(DMA_DEV_TO_MEM);
	cd->dma.residue_granularity = DMA_RESIDUE_GRANULARITY_BURST;
	cd->dma.copy_align = DMAENGINE_ALIGN_4_BYTES;
	
	cd->dma.dev = cd->dev;
	cd->dma.device_free_chan_resources = caninos_dma_free_chan_resources;
	cd->dma.device_tx_status = caninos_dma_tx_status;
	cd->dma.device_issue_pending = caninos_dma_issue_pending;
	cd->dma.device_prep_dma_memcpy = caninos_dma_prep_memcpy;
	cd->dma.device_terminate_all = caninos_device_terminate_all;
	cd->dma.device_pause = caninos_device_pause;
	cd->dma.device_resume = caninos_device_resume;
	cd->dma.device_config = caninos_device_config;
	cd->dma.device_prep_slave_sg = caninos_dma_prep_slave_sg;
	
	cd->dma.dev->dma_parms = &cd->dma_parms;
	dma_set_max_seg_size(cd->dev, CANINOS_DMA_FRAME_MAX_LENGTH);
	
	platform_set_drvdata(pdev, cd);
	
	/* Set DMA mask to 32 bits */
	ret = dma_set_mask_and_coherent(cd->dev, DMA_BIT_MASK(32));
	
	if (ret) {
		return ret;
	}
	
	/* Create a pool of consistent memory blocks for hardware descriptors */
	cd->lli_pool = dma_pool_create(dev_name(cd->dev), cd->dev,
	                               sizeof(struct lab_dma_lli),
	                               __alignof__(struct lab_dma_lli), 0);
	if (!cd->lli_pool) {
		return -ENOMEM;
	}
	
	/* Init hardware */
	ret = caninos_dma_hw_init(cd);
	
	if (ret){
		dma_pool_destroy(cd->lli_pool);
		return ret;
	}
	
	/* Register DMA controller device */
	ret = dma_async_device_register(&cd->dma);
	
	if (ret) {
		caninos_dma_hw_turnoff(cd);
		dma_pool_destroy(cd->lli_pool);
		return ret;
	}
	
	/* Device-tree DMA controller registration */
	ret = of_dma_controller_register(cd->dev->of_node,
	                                 caninos_of_dma_xlate, cd);
	if (ret) {
		dma_async_device_unregister(&cd->dma);
		caninos_dma_hw_turnoff(cd);
		dma_pool_destroy(cd->lli_pool);
		return ret;
	}
	
	return 0;
}

static int caninos_dma_remove(struct platform_device *pdev)
{
	struct lab_dma *cd = platform_get_drvdata(pdev);
	
	of_dma_controller_free(pdev->dev.of_node);
	dma_async_device_unregister(&cd->dma);
	caninos_dma_hw_turnoff(cd);
	caninos_dma_free(cd);
	dma_pool_destroy(cd->lli_pool);
	
	return 0;
}

static const struct dev_pm_ops caninos_dma_pm_ops = {
	SET_RUNTIME_PM_OPS(caninos_dma_runtime_suspend,
	                   caninos_dma_runtime_resume,
	                   NULL)
};

static const struct of_device_id caninos_dma_match[] = {
	{ .compatible = "caninos,k9-dma", .data = (void *)DEVID_K9_DMAC,},
	{ .compatible = "caninos,k7-dma", .data = (void *)DEVID_K7_DMAC,},
	{ .compatible = "caninos,k5-dma", .data = (void *)DEVID_K5_DMAC,},
	{ /* sentinel */ },
};

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

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
