/*
 * Caninos UART
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

#define DRIVER_NAME "caninos-uart"
#define pr_fmt(fmt) DRIVER_NAME": "fmt

#include <linux/clk.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/reset.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/pinctrl/consumer.h>

#define UART_PORT_NUM 7
#define UART_DEV_NAME "ttyS"

#define UART_CTL   0x000
#define UART_RXDAT 0x004
#define UART_TXDAT 0x008
#define UART_STAT  0x00c

#define UART_CTL_DWLS_MASK  GENMASK(1, 0)
#define UART_CTL_PRS_MASK   GENMASK(6, 4)
#define UART_STAT_TRFL_MASK	GENMASK(16, 11)
#define UART_CTL_DWLS_5BITS (0x0 << 0)
#define UART_CTL_DWLS_6BITS (0x1 << 0)
#define UART_CTL_DWLS_7BITS	(0x2 << 0)
#define UART_CTL_DWLS_8BITS	(0x3 << 0)
#define UART_CTL_PRS_NONE   (0x0 << 4)
#define UART_CTL_PRS_ODD    (0x4 << 4)
#define UART_CTL_PRS_MARK   (0x5 << 4)
#define UART_CTL_PRS_EVEN   (0x6 << 4)
#define UART_CTL_PRS_SPACE  (0x7 << 4)
#define UART_CTL_AFE        BIT(12)
#define UART_CTL_TRFS_TX    BIT(14)
#define UART_CTL_EN	        BIT(15)
#define UART_CTL_RXDE       BIT(16)
#define UART_CTL_TXDE       BIT(17)
#define UART_CTL_RXIE       BIT(18)
#define UART_CTL_TXIE       BIT(19)
#define UART_CTL_LBEN       BIT(20)
#define UART_CTL_STPS_2BITS BIT(2)
#define UART_STAT_RIP       BIT(0)
#define UART_STAT_TIP       BIT(1)
#define UART_STAT_RXER      BIT(2)
#define UART_STAT_TFER      BIT(3)
#define UART_STAT_RXST      BIT(4)
#define UART_STAT_RFEM      BIT(5)
#define UART_STAT_TFFU      BIT(6)
#define UART_STAT_CTSS      BIT(7)
#define UART_STAT_RTSS      BIT(8)
#define UART_STAT_TFES      BIT(10)
#define UART_STAT_UTBB      BIT(17)

static struct uart_driver caninos_uart_driver;

struct caninos_uart_info {
	unsigned int tx_fifosize;
};

struct caninos_uart_port
{
	struct uart_port port;
	struct clk *clk;
	struct reset_control *rst;
	struct pinctrl *pctl;
	struct pinctrl_state *def_state;
	struct pinctrl_state *extio_state;
};

#define to_caninos_uart_port(x) container_of(x, struct caninos_uart_port, port)

static struct caninos_uart_port *caninos_uart_ports[UART_PORT_NUM];

static inline void uart_writel(struct uart_port *port, u32 val, unsigned int off)
{
	writel(val, port->membase + off);
}

static inline u32 uart_readl(struct uart_port *port, unsigned int off)
{
	return readl(port->membase + off);
}

static void caninos_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	u32 ctl = uart_readl(port, UART_CTL);
	
	if (mctrl & TIOCM_LOOP) {
		ctl |= UART_CTL_LBEN;
	}
	else {
		ctl &= ~UART_CTL_LBEN;
	}
	
	uart_writel(port, ctl, UART_CTL);
}

static unsigned int caninos_uart_get_mctrl(struct uart_port *port)
{
	unsigned int mctrl = TIOCM_CAR | TIOCM_DSR;
	u32 stat, ctl;
	
	ctl = uart_readl(port, UART_CTL);
	stat = uart_readl(port, UART_STAT);
	
	if (stat & UART_STAT_RTSS) {
		mctrl |= TIOCM_RTS;
	}
	if ((stat & UART_STAT_CTSS) || !(ctl & UART_CTL_AFE)) {
		mctrl |= TIOCM_CTS;
	}
	
	return mctrl;
}

static unsigned int caninos_uart_tx_empty(struct uart_port *port)
{
	unsigned long flags, ret;
	u32 val;
	
	spin_lock_irqsave(&port->lock, flags);
	
	val = uart_readl(port, UART_STAT);
	ret = (val & UART_STAT_TFES) ? TIOCSER_TEMT : 0;
	
	spin_unlock_irqrestore(&port->lock, flags);
	return ret;
}

static void caninos_uart_stop_rx(struct uart_port *port)
{
	u32 val;
	
	val = uart_readl(port, UART_CTL);
	val &= ~(UART_CTL_RXIE | UART_CTL_RXDE);
	uart_writel(port, val, UART_CTL);
	
	val = uart_readl(port, UART_STAT);
	val |= UART_STAT_RIP;
	uart_writel(port, val, UART_STAT);
}

static void caninos_uart_stop_tx(struct uart_port *port)
{
	u32 val;
	
	val = uart_readl(port, UART_CTL);
	val &= ~(UART_CTL_TXIE | UART_CTL_TXDE);
	uart_writel(port, val, UART_CTL);
	
	val = uart_readl(port, UART_STAT);
	val |= UART_STAT_TIP;
	uart_writel(port, val, UART_STAT);
}

static void caninos_uart_start_tx(struct uart_port *port)
{
	u32 val;
	
	if (uart_tx_stopped(port)) {
		caninos_uart_stop_tx(port);
		return;
	}
	
	val = uart_readl(port, UART_STAT);
	val |= UART_STAT_TIP;
	uart_writel(port, val, UART_STAT);
	
	val = uart_readl(port, UART_CTL);
	val |= UART_CTL_TXIE;
	uart_writel(port, val, UART_CTL);
}

static void caninos_uart_send_chars(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	unsigned int ch;

	if (uart_tx_stopped(port)) {
		return;
	}

	if (port->x_char)
	{
		while (!(uart_readl(port, UART_STAT) & UART_STAT_TFFU)) {
			cpu_relax();
		}
		
		uart_writel(port, port->x_char, UART_TXDAT);
		port->icount.tx++;
		port->x_char = 0;
	}
	
	while (!(uart_readl(port, UART_STAT) & UART_STAT_TFFU))
	{
		if (uart_circ_empty(xmit)) {
			break;
		}
		
		ch = xmit->buf[xmit->tail];
		uart_writel(port, ch, UART_TXDAT);
		xmit->tail = (xmit->tail + 1) & (SERIAL_XMIT_SIZE - 1);
		port->icount.tx++;
	}
	
	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) {
		uart_write_wakeup(port);
	}

	if (uart_circ_empty(xmit)) {
		caninos_uart_stop_tx(port);
	}
}

static void caninos_uart_receive_chars(struct uart_port *port)
{
	u32 stat, val;
	
	val = uart_readl(port, UART_CTL);
	val &= ~UART_CTL_TRFS_TX;
	uart_writel(port, val, UART_CTL);
	
	stat = uart_readl(port, UART_STAT);
	
	while (!(stat & UART_STAT_RFEM))
	{
		char flag = TTY_NORMAL;
		
		if (stat & UART_STAT_RXER) {
			port->icount.overrun++;
		}
		
		if (stat & UART_STAT_RXST)
		{
			port->icount.brk++;
			port->icount.frame++;
			
			stat &= port->read_status_mask;
			
			if (stat & UART_STAT_RXST) {
				flag = TTY_PARITY;
			}
		}
		else {
			port->icount.rx++;
		}
		
		val = uart_readl(port, UART_RXDAT);
		val &= 0xff;
		
		if ((stat & port->ignore_status_mask) == 0) {
			tty_insert_flip_char(&port->state->port, val, flag);
		}
		
		stat = uart_readl(port, UART_STAT);
	}
	
	spin_unlock(&port->lock);
	tty_flip_buffer_push(&port->state->port);
	spin_lock(&port->lock);
}

static irqreturn_t caninos_uart_irq(int irq, void *dev_id)
{
	struct uart_port *port = dev_id;
	unsigned long flags;
	u32 stat;
	
	spin_lock_irqsave(&port->lock, flags);
	
	stat = uart_readl(port, UART_STAT);
	
	if (stat & UART_STAT_RIP) {
		caninos_uart_receive_chars(port);
	}

	if (stat & UART_STAT_TIP) {
		caninos_uart_send_chars(port);
	}
	
	stat = uart_readl(port, UART_STAT);
	stat |= UART_STAT_RIP | UART_STAT_TIP;
	uart_writel(port, stat, UART_STAT);
	
	spin_unlock_irqrestore(&port->lock, flags);
	
	return IRQ_HANDLED;
}

static void caninos_uart_shutdown(struct uart_port *port)
{
	struct caninos_uart_port *_port = to_caninos_uart_port(port);
	unsigned long flags;
	u32 val;
	
	spin_lock_irqsave(&port->lock, flags);
	
	val = uart_readl(port, UART_CTL);
	val &= ~(UART_CTL_TXIE | UART_CTL_RXIE
		| UART_CTL_TXDE | UART_CTL_RXDE | UART_CTL_EN);
	uart_writel(port, val, UART_CTL);
	
	spin_unlock_irqrestore(&port->lock, flags);
	
	free_irq(port->irq, port);
	
	if (_port->extio_state) {
		pinctrl_select_state(_port->pctl, _port->def_state);
	}
}

static int caninos_uart_startup(struct uart_port *port)
{
	struct caninos_uart_port *_port = to_caninos_uart_port(port);
	unsigned long flags;
	u32 val;
	int ret;
	
	if (_port->extio_state)
	{
		ret = pinctrl_select_state(_port->pctl, _port->extio_state);
	
		if (ret < 0) {
			return -EAGAIN;
		}
	}
	
	ret = request_irq(port->irq, caninos_uart_irq, IRQF_TRIGGER_HIGH,
		DRIVER_NAME, port);
	
	if (ret) {
		return ret;
	}
	
	spin_lock_irqsave(&port->lock, flags);

	val = uart_readl(port, UART_STAT);
	val |= UART_STAT_RIP | UART_STAT_TIP
		| UART_STAT_RXER | UART_STAT_TFER | UART_STAT_RXST;
	uart_writel(port, val, UART_STAT);
	
	val = uart_readl(port, UART_CTL);
	val |= UART_CTL_RXIE | UART_CTL_TXIE;
	val |= UART_CTL_EN;
	uart_writel(port, val, UART_CTL);
	
	spin_unlock_irqrestore(&port->lock, flags);
	
	return 0;
}

static void 
caninos_uart_set_termios(struct uart_port *port,
	struct ktermios *termios, struct ktermios *old)
{
	unsigned int baud;
	unsigned long flags;
	u32 ctl;
	
	spin_lock_irqsave(&port->lock, flags);
	
	ctl = uart_readl(port, UART_CTL);
	ctl &= ~UART_CTL_DWLS_MASK;
	
	switch (termios->c_cflag & CSIZE)
	{
	case CS5:
		ctl |= UART_CTL_DWLS_5BITS;
		break;
	case CS6:
		ctl |= UART_CTL_DWLS_6BITS;
		break;
	case CS7:
		ctl |= UART_CTL_DWLS_7BITS;
		break;
	case CS8:
	default:
		ctl |= UART_CTL_DWLS_8BITS;
		break;
	}
	
	if (termios->c_cflag & CSTOPB) {
		ctl |= UART_CTL_STPS_2BITS;
	}
	else {
		ctl &= ~UART_CTL_STPS_2BITS;
	}
	
	ctl &= ~UART_CTL_PRS_MASK;
	
	if (termios->c_cflag & PARENB)
	{
		if (termios->c_cflag & CMSPAR)
		{
			if (termios->c_cflag & PARODD) {
				ctl |= UART_CTL_PRS_MARK;
			}
			else {
				ctl |= UART_CTL_PRS_SPACE;
			}
		}
		else if (termios->c_cflag & PARODD) {
			ctl |= UART_CTL_PRS_ODD;
		}
		else {
			ctl |= UART_CTL_PRS_EVEN;
		}
	}
	else {
		ctl |= UART_CTL_PRS_NONE;
	}

	if (termios->c_cflag & CRTSCTS) {
		ctl |= UART_CTL_AFE;
	}
	else {
		ctl &= ~UART_CTL_AFE;
	}
	
	uart_writel(port, ctl, UART_CTL);
	
	baud = uart_get_baud_rate(port, termios, old, 9600, 3200000);
	
	clk_set_rate(to_caninos_uart_port(port)->clk, baud * 8);
	
	/* Don't rewrite B0 */
	if (tty_termios_baud_rate(termios)) {
		tty_termios_encode_baud_rate(termios, baud, baud);
	}
	
	port->read_status_mask |= UART_STAT_RXER;
	
	if (termios->c_iflag & INPCK) {
		port->read_status_mask |= UART_STAT_RXST;
	}
	
	uart_update_timeout(port, termios->c_cflag, baud);
	
	spin_unlock_irqrestore(&port->lock, flags);
}

static void caninos_uart_release_port(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	struct resource *res;
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res) {
		return;
	}
	
	if (port->flags & UPF_IOREMAP)
	{
		devm_release_mem_region(port->dev, port->mapbase,
			resource_size(res));
		devm_iounmap(port->dev, port->membase);
		port->membase = NULL;
	}
}

static int caninos_uart_request_port(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res) {
		return -ENXIO;
	}
	
	if (!devm_request_mem_region(port->dev, port->mapbase,
		resource_size(res), dev_name(port->dev))) {
		return -EBUSY;
	}
	
	if (port->flags & UPF_IOREMAP)
	{
		port->membase = devm_ioremap_nocache(port->dev, port->mapbase,
			resource_size(res));
		
		if (!port->membase) {
			return -EBUSY;
		}
	}
	return 0;
}

static const char *caninos_uart_type(struct uart_port *port)
{
	return (port->type == PORT_OWL) ? DRIVER_NAME : NULL;
}

static int
caninos_uart_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	if (port->type != PORT_OWL) {
		return -EINVAL;
	}
	
	if (port->irq != ser->irq) {
		return -EINVAL;
	}
	
	return 0;
}

static void caninos_uart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
	{
		port->type = PORT_OWL;
		caninos_uart_request_port(port);
	}
}

static const struct uart_ops caninos_uart_ops = {
	.set_mctrl    = caninos_uart_set_mctrl,
	.get_mctrl    = caninos_uart_get_mctrl,
	.tx_empty     = caninos_uart_tx_empty,
	.start_tx     = caninos_uart_start_tx,
	.stop_rx      = caninos_uart_stop_rx,
	.stop_tx      = caninos_uart_stop_tx,
	.startup      = caninos_uart_startup,
	.shutdown     = caninos_uart_shutdown,
	.set_termios  = caninos_uart_set_termios,
	.type         = caninos_uart_type,
	.config_port  = caninos_uart_config_port,
	.request_port = caninos_uart_request_port,
	.release_port = caninos_uart_release_port,
	.verify_port  = caninos_uart_verify_port,
};

#ifdef CONFIG_SERIAL_CANINOS_CONSOLE

static void caninos_console_putchar(struct uart_port *port, int ch)
{
	if (!port->membase) {
		return;
	}
	
	while (uart_readl(port, UART_STAT) & UART_STAT_TFFU) {
		cpu_relax();
	}
	
	uart_writel(port, ch, UART_TXDAT);
}

static void 
caninos_uart_port_write(struct uart_port *port, const char *s, u_int count)
{
	unsigned long flags;
	u32 old_ctl, val;
	int locked;
	
	local_irq_save(flags);
	
	if (port->sysrq) {
		locked = 0;
	}
	else if (oops_in_progress) {
		locked = spin_trylock(&port->lock);
	}
	else
	{
		spin_lock(&port->lock);
		locked = 1;
	}
	
	old_ctl = uart_readl(port, UART_CTL);
	val = old_ctl | UART_CTL_TRFS_TX;
	val &= ~(UART_CTL_RXIE | UART_CTL_TXIE); /* disable IRQ */
	uart_writel(port, val, UART_CTL);
	
	uart_console_write(port, s, count, caninos_console_putchar);
	
	/* wait until all contents have been sent out */
	while (uart_readl(port, UART_STAT) & UART_STAT_TRFL_MASK) {
		cpu_relax();
	}
	
	/* clear IRQ pending */
	val = uart_readl(port, UART_STAT);
	val |= UART_STAT_TIP | UART_STAT_RIP;
	uart_writel(port, val, UART_STAT);
	uart_writel(port, old_ctl, UART_CTL);
	
	if (locked) {
		spin_unlock(&port->lock);
	}
	
	local_irq_restore(flags);
}

static void
caninos_uart_console_write(struct console *co, const char *s, u_int count)
{
	struct caninos_uart_port *_port;
	
	_port = caninos_uart_ports[co->index];
	
	if (!_port) {
		return;
	}
	
	caninos_uart_port_write(&_port->port, s, count);
}

static int caninos_uart_console_setup(struct console *co, char *options)
{
	struct caninos_uart_port *_port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	
	if (co->index < 0 || co->index >= UART_PORT_NUM) {
		return -EINVAL;
	}
	
	_port = caninos_uart_ports[co->index];
	
	if (!_port || !_port->port.membase) {
		return -ENODEV;
	}
	
	if (options) {
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	}
	
	return uart_set_options(&_port->port, co, baud, parity, bits, flow);
}

static struct console caninos_uart_console = {
	.name = UART_DEV_NAME,
	.write = caninos_uart_console_write,
	.device = uart_console_device,
	.setup = caninos_uart_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.data = &caninos_uart_driver,
};

static int __init caninos_uart_console_init(void)
{
	register_console(&caninos_uart_console);
	return 0;
}

console_initcall(caninos_uart_console_init);

static void
caninos_uart_early_console_write(struct console *co, const char *s, u_int count)
{
	struct earlycon_device *dev = co->data;
	caninos_uart_port_write(&dev->port, s, count);
}

static int __init
caninos_uart_early_console_setup(struct earlycon_device *device, const char *opt)
{
	if (!device->port.membase) {
		return -ENODEV;
	}
	device->con->write = caninos_uart_early_console_write;
	return 0;
}

OF_EARLYCON_DECLARE(caninos, "caninos,early-uart", caninos_uart_early_console_setup);

#define CANINOS_UART_CONSOLE (&caninos_uart_console)
#else
#define CANINOS_UART_CONSOLE NULL
#endif

static struct uart_driver caninos_uart_driver = {
	.owner = THIS_MODULE,
	.driver_name = DRIVER_NAME,
	.dev_name = UART_DEV_NAME,
	.nr = UART_PORT_NUM,
	.cons = CANINOS_UART_CONSOLE,
};

static const struct caninos_uart_info s500_info = {
	.tx_fifosize = 16,
};

static const struct caninos_uart_info s700_info = {
	.tx_fifosize = 32,
};

static const struct caninos_uart_info s900_info = {
	.tx_fifosize = 32,
};

static const struct of_device_id caninos_uart_dt_matches[] = {
	{ .compatible = "caninos,k5-uart", .data = &s500_info },
	{ .compatible = "caninos,k7-uart", .data = &s700_info },
	{ .compatible = "caninos,k9-uart", .data = &s900_info },
	{ }
};

MODULE_DEVICE_TABLE(of, caninos_uart_dt_matches);

static int caninos_uart_probe(struct platform_device *pdev)
{
	const struct caninos_uart_info *info = NULL;
	const struct of_device_id *match;
	struct caninos_uart_port *port;
	struct resource *res_mem;
	int ret, irq;
	
	if (pdev->dev.of_node)
	{
		pdev->id = of_alias_get_id(pdev->dev.of_node, "serial");
		
		match = of_match_node(caninos_uart_dt_matches, pdev->dev.of_node);
		
		if (match) {
			info = match->data;
		}
	}
	
	if (pdev->id < 0 || pdev->id >= UART_PORT_NUM)
	{
		pr_err("Serial%d is out of range.\n", pdev->id);
		return -EINVAL;
	}
	
	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res_mem)
	{
		pr_err("Could not map uart base registers.\n");
		return -ENODEV;
	}
	
	irq = platform_get_irq(pdev, 0);
	
	if (irq < 0)
	{
		pr_err("Could not get uart IRQ from DTS.\n");
		return -EINVAL;
	}
	
	if (caninos_uart_ports[pdev->id])
	{
		pr_err("Uart%d already allocated.\n", pdev->id);
		return -EBUSY;
	}
	
	port = devm_kzalloc(&pdev->dev, sizeof(*port), GFP_KERNEL);
	
	if (!port)
	{
		pr_err("Could not allocate uart%d internal data.\n", pdev->id);
		return -ENOMEM;
	}
	
	port->clk = devm_clk_get(&pdev->dev, NULL);
	
	if (IS_ERR(port->clk))
	{
		pr_err("Could not get uart%d clock.\n", pdev->id);
		return PTR_ERR(port->clk);
	}

	port->rst = devm_reset_control_get(&pdev->dev, NULL);
	
	if (IS_ERR(port->rst))
	{
		pr_err("Could not get uart%d reset control.\n", pdev->id);
		return PTR_ERR(port->rst);
	}
	
	port->pctl = devm_pinctrl_get(&pdev->dev);
	
	if (IS_ERR(port->pctl))
	{
		dev_err(&pdev->dev, "devm_pinctrl_get() failed\n");
		return PTR_ERR(port->pctl);
	}
	
	port->def_state = pinctrl_lookup_state(port->pctl, PINCTRL_STATE_DEFAULT);
	
	if (IS_ERR(port->def_state))
	{
		dev_err(&pdev->dev, "could not get pinctrl default state\n");
		return PTR_ERR(port->def_state);
	}
	
	port->extio_state = pinctrl_lookup_state(port->pctl, "extio");
	
	if (IS_ERR(port->extio_state)) {
		port->extio_state = NULL; // it is optional
	}
	
	ret = pinctrl_select_state(port->pctl, port->def_state);
	
	if (ret < 0)
	{
		dev_err(&pdev->dev, "could not select default pinctrl state\n");
		return ret;
	}
	
	clk_prepare_enable(port->clk);
	
	clk_set_rate(port->clk, 115200 * 8);
	
	reset_control_deassert(port->rst);
	
	port->port.dev = &pdev->dev;
	port->port.line = pdev->id;
	port->port.type = PORT_OWL;
	port->port.iotype = UPIO_MEM;
	port->port.mapbase = res_mem->start;
	port->port.irq = irq;
	
	port->port.uartclk = clk_get_rate(port->clk);
	
	if (port->port.uartclk == 0)
	{
		pr_err("Uart%d clock rate is zero.\n", pdev->id);
		return -EINVAL;
	}
	
	port->port.flags = UPF_BOOT_AUTOCONF | UPF_IOREMAP | UPF_LOW_LATENCY;
	port->port.x_char = 0;
	port->port.fifosize = (info) ? info->tx_fifosize : 16;
	port->port.ops = &caninos_uart_ops;
	
	caninos_uart_ports[pdev->id] = port;

	platform_set_drvdata(pdev, port);
	
	ret = uart_add_one_port(&caninos_uart_driver, &port->port);
	
	if (ret) {
		caninos_uart_ports[pdev->id] = NULL;
	}
	
	return ret;
}

static int caninos_uart_remove(struct platform_device *pdev)
{
	struct caninos_uart_port *port = platform_get_drvdata(pdev);
	
	uart_remove_one_port(&caninos_uart_driver, &port->port);
	caninos_uart_ports[pdev->id] = NULL;
	
	return 0;
}

static struct platform_driver caninos_uart_platform_driver = {
	.probe = caninos_uart_probe,
	.remove = caninos_uart_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = caninos_uart_dt_matches,
	},
};

static int __init caninos_uart_init(void)
{
	int ret;
	
	if ((ret = uart_register_driver(&caninos_uart_driver)) != 0) {
		return ret;
	}
	
	if ((ret = platform_driver_register(&caninos_uart_platform_driver)) != 0) {
		return ret;
	}
	
	return 0;
}

static void __init caninos_uart_exit(void)
{
	platform_driver_unregister(&caninos_uart_platform_driver);
	uart_unregister_driver(&caninos_uart_driver);
}

module_init(caninos_uart_init);
module_exit(caninos_uart_exit);

MODULE_LICENSE("GPL");

