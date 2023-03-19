// SPDX-License-Identifier: GPL-2.0
/*
 * Caninos Labrador I2C Driver
 *
 * Copyright (c) 2022-2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2019 LSI-TEC - Caninos Loucos
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
#include <linux/iopoll.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/pinctrl/consumer.h>

#define CANINOS_I2C_KILO_HZ       (1000UL)
#define CANINOS_I2C_MEGA_HZ       (1000000UL)
#define CANINOS_I2C_TIMEOUT_MS    (4000UL)
#define CANINOS_I2C_FIFO_SIZE     (128UL)
#define CANINOS_I2C_MAX_ADDR_LEN  (6UL)
#define CANINOS_I2C_MAX_MSG_LEN   (240UL)
#define CANINOS_I2C_DEFAULT_FREQ  (400UL * CANINOS_I2C_KILO_HZ)
#define CANINOS_I2C_MAX_FREQ      (400UL * CANINOS_I2C_KILO_HZ)
#define CANINOS_I2C_MIN_FREQ      ( 25UL * CANINOS_I2C_KILO_HZ)

/* I2C registers */
#define I2C_CTL                   (0x00)
#define I2C_CLKDIV                (0x04)
#define I2C_STAT                  (0x08)
#define I2C_ADDR                  (0x0C)
#define I2C_TXDAT                 (0x10)
#define I2C_RXDAT                 (0x14)
#define I2C_CMD                   (0x18)
#define I2C_FIFOCTL               (0x1C)
#define I2C_FIFOSTAT              (0x20)
#define I2C_DATCNT                (0x24)
#define I2C_RCNT                  (0x28)

/* I2Cx_CTL */
#define I2C_CTL_GRAS              (0x1 << 0)
#define	I2C_CTL_GRAS_ACK          (0)
#define	I2C_CTL_GRAS_NACK         I2C_CTL_GRAS
#define I2C_CTL_RB                (0x1 << 1)
#define I2C_CTL_GBCC_MASK         (0x3 << 2)
#define I2C_CTL_GBCC(x)           (((x) & 0x3) << 2)
#define	I2C_CTL_GBCC_NONE         I2C_CTL_GBCC(0)
#define	I2C_CTL_GBCC_START        I2C_CTL_GBCC(1)
#define	I2C_CTL_GBCC_STOP         I2C_CTL_GBCC(2)
#define	I2C_CTL_GBCC_RESTART      I2C_CTL_GBCC(3)
#define I2C_CTL_IRQE              (0x1 << 5)
#define I2C_CTL_EN                (0x1 << 7)
#define I2C_CTL_AE                (0x1 << 8)

/* I2Cx_CLKDIV */
#define I2C_CLKDIV_DIV_MASK       (0xff << 0)
#define I2C_CLKDIV_DIV(x)         (((x) & 0xff) << 0)

/* I2Cx_STAT */
#define I2C_STAT_RACK             (0x1 << 0)
#define I2C_STAT_BEB              (0x1 << 1)
#define I2C_STAT_IRQP             (0x1 << 2)
#define I2C_STAT_LAB              (0x1 << 3)
#define I2C_STAT_STPD             (0x1 << 4)
#define I2C_STAT_STAD             (0x1 << 5)
#define I2C_STAT_BBB              (0x1 << 6)
#define I2C_STAT_TCB              (0x1 << 7)
#define I2C_STAT_LBST             (0x1 << 8)
#define I2C_STAT_SAMB             (0x1 << 9)
#define I2C_STAT_SRGC             (0x1 << 10)
#define I2C_BUS_ERR_MSK           (I2C_STAT_LAB | I2C_STAT_BEB)

/* I2Cx_CMD */
#define I2C_CMD_SBE               (0x1 << 0)
#define I2C_CMD_AS_MASK           (0x7 << 1)
#define I2C_CMD_AS(x)             (((x) & 0x7) << 1)
#define I2C_CMD_RBE               (0x1 << 4)
#define I2C_CMD_SAS_MASK          (0x7 << 5)
#define I2C_CMD_SAS(x)            (((x) & 0x7) << 5)
#define I2C_CMD_DE                (0x1 << 8)
#define I2C_CMD_NS                (0x1 << 9)
#define I2C_CMD_SE                (0x1 << 10)
#define I2C_CMD_MSS               (0x1 << 11)
#define I2C_CMD_WRS               (0x1 << 12)
#define I2C_CMD_EXEC              (0x1 << 15)

/* I2Cx_FIFOCTL */
#define I2C_FIFOCTL_NIB           (0x1 << 0)
#define I2C_FIFOCTL_RFR           (0x1 << 1)
#define I2C_FIFOCTL_TFR           (0x1 << 2)

/* I2Cx_FIFOSTAT */
#define I2C_FIFOSTAT_CECB         (0x1 << 0)
#define I2C_FIFOSTAT_RNB          (0x1 << 1)
#define I2C_FIFOSTAT_RFE          (0x1 << 2)
#define I2C_FIFOSTAT_RFF          (0x1 << 3)
#define I2C_FIFOSTAT_TFE          (0x1 << 4)
#define I2C_FIFOSTAT_TFF          (0x1 << 5)
#define I2C_FIFOSTAT_WRS          (0x1 << 6)
#define I2C_FIFOSTAT_RFD_MASK     (0xff << 8)
#define I2C_FIFOSTAT_RFD_SHIFT    (8)
#define I2C_FIFOSTAT_TFD_MASK     (0xff << 16)
#define I2C_FIFOSTAT_TFD_SHIFT    (16)

/* extract fifo level from fifostat */
#define I2C_RX_FIFO_LEVEL(x)      (((x) >> 8) & 0xff)
#define I2C_TX_FIFO_LEVEL(x)      (((x) >> 16) & 0xff)

enum i2c_state {
	STATE_INVALID = 1,
	STATE_READ_DATA,
	STATE_WRITE_DATA,
	STATE_TRANSFER_OVER,
	STATE_TRANSFER_ERROR,
};

enum i2c_hw_model {
	I2C_HW_MODEL_K5 = 1,
	I2C_HW_MODEL_K7,
};

struct caninos_i2c_dev
{
	struct i2c_adapter adapter;
	void __iomem *base;
	struct device *dev;
	struct clk *clk;
	struct reset_control *rst;
	struct pinctrl *pctl;
	struct pinctrl_state *def_state;
	struct pinctrl_state *extio_state;
	struct completion cmd_complete;
	enum i2c_state state;
	struct i2c_msg *msg;
	unsigned long idx;
	unsigned long freq;
	unsigned long pclk;
	int irq;
};

static inline void caninos_i2c_set_freq(struct caninos_i2c_dev *md)
{
	u32 div = DIV_ROUND_UP(md->pclk, md->freq * 16UL);
	writel(I2C_CLKDIV_DIV(div), md->base + I2C_CLKDIV);
}

static inline int caninos_i2c_wait_if_busy(struct caninos_i2c_dev *md)
{
	const unsigned long timeout_us = CANINOS_I2C_TIMEOUT_MS * 1000UL;
	u32 val;
	
	return readl_poll_timeout(md->base + I2C_STAT, val,
	                          !(val & I2C_STAT_BBB),
	                          10UL, timeout_us);
}

static inline void caninos_i2c_reset_fifo(struct caninos_i2c_dev *md)
{
	u32 val;
	val = readl(md->base + I2C_FIFOCTL);
	val |= I2C_FIFOCTL_RFR | I2C_FIFOCTL_TFR;
	writel(val, md->base + I2C_FIFOCTL);
	
	readl_poll_timeout_atomic(md->base + I2C_FIFOCTL, val,
	                          !(val & (I2C_FIFOCTL_RFR | I2C_FIFOCTL_TFR)),
	                          0UL, 0UL);
}

static inline void caninos_i2c_clear_status(struct caninos_i2c_dev *md)
{
	u32 val;
	val = readl(md->base + I2C_STAT);
	writel(val, md->base + I2C_STAT);
	caninos_i2c_reset_fifo(md);
}

static inline void caninos_i2c_reset(struct caninos_i2c_dev *md)
{
	writel(0U, md->base + I2C_CTL);
	writel(I2C_CTL_EN, md->base + I2C_CTL);
	caninos_i2c_reset_fifo(md);
}

static inline int caninos_i2c_hwinit(struct caninos_i2c_dev *md)
{
	writel(I2C_CTL_EN, md->base + I2C_CTL);
	caninos_i2c_clear_status(md);
	caninos_i2c_set_freq(md);
	return 0;
}

static inline void caninos_i2c_force_stop(struct caninos_i2c_dev *md, u32 addr)
{
	/* reset the i2c controller to stop current cmd */
	caninos_i2c_reset(md);
	caninos_i2c_hwinit(md);
	
	/* send start command */
	writel(addr, md->base + I2C_TXDAT);
	writel(I2C_CTL_EN | I2C_CTL_GBCC_START | I2C_CTL_RB, md->base + I2C_CTL);
	usleep_range(100UL, 400UL);
	
	/* send stop command */
	writel(I2C_CTL_EN | I2C_CTL_GBCC_STOP | I2C_CTL_RB, md->base + I2C_CTL);
	usleep_range(100UL, 400UL);
	readl(md->base + I2C_STAT); /* do not remove */
	
	/* clear stop command */
	writel(I2C_CTL_EN, md->base + I2C_CTL);
	caninos_i2c_clear_status(md);
}

static inline void caninos_i2c_fifo_rw(struct caninos_i2c_dev *md)
{
	if (md->msg->flags & I2C_M_RD) {
		for (; md->idx < md->msg->len; md->idx++)
		{
			if (!(readl(md->base + I2C_FIFOSTAT) & I2C_FIFOSTAT_RFE)) {
				break;
			}
			md->msg->buf[md->idx] = readl(md->base + I2C_RXDAT);
		}
	}
	else {
		for (; md->idx < md->msg->len; md->idx++)
		{
			if (readl(md->base + I2C_FIFOSTAT) & I2C_FIFOSTAT_TFF) {
				break;
			}
			writel(md->msg->buf[md->idx], md->base + I2C_TXDAT);
		}
	}
	if (md->idx == md->msg->len) {
		md->state = STATE_TRANSFER_OVER;
	}
}

static irqreturn_t i2c_irq_handler(int irq, void *data)
{
	struct caninos_i2c_dev *md = (struct caninos_i2c_dev *)(data);
	u32 stat = readl(md->base + I2C_STAT);
	u32 fifostat = readl(md->base + I2C_FIFOSTAT);
	
	if (fifostat & I2C_FIFOSTAT_RNB) {
		md->state = STATE_TRANSFER_ERROR;
	}
	else if (stat & I2C_STAT_LAB) {
		md->state = STATE_TRANSFER_ERROR;
	}
	else if (stat & I2C_STAT_BEB) {
		md->state = STATE_TRANSFER_ERROR;
	}
	else {
		caninos_i2c_fifo_rw(md);
	}
	
	readl(md->base + I2C_STAT); /* do not remove */
	writel(I2C_STAT_IRQP, md->base + I2C_STAT);
	
	if (md->state == STATE_TRANSFER_ERROR) {
		caninos_i2c_reset(md);
		complete_all(&md->cmd_complete);
	}
	else if (md->state == STATE_TRANSFER_OVER) {
		complete_all(&md->cmd_complete);
	}
	return IRQ_HANDLED;
}

static inline bool caninos_i2c_msg_is_valid(struct i2c_msg *msgs, int num)
{
	bool valid = false;
	
	if (num == 1) {
		valid = !(msgs[0].len > CANINOS_I2C_MAX_MSG_LEN);
	}
	else if (num == 2)
	{
		valid = !(msgs[0].len > CANINOS_I2C_MAX_ADDR_LEN);
		valid = valid && !(msgs[0].flags & I2C_M_RD);
		valid = valid && !(msgs[1].len > CANINOS_I2C_MAX_MSG_LEN);
	}
	return valid;
}

static int caninos_i2c_do_transfer(struct caninos_i2c_dev *md,
                                   struct i2c_msg *msgs, int num)
{
	unsigned long time_left = msecs_to_jiffies(CANINOS_I2C_TIMEOUT_MS);
	u32 fifo_cmd, addr;
	int i, ret;
	
	if (!caninos_i2c_msg_is_valid(msgs, num)) {
		return -EINVAL;
	}
	
	addr = ((u32)(msgs[0].addr) & 0x7f) << 1;
	
	init_completion(&md->cmd_complete);
	
	/* enable I2C controller IRQ */
	writel(I2C_CTL_IRQE | I2C_CTL_EN, md->base + I2C_CTL);
	
	fifo_cmd = I2C_CMD_EXEC | I2C_CMD_MSS | I2C_CMD_SE | I2C_CMD_DE;
	fifo_cmd |= I2C_CMD_NS | I2C_CMD_SBE;
	
	if (num == 2)
	{
		/* set internal address and restart cmd for read operation */
		fifo_cmd |= I2C_CMD_AS(msgs[0].len + 1);
		fifo_cmd |= I2C_CMD_SAS(1) | I2C_CMD_RBE;
		
		/* write i2c device address */
		writel(addr, md->base + I2C_TXDAT);
		
		/* write internal register address */
		for (i = 0; i < msgs[0].len; i++) {
			writel(msgs[0].buf[i], md->base + I2C_TXDAT);
		}
		
		md->msg = &msgs[1];
		md->idx = 0U;
	}
	else
	{
		/* only send device addess for 1 message */
		fifo_cmd |= I2C_CMD_AS(1);
		
		md->msg = &msgs[0];
		md->idx = 0U;
	}
	
	/* set data count for the message */
	writel(md->msg->len, md->base + I2C_DATCNT);
	
	if (md->msg->flags & I2C_M_RD)
	{
		/* read from device, with WR bit */
		writel(addr | 1U, md->base + I2C_TXDAT);
		md->state = STATE_READ_DATA;
	}
	else
	{
		/* write to device */
		writel(addr, md->base + I2C_TXDAT);
		
		/* write data to FIFO */
		for (i = 0; i < md->msg->len; i++)
		{
			if (readl(md->base + I2C_FIFOSTAT) & I2C_FIFOSTAT_TFF) {
				break;
			}
			writel(md->msg->buf[i], md->base + I2C_TXDAT);
		}
		
		md->idx = i;
		md->state = STATE_WRITE_DATA;
	}
	
	/* Ingore the NACK if needed */
	if (md->msg->flags & I2C_M_IGNORE_NAK) {
		writel(I2C_FIFOCTL_NIB, md->base + I2C_FIFOCTL);
	}
	else {
		writel(0U, md->base + I2C_FIFOCTL);
	}
	
	/* write fifo command to start transfer */
	writel(fifo_cmd, md->base + I2C_CMD);
	
	/* wait for transfer over or error */
	time_left = wait_for_completion_timeout(&md->cmd_complete, time_left);
	
	if ((md->state == STATE_TRANSFER_OVER)) {
		ret = 0;
	}
	else if (time_left == 0) {
		ret = -EREMOTEIO;
	}
	else {
		ret = -ENXIO;
	}
	if (ret < 0) {
		caninos_i2c_force_stop(md, addr);
	}
	
	/* disable i2c controller */
	writel(0U, md->base + I2C_CTL);
	return ret;
}

static int caninos_i2c_xfer(struct i2c_adapter *adap,
                            struct i2c_msg *msgs, int num)
{
	struct caninos_i2c_dev *md = i2c_get_adapdata(adap);
	int ret;
	
	if (md->extio_state) {
		if (pinctrl_select_state(md->pctl, md->extio_state) < 0)
			return -EAGAIN;
	}
	
	caninos_i2c_hwinit(md);
	
	ret = caninos_i2c_wait_if_busy(md);
	
	if (ret < 0) {
		if (md->extio_state) {
			pinctrl_select_state(md->pctl, md->def_state);
		}
		return ret;
	}
	
	ret = caninos_i2c_do_transfer(md, msgs, num);
	
	if (md->extio_state) {
		pinctrl_select_state(md->pctl, md->def_state);
	}
	
	return (ret < 0) ? ret : num;
}

static u32 caninos_i2c_func(struct i2c_adapter *adap) {
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm caninos_i2c_algorithm = {
	.master_xfer = caninos_i2c_xfer,
	.functionality = caninos_i2c_func,
	/* TODO: use master_xfer_atomic for PMIC at shutdown */
};

static int caninos_i2c_pinctrl_setup(struct caninos_i2c_dev *md)
{
	struct device *dev = md->dev;
	struct pinctrl_state *def;
	struct pinctrl_state *extio;
	struct pinctrl *pctl;
	int ret;
	
	pctl = devm_pinctrl_get(dev);
	
	if (IS_ERR_OR_NULL(pctl)) {
		dev_err(dev, "failed to get pinctrl handler\n");
		return (pctl == NULL) ? -ENODEV : PTR_ERR(pctl);
	}
	
	def = pinctrl_lookup_state(pctl, PINCTRL_STATE_DEFAULT);
	
	if (IS_ERR_OR_NULL(def)) {
		dev_err(dev, "could not get pinctrl default state\n");
		return (def == NULL) ? -ENODEV : PTR_ERR(def);
	}
	
	extio = pinctrl_lookup_state(pctl, "extio");
	
	if (IS_ERR(extio)) {
		extio = NULL; /* it is optional */
	}
	
	ret = pinctrl_select_state(pctl, def);
	
	if (ret < 0) {
		dev_err(dev, "could not select default pinctrl state\n");
		return ret;
	}
	
	md->pctl = pctl;
	md->def_state = def;
	md->extio_state = extio;
	return 0;
}

static int caninos_i2c_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct caninos_i2c_dev *md;
	enum i2c_hw_model hw_model;
	struct i2c_adapter *adap;
	struct resource *res;
	int irq, ret;
	u32 freq;
	
	if (!np) {
		dev_err(dev, "missing device OF node\n");
		return -ENODEV;
	}
	
	match = of_match_device(dev->driver->of_match_table, dev);
	
	if (!match || !match->data) {
		dev_err(dev, "could not get hardware specific data\n");
		return -EINVAL;
	}
	
	hw_model = (enum i2c_hw_model)(match->data);
	
	if (of_property_read_u32(np, "clock-frequency", &freq)) {
		freq = CANINOS_I2C_DEFAULT_FREQ;
	}
	if (freq > CANINOS_I2C_MAX_FREQ) {
		freq = CANINOS_I2C_MAX_FREQ;
	}
	if (freq < CANINOS_I2C_MIN_FREQ) {
		freq = CANINOS_I2C_MIN_FREQ;
	}
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res) {
		dev_err(dev, "could not get device registers\n");
		return -ENXIO;
	}
	
	irq = platform_get_irq(pdev, 0);
	
	if (irq < 0) {
		dev_err(dev, "could not get device irq\n");
		return -ENXIO;
	}
	
	md = devm_kzalloc(dev, sizeof(*md), GFP_KERNEL);
	
	if (!md) {
		dev_err(dev, "memory allocation failed\n");
		return -ENOMEM;
	}
	
	platform_set_drvdata(pdev, md);
	
	init_completion(&md->cmd_complete);
	
	md->state = STATE_INVALID;
	md->msg = NULL;
	md->idx = 0UL;
	md->freq = freq;
	md->dev = dev;
	md->irq = irq;
	
	switch (hw_model)
	{
	case I2C_HW_MODEL_K5:
		md->pclk = 100UL * CANINOS_I2C_MEGA_HZ;
		break;
		
	case I2C_HW_MODEL_K7:
		md->pclk = 24UL * CANINOS_I2C_MEGA_HZ;
		break;
		
	default:
		dev_err(dev, "unrecognized hw model\n");
		return -EINVAL;
	}
	
	adap = &md->adapter;
	adap->owner = THIS_MODULE;
	adap->class = I2C_CLASS_DEPRECATED;
	adap->algo = &caninos_i2c_algorithm;
	adap->timeout = CANINOS_I2C_TIMEOUT_MS;
	adap->dev.parent = dev;
	adap->dev.of_node = pdev->dev.of_node;
	
	i2c_set_adapdata(adap, md);
	
	strlcpy(adap->name, "caninos-i2c", sizeof(adap->name));
	
	md->base = devm_ioremap(dev, res->start, resource_size(res));
	
	if (!md->base) {
		dev_err(dev, "could not map device registers\n");
		return -ENOMEM;
	}
	
	md->rst = devm_reset_control_get(dev, NULL);
	
	if (IS_ERR_OR_NULL(md->rst)) {
		dev_err(dev, "could not get reset control\n");
		return -ENODEV;
	}
	
	ret = caninos_i2c_pinctrl_setup(md);
	
	if (ret < 0) {
		return ret;
	}
	
	reset_control_deassert(md->rst);
	
	ret = devm_request_irq(dev, irq, i2c_irq_handler, 0, dev_name(dev), md);
	
	if (ret < 0) {
		dev_err(dev, "failed to request device irq\n");
		return ret;
	}
	
	md->clk = devm_clk_get(dev, NULL);
	
	if (IS_ERR_OR_NULL(md->clk)) {
		dev_err(dev, "no clock defined\n");
		return -ENODEV;
	}
	
	clk_prepare_enable(md->clk);
	
	ret = i2c_add_adapter(adap);
	
	if (ret < 0) {
		dev_err(dev, "adapter registration failed\n");
		clk_disable_unprepare(md->clk);
		return ret;
	}
	
	dev_info(dev, "probe finished\n");
	return 0;
}

static int caninos_i2c_remove(struct platform_device *pdev)
{
	struct caninos_i2c_dev *dev = platform_get_drvdata(pdev);
	i2c_del_adapter(&dev->adapter);
	clk_disable_unprepare(dev->clk);
	return 0;
}

static const struct of_device_id caninos_i2c_of_match[] = {
	{ .compatible = "caninos,k7-i2c", .data = (void*)(I2C_HW_MODEL_K7) },
	{ .compatible = "caninos,k5-i2c", .data = (void*)(I2C_HW_MODEL_K5) },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, caninos_i2c_of_match);

static struct platform_driver caninos_i2c_driver = {
	.driver	= {
		.name = "caninos-i2c",
		.of_match_table = caninos_i2c_of_match,
		.owner = THIS_MODULE,
	},
	.probe = caninos_i2c_probe,
	.remove	= caninos_i2c_remove,
};

module_platform_driver(caninos_i2c_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("Caninos Labrador I2C Driver");
MODULE_LICENSE("GPL v2");
