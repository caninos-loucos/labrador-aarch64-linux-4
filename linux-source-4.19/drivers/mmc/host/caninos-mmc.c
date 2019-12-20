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
#include "caninos-mmc.h"

#define DRIVER_NAME "caninos-mmc"
#define DRIVER_DESC "Caninos Labrador MMC Controller Driver"

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
	
	void __iomem *base;
	
	spinlock_t lock;
	struct mutex pin_mutex;
	
	u32 id;
	
	unsigned long clock;
	
	int bus_width;
	int power_state;
	int chip_select;
	
	char write_delay_chain;
	char read_delay_chain;
	char write_delay_chain_bak;
	char read_delay_chain_bak;
	char adjust_write_delay_chain;
	char adjust_read_delay_chain;
	
	struct mmc_request *mrq;
	
	int irq;
	int power_gpio;
	int enable_gpio;
	int reset_gpio;
	
	int clk_on;
	
	struct con_delay wdelay;
	struct con_delay rdelay;
	
	struct clk *clk;
	struct pinctrl *pcl;
	
	struct dma_chan	*dma;
	enum dma_data_direction dma_dir;
	struct dma_async_tx_descriptor *desc;
	struct dma_slave_config dma_conf;
	
	struct completion dma_complete;
	struct completion sdc_complete;
	
	struct reset_control *rst;
};

static const struct of_device_id caninos_mmc_of_match[] = {
	{ .compatible = "caninos,k7-mmc", },
	{ }
};

static void hard_reset_controller(struct caninos_mmc_host *host)
{
	u32 sden, sdctl, sdstate;
	
	sden = readl(HOST_EN(host));
	sdctl = readl(HOST_CTL(host)) & ~(SD_CTL_TS);
	sdstate = readl(HOST_STATE(host));
	mb();
	
	reset_control_assert(host->rst);
	udelay(20);
	reset_control_deassert(host->rst);
	
	writel(SD_ENABLE, HOST_EN(host));
	writel(sden, HOST_EN(host));
	writel(sdctl, HOST_CTL(host));
	writel(sdstate, HOST_STATE(host));
}

static void stop_pending_transfers(struct caninos_mmc_host *host)
{
	int timeout;
	u32 sdctl;
	
	for (timeout = 100; timeout > 0; timeout--)
	{
		/* stop pending transfer */
		writel(readl(HOST_CTL(host)) & ~(SD_CTL_TS), HOST_CTL(host));
		mb();
		
		udelay(10);
		
		/* did it stop ? */
		sdctl = readl(HOST_CTL(host));
		mb();
		
		if (!(sdctl & SD_CTL_TS)) {
			return;
		}
	}
	
	/* it timedout, so hard reset the controller because it may be stuck */
	hard_reset_controller(host);
}

static void sdc_dma_complete(void *param)
{
	struct caninos_mmc_host *host = (struct caninos_mmc_host *)param;
	
	if (host) {
		complete(&host->dma_complete);
	}
}

static int prepare_data(struct caninos_mmc_host *host, struct mmc_data *data)
{
	enum dma_transfer_direction slave_dirn;
	int sglen, total;
	
	/* use DMA channel */
	writel(readl(HOST_EN(host)) | SD_EN_BSEL, HOST_EN(host));
	mb();
	
	/* set block size and count */
	writel(data->blksz, HOST_BLK_SIZE(host));
	writel(data->blocks, HOST_BLK_NUM(host));
	mb();
	
	total = (data->blksz * data->blocks);
	
	/* set total size */
	if (total < 512)
	{
		writel(total, HOST_BUF_SIZE(host));
		mb();
	}
	else
	{
		writel(512, HOST_BUF_SIZE(host));
		mb();
	}
	
	if (data->flags & MMC_DATA_READ)
	{
		host->dma_dir = DMA_FROM_DEVICE;
		host->dma_conf.direction = slave_dirn = DMA_DEV_TO_MEM;
	}
	else
	{
		host->dma_dir = DMA_TO_DEVICE;
		host->dma_conf.direction = slave_dirn = DMA_MEM_TO_DEV;
	}
	
	sglen = dma_map_sg(host->dma->device->dev,
	                   data->sg, data->sg_len, host->dma_dir);
	
	if (dmaengine_slave_config(host->dma, &host->dma_conf))
	{
		dma_unmap_sg(host->dma->device->dev,
		             data->sg, data->sg_len, host->dma_dir);
		return -EINVAL;
	}
	
	host->desc = dmaengine_prep_slave_sg(host->dma, data->sg, sglen, slave_dirn,
	                                     DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	
	if (!host->desc)
	{
		dma_unmap_sg(host->dma->device->dev,
		             data->sg, data->sg_len, host->dma_dir);
		return -EBUSY;
	}
	
	host->desc->callback = sdc_dma_complete;
	host->desc->callback_param = host;
	data->error = 0;
	return 0;
}

static int wait_for_dma_transfer_completion(struct caninos_mmc_host *host)
{
	const unsigned long timeout = msecs_to_jiffies(1000);
	if (!wait_for_completion_timeout(&host->dma_complete, timeout)) {
		return -1;
	}
	return 0;
}

static void dma_transfer_start(struct caninos_mmc_host *host)
{
	reinit_completion(&host->dma_complete);
	dmaengine_submit(host->desc);
	dma_async_issue_pending(host->dma);
}

static void dma_transfer_force_termination(struct caninos_mmc_host *host)
{
	dmaengine_terminate_all(host->dma);
}

static void enable_transfer_end_interrupt(struct caninos_mmc_host *host)
{
	unsigned long flags;
	u32 state;
	
	spin_lock_irqsave(&host->lock, flags);

	/* clear transfer end interrupt and enable it */
	state = readl(HOST_STATE(host));
	rmb();
	state &= ~SD_STATE_SDIOA_P; /* protect sdio irq from being discarded */
	state |= SD_STATE_TEI;
	state |= SD_STATE_TEIE;
	writel(state, HOST_STATE(host));
	wmb();
	
	/* clear completion */
	reinit_completion(&host->sdc_complete);
	
	spin_unlock_irqrestore(&host->lock, flags);
}

static void disable_transfer_end_interrupt(struct caninos_mmc_host *host)
{
	unsigned long flags;
	u32 state;
	
	spin_lock_irqsave(&host->lock, flags);
	
	/* disable transfer end interrupt and clear it */
	state = readl(HOST_STATE(host));
	rmb();
	state &= ~SD_STATE_SDIOA_P; /* protect sdio irq from being discarded */
	state |= SD_STATE_TEI;
	state &= ~SD_STATE_TEIE;
	writel(state, HOST_STATE(host));
	wmb();
	
	spin_unlock_irqrestore(&host->lock, flags);
}

static int sdc_send_cmd(struct caninos_mmc_host *host,
	struct mmc_command *cmd, struct mmc_data *data)
{
	u32 mode, rsp[2], status, cmd_rsp_mask = 0;
	int timeout_us, ret;
	
	if (!cmd)
	{
		cmd->error = -EINVAL;
		if (data) {
			data->error = cmd->error;
		}
		return -1;
	}
	
	cmd->error = 0;
	
    switch (mmc_resp_type(cmd))
	{
	case MMC_RSP_NONE:
		mode = SD_CTL_TM(0);
		break;
		
	case MMC_RSP_R1:
		if (data)
		{
			if (data->flags & MMC_DATA_READ) {
				mode = SD_CTL_TM(4);
			}
			else {
				mode = SD_CTL_TM(5);
			}
		}
		else {
			mode = SD_CTL_TM(1);
		}
		cmd_rsp_mask = SD_STATE_CLNR | SD_STATE_CRC7ER;
		break;

	case MMC_RSP_R1B:
		mode = SD_CTL_TM(3);
		cmd_rsp_mask = SD_STATE_CLNR | SD_STATE_CRC7ER;
		break;
		
	case MMC_RSP_R2:
		mode = SD_CTL_TM(2);
		cmd_rsp_mask = SD_STATE_CLNR | SD_STATE_CRC7ER;
		break;
		
	case MMC_RSP_R3:
		mode = SD_CTL_TM(1);
		cmd_rsp_mask = SD_STATE_CLNR;
		break;
		
	default:
		cmd->error = -EINVAL;
		if (data) {
			data->error = cmd->error;
		}
		return -1;
	}
	
    /* keep current RDELAY & WDELAY value */
	mode |= (readl(HOST_CTL(host)) & (0xff << 16));
	
	/* start to send corresponding command type */
	writel(cmd->arg, HOST_ARG(host));
	writel(cmd->opcode, HOST_CMD(host));
	mb();
	
	if (data)
	{
		ret = prepare_data(host, data);
		
		if (ret)
		{
			data->error = ret;
			return -1;
		}
		
		/* set lbe to send clk after busy */
		mode |= SD_CTL_LBE;
		
		/* start dma transfer */
		dma_transfer_start(host);
	}
	
	mode |= SD_CTL_TS;
	
	/* clear completions and enable interrupts */
	enable_transfer_end_interrupt(host);
	
	/* start transfer */
	writel(mode, HOST_CTL(host));
	mb();
	
	if (data)
	{
		cmd->error = data->error = 0;
		timeout_us = data->timeout_ns / 1000;
		
		if (timeout_us < 100000) {
			timeout_us = 100000;
		}
		
		while (timeout_us > 0)
		{
			/* wait for SDC transfer to complete */
			if (completion_done(&host->sdc_complete)) {
				break;
			}
			udelay(10);
			timeout_us -= 10;
		}
		
		
		status = readl(HOST_STATE(host));
		mb();
			
		if (status & SD_STATE_CLNR)
		{
			dev_err(host->dev, "CLNR error\n");
			cmd->error = data->error = -EILSEQ;
		}
		else if (status & SD_STATE_WC16ER)
		{
			dev_err(host->dev, "WC16 error\n");
			cmd->error = data->error = -EILSEQ;
		}
		else if (status & SD_STATE_RC16ER)
		{
			dev_err(host->dev, "RC16 error\n");
			cmd->error = data->error = -EILSEQ;
		}
		else if (status & SD_STATE_CRC7ER)
		{
			dev_err(host->dev, "CRC7 error\n");
			cmd->error = data->error = -EILSEQ;
		}
		else if (timeout_us <= 0)
		{
			dev_err(host->dev, "SDC error\n");
			hard_reset_controller(host);
			cmd->error = data->error = -ETIMEDOUT;
		}
		
		if (wait_for_dma_transfer_completion(host))
		{
			dev_err(host->dev, "dma timedout\n");
			dma_transfer_force_termination(host);
			cmd->error = data->error = -ETIMEDOUT;
		}
		
		dma_unmap_sg(host->dma->device->dev,
		             data->sg, data->sg_len, host->dma_dir);
		
		disable_transfer_end_interrupt(host);
		
		if (data->error) {
			return -1;
		}
		
		data->bytes_xfered = data->blocks * data->blksz;
	}
	else
	{
		if (!wait_for_completion_timeout(&host->sdc_complete,
		                                 msecs_to_jiffies(1000)))
		{
			cmd->error = -ETIMEDOUT;
			disable_transfer_end_interrupt(host);
			return -1;
		}
		
		disable_transfer_end_interrupt(host);
		
		status = readl(HOST_STATE(host));
		mb();
		
		if ((cmd->flags & MMC_RSP_PRESENT) && (cmd_rsp_mask & status))
		{
			if (status & SD_STATE_CLNR)
			{
				cmd->error = -EILSEQ;
				return -1;
			}
			if (status & SD_STATE_CRC7ER)
			{
				cmd->error = -EILSEQ;
				return -1;
			}
		}
	}
	
	if (cmd->flags & MMC_RSP_PRESENT)
	{
		if (cmd->flags & MMC_RSP_136)
		{
			cmd->resp[3] = readl(HOST_RSPBUF0(host));
			cmd->resp[2] = readl(HOST_RSPBUF1(host));
			cmd->resp[1] = readl(HOST_RSPBUF2(host));
			cmd->resp[0] = readl(HOST_RSPBUF3(host));
			mb();
		}
		else
		{
			rsp[0] = readl(HOST_RSPBUF0(host));
			rsp[1] = readl(HOST_RSPBUF1(host));
			mb();
			
			cmd->resp[0] = rsp[1] << 24 | rsp[0] >> 8;
			cmd->resp[1] = rsp[1] >> 8;
		}
	}
	
    return 0;
}



static int sdc_setup_timing(struct mmc_host * mmc, u32 rate)
{
	struct caninos_mmc_host *host = mmc_priv(mmc);
	
	/* Set the RDELAY and WDELAY based on the new clock. */
	
	u32 val = readl(HOST_CTL(host)) & ~(0xff << 16);
	rmb();
	
	if (rate <= 1000000)
	{
		val |= SD_CTL_RDELAY(host->rdelay.delay_lowclk);
		val |= SD_CTL_WDELAY(host->wdelay.delay_lowclk);
	}
	else if ((rate > 1000000) && (rate <= 26000000))
	{
		val |= SD_CTL_RDELAY(host->rdelay.delay_midclk);
		val |= SD_CTL_WDELAY(host->wdelay.delay_midclk);
	}
	else if ((rate > 26000000) && (rate <= 52000000))
	{
		val |= SD_CTL_RDELAY(host->rdelay.delay_highclk);
		val |= SD_CTL_WDELAY(host->wdelay.delay_highclk);
	}
	else {
		return -1;
	}
	
	writel(val, HOST_CTL(host));
	wmb();
	return 0;
}

static void sdc_power_up(struct mmc_host * mmc)
{
	struct caninos_mmc_host *host = mmc_priv(mmc);
	u32 mode;
	
	host->clock = mmc->f_init;
	host->bus_width = MMC_BUS_WIDTH_1;
	
	reset_control_assert(host->rst);
	
	clk_prepare_enable(host->clk);
	
	if (gpio_is_valid(host->power_gpio)) {
		gpio_set_value(host->power_gpio, 1);
	}
	
	mdelay(10);
	
	clk_set_rate(host->clk, host->clock);
	
	if (gpio_is_valid(host->enable_gpio)) {
		gpio_set_value(host->enable_gpio, 1);
	}

	mdelay(10);

	if (gpio_is_valid(host->reset_gpio)) {
		gpio_set_value(host->reset_gpio, 1);
		mdelay(10);
		gpio_set_value(host->reset_gpio, 0);
		mdelay(10);
		gpio_set_value(host->reset_gpio, 1);
	}
	
	
	reset_control_deassert(host->rst);
	
	/* enable and reset the SD controller state machine */
	writel(SD_ENABLE | SD_EN_RESE, HOST_EN(host));
	mb();
	
	/* enable sdio function */
	writel(readl(HOST_EN(host)) | SD_EN_SDIOEN, HOST_EN(host));
	mb();
	
	/* set bus width to 1 bit */
	writel(readl(HOST_EN(host)) & ~(0x3), HOST_EN(host));
	mb();
	
	/* setup bus delays */
	sdc_setup_timing(mmc, host->clock);
	
	/* clear completions and enable transfer end interrupt */
	enable_transfer_end_interrupt(host);
	
    /* send 80 clocks */
	mode = SD_CTL_TS  | SD_CTL_TCN(5) | SD_CTL_TM(8);
	mode |= (readl(HOST_CTL(host)) & (0xff << 16));
	writel(mode, HOST_CTL(host));
	mb();
	
	if (!wait_for_completion_timeout(&host->sdc_complete, msecs_to_jiffies(2))){
		stop_pending_transfers(host);
	}
	
	disable_transfer_end_interrupt(host);
}

static void sdc_power_off(struct mmc_host * mmc)
{
	struct caninos_mmc_host *host = mmc_priv(mmc);
	
	host->clock = 0;
	host->bus_width = 0;
	
	clk_disable_unprepare(host->clk);
	
	if (gpio_is_valid(host->enable_gpio)) {
		gpio_set_value(host->enable_gpio, 0);
	}
	
    if (gpio_is_valid(host->power_gpio)) {
		gpio_set_value(host->power_gpio, 0);
	}

	if (gpio_is_valid(host->reset_gpio)) {
		gpio_set_value(host->reset_gpio, 0);
	}
}

static void sdc_set_clk(struct mmc_host * mmc, u32 clock)
{
	struct caninos_mmc_host *host = mmc_priv(mmc);
	unsigned long rate;
	
	if (clock == 0) {
		return;
	}
	
	if (clock != host->clock)
	{
		rate = clk_round_rate(host->clk, clock << 1);
		
		sdc_setup_timing(mmc, rate >> 1);
		
		clk_set_rate(host->clk, rate);
		
		host->clock = clock;
	}
}

static void sdc_set_bus_width(struct mmc_host * mmc, u32 bus_width)
{
	struct caninos_mmc_host *host = mmc_priv(mmc);
	u32 val;
	
	if (bus_width != host->bus_width)
	{
		val = readl(HOST_EN(host));
		rmb();
		
		switch (bus_width)
		{
		case MMC_BUS_WIDTH_8:
			val &= ~(0x3);
			val |= 0x2;
			break;
			
		case MMC_BUS_WIDTH_4:
			val &= ~(0x3);
			val |= 0x1;
			break;
			
		case MMC_BUS_WIDTH_1:
			val &= ~(0x3);
			break;
			
		default:
			return;
		}
		
		writel(val, HOST_EN(host));
		
		host->bus_width = bus_width;
		wmb();
	}
}










static void caninos_mmc_en_sdio_irq(struct mmc_host * mmc, int enable)
{
	struct caninos_mmc_host *host = mmc_priv(mmc);
	unsigned long flags;
	u32 state;
	
	spin_lock_irqsave(&host->lock, flags);
	
	state = readl(HOST_STATE(host));
	
	if (enable)
	{
		/* enable sdio interrupt without messing up with pending ones */
		state |= SD_STATE_SDIOA_EN;
		/* protect pending interrupts */
		state &= ~SD_STATE_SDIOA_P;
		state &= ~SD_STATE_TEI;
	}
	else
	{
		/* disable sdio interrupt without messing up with pending ones */
		state |= SD_STATE_SDIOA_P;
		state &= ~SD_STATE_SDIOA_EN;
		state &= ~SD_STATE_TEI;
	}
	
	writel(state, HOST_STATE(host));
	
	spin_unlock_irqrestore(&host->lock, flags);
}

static int caninos_check_trs_date_status
	(struct caninos_mmc_host *host, struct mmc_request *mrq)
{
	struct mmc_data *data = mrq->data;
	struct mmc_command *cmd = mrq->cmd;
	u32 status = readl(HOST_STATE(host));
	u32 check_status = 0;
	u32 cmd_rsp_mask = 0;
	
	if (!host || !host->mrq || !host->mrq->data) {
		BUG_ON(1);
		return -1;
	}
	
	cmd_rsp_mask = SD_STATE_TOUTE
	    | SD_STATE_CLNR
	    | SD_STATE_WC16ER | SD_STATE_RC16ER | SD_STATE_CRC7ER;
	
	check_status = status & cmd_rsp_mask;
	
	if (check_status) {
		if (check_status & SD_STATE_TOUTE) {
			data->error = HW_TIMEOUT_ERR;
			goto out;

		}
		if (check_status & SD_STATE_CLNR) {
			data->error = CMD_RSP_ERR;
			goto out;
		}
		if (check_status & SD_STATE_WC16ER) {

			data->error = DATA_WR_CRC_ERR;
			cmd->error = -EILSEQ;
			goto out;
		}
		if (check_status & SD_STATE_RC16ER) {
			data->error = DATA_RD_CRC_ERR;
			cmd->error = -EILSEQ;
			goto out;
		}
		if (check_status & SD_STATE_CRC7ER) {
			data->error = CMD_RSP_CRC_ERR;
			goto out;
		}
	}
	
out:
	if (data->error == DATA_RD_CRC_ERR) {
		if(host->read_delay_chain > 0 )
			host->read_delay_chain--;
		else
			host->read_delay_chain = 0xf;
		printk("try read delay chain:%d\n", host->read_delay_chain);

	} else if (data->error == DATA_WR_CRC_ERR) {

		if (host->write_delay_chain > 0)
			host->write_delay_chain--;
		else
			host->write_delay_chain = 0xf;


		printk("try write delay chain:%d\n", host->write_delay_chain);
	}
	
	if (data->error == DATA_WR_CRC_ERR || data->error == DATA_RD_CRC_ERR) {
		writel((readl(HOST_CTL(host)) & (~(0xff << 16))) |
		       SD_CTL_RDELAY(host->read_delay_chain) |
		       SD_CTL_WDELAY(host->write_delay_chain), HOST_CTL(host));
	}
	
	return data->error;
}

static void caninos_mmc_dma_complete(void *dma_async_param)
{
	struct caninos_mmc_host *host = (struct caninos_mmc_host *)dma_async_param;



	if (host->mrq->data) {
		complete(&host->dma_complete);
	}
}

static int caninos_mmc_prepare_data(struct caninos_mmc_host *host, struct mmc_data *data)
{
	struct scatterlist *sg;
	enum dma_transfer_direction slave_dirn;
	int i, sglen;
	unsigned total;
	
	writel(readl(HOST_EN(host)) | SD_EN_BSEL, HOST_EN(host));
	
	writel(data->blocks, HOST_BLK_NUM(host));
	writel(data->blksz, HOST_BLK_SIZE(host));
	
	total = data->blksz * data->blocks;

	if (total < 512)
	{
		writel(total, HOST_BUF_SIZE(host));
	}
	else
	{
		writel(512, HOST_BUF_SIZE(host));
	}

	/*
	 * We don't do DMA on "complex" transfers, i.e. with
	 * non-word-aligned buffers or lengths.
	 */
	for_each_sg(data->sg, sg, data->sg_len, i) {
		if (sg->offset & 3 || sg->length & 3)
			pr_err("SD tag: non-word-aligned buffers or lengths.\n");
	}

	if (data->flags & MMC_DATA_READ) {
		host->dma_dir = DMA_FROM_DEVICE;
		host->dma_conf.direction = slave_dirn = DMA_DEV_TO_MEM;
	} else if (data->flags & MMC_DATA_WRITE) {
		host->dma_dir = DMA_TO_DEVICE;
		host->dma_conf.direction = slave_dirn = DMA_MEM_TO_DEV;
	} else {
		BUG_ON(1);
	}

	sglen = dma_map_sg(host->dma->device->dev, data->sg,
			   data->sg_len, host->dma_dir);

	if (dmaengine_slave_config(host->dma, &host->dma_conf)) {
		pr_err("Failed to config DMA channel\n");
	}

	host->desc = dmaengine_prep_slave_sg(host->dma,
					     data->sg, sglen, slave_dirn,
					     DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!host->desc) {
		pr_err("dmaengine_prep_slave_sg() fail\n");
		return -EBUSY;
	}

	host->desc->callback = caninos_mmc_dma_complete;
	host->desc->callback_param = host;

	data->error = 0;

	return 0;
}

static void caninos_mmc_finish_request(struct caninos_mmc_host *host)
{
	struct mmc_request *mrq;
	struct mmc_data *data;
	
	mrq = host->mrq;
	host->mrq = NULL;
	
	if (mrq->data)
	{
		data = mrq->data;
		
		if (host->dma) {
			dma_unmap_sg(host->dma->device->dev, data->sg, data->sg_len, host->dma_dir);
		}
	}
	
	mmc_request_done(host->mmc, mrq);
}

static int caninos_mmc_send_command(struct caninos_mmc_host *host, struct mmc_command *cmd, struct mmc_data *data)
{
	u32 mode;
	u32 rsp[2];
	unsigned int cmd_rsp_mask = 0;
	u32 status;

	cmd->error = 0;
	reinit_completion(&host->sdc_complete);
	
	switch (mmc_resp_type(cmd))
	{
	case MMC_RSP_NONE:
		mode = SD_CTL_TM(0);
		break;

	case MMC_RSP_R1:
		if (data) {
			if (data->flags & MMC_DATA_READ)
				mode = SD_CTL_TM(4);
			else
				mode = SD_CTL_TM(5);
		} else {
			mode = SD_CTL_TM(1);
		}
		cmd_rsp_mask = SD_STATE_CLNR | SD_STATE_CRC7ER;

		break;

	case MMC_RSP_R1B:
		mode = SD_CTL_TM(3);
		cmd_rsp_mask = SD_STATE_CLNR | SD_STATE_CRC7ER;
		break;

	case MMC_RSP_R2:
		mode = SD_CTL_TM(2);
		cmd_rsp_mask = SD_STATE_CLNR | SD_STATE_CRC7ER;
		break;

	case MMC_RSP_R3:
		mode = SD_CTL_TM(1);
		cmd_rsp_mask = SD_STATE_CLNR;
		break;

	default:
		cmd->error = -1;
		BUG_ON(cmd->error);
		return MMC_CMD_COMPLETE;
	}

	/* keep current RDELAY & WDELAY value */
	mode |= (readl(HOST_CTL(host)) & (0xff << 16));

	/* start to send corresponding command type */
	writel(cmd->arg, HOST_ARG(host));
	writel(cmd->opcode, HOST_CMD(host));


	/*set lbe to send clk after busy */
	if (data)
	{
		/* enable HW tiemout, use sw timeout */
		mode |= (SD_CTL_TS | SD_CTL_LBE | HW_TIMEOUT);
		
	} else {
		/*pure cmd disable hw timeout and SD_CTL_LBE */
		mode &= ~(SD_CTL_TOUTEN | SD_CTL_LBE);
		mode |= SD_CTL_TS;
	}

	/* start transfer */
	writel(mode, HOST_CTL(host));
	

	/* data cmd return */
	if (data) {
		return DATA_CMD;
	}
	
	if (!wait_for_completion_timeout(&host->sdc_complete, msecs_to_jiffies(1000)))
	{
		cmd->error = -ETIMEDOUT;
		goto out;
	}
	
	status = readl(HOST_STATE(host));

	if (cmd->flags & MMC_RSP_PRESENT)
	{
		if (cmd_rsp_mask & status)
		{
			if (status & SD_STATE_CLNR)
			{
				cmd->error = -EILSEQ;
				goto out;
			}

			if (status & SD_STATE_CRC7ER)
			{
				cmd->error = -EILSEQ;
				goto out;
			}

		}

		if (cmd->flags & MMC_RSP_136)
		{
			cmd->resp[3] = readl(HOST_RSPBUF0(host));
			cmd->resp[2] = readl(HOST_RSPBUF1(host));
			cmd->resp[1] = readl(HOST_RSPBUF2(host));
			cmd->resp[0] = readl(HOST_RSPBUF3(host));
		}
		else
		{
			rsp[0] = readl(HOST_RSPBUF0(host));
			rsp[1] = readl(HOST_RSPBUF1(host));
			cmd->resp[0] = rsp[1] << 24 | rsp[0] >> 8;
			cmd->resp[1] = rsp[1] >> 8;
		}
	}

out:
	return PURE_CMD;
}

void caninos_mmc_ctr_reset(struct caninos_mmc_host *host)
{
	reset_control_assert(host->rst);
	udelay(20);
	reset_control_deassert(host->rst);
}

static void caninos_mmc_err_reset(struct caninos_mmc_host *host)
{
	u32 reg_en, reg_ctr, reg_state;

	reg_en = readl(HOST_EN(host));
	reg_ctr = readl(HOST_CTL(host));
	reg_state = readl(HOST_STATE(host));

	caninos_mmc_ctr_reset(host);

	writel(SD_ENABLE, HOST_EN(host));
	writel(reg_en, HOST_EN(host));
	reg_ctr &= ~SD_CTL_TS;
	writel(reg_ctr, HOST_CTL(host));
	writel(reg_state, HOST_STATE(host));
}

static void caninos_mmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct caninos_mmc_host *host = mmc_priv(mmc);
	int ret = 0;
	
	host->mrq = mrq;
	
	if (mrq->data)
	{
		ret = caninos_mmc_prepare_data(host, mrq->data);
		
		if (ret != 0)
		{
			mrq->data->error = ret;
			caninos_mmc_finish_request(host);
			return;
		}
		else
		{
			reinit_completion(&host->dma_complete);
			dmaengine_submit(host->desc);
			dma_async_issue_pending(host->dma);
		}
	}
	
	ret = caninos_mmc_send_command(host, mrq->cmd, mrq->data);
	
	if (ret == DATA_CMD)
	{
		int timeout_us = mrq->data->timeout_ns / 1000;
		
		if (timeout_us < 100000) {
			timeout_us = 100000;
		}
		
		while (timeout_us > 0)
		{
			/* wait for SDC transfer to complete */
			
			if (wait_for_completion_timeout(&host->sdc_complete, msecs_to_jiffies(1)))
			{
				break;
			}
			
			timeout_us -= 1000;
		}
		
		if (caninos_check_trs_date_status(host, mrq))
		{
			if (host->dma) {
				dmaengine_terminate_all(host->dma);
			}
			
			caninos_mmc_err_reset(host);
			goto finish;
		}
		
		if (!wait_for_completion_timeout(&host->dma_complete, msecs_to_jiffies(1000)))
		{
			mrq->data->error = CMD_DATA_TIMEOUT;
			mrq->cmd->error = -ETIMEDOUT;
			
			if (host->dma) {
				dmaengine_terminate_all(host->dma);
			}
			
			caninos_mmc_err_reset(host);
			goto finish;
		}
		
		if (mrq->data->stop)
		{
			caninos_mmc_send_command(host, mrq->data->stop, NULL);
			
			if (mrq->data->stop->error)
			{
				caninos_mmc_err_reset(host);
				goto finish;
			}
		}
		mrq->data->bytes_xfered = mrq->data->blocks * mrq->data->blksz;
	}
	
finish:
	caninos_mmc_finish_request(host);
}

static int caninos_mmc_clk_set_rate(struct caninos_mmc_host *host, unsigned long freq)
{
	unsigned long rate;
	int ret;
	freq = freq << 1;
	rate = clk_round_rate(host->clk, freq);
	
	if (rate < 0) {
		return -ENXIO;
	}
	
	ret = clk_set_rate(host->clk, rate);
	
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static void caninos_mmc_set_clk(struct caninos_mmc_host *host, int rate)
{
	if (0 == rate) {
		return;
	}
	
	/*
	 * Set the RDELAY and WDELAY based on the sd clk.
	 */
	if (rate <= 1000000)
	{
		writel((readl(HOST_CTL(host)) & (~(0xff << 16))) |
		       SD_CTL_RDELAY(host->rdelay.delay_lowclk) |
		       SD_CTL_WDELAY(host->wdelay.delay_lowclk),
		       HOST_CTL(host));
	}
	else if ((rate > 1000000) && (rate <= 26000000)) {
		writel((readl(HOST_CTL(host)) & (~(0xff << 16))) |
		       SD_CTL_RDELAY(host->rdelay.delay_midclk) |
		       SD_CTL_WDELAY(host->wdelay.delay_midclk),
		       HOST_CTL(host));
	}
	else if ((rate > 26000000) && (rate <= 52000000)) {
		
			writel((readl(HOST_CTL(host)) & (~(0xff << 16))) |
			       SD_CTL_RDELAY(host->rdelay.delay_highclk) |
			       SD_CTL_WDELAY(host->wdelay.delay_highclk),
			       HOST_CTL(host));

	}
	
	host->read_delay_chain = (readl(HOST_CTL(host)) & (0xf << 20)) >> 20;
	host->write_delay_chain = (readl(HOST_CTL(host)) & (0xf << 16)) >> 16;
	host->write_delay_chain_bak = host->write_delay_chain;
	host->read_delay_chain_bak = host->read_delay_chain;
	
	caninos_mmc_clk_set_rate(host, rate);
}

static void caninos_mmc_power_off(struct caninos_mmc_host *host)
{
	if (host->clk_on)
	{
		clk_disable_unprepare(host->clk);
		host->clk_on = 0;
	}
	
	if (gpio_is_valid(host->enable_gpio)) {
		gpio_set_value(host->enable_gpio, 0);
	}
	
    if (gpio_is_valid(host->power_gpio)) {
		gpio_set_value(host->power_gpio, 0);
	}

	if (gpio_is_valid(host->reset_gpio)) {
		gpio_set_value(host->reset_gpio, 0);
	}
}

static void caninos_mmc_power_up(struct caninos_mmc_host *host)
{
	int ret;
	
	if (!host->clk_on)
	{
		ret = clk_prepare_enable(host->clk);
		
		if (ret) {
			return ret;
		}
		
		host->clk_on = 1;
	}
	
	if (gpio_is_valid(host->power_gpio)) {
		gpio_set_value(host->power_gpio, 1);
	}
	
	caninos_mmc_ctr_reset(host);
	
	writel(SD_ENABLE | SD_EN_RESE, HOST_EN(host));
}

static int caninos_mmc_send_init_clk(struct caninos_mmc_host *host)
{
	u32 mode;
	
	reinit_completion(&host->sdc_complete);
	
	mode = SD_CTL_SCC;
	mode |= (readl(HOST_CTL(host)) & (0xff << 16));
	writel(mode, HOST_CTL(host));
	
	return 0;
}

static void caninos_mmc_power_on(struct caninos_mmc_host *host)
{
	if (gpio_is_valid(host->enable_gpio)) {
		gpio_set_value(host->enable_gpio, 1);
	}

	if (gpio_is_valid(host->reset_gpio)) {
		gpio_set_value(host->reset_gpio, 1);
	}
	
	writel(readl(HOST_STATE(host)) | SD_STATE_TEIE, HOST_STATE(host));
	
	writel(readl(HOST_EN(host)) | SD_EN_SDIOEN, HOST_EN(host));
	
	caninos_mmc_send_init_clk(host);
}

static void caninos_mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct caninos_mmc_host *host = mmc_priv(mmc);
	u32 ctrl_reg;

	if (ios->clock && ios->clock != host->clock)
	{
		host->clock = ios->clock;
		caninos_mmc_set_clk(host, ios->clock);
	}
	
	if (ios->power_mode != host->power_state)
	{
		host->power_state = ios->power_mode;
		
		switch (ios->power_mode)
		{
			case MMC_POWER_UP:
				caninos_mmc_power_up(host);
				break;

			case MMC_POWER_ON:
				caninos_mmc_power_on(host);
				break;

			case MMC_POWER_OFF:
				caninos_mmc_power_off(host);
				return;
		}
	}

	ctrl_reg = readl(HOST_EN(host));
	
	host->bus_width = ios->bus_width;
	
	switch (ios->bus_width)
	{
	case MMC_BUS_WIDTH_8:
		ctrl_reg &= ~0x3;
		ctrl_reg |= 0x2;
		break;
		
	case MMC_BUS_WIDTH_4:
		ctrl_reg &= ~0x3;
		ctrl_reg |= 0x1;
		break;
		
	case MMC_BUS_WIDTH_1:
		ctrl_reg &= ~0x3;
		break;
	}

	if (ios->chip_select != host->chip_select)
	{
		host->chip_select = ios->chip_select;
		
		switch (ios->chip_select)
		{
		case MMC_CS_DONTCARE:
			break;
			
		case MMC_CS_HIGH:
			ctrl_reg &= ~0x3;
			ctrl_reg |= 0x1;
			break;
			
		case MMC_CS_LOW:
			ctrl_reg &= ~0x3;
			break;
		}
	}
	
	writel(ctrl_reg, HOST_EN(host));
}

static int caninos_mmc_card_busy(struct mmc_host * mmc)
{
	struct caninos_mmc_host *host = mmc_priv(mmc);
	int ret = 0;
	u32 state;
	
	state = readl(HOST_STATE(host));
	ret = !((state & SD_STATE_DAT0S) && (state & SD_STATE_CMDS));
	
	if (ret == 0) {
		writel(readl(HOST_CTL(host)) & ~SD_CTL_SCC, HOST_CTL(host));
	}
	
	return ret;
}

static int caninos_mmc_vswitch(struct mmc_host * mmc, struct mmc_ios * ios)
{		
	struct caninos_mmc_host *host = mmc_priv(mmc);
	
    switch (ios->signal_voltage)
    {
    case MMC_SIGNAL_VOLTAGE_330:
    	writel(readl(HOST_EN(host)) & ~SD_EN_S18EN, HOST_EN(host));
    	mdelay(5);
    	break;
    
    case MMC_SIGNAL_VOLTAGE_180:
    	writel(readl(HOST_EN(host)) | SD_EN_S18EN, HOST_EN(host));
		/* controller needs at least 12.8ms because of a hardware bug */
		mdelay(20);
		writel(readl(HOST_CTL(host)) | SD_CTL_SCC, HOST_CTL(host));
		break;
		
	default:
		return -ENOSYS;
	}
	return 0;
}

static const struct mmc_host_ops caninos_mmc_ops = {
    .request = caninos_mmc_request,
    .set_ios = caninos_mmc_set_ios,
	.card_busy = caninos_mmc_card_busy,
	.enable_sdio_irq = caninos_mmc_en_sdio_irq,
	.start_signal_voltage_switch = caninos_mmc_vswitch,
};

static irqreturn_t sdc_irq_handler(int irq, void *devid)
{
	struct caninos_mmc_host *host = devid;
	unsigned long flags;
	u32 state;
	
	spin_lock_irqsave(&host->lock, flags);
	
	state = readl(HOST_STATE(host));
	writel(state, HOST_STATE(host));
	
	spin_unlock_irqrestore(&host->lock, flags);
	
	if ((state & SD_STATE_TEIE) && (state & SD_STATE_TEI)) {
		complete(&host->sdc_complete);
	}
	
	if ((state & SD_STATE_SDIOA_P) && (state & SD_STATE_SDIOA_EN)) {
		mmc_signal_sdio_irq(host->mmc);
	}
	
	return IRQ_HANDLED;
}

static int caninos_mmc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct caninos_mmc_host *host;
    struct mmc_host *mmc;
    int ret;
    
	platform_set_drvdata(pdev, NULL);
	
	mmc = mmc_alloc_host(sizeof(*host), dev);
    
    if (!mmc)
    {
        dev_err(dev, "host allocation failed.\n");
        return -ENOMEM;
    }
	
	host = mmc_priv(mmc);
	spin_lock_init(&host->lock);
	mutex_init(&host->pin_mutex);
	host->mmc = mmc;
	host->power_state = host->bus_width = host->chip_select = -1;
	host->clock = 0;
	host->clk_on = 0;
	host->mrq = NULL;
	host->dev = dev;
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
    if (!res)
    {
    	dev_err(dev, "could not get device registers.");
    	return -ENOMEM;
    }
    
    if ((res->start & 0xFFFF) == 0x0) {
    	pdev->id = 0;
    }
    else if ((res->start & 0xFFFF) == 0x4000) {
    	pdev->id = 1;
    }
    else if ((res->start & 0xFFFF) == 0x8000) {
    	pdev->id = 2;
    }
    else
    {
    	dev_err(dev, "could not match a device id.\n");
    	return -ENODEV;
    }
    
    host->id = pdev->id;
    
    if (!request_mem_region(res->start, resource_size(res), dev_name(dev)))
	{
		dev_err(dev, "could not request memory region.\n");
		mmc_free_host(mmc);
		return -EBUSY;
	}
    
    host->base = devm_ioremap_nocache(dev, res->start, resource_size(res));
    
    if (!host->base)
	{
		dev_err(dev, "could not map device registers.\n");
		mmc_free_host(mmc);
		return -ENOMEM;
	}
	
    switch (host->id)
    {
    case 0:
    	host->wdelay.delay_lowclk  = SDC0_WDELAY_LOW_CLK;
		host->wdelay.delay_midclk  = SDC0_WDELAY_MID_CLK;
		host->wdelay.delay_highclk = SDC0_WDELAY_HIGH_CLK;
		host->rdelay.delay_lowclk  = SDC0_RDELAY_LOW_CLK;
		host->rdelay.delay_midclk  = SDC0_RDELAY_MID_CLK;
		host->rdelay.delay_highclk = SDC0_RDELAY_HIGH_CLK;
    	break;
    	
    case 1:
    	host->wdelay.delay_lowclk  = SDC1_WDELAY_LOW_CLK;
		host->wdelay.delay_midclk  = SDC1_WDELAY_MID_CLK;
		host->wdelay.delay_highclk = SDC1_WDELAY_HIGH_CLK;
		host->rdelay.delay_lowclk  = SDC1_RDELAY_LOW_CLK;
		host->rdelay.delay_midclk  = SDC1_RDELAY_MID_CLK;
		host->rdelay.delay_highclk = SDC1_RDELAY_HIGH_CLK;
    	break;
    	
    case 2:
    	host->wdelay.delay_lowclk  = SDC2_WDELAY_LOW_CLK;
		host->wdelay.delay_midclk  = SDC2_WDELAY_MID_CLK;
		host->wdelay.delay_highclk = SDC2_WDELAY_HIGH_CLK;
		host->rdelay.delay_lowclk  = SDC2_RDELAY_LOW_CLK;
		host->rdelay.delay_midclk  = SDC2_RDELAY_MID_CLK;
		host->rdelay.delay_highclk = SDC2_RDELAY_HIGH_CLK;
    	break;
    }
    
	host->clk = devm_clk_get(dev, NULL);
	
	if (IS_ERR(host->clk))
	{
		dev_err(dev, "could not get device clock.\n");
		mmc_free_host(mmc);
		return PTR_ERR(host->clk);
	}
	
	host->pcl = pinctrl_get_select_default(dev);
	
	if (IS_ERR(host->pcl))
	{
		dev_err(dev, "could not get device pinctrl configuration.\n");
		mmc_free_host(mmc);
		return -ENODEV;
	}
	
	host->enable_gpio = of_get_named_gpio(dev->of_node, "enable-gpios", 0);
	
	if (gpio_is_valid(host->enable_gpio))
	{
		if (devm_gpio_request(dev, host->enable_gpio, "enable_gpio"))
		{
			dev_err(dev, "could not request enable gpio.\n");
			mmc_free_host(mmc);
			return -ENODEV;
		}
		else {
			gpio_direction_output(host->enable_gpio, 0);
		}
	}
	
	host->power_gpio = of_get_named_gpio(dev->of_node, "power-gpios", 0);
	
	if (gpio_is_valid(host->power_gpio))
	{
		if (devm_gpio_request(dev, host->power_gpio, "power_gpio"))
		{
			dev_err(dev, "could not request power gpio.\n");
			mmc_free_host(mmc);
			return -ENODEV;
		}
		else {
			gpio_direction_output(host->power_gpio, 0);
		}
	}

	host->reset_gpio = of_get_named_gpio(dev->of_node, "reset-gpios", 0);
	
	if (gpio_is_valid(host->reset_gpio))
	{
		if (devm_gpio_request(dev, host->reset_gpio, "reset_gpio"))
		{
			dev_err(dev, "could not request reset gpio.\n");
			mmc_free_host(mmc);
			return -ENODEV;
		}
		else {
			gpio_direction_output(host->reset_gpio, 0);
		}
	}
	
	host->irq = platform_get_irq(pdev, 0);
	
	if (host->irq <= 0)
	{
		dev_err(dev, "invalid device irq number.\n");
		mmc_free_host(mmc);
		return -ENODEV;
	}
	
	host->rst = devm_reset_control_get(dev, NULL);
	
	if (!host->rst)
	{
		dev_err(dev, "could not get device reset control.\n");
		mmc_free_host(mmc);
		return -ENODEV;
	}
	
	/* dma controller does not support 64bit addresses */
	dev->coherent_dma_mask = DMA_BIT_MASK(32); 
	dev->dma_mask = &dev->coherent_dma_mask;
	
	host->dma = dma_request_slave_channel(dev, "mmc");
	
	if (!host->dma)
	{
	    dev_err(dev, "failed to request dma channel.\n");
	    mmc_free_host(mmc);
		return -ENODEV;
	}
	
	dev_info(dev, "using %s for DMA transfers\n", dma_chan_name(host->dma));
	
	host->dma_conf.src_addr = res->start + SD_DAT_OFFSET;
	host->dma_conf.dst_addr = res->start + SD_DAT_OFFSET;
	host->dma_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	host->dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	host->dma_conf.device_fc = false;
	
	host->clock = 0;
	host->bus_width = 0;
	
	init_completion(&host->dma_complete);
	init_completion(&host->sdc_complete);
	
	mmc->ops = &caninos_mmc_ops;
	
	mmc->ocr_avail  = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30;
    mmc->ocr_avail |= MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33;
    mmc->ocr_avail |= MMC_VDD_33_34 | MMC_VDD_34_35 | MMC_VDD_35_36;
    mmc->ocr_avail |= MMC_VDD_165_195;
    
    mmc->f_min = 187500;
    mmc->f_max = 52000000;
    
    mmc->max_seg_size  = 512 * 256;
    mmc->max_segs = 128;
    mmc->max_req_size  = 512 * 256;
    mmc->max_blk_size  = 512;
    mmc->max_blk_count = 256;
    
    mmc->caps  = MMC_CAP_4_BIT_DATA | MMC_CAP_SDIO_IRQ;
    mmc->caps |= MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED;
    mmc->caps |= MMC_CAP_ERASE | MMC_CAP_NEEDS_POLL;
    mmc->caps |= MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25;
    mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY;
    
    mmc->caps2 = MMC_CAP2_NO_WRITE_PROTECT | MMC_CAP2_BOOTPART_NOACC;
    
    ret = request_irq(host->irq, sdc_irq_handler, 0, dev_name(dev), host);
    
	if (ret)
	{
		dev_err(dev, "failed to request device irq.\n");
		dma_release_channel(host->dma);
		mmc_free_host(mmc);
		return ret;
	}
    
    ret = mmc_add_host(mmc);
	
	if (ret)
	{
		dev_err(dev, "could not add mmc host\n");
		dma_release_channel(host->dma);
		free_irq(host->irq, host);
		mmc_free_host(mmc);
		return ret;
	}
	
	platform_set_drvdata(pdev, host);
	return 0;
}

static int caninos_mmc_remove(struct platform_device *pdev)
{
	struct caninos_mmc_host *host;
	struct mmc_host * mmc;
	
	host = platform_get_drvdata(pdev);
	
	if (host)
	{
		platform_set_drvdata(pdev, NULL);
		mmc = host->mmc;
		mmc_remove_host(mmc);
		dma_release_channel(host->dma);
		free_irq(host->irq, host);
		mmc_free_host(mmc);
	}
	return 0;
}

MODULE_DEVICE_TABLE(of, caninos_mmc_of_match);

static struct dev_pm_ops caninos_mmc_pm = {
	.suspend = NULL,
	.resume = NULL,
};

static struct platform_driver caninos_mmc_driver = {
	.probe  = caninos_mmc_probe,
	.remove = caninos_mmc_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name  = DRIVER_NAME,
		.of_match_table = caninos_mmc_of_match,
		.pm = &caninos_mmc_pm,
	},
};

static int __init caninos_driver_init(void) {
	return platform_driver_register(&caninos_mmc_driver);
}

static void __exit caninos_driver_exit(void) {
	platform_driver_unregister(&caninos_mmc_driver);
}

module_init(caninos_driver_init);
module_exit(caninos_driver_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

