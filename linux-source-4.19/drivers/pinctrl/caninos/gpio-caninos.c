#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/module.h>

#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include <asm-generic/gpio.h>

enum owl_gpio_id {
	S700_GPIO,
};

#define GPIO_PER_BANK				(32)
#define GPIO_BIT(gpio)				(1 << (gpio))

/* CTLR */
#define GPIO_CTLR_PENDING			(0x1 << 0)
#define GPIO_CTLR_ENABLE			(0x1 << 1)
#define GPIO_CTLR_SAMPLE_CLK			(0x1 << 2)
#define		GPIO_CTLR_SAMPLE_CLK_32K		(0x0 << 2)
#define		GPIO_CTLR_SAMPLE_CLK_24M		(0x1 << 2)

/* TYPE */
#define GPIO_INT_TYPE_MASK			(0x3)
#define GPIO_INT_TYPE_HIGH			(0x0)
#define GPIO_INT_TYPE_LOW			(0x1)
#define GPIO_INT_TYPE_RISING		(0x2)
#define GPIO_INT_TYPE_FALLING		(0x3)

/* pending mask for share intc_ctlr */
#define GPIO_CTLR_PENDING_MASK		(0x42108421)

struct gpio_regs {
	unsigned long		outen;
	unsigned long		inen;
	unsigned long		dat;
	unsigned long		intc_pd;
	unsigned long		intc_mask;
	unsigned long		intc_type;
	unsigned long		intc_ctlr;
};

struct caninos_gpio_bank_data {
	int			nr_gpios;
	const struct gpio_regs	regs;
};

struct caninos_gpio_bank {
	struct gpio_chip	gpio_chip;

	void __iomem		*base;
	int			id;
	int			nr_gpios;
	int			irq;
	struct irq_domain	*irq_domain;
	struct platform_device	*pdev;

	enum owl_gpio_id	devid;
	const struct gpio_regs	*regs;

	spinlock_t	lock;
};

/* lock for shared register between banks,  e.g. share_intc_ctlr in s700 */
static DEFINE_SPINLOCK(gpio_share_lock);

static inline struct caninos_gpio_bank *to_owl_bank(struct gpio_chip *chip)
{
	return container_of(chip, struct caninos_gpio_bank, gpio_chip);
}

static unsigned int read_intc_ctlr(struct caninos_gpio_bank *bank)
{
	unsigned int val;
	
	/* all banks share one INTC_CTLR register */
	
	val = readl_relaxed(bank->base + bank->regs->intc_ctlr);
	val = (val >> (5 * bank->id)) & 0x1f;
	
	return val;
}

static void write_intc_ctlr(struct caninos_gpio_bank *bank, unsigned int ctlr)
{
	unsigned int val;

	/* all banks share one INTC_CTLR register */

	unsigned long flags;

	spin_lock_irqsave(&gpio_share_lock, flags);
	
	val = readl_relaxed(bank->base + bank->regs->intc_ctlr);
	/* don't clear other bank pending mask */
	val &= ~GPIO_CTLR_PENDING_MASK;
	val &= ~(0x1f << (5 * bank->id));
	val |= (ctlr & 0x1f) << (5 * bank->id);
	writel_relaxed(val, bank->base + bank->regs->intc_ctlr);
	
	spin_unlock_irqrestore(&gpio_share_lock, flags);
}

static int owl_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	return 0;
}

static void owl_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	struct caninos_gpio_bank *bank = to_owl_bank(chip);
	const struct gpio_regs *regs = bank->regs;
	unsigned long flags = 0;
	u32 val;

	spin_lock_irqsave(&bank->lock, flags);

	/* disable gpio output */
	val = readl(bank->base + regs->outen);
	val &= ~GPIO_BIT(offset);
	writel(val, bank->base + regs->outen);

	/* disable gpio input */
	val = readl(bank->base + regs->inen);
	val &= ~GPIO_BIT(offset);
	writel(val, bank->base + regs->inen);

	spin_unlock_irqrestore(&bank->lock, flags);

}

static int owl_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct caninos_gpio_bank *bank = to_owl_bank(chip);
	const struct gpio_regs *regs = bank->regs;
	u32 val;

	val = readl(bank->base + regs->dat);

	return !!(val & GPIO_BIT(offset));
}

static void owl_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct caninos_gpio_bank *bank = to_owl_bank(chip);
	const struct gpio_regs *regs = bank->regs;
	u32 val;

	val = readl(bank->base + regs->dat);

	if (value)
		val |= GPIO_BIT(offset);
	else
		val &= ~GPIO_BIT(offset);

	writel(val, bank->base + regs->dat);
}

static int owl_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct caninos_gpio_bank *bank = to_owl_bank(chip);
	const struct gpio_regs *regs = bank->regs;
	unsigned long flags = 0;
	u32 val;

	spin_lock_irqsave(&bank->lock, flags);

	val = readl(bank->base + regs->outen);
	val &= ~GPIO_BIT(offset);
	writel(val, bank->base + regs->outen);

	val = readl(bank->base + regs->inen);
	val |= GPIO_BIT(offset);
	writel(val, bank->base + regs->inen);

	spin_unlock_irqrestore(&bank->lock, flags);

	return 0;
}

static int owl_gpio_direction_output(struct gpio_chip *chip,
		unsigned offset, int value)
{
	struct caninos_gpio_bank *bank = to_owl_bank(chip);
	const struct gpio_regs *regs = bank->regs;
	unsigned long flags = 0;
	u32 val;

	spin_lock_irqsave(&bank->lock, flags);

	val = readl(bank->base + regs->inen);
	val &= ~GPIO_BIT(offset);
	writel(val, bank->base + regs->inen);

	val = readl(bank->base + regs->outen);
	val |= GPIO_BIT(offset);
	writel(val, bank->base + regs->outen);

	owl_gpio_set(chip, offset, value);

	spin_unlock_irqrestore(&bank->lock, flags);

	return 0;
}

static int owl_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct caninos_gpio_bank *bank = to_owl_bank(chip);

	return irq_create_mapping(bank->irq_domain, offset);
}

static void owl_gpio_irq_mask(struct irq_data *d)
{
	struct caninos_gpio_bank *bank = irq_data_get_irq_chip_data(d);
	const struct gpio_regs *regs = bank->regs;
	int gpio = d->hwirq;
	unsigned long flags = 0;
	unsigned int val;

	spin_lock_irqsave(&bank->lock, flags);

	val = readl(bank->base + regs->intc_mask);
	val &= ~GPIO_BIT(gpio);
	writel(val, bank->base + regs->intc_mask);

	if (val == 0)
	{
		val = read_intc_ctlr(bank);
		val &= ~BIT(GPIO_CTLR_ENABLE);
		write_intc_ctlr(bank, val);
	}

	spin_unlock_irqrestore(&bank->lock, flags);
}

static void owl_gpio_irq_unmask(struct irq_data *d)
{
	struct caninos_gpio_bank *bank = irq_data_get_irq_chip_data(d);
	const struct gpio_regs *regs = bank->regs;
	int gpio = d->hwirq;
	unsigned long flags = 0;
	unsigned int val;
	
	spin_lock_irqsave(&bank->lock, flags);
	
	val = read_intc_ctlr(bank);
	val |= GPIO_CTLR_ENABLE | GPIO_CTLR_SAMPLE_CLK_24M;
	write_intc_ctlr(bank, val);
	
	val = readl(bank->base + regs->intc_mask);
	val |= GPIO_BIT(gpio);
	writel(val, bank->base + regs->intc_mask);
	
	spin_unlock_irqrestore(&bank->lock, flags);
}

static void owl_gpio_irq_ack(struct irq_data *d)
{
	struct caninos_gpio_bank *bank = irq_data_get_irq_chip_data(d);
	unsigned long flags = 0;
	unsigned int val;
	unsigned int gpio = d->hwirq;

	spin_lock_irqsave(&bank->lock, flags);

	/* clear gpio pending */
	writel(GPIO_BIT(gpio), bank->base + bank->regs->intc_pd);

	/* clear bank pending */
	val = read_intc_ctlr(bank);
	write_intc_ctlr(bank, val);
	
	spin_unlock_irqrestore(&bank->lock, flags);
}

static int owl_gpio_irq_set_type(struct irq_data *d, unsigned int flow_type)
{
	struct caninos_gpio_bank *bank = irq_data_get_irq_chip_data(d);
	const struct gpio_regs *regs = bank->regs;
	int gpio = d->hwirq;
	unsigned long flags = 0;
	unsigned int type, val, offset;

	spin_lock_irqsave(&bank->lock, flags);

	if (flow_type & IRQ_TYPE_EDGE_BOTH)
	{
		irq_set_handler_locked(d, handle_edge_irq);
	}
	else
	{
		irq_set_handler_locked(d, handle_level_irq);
	}

	flow_type &= IRQ_TYPE_SENSE_MASK;

	switch (flow_type) {
	case IRQ_TYPE_EDGE_RISING:
		type = GPIO_INT_TYPE_RISING;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		type = GPIO_INT_TYPE_FALLING;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		type = GPIO_INT_TYPE_HIGH;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		type = GPIO_INT_TYPE_LOW;
		break;
	default:
		pr_err("[GPIO] %s: gpio %d, unknow irq type %d\n",
			__func__, gpio, flow_type);
	return -1;
	}

	offset = gpio < 16 ? 4 : 0;
	val = readl(bank->base + regs->intc_type + offset);
	val &= ~(0x3 << ((gpio % 16) * 2));
	val |= type << ((gpio % 16) * 2);
	writel(val, bank->base + regs->intc_type + offset);
	
	spin_unlock_irqrestore(&bank->lock, flags);

	return 0;
}

/*
 * When the summary IRQ is raised, any number of GPIO lines may be high.
 * It is the job of the summary handler to find all those GPIO lines
 * which have been set as summary IRQ lines and which are triggered,
 * and to call their interrupt handlers.
 */
 static void owl_gpio_irq_bank(struct caninos_gpio_bank *bank)
{
	unsigned int child_irq, i;
	unsigned long flags, pending;
	/* get unmasked irq pending */
	spin_lock_irqsave(&bank->lock, flags);
	pending = readl(bank->base + bank->regs->intc_pd) &
		  readl(bank->base + bank->regs->intc_mask);
	spin_unlock_irqrestore(&bank->lock, flags);

	for_each_set_bit(i, &pending, 32) {
		child_irq = irq_find_mapping(bank->irq_domain, i);
		generic_handle_irq(child_irq);
	}
}
static void owl_gpio_irq_handler(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct caninos_gpio_bank *bank;
	
	bank = irq_desc_get_handler_data(desc);
	
	chained_irq_enter(chip, desc);
	
	owl_gpio_irq_bank(bank);
	
	chained_irq_exit(chip, desc);
}

static struct irq_chip owl_gpio_irq_chip = {
	.name           = "GPIO",
	.irq_mask       = owl_gpio_irq_mask,
	.irq_unmask     = owl_gpio_irq_unmask,
	.irq_ack        = owl_gpio_irq_ack,
	.irq_set_type   = owl_gpio_irq_set_type,
};


static int caninos_gpio_bank_setup(struct caninos_gpio_bank *bank)
{
	struct gpio_chip *chip = &bank->gpio_chip;

	chip->base = bank->id * GPIO_PER_BANK;
	chip->ngpio = bank->nr_gpios;
	chip->request = owl_gpio_request;
	chip->free = owl_gpio_free;
	chip->get = owl_gpio_get;
	chip->set = owl_gpio_set;
	chip->direction_input = owl_gpio_direction_input;
	chip->direction_output = owl_gpio_direction_output;
	chip->to_irq = owl_gpio_to_irq;
	chip->owner = THIS_MODULE;
	chip->parent = &bank->pdev->dev;
	chip->label = dev_name(chip->parent);
	chip->of_node = chip->parent->of_node;
	chip->owner = THIS_MODULE;
	
	// TODO: Add init_valid_mask implementation.

	return 0;
}

/*
 * This lock class tells lockdep that GPIO irqs are in a different
 * category than their parents, so it won't report false recursion.
 */
static struct lock_class_key gpio_lock_class;
static struct lock_class_key gpio_request_class;

static int owl_gpio_irq_setup(struct caninos_gpio_bank *bank)
{
	struct gpio_chip *chip = &bank->gpio_chip;
	int irq, i;

	bank->irq_domain = irq_domain_add_linear(chip->of_node,
			  chip->ngpio,
			  &irq_domain_simple_ops,
			  NULL);
	if (!bank->irq_domain) {
		dev_err(chip->parent, "Couldn't allocate IRQ domain\n");
		return -ENXIO;
	}

	for (i = 0; i < chip->ngpio; i++)
	{
		irq = owl_gpio_to_irq(chip, i);

		irq_set_lockdep_class(irq, &gpio_lock_class, &gpio_request_class);
		
		irq_set_chip_data(irq, bank);
		
		irq_set_chip_and_handler(irq, &owl_gpio_irq_chip, handle_level_irq);
		
		irq_clear_status_flags(irq, IRQ_NOREQUEST | IRQ_NOPROBE);
	}
	
	irq = bank->irq;
	irq_set_chained_handler(irq, owl_gpio_irq_handler);
	irq_set_handler_data(irq, bank);

	
	return 0;
}

static const struct caninos_gpio_bank_data s700_banks_data[5] = {
	/*      outen   inen     dat   intc_pd  intc_mask intc_type intc_ctlr */
	{ 32, {  0x0,    0x4,    0x8,    0x208,   0x20c,   0x230,   0x204 } },
	{ 32, {  0xc,    0x10,   0x14,   0x210,   0x214,   0x238,   0x204 } },
	{ 32, {  0x18,   0x1c,   0x20,   0x218,   0x21c,   0x240,   0x204 } },
	{ 32, {  0x24,   0x28,   0x2c,   0x220,   0x224,   0x248,   0x204 } },
	{ 8,  {  0x30,   0x34,   0x38,   0x228,   0x22c,   0x250,   0x204 } },
};

static const struct of_device_id caninos_gpio_dt_ids[] = {
	{ .compatible = "caninos,k7-gpio", .data = (void *)S700_GPIO, },
	{ },
};
MODULE_DEVICE_TABLE(of, caninos_gpio_dt_ids);

static int caninos_gpio_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	const struct caninos_gpio_bank_data *pdata;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct caninos_gpio_bank *bank;
	int ret;
	
	of_id = of_match_device(caninos_gpio_dt_ids, &pdev->dev);
	
	bank = devm_kzalloc(dev, sizeof(*bank), GFP_KERNEL);
	
	if (bank == NULL) {
		return -ENOMEM;
	}
	
	bank->id = of_alias_get_id(np, "gpio");
	
	if (bank->id < 0) {
		return bank->id;
	}
	
	bank->devid = (enum owl_gpio_id) of_id->data;
	
	dev_info(dev, "GPIO%c probe", 'A' + bank->id);
	
	if (bank->devid == S700_GPIO) {
		pdata = &s700_banks_data[bank->id];
	}
	else
	{
		dev_err(dev, "invalid device id %d", bank->devid);
		return -ENOENT;
	}
	
	bank->base = of_iomap(dev->of_node, 0);
	
	if (bank->base == NULL)
	{
		dev_err(dev, "failed to map memory");
		return -ENOENT;
	}
	
	bank->irq = irq_of_parse_and_map(np, 0);
	
	if (bank->irq < 0)
	{
		dev_err(dev, "failed to get IRQ");
		return -ENOENT;
	}
	
	bank->nr_gpios = pdata->nr_gpios;
	bank->regs = &pdata->regs;
	bank->pdev = pdev;
	
	spin_lock_init(&bank->lock);
	caninos_gpio_bank_setup(bank);
	platform_set_drvdata(pdev, bank);
	
	ret = owl_gpio_irq_setup(bank);
	
	if (ret < 0)
	{
		dev_err(dev, "failed to setup irq, ret %d\n", ret);
		return ret;
	}
	
	ret = gpiochip_add(&bank->gpio_chip);
	
	if (ret < 0)
	{
		dev_err(dev, "failed to add GPIO chip, ret %d\n", ret);
		return ret;
	}
	
	return 0;
}

static struct platform_driver caninos_gpio_driver = {
	.driver	= {
		.name = "gpio-caninos",
		.owner = THIS_MODULE,
		.of_match_table = caninos_gpio_dt_ids,
	},
	.probe = caninos_gpio_probe,
};

static int __init caninos_gpio_init(void)
{
	return platform_driver_register(&caninos_gpio_driver);
}

postcore_initcall(caninos_gpio_init);

