// SPDX-License-Identifier: GPL-2.0
/*
 * MMC Host Controller Driver for Caninos Labrador
 *
 * Copyright (c) 2022-2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2018-2020 LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
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

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h> 
#include <linux/of_platform.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/mmc/host.h>
#include <linux/clk.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pinctrl/consumer.h>
#include <linux/reset.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/iopoll.h>

#include "caninos-mmc.h"

enum mmc_hw_model {
	MMC_HW_MODEL_INV = 0,
	MMC_HW_MODEL_K5  = 1,
	MMC_HW_MODEL_K7  = 2,
};

struct con_delay
{
	u8 delay_lowclk;
	u8 delay_midclk;
	u8 delay_highclk;
};

struct caninos_mmc_host
{
	struct device *dev;
	struct mmc_host *mmc;
	enum mmc_hw_model model;
	struct mmc_host_ops ops;
	void __iomem *base;
	spinlock_t lock;
	bool device_powered;
	int irq;
	int power_gpio;
	int enable_gpio;
	int reset_gpio;
	int id;
	struct clk *clk;
	struct dma_chan *dma;
	struct dma_async_tx_descriptor *tx;
	
	struct con_delay wdelay;
	struct con_delay rdelay;
	
	struct completion dma_complete;
	struct completion sdc_complete;
	
	struct reset_control *rst;
	
	u8 curr_rdelay, curr_wdelay;
};

static inline void caninos_mmc_disable_all_irqs(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	u32 state = readl(HOST_STATE(priv));
	state &= ~(SD_STATE_TEIE | SD_STATE_SDIOB_EN | SD_STATE_SDIOA_EN);
	writel(state, HOST_STATE(priv)); /* also clear pending irqs */
}

static inline bool caninos_mmc_is_scc_en(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	return !!(readl(HOST_CTL(priv)) & SD_CTL_SCC);
}

static inline bool caninos_mmc_is_low_voltage_en(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	return !!(readl(HOST_EN(priv)) & SD_EN_S18EN);
}

static inline void caninos_mmc_controller_enable(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	
	/* enable the controller and put it in a known state */
	if (priv->model == MMC_HW_MODEL_K7) {
		writel(SD_ENABLE, HOST_EN(priv));
	}
	else {
		writel(SD_ENABLE | SD_EN_RESE, HOST_EN(priv));
	}
	
	writel(0x0, HOST_CTL(priv));
	writel(0x0, HOST_STATE(priv));
}

static inline void caninos_mmc_voltage_change_delay(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	/* k7 needs at least 12.8ms after voltage change (hardware bug) */
	if (priv->model == MMC_HW_MODEL_K7) {
		usleep_range(14000, 17500);
	}
}

static void caninos_mmc_sdio_enable(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	u32 val, new_val;
	
	val = readl(HOST_EN(priv));
	new_val = val | SD_EN_SDIOEN;
	
	if (val != new_val) {
		writel(new_val, HOST_EN(priv));
	}
}

static bool caninos_mmc_set_raw_clk(struct mmc_host *host, unsigned long clock)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	unsigned long rate;
	
	clock = max((unsigned long)(host->f_min), clock);
	
	/* controller clock is twice the device clock */
	rate = clk_round_rate(priv->clk, clock * 2UL);
	
	if (!rate || (rate == host->actual_clock)){
		return false; /* clk remains the same */
	}
	
	clk_set_rate(priv->clk, rate);
	usleep_range(1000, 1500);
	
	host->actual_clock = clk_get_rate(priv->clk);
	return true;
}

static void caninos_mmc_update_delays(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	u32 val, new_val;
	
	val = readl(HOST_CTL(priv));
	new_val = val & ~(0xff << 16);
	
	new_val |= SD_CTL_RDELAY(priv->curr_rdelay);
	new_val |= SD_CTL_WDELAY(priv->curr_wdelay);
	
	if (new_val != val) {
		writel(new_val, HOST_CTL(priv));
		readl(HOST_CTL(priv));
	}
}

static void caninos_mmc_reset_delays(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	unsigned long clock;
	u8 rdelay, wdelay;
	
	clock = host->actual_clock / 2UL;
	
	if (clock <= 1000000UL) {
		rdelay = priv->rdelay.delay_lowclk;
		wdelay = priv->wdelay.delay_lowclk;
	}
	else if (clock <= 26000000UL) {
		rdelay = priv->rdelay.delay_midclk;
		wdelay = priv->wdelay.delay_midclk;
	}
	else {
		rdelay = priv->rdelay.delay_highclk;
		wdelay = priv->wdelay.delay_highclk;
	}
	
	priv->curr_rdelay = rdelay;
	priv->curr_wdelay = wdelay;
	
	caninos_mmc_update_delays(host);
}

static void caninos_mmc_hard_reset(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	bool s18en;
	
	s18en = caninos_mmc_is_low_voltage_en(host);
	
	reset_control_assert(priv->rst);
	usleep_range(1000, 1500);
	reset_control_deassert(priv->rst);
	
	/* this is important! */
	if (s18en) {
		caninos_mmc_voltage_change_delay(host);
	}
}

static void caninos_mmc_stop_transmission(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	u32 config, enable, state;
	int retry;
	
	config = readl(HOST_CTL(priv));
	
	if (!(config & SD_CTL_TS)) {
		return;
	}
	
	config &= ~SD_CTL_TS;
	
	for (retry = 4; retry > 0; retry--)
	{
		writel(config, HOST_CTL(priv));
		usleep_range(10, 15);
		
		if (!(readl(HOST_CTL(priv)) & SD_CTL_TS)) {
			return;
		}
	}
	
	enable = readl(HOST_EN(priv));
	state = readl(HOST_STATE(priv));
	
	/* the controller is stuck, just reset it */
	caninos_mmc_hard_reset(host);
	
	writel(enable, HOST_EN(priv));
	writel(config, HOST_CTL(priv));
	writel(state, HOST_STATE(priv));
}

static void caninos_mmc_en_sdio_irq(struct mmc_host * mmc, int enable)
{
	struct caninos_mmc_host *priv = mmc_priv(mmc);
	unsigned long flags;
	u32 val, new_val;
	
	spin_lock_irqsave(&priv->lock, flags);
	
	val = readl(HOST_STATE(priv));
	
	/* protect any pending interrupt */
	val &= ~(SD_STATE_SDIOA_P | SD_STATE_SDIOB_P | SD_STATE_TEI);
	
	new_val = val & ~SD_STATE_SDIOA_EN;
	
	if (enable) {
		new_val |= SD_STATE_SDIOA_EN;
	}
	
	if (val != new_val) {
		writel(new_val, HOST_STATE(priv));
	}
	
	spin_unlock_irqrestore(&priv->lock, flags);
}

static void caninos_mmc_enable_disable_tei(struct mmc_host *host, bool enable)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	unsigned long flags;
	u32 state;
	
	spin_lock_irqsave(&priv->lock, flags);
	
	/* read current state reg value */
	state = readl(HOST_STATE(priv));
	
	/* protect any pending SDIO interrupt */
	state &= ~(SD_STATE_SDIOA_P | SD_STATE_SDIOB_P);
	
	/* disable transfer end interrupt */
	state &= ~(SD_STATE_TEIE);
	
	/* update state reg and also clear transfer end interrupt */
	writel(state | SD_STATE_TEI, HOST_STATE(priv));
	
	if (enable) {
		/* enable transfer end interrupt */
		writel(state | SD_STATE_TEIE, HOST_STATE(priv));
	}
	
	spin_unlock_irqrestore(&priv->lock, flags);
}

static void caninos_mmc_dma_complete(void *param)
{
	struct caninos_mmc_host *priv = (struct caninos_mmc_host *)param;
	complete(&priv->dma_complete);
}

static int caninos_mmc_prep_dma(struct mmc_host *host, struct mmc_data *data)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	struct dma_chan *chan = priv->dma;
	enum dma_data_direction direction;
	unsigned int len, nr_sg;
	struct scatterlist *sg;
	int i;
	
	if (!data) {
		return 0;
	}
	
	for_each_sg(data->sg, sg, data->sg_len, i) {
		if ((sg->offset & 0x3) || (sg->length & 0x3)) {
			dev_err(priv->dev, "unsupported block with off:0x%x length:0x%x\n",
			        sg->offset, sg->length);
			return -EINVAL;
		}
	}
	
	if (data->flags & MMC_DATA_WRITE) {
		direction = DMA_MEM_TO_DEV;
	}
	else {
		direction = DMA_DEV_TO_MEM;
	}
	
	nr_sg = dma_map_sg(chan->device->dev, data->sg, data->sg_len, direction);
	
	if (nr_sg == 0) {
		dev_err(priv->dev, "dma_map_sg() failed\n");
		return -EINVAL;
	}
	
	priv->tx = dmaengine_prep_slave_sg(chan, data->sg, nr_sg, direction,
	                                   DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	
	if (!priv->tx) {
		dma_unmap_sg(chan->device->dev, data->sg, data->sg_len, direction);
		dev_err(priv->dev, "dmaengine_prep_slave_sg() failed\n");
		return -EINVAL;
	}
	
	priv->tx->callback = caninos_mmc_dma_complete;
	priv->tx->callback_param = priv;
	
	reinit_completion(&priv->dma_complete);
	dmaengine_submit(priv->tx);
	
	len = data->blksz * data->blocks;
	
	/* use DMA */
	writel(readl(HOST_EN(priv)) | SD_EN_BSEL, HOST_EN(priv));
	
	writel(data->blocks, HOST_BLK_NUM(priv));
	writel(data->blksz, HOST_BLK_SIZE(priv));
	
	if (len < 512U) {
		writel(len, HOST_BUF_SIZE(priv));
	}
	else {
		writel(512U, HOST_BUF_SIZE(priv));
	}
	return 0;
}

static void caninos_mmc_start_dma(struct mmc_host *host, struct mmc_data *data)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	struct dma_chan *chan = priv->dma;
	
	if (data) {
		dma_async_issue_pending(chan);
	}
}

static int caninos_mmc_wait_dma(struct mmc_host *host, struct mmc_data *data,
                                unsigned int timeout_ms)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	struct dma_chan *chan = priv->dma;
	enum dma_data_direction direction;
	unsigned long time_left;
	
	if (!data) {
		return 0;
	}
	if (data->flags & MMC_DATA_WRITE) {
		direction = DMA_MEM_TO_DEV;
	}
	else {
		direction = DMA_DEV_TO_MEM;
	}
	
	time_left = wait_for_completion_timeout(&priv->dma_complete,
	                                        msecs_to_jiffies(timeout_ms));
	
	if (!time_left) {
		dmaengine_terminate_async(chan);
		
		if (timeout_ms) {
			dev_err(priv->dev, "dma completion timed out\n");
		}
	}
	
	dma_unmap_sg(chan->device->dev, data->sg, data->sg_len, direction);
	
	/* use AHB */
	writel(readl(HOST_EN(priv)) & ~SD_EN_BSEL, HOST_EN(priv));
	
	return !time_left ? -ETIMEDOUT : 0;
}

static int caninos_mmc_check_err(struct mmc_host *host, u32 err_mask)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	u32 val;
	
	val = readl(HOST_STATE(priv));
	
	if (!(val & SD_STATE_CLC)) {
		return -ETIMEDOUT;
	}
	
	val &= err_mask;
	
	if (val & SD_STATE_CRC7ER) {
		return -EILSEQ;
	}
	if (val & SD_STATE_WC16ER)
	{
		if (priv->curr_wdelay > 0) {
			priv->curr_wdelay--;
		}
		else {
			priv->curr_wdelay = 0xf;
		}
		dev_err(priv->dev, "write crc error, wdelay set to 0x%x\n",
		        priv->curr_wdelay);
		return -EILSEQ;
	}
	if (val & SD_STATE_RC16ER)
	{
		if (priv->curr_rdelay > 0) {
			priv->curr_rdelay--;
		}
		else {
			priv->curr_rdelay = 0xf;
		}
		dev_err(priv->dev, "read crc error, rdelay set to 0x%x\n",
		        priv->curr_rdelay);
		return -EILSEQ;
	}
	if (val & SD_STATE_CLNR) {
		return -EILSEQ;
	}
	return 0;
}

static void caninos_mmc_wait_transfer(struct mmc_host *host,
                                      unsigned int ms, unsigned int us)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	#define TE_COND(x) (!(x & SD_CTL_TS))
	u32 val;
	
	if (ms) {
		wait_for_completion_timeout(&priv->sdc_complete, msecs_to_jiffies(ms));
		/* disable transfer end interrupt */
		caninos_mmc_enable_disable_tei(host, false);
	}
	else if (us <= 2U) {
		readl_poll_timeout_atomic(HOST_CTL(priv), val, TE_COND(val), 0U, us);
	}
	else if (us <= 20U) {
		readl_poll_timeout_atomic(HOST_CTL(priv), val, TE_COND(val), 2U, us);
	}
	else {
		readl_poll_timeout(HOST_CTL(priv), val, TE_COND(val), 20U, us);
	}
	#undef TE_COND
}

static int caninos_mmc_do_transfer
	(struct mmc_host *host, struct mmc_command *cmd, struct mmc_data *data)
{
	u32 mode, config, err_mask, orig_config, rsp[2];
	struct caninos_mmc_host *priv = mmc_priv(host);
	unsigned int timeout_ms, timeout_us, khz;
	u8 rdelay, wdelay;
	int err;
	
	rdelay = priv->curr_rdelay;
	wdelay = priv->curr_wdelay;
	
	if (!cmd) {
		return -EINVAL;
	}
	
	/* device clock is half of controller clock (host->actual_clock / 2) */
	khz = max(DIV_ROUND_UP(host->actual_clock, 2000U), 200U);
	
	timeout_ms = 0U;
	timeout_us = 0U;
	
	if (data)
	{
		if (mmc_resp_type(cmd) != MMC_RSP_R1) {
			return -EINVAL;
		}
		else if (data->flags & MMC_DATA_READ)
		{
			mode = SD_CTL_TM(4) | SD_CTL_LBE;
			err_mask  = SD_STATE_CLNR | SD_STATE_CRC7ER | SD_STATE_RC16ER;
		}
		else if (data->flags & MMC_DATA_WRITE)
		{
			mode = SD_CTL_TM(5);
			err_mask = SD_STATE_CLNR | SD_STATE_CRC7ER | SD_STATE_WC16ER;
		}
		else {
			return -EINVAL;
		}
		
		timeout_ms = DIV_ROUND_UP(data->timeout_ns, 1000000U);
		timeout_ms += DIV_ROUND_UP(data->timeout_clks, khz);
		
		if (timeout_ms < 20U)
		{
			timeout_ms = 0U;
			timeout_us = DIV_ROUND_UP(data->timeout_ns, 1000U);
			timeout_us += DIV_ROUND_UP(data->timeout_clks * 1000U, khz);
			
			if (!timeout_us) {
				timeout_us = 1U;
			}
		}
	}
	else
	{
		switch (mmc_resp_type(cmd))
		{
		case MMC_RSP_NONE:
			mode = SD_CTL_TM(0);
			err_mask = 0U;
			timeout_us = DIV_ROUND_UP(150000U, khz);
			break;
			
		case MMC_RSP_R1:
			mode = SD_CTL_TM(1);
			err_mask = SD_STATE_CLNR | SD_STATE_CRC7ER;
			timeout_us = DIV_ROUND_UP(300000U, khz);
			break;
			
		case MMC_RSP_R1B:
			mode = SD_CTL_TM(3);
			err_mask = SD_STATE_CLNR | SD_STATE_CRC7ER;
			if (cmd->busy_timeout) {
				timeout_ms = cmd->busy_timeout;
			}
			else {
				timeout_ms = 300U;
			}
			break;
			
		case MMC_RSP_R2:
			mode = SD_CTL_TM(2);
			err_mask = SD_STATE_CLNR | SD_STATE_CRC7ER;
			timeout_us = DIV_ROUND_UP(600000U, khz);
			break;
			
		case MMC_RSP_R3:
			mode = SD_CTL_TM(1) | SD_CTL_C7EN;
			err_mask = SD_STATE_CLNR;
			timeout_us = DIV_ROUND_UP(300000U, khz);
			break;
			
		default:
			return -EINVAL;
		}
	}
	
	/* read current configuration */
	orig_config = readl(HOST_CTL(priv));
	
	/* if transfer start bit is set, quit */
	if (orig_config & SD_CTL_TS) {
		return -EINVAL;
	}
	
	if (caninos_mmc_prep_dma(host, data)) {
		return -EINVAL;
	}
	
	/* irqs are only used for long timeouts (milliseconds) */
	if (timeout_ms) {
		/* enable transfer end interrupt */
		caninos_mmc_enable_disable_tei(host, true);
		reinit_completion(&priv->sdc_complete);
	}
	
	/* copy mode to new config */
	config = mode;
	/* copy current RDELAY and WDELAY to new config */
	config |= orig_config & (SD_CTL_RDELAY(0xf) | SD_CTL_WDELAY(0xf));
	/* copy current SCC setting to new config */
	config |= orig_config & SD_CTL_SCC;
	
	/* write new configuration */
	writel(config, HOST_CTL(priv));
	
	caninos_mmc_start_dma(host, data);
	
	/* set command and argument */
	writel(cmd->arg, HOST_ARG(priv)); /* arg must be before the opcode */
	writel(cmd->opcode, HOST_CMD(priv));
	
	/* start transfer */
	writel(config | SD_CTL_TS, HOST_CTL(priv));
	
	caninos_mmc_wait_transfer(host, timeout_ms, timeout_us);
	
	err = caninos_mmc_check_err(host, err_mask);
	
	if (err == -ETIMEDOUT) {
		caninos_mmc_stop_transmission(host);
	}
	
	if (!err && (cmd->flags & MMC_RSP_PRESENT))
	{
		/* read command response */
		if (cmd->flags & MMC_RSP_136)
		{
			cmd->resp[3] = readl(HOST_RSPBUF0(priv));
			cmd->resp[2] = readl(HOST_RSPBUF1(priv));
			cmd->resp[1] = readl(HOST_RSPBUF2(priv));
			cmd->resp[0] = readl(HOST_RSPBUF3(priv));
		}
		else
		{
			rsp[0] = readl(HOST_RSPBUF0(priv));
			rsp[1] = readl(HOST_RSPBUF1(priv));
			cmd->resp[0] = rsp[1] << 24 | rsp[0] >> 8;
			cmd->resp[1] = rsp[1] >> 8;
		}
	}
	
	/* recover original configuration */
	writel(orig_config, HOST_CTL(priv));
	
	/* update delays (if needed) */
	if ((priv->curr_rdelay != rdelay) || (priv->curr_wdelay != wdelay)) {
		caninos_mmc_update_delays(host);
	}
	
	if (err) {
		caninos_mmc_wait_dma(host, data, 0U);
	}
	else {
		err = caninos_mmc_wait_dma(host, data, 20U);
	}
	return err;
}

static void caninos_mmc_request(struct mmc_host *host, struct mmc_request *mrq)
{
	struct mmc_command *stop = mrq->stop;
	struct mmc_command *cmd = mrq->cmd;
	struct mmc_data *data = mrq->data;
	int ret;
	
	ret = caninos_mmc_do_transfer(host, cmd, data);
	
	if (!ret && data) {
		data->bytes_xfered = data->blocks * data->blksz;
		if (stop) {
			stop->error = caninos_mmc_do_transfer(host, stop, NULL);
		}
	}
	if (cmd) {
		cmd->error = ret;
	}
	if (data) {
		data->error = ret;
	}
	mmc_request_done(host, mrq);
}

static int caninos_mmc_card_busy(struct mmc_host * mmc)
{
	struct caninos_mmc_host *priv = mmc_priv(mmc);
	u32 val = readl(HOST_STATE(priv));
	/* BUSY -> DAT0 line is kept low */
	return ((val & SD_STATE_DAT0S) == SD_STATE_DAT0S) ? 0 : 1;
}

static void caninos_mmc_set_scc(struct mmc_host *host, bool enable)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	u32 val;
	
	if (caninos_mmc_is_scc_en(host) == enable) {
		return; /* already enabled or disabled */
	}
	if (enable) {
		/* enable scc */
		val = readl(HOST_CTL(priv));
		val |= SD_CTL_SCC;
		writel(val, HOST_CTL(priv));
	}
	/* a transmission should be started before clearing the scc */
	/* enable transfer end interrupt */
	caninos_mmc_enable_disable_tei(host, true);
	reinit_completion(&priv->sdc_complete);
	
	/* start transmitting 16 clock cycles */
	val = readl(HOST_CTL(priv));
	val &= ~(SD_CTL_TCN(0xf) | SD_CTL_TM(0xf));
	val |= SD_CTL_TS | SD_CTL_TCN(1) | SD_CTL_TM(8);
	writel(val, HOST_CTL(priv));
	
	/* clear scc */
	val = readl(HOST_CTL(priv));
	val &= ~SD_CTL_SCC;
	writel(val, HOST_CTL(priv));
	
	/* wait for the transmission to finish */
	wait_for_completion_timeout(&priv->sdc_complete, msecs_to_jiffies(1U));
	
	/* disable transfer end interrupt */
	caninos_mmc_enable_disable_tei(host, true);
	caninos_mmc_stop_transmission(host);
	
	/* sanity check */
	if (caninos_mmc_is_scc_en(host)) {
		dev_err(priv->dev, "could not clear scc\n");
	}
}

static void caninos_mmc_en_low_voltage(struct mmc_host *host, bool enable)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	u32 new_val, val;
	
	val = readl(HOST_EN(priv));
	
	if (enable) {
		new_val = val | SD_EN_S18EN;
	}
	else {
		new_val = val & ~SD_EN_S18EN;
	}
	if (val != new_val)
	{
		if (caninos_mmc_is_scc_en(host)) { /* this must not happen */
			dev_err(priv->dev, "voltage switch with scc enabled\n");
		}
		writel(new_val, HOST_EN(priv));
		caninos_mmc_voltage_change_delay(host);
	}
}

static int caninos_mmc_vswitch(struct mmc_host *mmc, struct mmc_ios *ios)
{
	switch (ios->signal_voltage)
	{
	case MMC_SIGNAL_VOLTAGE_330:
		caninos_mmc_en_low_voltage(mmc, false);
		break;
	case MMC_SIGNAL_VOLTAGE_180:
		caninos_mmc_en_low_voltage(mmc, true);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void caninos_mmc_set_ios(struct mmc_host *host, struct mmc_ios *ios)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	u32 val, new_val;
	int bus_width;
	
	if (ios->power_mode == MMC_POWER_UP)
	{
		caninos_mmc_set_raw_clk(host, host->f_init);
		caninos_mmc_hard_reset(host);
		caninos_mmc_controller_enable(host);
		caninos_mmc_sdio_enable(host);
		caninos_mmc_reset_delays(host);
	}
	
	val = readl(HOST_EN(priv));
	new_val = SD_EN_DDREN | 0x3;
	new_val = val & ~(new_val);
	
	bus_width = ios->bus_width;
	
	if (ios->chip_select == MMC_CS_HIGH) {
		bus_width = MMC_BUS_WIDTH_4;
	}
	if ((ios->timing == MMC_TIMING_UHS_DDR50) || 
	    (ios->timing == MMC_TIMING_MMC_DDR52)) {
		new_val |= SD_EN_DDREN;
	}
	
	switch (bus_width)
	{
	case MMC_BUS_WIDTH_8:
		new_val |= 0x2;
		break;
		
	case MMC_BUS_WIDTH_4:
		new_val |= 0x1;
		break;
		
	case MMC_BUS_WIDTH_1:
	default:
		break;
	}
	
	if (new_val != val) {
		writel(new_val, HOST_EN(priv));
		usleep_range(1000, 1500);
	}
	if (ios->clock) {
		if (caninos_mmc_set_raw_clk(host, ios->clock)) {
			caninos_mmc_reset_delays(host);
		}
		caninos_mmc_set_scc(host, true);
	}
	else {
		caninos_mmc_set_scc(host, false);
	}
	
	if (ios->power_mode == MMC_POWER_ON)
	{
		if (!priv->device_powered)
		{
			/* power on the SD/SDIO/EMMC device (optional) */
			if (gpio_is_valid(priv->power_gpio)) {
				gpio_set_value(priv->power_gpio, 1);
				usleep_range(10000, 12500);
			}
			/* enable the SD/SDIO/EMMC device (optional) */
			if (gpio_is_valid(priv->enable_gpio)) {
				gpio_set_value(priv->enable_gpio, 1);
			}
			if (gpio_is_valid(priv->reset_gpio)) {
				gpio_set_value(priv->reset_gpio, 1);
			}
			priv->device_powered = true;
		}
	}
	
	if (ios->power_mode == MMC_POWER_OFF)
	{
		if (priv->device_powered)
		{
			/* disable the SD/SDIO/EMMC device (optional) */
			if (gpio_is_valid(priv->reset_gpio)) {
				gpio_set_value(priv->reset_gpio, 0);
			}
			if (gpio_is_valid(priv->enable_gpio)) {
				gpio_set_value(priv->enable_gpio, 0);
			}
			/* turn off the SD/SDIO/EMMC device (optional) */
			if (gpio_is_valid(priv->power_gpio)) {
				gpio_set_value(priv->power_gpio, 0);
			}
			priv->device_powered = false;
		}
	}
}

static irqreturn_t caninos_mmc_irq_handler(int irq, void *devid)
{
	struct caninos_mmc_host *priv = devid;
	unsigned long flags;
	u32 state;
	
	spin_lock_irqsave(&priv->lock, flags);
	
	state = readl(HOST_STATE(priv));
	writel(state, HOST_STATE(priv));
	
	spin_unlock_irqrestore(&priv->lock, flags);
	
	if ((state & SD_STATE_TEIE) && (state & SD_STATE_TEI)) {
		complete(&priv->sdc_complete);
	}
	if ((state & SD_STATE_SDIOA_P) && (state & SD_STATE_SDIOA_EN)) {
		mmc_signal_sdio_irq(priv->mmc);
	}
	return IRQ_HANDLED;
}

static int caninos_mmc_get_all_gpios(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	struct device *dev = priv->dev;
	int ret;
	
	priv->enable_gpio = of_get_named_gpio(dev->of_node, "enable-gpios", 0);
	
	if (priv->enable_gpio == -EPROBE_DEFER) {
		return -EPROBE_DEFER;
	}
	
	priv->power_gpio = of_get_named_gpio(dev->of_node, "power-gpios", 0);
	
	if (priv->power_gpio == -EPROBE_DEFER) {
		return -EPROBE_DEFER;
	}
	
	priv->reset_gpio = of_get_named_gpio(dev->of_node, "reset-gpios", 0);
	
	if (priv->reset_gpio == -EPROBE_DEFER) {
		return -EPROBE_DEFER;
	}
	
	if (gpio_is_valid(priv->enable_gpio))
	{
		ret = devm_gpio_request(dev, priv->enable_gpio, "enable_gpio");
		
		if (ret < 0) {
			dev_err(dev, "could not request enable gpio\n");
			return ret;
		}
	}
	
	if (gpio_is_valid(priv->power_gpio))
	{
		ret = devm_gpio_request(dev, priv->power_gpio, "power_gpio");
		
		if (ret < 0) {
			dev_err(dev, "could not request power gpio\n");
			return ret;
		}
	}
	
	if (gpio_is_valid(priv->reset_gpio))
	{
		ret = devm_gpio_request(dev, priv->reset_gpio, "reset_gpio");
		
		if (ret < 0) {
			dev_err(dev, "could not request reset gpio\n");
			return ret;
		}
	}
	return 0;
}

static int caninos_mmc_get_model_and_delays(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	const struct of_device_id *match;
	struct device *dev = priv->dev;
	int id, model;
	
	match = of_match_device(dev->driver->of_match_table, dev);
	model = (!match) ? (MMC_HW_MODEL_INV) : (int)((phys_addr_t)(match->data));
	
	switch (model)
	{
	case MMC_HW_MODEL_K5:
	case MMC_HW_MODEL_K7:
		priv->model = (enum mmc_hw_model)(model);
		break;
		
	default:
		dev_err(dev, "could not get hardware specific data\n");
		return -EINVAL;
	}
	
	id = of_alias_get_id(dev->of_node, "mmc");
	
	if ((id < 0) || (id > 2)) {
		dev_err(dev, "invalid device alias: %d\n", id);
		return -EINVAL;
	}
	
	priv->id = id;
	
	priv->wdelay.delay_lowclk  = 0xf;
	priv->wdelay.delay_midclk  = 0xa;
	priv->wdelay.delay_highclk = 0x6;
	
	priv->rdelay.delay_lowclk  = priv->wdelay.delay_lowclk;
	priv->rdelay.delay_midclk  = priv->wdelay.delay_midclk;
	priv->rdelay.delay_highclk = priv->wdelay.delay_highclk;
	return 0;
}

static int caninos_mmc_hardware_init(struct mmc_host *host)
{
	struct caninos_mmc_host *priv = mmc_priv(host);
	int ret;
	
	if (gpio_is_valid(priv->reset_gpio)) {
		gpio_direction_output(priv->reset_gpio, 0);
	}
	if (gpio_is_valid(priv->enable_gpio)) {
		gpio_direction_output(priv->enable_gpio, 0);
	}
	if (gpio_is_valid(priv->power_gpio)) {
		gpio_direction_output(priv->power_gpio, 0);
	}
	
	priv->device_powered = false;
	clk_prepare_enable(priv->clk);
	reset_control_deassert(priv->rst);
	caninos_mmc_disable_all_irqs(host);
	host->actual_clock = clk_get_rate(priv->clk);
	
	ret = devm_request_irq(priv->dev, priv->irq, caninos_mmc_irq_handler, 0,
	                       dev_name(priv->dev), priv);
	
	if (ret) {
		dev_err(priv->dev, "unable to request device irq: %d\n", priv->irq);
		reset_control_assert(priv->rst);
		clk_disable_unprepare(priv->clk);
	}
	return ret;
}

static int caninos_mmc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct caninos_mmc_host *priv;
	struct mmc_host *mmc;
	struct resource *res;
	struct dma_slave_config dma_conf;
	int ret;
	
	if (!pdev->dev.of_node) {
		dev_err(dev, "missing device OF node\n");
		return -ENODEV;
	}
	
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	
	if (ret) {
		dev_err(dev, "unable to set DMA mask\n");
		return ret;
	}
	
	mmc = mmc_alloc_host(sizeof(*priv), dev);
	
	if (!mmc) {
		dev_err(dev, "host allocation failed\n");
		return -ENOMEM;
	}
	
	priv = mmc_priv(mmc);
	priv->mmc = mmc;
	priv->dev = dev;
	
	spin_lock_init(&priv->lock);
	init_completion(&priv->dma_complete);
	init_completion(&priv->sdc_complete);
	platform_set_drvdata(pdev, priv);
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res) {
		dev_err(dev, "could not get device registers\n");
		mmc_free_host(mmc);
		return -ENXIO;
	}
	
	priv->irq = platform_get_irq(pdev, 0);
	
	if (priv->irq < 0) {
		dev_err(dev, "could not get device irq\n");
		mmc_free_host(mmc);
		return -ENXIO;
	}
	
	ret = caninos_mmc_get_model_and_delays(mmc);
	
	if (ret < 0) {
		mmc_free_host(mmc);
		return ret;
	}
	
	ret = caninos_mmc_get_all_gpios(mmc);
	
	if (ret < 0) {
		if (ret == -EPROBE_DEFER) {
			dev_info(dev, "gpio is not ready\n");
		}
		mmc_free_host(mmc);
		return ret;
	}
	
	priv->base = devm_ioremap(dev, res->start, resource_size(res));
	
	if (IS_ERR_OR_NULL(priv->base)) {
		dev_err(dev, "could not map device registers\n");
		mmc_free_host(mmc);
		return IS_ERR(priv->base) ? PTR_ERR(priv->base) : -ENOMEM;
	}
	
	priv->clk = devm_clk_get(dev, NULL);
	
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "could not get device clock\n");
		mmc_free_host(mmc);
		return PTR_ERR(priv->clk);
	}
	
	priv->rst = devm_reset_control_get(dev, NULL);
	
	if (IS_ERR(priv->rst)) {
		dev_err(dev, "could not get device reset control\n");
		mmc_free_host(mmc);
		return PTR_ERR(priv->rst);
	}
	
	priv->dma = dma_request_slave_channel(dev, "mmc");
	
	if (!priv->dma) {
	    dev_err(dev, "failed to request dma channel\n");
	    mmc_free_host(mmc);
		return -ENODEV;
	}
	
	dma_conf.src_addr = res->start + SD_DAT_OFFSET;
	dma_conf.dst_addr = res->start + SD_DAT_OFFSET;
	dma_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_conf.device_fc = false;
	
	if (dmaengine_slave_config(priv->dma, &dma_conf))
	{
		dev_err(dev, "unable to config dma channel\n");
		dma_release_channel(priv->dma);
		mmc_free_host(mmc);
		return -EINVAL;
	}
	
	mmc->f_min = 187500;
	mmc->f_max = 52000000;
	mmc->max_seg_size = 512 * 256;
	mmc->max_segs = 128;
	mmc->max_req_size = 512 * 256;
	mmc->max_blk_size = 512;
	mmc->max_blk_count = 256;
	
	mmc->ops = &priv->ops;
	
	mmc->ocr_avail  = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30;
	mmc->ocr_avail |= MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33;
	mmc->ocr_avail |= MMC_VDD_33_34 | MMC_VDD_34_35 | MMC_VDD_35_36;
	mmc->ocr_avail |= MMC_VDD_165_195;
	
	mmc->ocr_avail_mmc = mmc->ocr_avail;
	mmc->ocr_avail_sd = mmc->ocr_avail;
	
	mmc->caps  = MMC_CAP_4_BIT_DATA;
	mmc->caps |= MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED;
	mmc->caps |= MMC_CAP_ERASE | MMC_CAP_NEEDS_POLL;
	mmc->caps |= MMC_CAP_UHS_SDR12 | MMC_CAP_SDIO_IRQ;
	mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY;
	
	mmc->caps2 = MMC_CAP2_NO_WRITE_PROTECT | MMC_CAP2_BOOTPART_NOACC;
	
	if (priv->model == MMC_HW_MODEL_K7) {
		mmc->caps |= MMC_CAP_1_8V_DDR | MMC_CAP_8_BIT_DATA;
		mmc->caps |= MMC_CAP_UHS_DDR50 | MMC_CAP_UHS_SDR25;
	}
	
	priv->ops.request   = caninos_mmc_request;
	priv->ops.set_ios   = caninos_mmc_set_ios;
	priv->ops.card_busy = caninos_mmc_card_busy;
	
	priv->ops.start_signal_voltage_switch = caninos_mmc_vswitch;
	priv->ops.enable_sdio_irq = caninos_mmc_en_sdio_irq;
	
	ret = caninos_mmc_hardware_init(mmc);
	
	if (ret) {
		dma_release_channel(priv->dma);
		mmc_free_host(mmc);
		return ret;
	}
	
	ret = mmc_add_host(mmc);
	
	if (ret)
	{
		dev_err(dev, "could not add mmc host\n");
		dma_release_channel(priv->dma);
		mmc_free_host(mmc);
		devm_free_irq(priv->dev, priv->irq, priv);
		reset_control_assert(priv->rst);
		clk_disable_unprepare(priv->clk);
		return ret;
	}
	
	dev_info(dev, "probe finished\n");
	return 0;
}

static int caninos_mmc_remove(struct platform_device *pdev)
{
	struct caninos_mmc_host *priv = platform_get_drvdata(pdev);
	struct mmc_host *host = priv->mmc;
	mmc_remove_host(host);
	dma_release_channel(priv->dma);
	mmc_free_host(host);
	devm_free_irq(priv->dev, priv->irq, priv);
	reset_control_assert(priv->rst);
	clk_disable_unprepare(priv->clk);
	return 0;
}

static const struct of_device_id caninos_mmc_of_match[] = {
	{ .compatible = "caninos,k7-mmc", .data = (void*)(MMC_HW_MODEL_K7), },
	{ .compatible = "caninos,k5-mmc", .data = (void*)(MMC_HW_MODEL_K5), },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, caninos_mmc_of_match);

static struct platform_driver caninos_mmc_driver = {
	.driver = {
		.name = "caninos-mmc",
		.of_match_table = caninos_mmc_of_match,
		.owner = THIS_MODULE,
	},
	.probe  = caninos_mmc_probe,
	.remove = caninos_mmc_remove,
};

static int __init caninos_driver_init(void) {
	return platform_driver_register(&caninos_mmc_driver);
}

static void __exit caninos_driver_exit(void) {
	platform_driver_unregister(&caninos_mmc_driver);
}

module_init(caninos_driver_init);
module_exit(caninos_driver_exit);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("Caninos Labrador MMC Host Controller Driver");
MODULE_LICENSE("GPL v2");
