#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

/* debug stuff */
#define OWL_I2C_DBG_LEVEL_OFF		0
#define OWL_I2C_DBG_LEVEL_ON		1
#define OWL_I2C_DBG_LEVEL_VERBOSE	2
#define OWL_I2C_DEFAULT_DBG_LEVEL	OWL_I2C_DBG_LEVEL_OFF

#define OWL_I2C_TIMEOUT			(4 * 1000) /* ms */
#define OWL_I2C_FIFO_SIZE		(128)
#define OWL_I2C_MAX_INTER_ADDR_LEN	(6)
#define OWL_I2C_MAX_MSG_LEN		(240)

#define OWL_I2C_DEFAULT_FREQ		(400 * 1000) /* 400K */

/* I2C registers */
#define I2C_CTL				0x0000
#define I2C_CLKDIV			0x0004
#define I2C_STAT			0x0008
#define I2C_ADDR			0x000C
#define I2C_TXDAT			0x0010
#define I2C_RXDAT			0x0014
#define I2C_CMD				0x0018
#define I2C_FIFOCTL			0x001C
#define I2C_FIFOSTAT			0x0020
#define I2C_DATCNT			0x0024
#define I2C_RCNT			0x0028

/* I2Cx_CTL */
#define I2C_CTL_GRAS			(0x1 << 0)
#define		I2C_CTL_GRAS_ACK		0
#define		I2C_CTL_GRAS_NACK		I2C_CTL_GRAS
#define I2C_CTL_RB				(0x1 << 1)
#define I2C_CTL_GBCC_MASK		(0x3 << 2)
#define I2C_CTL_GBCC(x)			(((x) & 0x3) << 2)
#define		I2C_CTL_GBCC_NONE		I2C_CTL_GBCC(0)
#define		I2C_CTL_GBCC_START		I2C_CTL_GBCC(1)
#define		I2C_CTL_GBCC_STOP		I2C_CTL_GBCC(2)
#define		I2C_CTL_GBCC_RESTART		I2C_CTL_GBCC(3)
#define I2C_CTL_IRQE			(0x1 << 5)
#define I2C_CTL_EN			(0x1 << 7)
#define I2C_CTL_AE			(0x1 << 8)
#define I2C_CTL_FHSM			(0x1 << 9)
#define I2C_CTL_SHSM			(0x1 << 10)
#define I2C_CTL_CSE			(0x1 << 11)

/* I2Cx_CLKDIV */
#define I2C_CLKDIV_DIV_MASK		(0xff << 0)
#define I2C_CLKDIV_DIV(x)		(((x) & 0xff) << 0)
#define I2C_CLKDIV_HDIV_MASK		(0xff << 8)
#define I2C_CLKDIV_HDIV(x)		(((x) & 0xff) << 8)
#define I2C_CLKDIV_CLKCOM(x)		(((x) & 0x3) << 16)
#define		I2C_CLKDIV_CLKCOM_0NS		I2C_CLKDIV_CLKCOM(0)
#define		I2C_CLKDIV_CLKCOM_10NS		I2C_CLKDIV_CLKCOM(1)
#define		I2C_CLKDIV_CLKCOM_20NS		I2C_CLKDIV_CLKCOM(2)
#define		I2C_CLKDIV_CLKCOM_30NS		I2C_CLKDIV_CLKCOM(3)

/* I2Cx_STAT */
#define I2C_STAT_RACK			(0x1 << 0)
#define I2C_STAT_BEB			(0x1 << 1)
#define I2C_STAT_IRQP			(0x1 << 2)
#define I2C_STAT_LAB			(0x1 << 3)
#define I2C_STAT_STPD			(0x1 << 4)
#define I2C_STAT_STAD			(0x1 << 5)
#define I2C_STAT_BBB			(0x1 << 6)
#define I2C_STAT_TCB			(0x1 << 7)
#define I2C_STAT_LBST			(0x1 << 8)
#define I2C_STAT_SAMB			(0x1 << 9)
#define I2C_STAT_SRGC			(0x1 << 10)

#define I2C_BUS_ERR_MSK			(I2C_STAT_LAB | I2C_STAT_BEB)

/* I2Cx_CMD */
#define I2C_CMD_SBE			(0x1 << 0)
#define I2C_CMD_AS_MASK			(0x7 << 1)
#define I2C_CMD_AS(x)			(((x) & 0x7) << 1)
#define I2C_CMD_RBE			(0x1 << 4)
#define I2C_CMD_SAS_MASK		(0x7 << 5)
#define I2C_CMD_SAS(x)			(((x) & 0x7) << 5)
#define I2C_CMD_DE			(0x1 << 8)
#define I2C_CMD_NS			(0x1 << 9)
#define I2C_CMD_SE			(0x1 << 10)
#define I2C_CMD_MSS			(0x1 << 11)
#define I2C_CMD_WRS			(0x1 << 12)
#define I2C_CMD_EXEC			(0x1 << 15)

/* I2Cx_FIFOCTL */
#define I2C_FIFOCTL_NIB			(0x1 << 0)
#define I2C_FIFOCTL_RFR			(0x1 << 1)
#define I2C_FIFOCTL_TFR			(0x1 << 2)

/* I2Cx_FIFOSTAT */
#define I2C_FIFOSTAT_CECB		(0x1 << 0)
#define I2C_FIFOSTAT_RNB		(0x1 << 1)
#define I2C_FIFOSTAT_RFE		(0x1 << 2)
#define I2C_FIFOSTAT_RFF		(0x1 << 3)
#define I2C_FIFOSTAT_TFE		(0x1 << 4)
#define I2C_FIFOSTAT_TFF		(0x1 << 5)
#define I2C_FIFOSTAT_WRS		(0x1 << 6)
#define I2C_FIFOSTAT_RFD_MASK		(0xff << 8)
#define I2C_FIFOSTAT_RFD_SHIFT		(8)
#define I2C_FIFOSTAT_TFD_MASK		(0xff << 16)
#define I2C_FIFOSTAT_TFD_SHIFT		(16)

/* extract fifo level from fifostat */
#define I2C_RX_FIFO_LEVEL(x)		(((x) >> 8) & 0xff)
#define I2C_TX_FIFO_LEVEL(x)		(((x) >> 16) & 0xff)

enum i2c_freq_mode {
	I2C_FREQ_MODE_HDMI,		/* up to 87 Kb/s */
	I2C_FREQ_MODE_STANDARD,		/* up to 100 Kb/s */
	I2C_FREQ_MODE_FAST,		/* up to 400 Kb/s */
	I2C_FREQ_MODE_HIGH_SPEED,	/* up to 3.4 Mb/s */
};

enum i2c_state {
	STATE_INVALID,
	STATE_READ_DATA,
	STATE_WRITE_DATA,
	STATE_TRANSFER_OVER,
	STATE_TRANSFER_ERROR,
};

struct owl_i2c_dev {
	struct i2c_adapter	adapter;
	void __iomem		*base;
	struct device		*dev;

	phys_addr_t		phys;
	int			irq;
	int			clk_freq;
	enum i2c_freq_mode	freq_mode;
	struct clk		*clk;

	struct i2c_msg		*curr_msg;
	unsigned int		msg_ptr;
	struct completion	cmd_complete;
	enum i2c_state		state;
};

static int debug_level = OWL_I2C_DEFAULT_DBG_LEVEL;
module_param(debug_level, uint, 0644);
MODULE_PARM_DESC(debug_level, "module debug level (0=off,1=on,2=verbose)");

/*
 * Debug macros
 */
#define i2c_dbg(i2c_dev, format, args...)	\
	do { \
		if (debug_level >= OWL_I2C_DBG_LEVEL_ON) \
			dev_dbg((i2c_dev)->dev, \
				"[i2c%d] %s: " format, \
				(i2c_dev)->adapter.nr, __func__, ##args); \
	} while (0)

#define i2c_vdbg(i2c_dev, format, args...)	\
	do { \
		if (debug_level >= OWL_I2C_DBG_LEVEL_VERBOSE) \
			dev_dbg((i2c_dev)->dev, \
				"[i2c%d]  " format, \
				(i2c_dev)->adapter.nr, ##args); \
	} while (0)

#define i2c_err(i2c_dev, format, args...) \
	dev_err((i2c_dev)->dev, "[i2c%d] " format, \
		(i2c_dev)->adapter.nr, ##args)

#define i2c_warn(i2c_dev, format, args...) \
	dev_warn((i2c_dev)->dev, "[i2c%d] " format, \
		(i2c_dev)->adapter.nr, ##args)

#define i2c_info(i2c_dev, format, args...) \
	dev_info((i2c_dev)->dev, "[i2c%d] " format, \
		(i2c_dev)->adapter.nr, ##args)


static inline void owl_i2c_writel(struct owl_i2c_dev *dev,
			u32 val, unsigned int reg)
{
	i2c_vdbg(dev, "-->> write 0x%02x to 0x%08x\n", val,
			(u32)(dev->phys + reg));
	writel(val, dev->base + reg);
}

static inline u32 owl_i2c_readl(struct owl_i2c_dev *dev, unsigned int reg)
{
	return readl(dev->base + reg);
}

#ifdef DEBUG
static void owl_i2c_dump_regs(struct owl_i2c_dev *dev)
{
	i2c_dbg(dev, "dump phys %08x regs:\n"
		"  ctl:      %.8x  clkdiv: %.8x  stat:    %.8x\n"
		"  addr:     %.8x  cmd:    %.8x  fifoctl: %.8x\n"
		"  fifostat: %.8x  datcnt: %.8x  rcnt:    %.8x\n",
		(u32)dev->phys,
		owl_i2c_readl(dev, I2C_CTL),
		owl_i2c_readl(dev, I2C_CLKDIV),
		owl_i2c_readl(dev, I2C_STAT),
		owl_i2c_readl(dev, I2C_ADDR),
		owl_i2c_readl(dev, I2C_CMD),
		owl_i2c_readl(dev, I2C_FIFOCTL),
		owl_i2c_readl(dev, I2C_FIFOSTAT),
		owl_i2c_readl(dev, I2C_DATCNT),
		owl_i2c_readl(dev, I2C_RCNT));
}

static void owl_i2c_dump_mem(char *label, void *base, int len)
{
	int i, j;
	char *data = base;
	char buf[10], line[80];

	/* only for verbose debug */
	if (debug_level < OWL_I2C_DBG_LEVEL_VERBOSE)
		return;

	pr_debug("%s: dump of %d bytes of data at 0x%p\n",
		label, len, data);

	for (i = 0; i < len; i += 16) {
		sprintf(line, "%.8x: ", i);
		for (j = 0; j < 16; j++) {
			if ((i + j < len))
				sprintf(buf, "%02x ", data[i + j]);
			else
				sprintf(buf, "   ");
			strcat(line, buf);
		}
		strcat(line, " ");
		buf[1] = 0;
		for (j = 0; (j < 16) && (i + j < len); j++) {
			buf[0] = isprint(data[i + j]) ? data[i + j] : '.';
			strcat(line, buf);
		}
		pr_debug("%s\n", line);
	}
}
#else
static void owl_i2c_dump_regs(struct owl_i2c_dev *dev)
{
}

static inline void owl_i2c_dump_mem(char *label, void *base, int len)
{
}
#endif

static int owl_i2c_wait_if_busy(struct owl_i2c_dev *dev)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(OWL_I2C_TIMEOUT);

	while (owl_i2c_readl(dev, I2C_STAT) & I2C_STAT_BBB) {
		if (time_after(jiffies, timeout)) {
			i2c_err(dev, "Bus busy timeout, stat 0x%x\n",
				owl_i2c_readl(dev, I2C_STAT));
			owl_i2c_dump_regs(dev);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static inline void owl_i2c_reset_fifo(struct owl_i2c_dev *dev)
{
	unsigned int val;

	val = owl_i2c_readl(dev, I2C_FIFOCTL);
	val |= I2C_FIFOCTL_RFR | I2C_FIFOCTL_TFR;
	owl_i2c_writel(dev, val, I2C_FIFOCTL);

	/* wait fifo reset complete */
	do {
		val = owl_i2c_readl(dev, I2C_FIFOCTL);
		if (!(val & (I2C_FIFOCTL_RFR | I2C_FIFOCTL_TFR)))
			break;
	} while (1);
}

static inline void owl_i2c_clear_status(struct owl_i2c_dev *dev)
{
	unsigned int val;

	val = owl_i2c_readl(dev, I2C_STAT);
	owl_i2c_writel(dev, val, I2C_STAT);

	owl_i2c_reset_fifo(dev);
}

static void owl_i2c_reset(struct owl_i2c_dev *dev)
{
	/* reset i2c controller */
	owl_i2c_writel(dev, 0, I2C_CTL);
	owl_i2c_writel(dev, I2C_CTL_EN, I2C_CTL);

	owl_i2c_reset_fifo(dev);
}

static void owl_i2c_set_freq(struct owl_i2c_dev *dev)
{
	u32 pclk, div_factor;

	if (dev->clk_freq == 0)
		return;

	pclk = clk_get_rate(dev->clk);

	div_factor = (pclk + dev->clk_freq * 16 - 1) / (dev->clk_freq * 16);
	owl_i2c_writel(dev, I2C_CLKDIV_DIV(div_factor), I2C_CLKDIV);

	return;
}

static int owl_i2c_hwinit(struct owl_i2c_dev *dev)
{
	owl_i2c_writel(dev, I2C_CTL_EN, I2C_CTL);
	owl_i2c_clear_status(dev);

	owl_i2c_set_freq(dev);

	return 0;
}

static void owl_i2c_force_stop(struct owl_i2c_dev *dev, int addr)
{
	i2c_warn(dev, "before send stop i2c_stat 0x%x",
		owl_i2c_readl(dev, I2C_STAT));

	/* reset the i2c controller to stop current cmd */
	owl_i2c_reset(dev);
	owl_i2c_hwinit(dev);

	/* send start command */
	owl_i2c_writel(dev, addr, I2C_TXDAT);
	owl_i2c_writel(dev,
		I2C_CTL_EN | I2C_CTL_GBCC_START | I2C_CTL_RB, I2C_CTL);

	mdelay(1);

	/* send stop command */
	owl_i2c_writel(dev,
		I2C_CTL_EN | I2C_CTL_GBCC_STOP | I2C_CTL_RB, I2C_CTL);

	mdelay(1);

	/* Make sure the hardware is ready */
	if (owl_i2c_readl(dev, I2C_STAT) & I2C_STAT_BBB) {
		i2c_warn(dev, "cannot clear bus busy, i2c_stat 0x%x",
			owl_i2c_readl(dev, I2C_STAT));
	}

	/* clear stop command */
	owl_i2c_writel(dev, I2C_CTL_EN, I2C_CTL);
	owl_i2c_clear_status(dev);
}

static irqreturn_t owl_i2c_interrupt(int irq, void *dev_id)
{
	struct owl_i2c_dev *dev = dev_id;
	unsigned int stat, fifostat;
	struct i2c_msg *msg = dev->curr_msg;

	stat = owl_i2c_readl(dev, I2C_STAT);
	fifostat = owl_i2c_readl(dev, I2C_FIFOSTAT);

	i2c_vdbg(dev, "msg len %d, msg_ptr %d, stat 0x%x, fifostat 0x%x\n",
		msg->len, dev->msg_ptr, stat, fifostat);

	if (fifostat & I2C_FIFOSTAT_RNB) {
		i2c_warn(dev, "no ACK from device");
		owl_i2c_dump_regs(dev);
		dev->state = STATE_TRANSFER_ERROR;
		goto stop;
	} else if (stat & I2C_STAT_LAB) {
		i2c_warn(dev, "lose arbitration");
		owl_i2c_dump_regs(dev);
		dev->state = STATE_TRANSFER_ERROR;
		goto stop;
	} else if (stat & I2C_STAT_BEB) {
		i2c_warn(dev, "bus error");
		owl_i2c_dump_regs(dev);
		dev->state = STATE_TRANSFER_ERROR;
		goto stop;
	}

	if (msg->flags & I2C_M_RD) {
		while ((owl_i2c_readl(dev, I2C_FIFOSTAT) & I2C_FIFOSTAT_RFE)
				&& dev->msg_ptr < msg->len) {
			msg->buf[dev->msg_ptr++] =
					owl_i2c_readl(dev, I2C_RXDAT);
		}
	} else {
		while (!(owl_i2c_readl(dev, I2C_FIFOSTAT) & I2C_FIFOSTAT_TFF)
			   && dev->msg_ptr < msg->len) {
			owl_i2c_writel(dev, msg->buf[dev->msg_ptr++],
					I2C_TXDAT);
		}
	}

	if (dev->msg_ptr == msg->len)
		dev->state = STATE_TRANSFER_OVER;

stop:
	stat = owl_i2c_readl(dev, I2C_STAT);
	owl_i2c_writel(dev, I2C_STAT_IRQP, I2C_STAT);

	if (dev->state == STATE_TRANSFER_ERROR) {
		i2c_dbg(dev, "reset i2c");
		owl_i2c_reset(dev);
	}

	if (dev->state == STATE_TRANSFER_ERROR
		|| dev->state == STATE_TRANSFER_OVER) {
		i2c_dbg(dev, "complete_all\n");
		complete_all(&dev->cmd_complete);
	}

	return IRQ_HANDLED;
}

/*
 * Validate message format
 *
 * return 0 if messages is invalid; otherwise return 1
*/
static int msgs_is_valid(struct owl_i2c_dev *dev, struct i2c_msg *msgs, int num)
{
	if (num > 2) {
		i2c_warn(dev,
			"cannot handle more than two concatenated messages.\n");
		return 0;
	} else if (num == 2) {
		/* 2 message */
		if (msgs[0].len > OWL_I2C_MAX_INTER_ADDR_LEN) {
			i2c_warn(dev, "internal address length must < %d bytes.\n",
				OWL_I2C_MAX_INTER_ADDR_LEN);
			return 0;
		}

		if (msgs[0].flags & I2C_M_RD) {
			i2c_warn(dev, "first transfer must be write.\n");
			return 0;
		}

		if (msgs[1].len > OWL_I2C_MAX_MSG_LEN) {
			i2c_warn(dev, "message length must < %d bytes.\n",
				OWL_I2C_MAX_MSG_LEN);
			return 0;
		}
	} else {
		/* 1 message */
		if (msgs[0].len > OWL_I2C_MAX_MSG_LEN) {
			i2c_warn(dev, "message length must < %d bytes.\n",
				OWL_I2C_MAX_MSG_LEN);
			return 0;
		}
	}

	return 1;
}

/*
 * The hardware can handle at most two messages concatenated by a
 * repeated start via it's internal address feature.
 *
 * For simplicity, only support the following data pattern:
 *  1) 1 message,  msg[0] write (MAX 240 bytes)
 *  2) 1 message,  msg[0] read (MAX 240 bytes)
 *  3) 2 message,  msg[0] write internal address (MAX 6 bytes)
 *                 msg[1] write (MAX 240 bytes)
 *  4) 2 message,  msg[0] write internal address (MAX 6 bytes)
 *                 msg[1] read (MAX 240 bytes)
 */
static int owl_i2c_do_transfer(struct owl_i2c_dev *dev,
		struct i2c_msg *msgs, int num)
{
	unsigned int addr = (msgs[0].addr & 0x7f) << 1;
	struct i2c_msg *msg;
	int i, ret;
	unsigned int fifo_cmd;
	unsigned long time_left = msecs_to_jiffies(OWL_I2C_TIMEOUT);

	/* validate messages format */
	if (!msgs_is_valid(dev, msgs, num))
		return -EINVAL;

	init_completion(&dev->cmd_complete);

	/* enable I2C controller IRQ */
	owl_i2c_writel(dev, I2C_CTL_IRQE | I2C_CTL_EN, I2C_CTL);

	fifo_cmd = I2C_CMD_EXEC | I2C_CMD_MSS | I2C_CMD_SE | I2C_CMD_DE
		| I2C_CMD_NS | I2C_CMD_SBE;

	if (num == 2) {
		/* set internal address and restart cmd for read operation */
		fifo_cmd |= I2C_CMD_AS(msgs[0].len + 1) | I2C_CMD_SAS(1)
				| I2C_CMD_RBE;

		/* write i2c device address */
		owl_i2c_writel(dev, addr, I2C_TXDAT);

		/* write internal register address */
		for (i = 0; i < msgs[0].len; i++)
			owl_i2c_writel(dev, msgs[0].buf[i], I2C_TXDAT);

		msg = &msgs[1];
	} else {
		/* only send device addess for 1 message */
		fifo_cmd |= I2C_CMD_AS(1);
		msg = &msgs[0];
	}

	dev->curr_msg = msg;
	dev->msg_ptr = 0;

	 /* set data count for the message */
	owl_i2c_writel(dev, msg->len, I2C_DATCNT);

	if (msg->flags & I2C_M_RD) {
		/* read from device, with WR bit */
		owl_i2c_writel(dev, addr | 1, I2C_TXDAT);
		dev->state = STATE_READ_DATA;
	} else {
		/* write to device */
		owl_i2c_writel(dev, addr, I2C_TXDAT);

		/* Write data to FIFO */
		for (i = 0; i < msg->len; i++) {
			if (owl_i2c_readl(dev, I2C_FIFOSTAT) & I2C_FIFOSTAT_TFF)
				break;

			owl_i2c_writel(dev, msg->buf[i], I2C_TXDAT);
		}

		dev->msg_ptr = i;
		dev->state = STATE_WRITE_DATA;
	}

	/* Ingore the NACK if needed */
	if (msg->flags & I2C_M_IGNORE_NAK)
		owl_i2c_writel(dev, I2C_FIFOCTL_NIB, I2C_FIFOCTL);
	else
		owl_i2c_writel(dev, 0, I2C_FIFOCTL);

	/* write fifo command to start transfer */
	owl_i2c_writel(dev, fifo_cmd, I2C_CMD);

	/* Wait for transfer over or error */
	time_left = wait_for_completion_timeout(&dev->cmd_complete, time_left);
	if ((dev->state == STATE_TRANSFER_OVER)) {
		ret = 0;
	} else if (time_left == 0) {
		i2c_err(dev, "transfer timed out\n");
		ret = -EREMOTEIO;
	} else {
		i2c_err(dev, "transfer error\n");
		ret = -ENXIO;
	}

	if (ret < 0) {
		i2c_dbg(dev, "transfer error, ret %d\n", ret);

		owl_i2c_force_stop(dev, addr);
		owl_i2c_dump_regs(dev);
	}

	/* disable i2c controller */
	owl_i2c_writel(dev, 0, I2C_CTL);

	return ret;
}

static int owl_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
		int num)
{
	struct owl_i2c_dev *dev = i2c_get_adapdata(adap);
	int i, ret;

	i2c_dbg(dev, "msg num %d\n", num);

	/* dump messages for debug */
	for (i = 0; i < num; i++) {
		i2c_dbg(dev, "  msg[%d]: addr 0x%x, len %d, flags 0x%x\n",
			i, msgs[i].addr, msgs[i].len, msgs[i].flags);

		if (!(msgs[i].flags & I2C_M_RD))
			owl_i2c_dump_mem("[msg] write buf", msgs[i].buf,
				msgs[i].len);
	}

	owl_i2c_hwinit(dev);

	/* Make sure the hardware is ready */
	ret = owl_i2c_wait_if_busy(dev);
	if (ret < 0) {
		i2c_warn(dev, "bus busy before transfer\n");
		return ret;
	}

	/* Do transfer */
	ret = owl_i2c_do_transfer(dev, msgs, num);
	if (ret)
		return ret;

	/* dump messages for debug */
	if (num == 2 && msgs[1].flags & I2C_M_RD)
		owl_i2c_dump_mem("[msg] read buf", msgs[1].buf, msgs[1].len);

	return (ret < 0) ? ret : i;
}

static u32 owl_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static struct i2c_algorithm owl_i2c_algorithm = {
	.master_xfer    = owl_i2c_xfer,
	.functionality  = owl_i2c_func,
};

static int owl_i2c_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct owl_i2c_dev *dev;
	struct i2c_adapter *adap;
	struct resource *res;
	int ret;
	u32 phy_addr;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	init_completion(&dev->cmd_complete);
	dev->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;
	phy_addr = res->start;

	dev->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dev->base))
		return PTR_ERR(dev->base);
	dev->phys = res->start;

	dev->irq = platform_get_irq(pdev, 0);
	if (dev->irq < 0)
		return dev->irq;

	ret = devm_request_irq(&pdev->dev, dev->irq, owl_i2c_interrupt, 0,
			 dev_name(dev->dev), dev);
	if (ret) {
		dev_err(dev->dev, "Cannot get irq %d: %d\n", dev->irq, ret);
		return ret;
	}

	platform_set_drvdata(pdev, dev);

	dev->clk = devm_clk_get(dev->dev, NULL);
	if (IS_ERR(dev->clk)) {
		dev_err(dev->dev, "no clock defined\n");
		return -ENODEV;
	}
	clk_prepare_enable(dev->clk);

	if (np) {
		/* Default to 100 kHz if no frequency is given in the node */
		if (of_property_read_u32(np, "clock-frequency", &dev->clk_freq))
			dev->clk_freq = OWL_I2C_DEFAULT_FREQ;

		if (dev->clk_freq <= 100000)
			dev->freq_mode = I2C_FREQ_MODE_STANDARD;
		else
			dev->freq_mode = I2C_FREQ_MODE_FAST;
	}

	adap = &dev->adapter;
	
	snprintf(dev->adapter.name, sizeof(dev->adapter.name), "OWL I2C adapter");
	
	i2c_set_adapdata(adap, dev);
	
	adap->owner = THIS_MODULE;
	adap->class = I2C_CLASS_HWMON;
	adap->algo = &owl_i2c_algorithm;
	adap->timeout = OWL_I2C_TIMEOUT;
	adap->dev.parent = dev->dev;
	adap->dev.of_node = pdev->dev.of_node;
	adap->nr = pdev->id;
	
	ret = i2c_add_numbered_adapter(adap);
	
	if (ret) {
		dev_err(dev->dev, "Adapter %s registration failed\n",
			adap->name);
		clk_disable_unprepare(dev->clk);
		return ret;
	}



	dev_info(dev->dev, "I2C adapter ready to operate.\n");

	return 0;
}


static int owl_i2c_remove(struct platform_device *pdev)
{
	struct owl_i2c_dev *dev = platform_get_drvdata(pdev);

	i2c_del_adapter(&dev->adapter);
	clk_disable_unprepare(dev->clk);

	return 0;
}

static const struct of_device_id owl_i2c_dt_ids[] = {
	{.compatible = "caninos,k9-i2c"},
	{.compatible = "caninos,k7-i2c"},
	{},
};
MODULE_DEVICE_TABLE(of, owl_i2c_dt_ids);

static struct platform_driver owl_i2c_driver = {
	.probe		= owl_i2c_probe,
	.remove		= owl_i2c_remove,
	.driver		= {
		.name	= "i2c-caninos",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(owl_i2c_dt_ids),
	},
};

static int __init owl_i2c_init(void)
{
	pr_info("[OWL] I2C controller initialization\n");

	return platform_driver_register(&owl_i2c_driver);
}

static void __exit owl_i2c_exit(void)
{
	platform_driver_unregister(&owl_i2c_driver);
}

subsys_initcall(owl_i2c_init);
module_exit(owl_i2c_exit);

MODULE_AUTHOR("David Liu <liuwei@actions-semi.com>");
MODULE_DESCRIPTION("I2C driver for Actions OWL SoCs");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:i2c-owl");
