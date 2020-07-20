#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/machine.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>

#include "pinctrl-caninos.h"

#define GPIO_AOUTEN  0x00
#define GPIO_AINEN   0x04
#define GPIO_ADAT    0x08
#define GPIO_BOUTEN  0x0C
#define GPIO_BINEN   0x10
#define GPIO_BDAT    0x14
#define GPIO_COUTEN  0x18
#define GPIO_CINEN   0x1C
#define GPIO_CDAT    0x20
#define GPIO_DOUTEN  0x24
#define GPIO_DINEN   0x28
#define GPIO_DDAT    0x2C
#define GPIO_EOUTEN  0x30
#define GPIO_EINEN   0x34
#define GPIO_EDAT    0x38
#define MFP_CTL0     0x40
#define MFP_CTL1     0x44
#define MFP_CTL2     0x48
#define MFP_CTL3     0x4C
#define PAD_PULLCTL0 0x60
#define PAD_PULLCTL1 0x64
#define PAD_PULLCTL2 0x68
#define PAD_ST0      0x6C
#define PAD_ST1      0x70
#define PAD_CTL      0x74
#define PAD_DRV0     0x80
#define PAD_DRV1     0x84
#define PAD_DRV2     0x88

/* CTLR */
#define GPIO_CTLR_PENDING        (0x1 << 0)
#define GPIO_CTLR_ENABLE         (0x1 << 1)
#define GPIO_CTLR_SAMPLE_CLK     (0x1 << 2)
#define	GPIO_CTLR_SAMPLE_CLK_32K (0x0 << 2)
#define	GPIO_CTLR_SAMPLE_CLK_24M (0x1 << 2)

/* TYPE */
#define GPIO_INT_TYPE_MASK    (0x3)
#define GPIO_INT_TYPE_HIGH    (0x0)
#define GPIO_INT_TYPE_LOW     (0x1)
#define GPIO_INT_TYPE_RISING  (0x2)
#define GPIO_INT_TYPE_FALLING (0x3)

/* pending mask for share intc_ctlr */
#define GPIO_CTLR_PENDING_MASK (0x42108421)

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

	const struct gpio_regs	*regs;

	spinlock_t	lock;
};

struct caninos_group {
	const char *name;
	unsigned *pins;
	unsigned num_pins;
};

static unsigned int uart0_pins[] = { GPIOC(27), GPIOC(26) };
static unsigned int i2c2_pins[]  = { GPIOE(3), GPIOE(2) };
static unsigned int pwm_pins[]   = { GPIOB(8) };

static struct caninos_group caninos_groups[] = {
	{
		.name = "uart0_grp",
		.pins = uart0_pins,
		.num_pins = ARRAY_SIZE(uart0_pins),
	},
	{
		.name = "i2c2_grp",
		.pins = i2c2_pins,
		.num_pins = ARRAY_SIZE(i2c2_pins),
	},
	{
		.name = "pwm_grp",
		.pins = pwm_pins,
		.num_pins = ARRAY_SIZE(pwm_pins),
	},
};

static int caninos_get_groups_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(caninos_groups);
}

static const char *caninos_get_group_name(struct pinctrl_dev *pctldev,
                                        unsigned selector)
{
	return caninos_groups[selector].name;
}

static int caninos_get_group_pins(struct pinctrl_dev *pctldev, unsigned selector,
                                const unsigned ** pins,
                                unsigned * num_pins)
{
	*pins = caninos_groups[selector].pins;
	*num_pins = caninos_groups[selector].num_pins;
	return 0;
}

static int caninos_dt_node_to_map(struct pinctrl_dev *pctldev,
                                struct device_node *np_config,
                                struct pinctrl_map **map,
                                unsigned *num_maps)
{
	const char *group, *function;
	struct pinctrl_map *new_map;
	
	int ret;
	
	ret = of_property_read_string(np_config, "caninos,group", &group);
	
	if (ret < 0) {
		group = NULL;
	}
	
	ret = of_property_read_string(np_config, "caninos,function", &function);
	
	if (ret < 0) {
		function = NULL;
	}
	
	new_map = kzalloc(sizeof(*new_map), GFP_KERNEL);
	
	new_map[0].type = PIN_MAP_TYPE_DUMMY_STATE;
	
	if (function)
	{
		new_map[0].type = PIN_MAP_TYPE_MUX_GROUP;
		new_map[0].data.mux.group = group;
		new_map[0].data.mux.function = function;
	}
	
	*num_maps = 1;
	*map = new_map;
	
	return 0;
}

static void caninos_dt_free_map(struct pinctrl_dev *pctldev,
                              struct pinctrl_map *map, unsigned num_maps)
{
	kfree(map);
}

struct caninos_pmx_func {
	const char *name;
	const char * const *groups;
	const unsigned num_groups;
};

static const char * const uart0_groups[] = { "uart0_grp"};
static const char * const i2c2_groups[]  = { "i2c2_grp" };
static const char * const pwm_groups[]   = { "pwm_grp"  };

enum
{
	FUNCTION_UART0 = 0,
	FUNCTION_I2C2,
	FUNCTION_PWM,
	FUNCTION_LAST,
};

static const struct caninos_pmx_func caninos_functions[] = {
	[FUNCTION_UART0] = {
		.name = "uart0",
		.groups = uart0_groups,
		.num_groups = ARRAY_SIZE(uart0_groups),
	},
	[FUNCTION_I2C2] = {
		.name = "i2c2",
		.groups = i2c2_groups,
		.num_groups = ARRAY_SIZE(i2c2_groups),
	},
	[FUNCTION_PWM] = {
		.name = "pwm",
		.groups = pwm_groups,
		.num_groups = ARRAY_SIZE(pwm_groups),
	},
};

int caninos_pmx_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(caninos_functions);
}

const char *caninos_pmx_get_fname(struct pinctrl_dev *pctldev, unsigned selector)
{
	return caninos_functions[selector].name;
}

static int caninos_pmx_get_fgroups(struct pinctrl_dev *pctldev, unsigned selector,
			  const char * const **groups,
			  unsigned * num_groups)
{
	*groups = caninos_functions[selector].groups;
	*num_groups = caninos_functions[selector].num_groups;
	return 0;
}

static int caninos_pmx_request(struct pinctrl_dev *pctldev, unsigned offset)
{
	//request a pin from gpio-caninos?
	
	pr_info("caninos_pmx_request: %u\n",offset);
	return 0;
}

static int caninos_pmx_free(struct pinctrl_dev *pctldev, unsigned offset)
{
	pr_info("caninos_pmx_free: %u\n",offset);
	
	return 0;
}

static int caninos_pmx_set_mux(struct pinctrl_dev *pctldev,
                               unsigned func_selector, unsigned group_selector)
{
	pr_info("caninos_pmx_set_mux: %u\n",func_selector);

	/* still no need to check group_selector */
	switch(func_selector)
	{
	case FUNCTION_UART0:
		/* only need to disable gpio */
		break;
		
	case FUNCTION_I2C2:
		/* only need to disable gpio */
		break;
	
	case FUNCTION_PWM:
		/* only need to disable gpio */
		break;
	
	default:
		return -EINVAL;
	}
	
	/* 
	I2C3
	Header Pin 19 -> (Pad TWI3_SDATA) TWI3_SDATA -> nothing to do
	Header Pin 23 -> (Pad TWI3_SCLK)  TWI3_SCLK -> nothing to do
	*/
	
	return 0;
}

static int caninos_pin_config_get(struct pinctrl_dev *pctldev,
                                unsigned pin, unsigned long *config)
{
	return 0;
}

static int caninos_pin_config_set(struct pinctrl_dev *pctldev,
                                unsigned pin, unsigned long *configs,
                                unsigned num_configs)
{
	return 0;
}

static int caninos_pin_config_group_get(struct pinctrl_dev *pctldev,
                                      unsigned selector, unsigned long *config)
{
	return 0;
}

static int caninos_pin_config_group_set(struct pinctrl_dev *pctldev,
                                      unsigned selector, unsigned long *configs,
                                      unsigned num_configs)
{
	return 0;
}

/* lock for shared register between banks,  e.g. share_intc_ctlr */
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
	chip->parent = &bank->pdev->dev;
	chip->label = dev_name(chip->parent);
	chip->of_node = chip->parent->of_node;
	chip->owner = THIS_MODULE;

	return 0;
}

/*
 * This lock class tells lockdep that GPIO irqs are in a different
 * category than their parents, so it won't report false recursion.
 */
static struct lock_class_key gpio_lock_class;
static struct lock_class_key gpio_request_class;

static int caninos_gpio_irq_setup(struct caninos_gpio_bank *bank)
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



static struct pinctrl_ops caninos_pctrl_ops = {
	.get_groups_count = caninos_get_groups_count,
	.get_group_name = caninos_get_group_name,
	.get_group_pins = caninos_get_group_pins,
	.dt_node_to_map = caninos_dt_node_to_map,
	.dt_free_map = caninos_dt_free_map,
};

static struct pinmux_ops caninos_pmxops = {
	.request = caninos_pmx_request,
	.free = caninos_pmx_free,
	.set_mux = caninos_pmx_set_mux,
	.get_functions_count = caninos_pmx_get_functions_count,
	.get_function_name = caninos_pmx_get_fname,
	.get_function_groups = caninos_pmx_get_fgroups,
};

static struct pinconf_ops caninos_pconf_ops = {
	.pin_config_get = caninos_pin_config_get,
	.pin_config_set = caninos_pin_config_set,
	.pin_config_group_get = caninos_pin_config_group_get,
	.pin_config_group_set = caninos_pin_config_group_set,
};

static struct pinctrl_desc caninos_desc = {
	.pins = caninos_pins,
	.npins = ARRAY_SIZE(caninos_pins),
	.pctlops = &caninos_pctrl_ops,
	.pmxops = &caninos_pmxops,
	.confops = &caninos_pconf_ops,
	.owner = THIS_MODULE,
};



/////////////////----------------------------------------------/////////////////

struct caninos_pinctrl
{
	void __iomem *base;
	struct clk *clk;
	raw_spinlock_t lock;
	struct device *dev;
	struct pinctrl_dev *pinctrl;
	struct caninos_gpio_bank banks[5];
};

static const struct caninos_gpio_bank_data banks_data[5] = {
	/*      outen   inen     dat   intc_pd  intc_mask intc_type intc_ctlr */
	{ 32, {  0x0,    0x4,    0x8,    0x208,   0x20c,   0x230,   0x204 } },
	{ 32, {  0xc,    0x10,   0x14,   0x210,   0x214,   0x238,   0x204 } },
	{ 32, {  0x18,   0x1c,   0x20,   0x218,   0x21c,   0x240,   0x204 } },
	{ 32, {  0x24,   0x28,   0x2c,   0x220,   0x224,   0x248,   0x204 } },
	{ 8,  {  0x30,   0x34,   0x38,   0x228,   0x22c,   0x250,   0x204 } },
};

static int caninos_pinctrl_probe(struct platform_device *pdev)
{
	struct caninos_pinctrl *data;
	struct resource *res;
	int ret;
	
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	
	if (!data) {
		return -ENOMEM;
	}
	
	platform_set_drvdata(pdev, data);
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	data->base = devm_ioremap_resource(&pdev->dev, res);
	
	if (IS_ERR(data->base)) {
		return PTR_ERR(data->base);
	}
	
	/* get GPIO/MFP clock */
	data->clk = devm_clk_get(&pdev->dev, NULL);
	
	if (IS_ERR(data->clk))
	{
		dev_err(&pdev->dev, "could not get clock\n");
		return PTR_ERR(data->clk);
	}
	
	/* enable GPIO/MFP clock (just a formality, it is already enabled) */
	ret = clk_prepare_enable(data->clk);
	
	if (ret)
	{
		dev_err(&pdev->dev, "could not enable clock\n");
		return ret;
	}
	
	raw_spin_lock_init(&data->lock);
	
	data->dev = &pdev->dev;
	
	caninos_desc.name = dev_name(&pdev->dev);
	
	/* This is a temporary solution (until all mux functions are implemented) */
	writel(0x00000000, data->base + MFP_CTL0);
	writel(0x2e400000, data->base + MFP_CTL1);
	writel(0x00000600, data->base + MFP_CTL2);
	writel(0x40000008, data->base + MFP_CTL3);
	writel(0x00000000, data->base + PAD_PULLCTL0);
	writel(0x0003e001, data->base + PAD_PULLCTL1);
	writel(0x00000000, data->base + PAD_PULLCTL2);
	writel(0x40401880, data->base + PAD_ST0);
	writel(0x00000140, data->base + PAD_ST1);
	writel(0x00000002, data->base + PAD_CTL);
	writel(0x2aaaaaaa, data->base + PAD_DRV0);
	writel(0xaacf0800, data->base + PAD_DRV1);
	writel(0xa9482008, data->base + PAD_DRV2);
	
	/*
	UART0
	Header Pin  8 -> (Pad UART0_TX) UART0_TX -> mfp3_21_19 -> (000b << 19)
	Header Pin 10 -> (Pad UART0_RX) UART0_RX -> mfp2_2_0 -> (000b << 0)
	*/
	
	/*
	Never mess with PWM pads -> the CPU/GPU will crash (or even burn)!
	MFP_CTL1 (011b << 26)
	KS_IN3  -> PWM1 -> GPU power supply
	KS_OUT0 -> PWM2 -> CPU power supply
	KS_OUT1 -> PWM3 -> PWM exported at extio header
	*/
	
	/*
	Do not enable internal pull-up for the following devices:
	-> i2c0 already has a 4k7 pull-up resistor at core board V3.1
	-> i2c1 and i2c2 already have a 2k2 pull-up resistor at core board V3.1
	-> ethphy already has pull-up/down resistors installed for configuration
	*/
	
	data->pinctrl = devm_pinctrl_register(&pdev->dev, &caninos_desc, data);
	
	if (IS_ERR(data->pinctrl))
	{
		dev_err(&pdev->dev, "could not register pin driver\n");
		return PTR_ERR(data->pinctrl);
	}
	
	
	return 0;
}

static int caninos_gpio_probe(struct platform_device *pdev)
{
	struct platform_device *pinctrl_pdev;
	struct caninos_pinctrl *data = NULL;
	struct device_node *pinctrl_np;
	struct caninos_gpio_bank *bank;
	int ret, id;
	
	pinctrl_np = of_parse_phandle(pdev->dev.of_node, "gpio-ranges", 0);
	
	if (pinctrl_np)
	{
		pinctrl_pdev = of_find_device_by_node(pinctrl_np);
		
		if (pinctrl_pdev) {
			data = platform_get_drvdata(pinctrl_pdev);
		}
		
		of_node_put(pinctrl_np);
	}
	
	if (data == NULL)
	{
		dev_err(&pdev->dev, "pinctrl is not ready\n");
		return -EPROBE_DEFER;
	}
	
	id = of_alias_get_id(pdev->dev.of_node, "gpio");
	
	if (id < 0) {
		return id;
	}
	
	bank = &data->banks[id];
	bank->id = id;
	
	dev_info(&pdev->dev, "GPIO%c probe", 'A' + id);
	
	bank->base = of_iomap(pdev->dev.of_node, 0);
	
	if (!bank->base)
	{
		dev_err(&pdev->dev, "failed to map memory");
		return -ENOENT;
	}
	
	bank->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	
	if (bank->irq < 0)
	{
		dev_err(&pdev->dev, "failed to get IRQ");
		return -ENOENT;
	}
	
	bank->nr_gpios = banks_data[id].nr_gpios;
	bank->regs = &banks_data[id].regs;
	bank->pdev = pdev;
	
	spin_lock_init(&bank->lock);
	caninos_gpio_bank_setup(bank);
	platform_set_drvdata(pdev, bank);
	
	ret = caninos_gpio_irq_setup(bank);
	
	if (ret < 0)
	{
		dev_err(&pdev->dev, "failed to setup irq, ret %d\n", ret);
		return ret;
	}
	
	return devm_gpiochip_add_data(&pdev->dev, &bank->gpio_chip, NULL);
}

static const struct of_device_id caninos_pinctrl_dt_ids[] = {
	{ .compatible = "caninos,k7-pinctrl", },
	{ },
};

static struct platform_driver caninos_pinctrl_driver = {
	.driver	= {
		.name = "pinctrl-caninos",
		.of_match_table = caninos_pinctrl_dt_ids,
		.suppress_bind_attrs = true,
	},
	.probe = caninos_pinctrl_probe,
};

static const struct of_device_id caninos_gpio_dt_ids[] = {
	{ .compatible = "caninos,k7-gpio", },
	{ },
};

static struct platform_driver caninos_gpio_driver = {
	.driver	= {
		.name = "gpio-caninos",
		.of_match_table = caninos_gpio_dt_ids,
		.suppress_bind_attrs = true,
	},
	.probe = caninos_gpio_probe,
};

static struct platform_driver * const drivers[] = {
	&caninos_pinctrl_driver,
	&caninos_gpio_driver,
};

static int __init caninos_pinctrl_init(void)
{
	return platform_register_drivers(drivers, ARRAY_SIZE(drivers));
}
arch_initcall(caninos_pinctrl_init);

