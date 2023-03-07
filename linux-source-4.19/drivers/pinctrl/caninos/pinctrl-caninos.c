#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>

#include "../pinctrl-utils.h"
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

#define to_caninos_gpio_chip(x) \
	container_of(x, struct caninos_gpio_chip, gpio_chip)

#define to_caninos_pinctrl(x) \
	(struct caninos_pinctrl*) pinctrl_dev_get_drvdata(x)

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

static const struct caninos_pmx_func caninos_functions[] = {
	{
		.name = "uart0",
		.groups = uart0_groups,
		.num_groups = ARRAY_SIZE(uart0_groups),
	},
	{
		.name = "i2c2",
		.groups = i2c2_groups,
		.num_groups = ARRAY_SIZE(i2c2_groups),
	},
	{
		.name = "pwm",
		.groups = pwm_groups,
		.num_groups = ARRAY_SIZE(pwm_groups),
	},
	{
		.name = "eth",
		.groups = eth_groups,
		.num_groups = ARRAY_SIZE(eth_groups),
	},
};

int caninos_pmx_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(caninos_functions);
}

const char *caninos_pmx_get_fname(struct pinctrl_dev *pctldev,
                                  unsigned selector)
{
	return caninos_functions[selector].name;
}

static int caninos_pmx_get_fgroups(struct pinctrl_dev *pctldev,
                                   unsigned selector,
                                   const char * const **groups,
                                   unsigned * num_groups)
{
	*groups = caninos_functions[selector].groups;
	*num_groups = caninos_functions[selector].num_groups;
	return 0;
}

static void caninos_raw_direction_output(struct caninos_gpio_chip *chip,
                                         unsigned offset, int value)
{
	struct caninos_pinctrl *pinctrl = chip->pinctrl;
	unsigned long flags = 0;
	
	void __iomem *base  = pinctrl->base + (chip->addr * 0xc);
	void __iomem *inen  = base + GPIO_AINEN;
	void __iomem *outen = base + GPIO_AOUTEN;
	void __iomem *dat   = base + GPIO_ADAT;
	
	u32 val;
	
	if (chip->mask & BIT(offset))
	{
		spin_lock_irqsave(&pinctrl->lock, flags);
		
		val = readl(inen);
		val &= ~BIT(offset);
		writel(val, inen);
		
		val = readl(outen);
		val |= BIT(offset);
		writel(val, outen);
		
		val = readl(dat);
		
		if (value) {
			val |= BIT(offset);
		}
		else {
			val &= ~BIT(offset);
		}
		
		writel(val, dat);
		
		spin_unlock_irqrestore(&pinctrl->lock, flags);
	}
}

static void caninos_raw_direction_input(struct caninos_gpio_chip *chip,
                                        unsigned offset)
{
	struct caninos_pinctrl *pinctrl = chip->pinctrl;
	unsigned long flags = 0;
	u32 val;
	
	void __iomem *base  = pinctrl->base + (chip->addr * 0xc);
	void __iomem *inen  = base + GPIO_AINEN;
	void __iomem *outen = base + GPIO_AOUTEN;
	
	if (chip->mask & BIT(offset))
	{
		spin_lock_irqsave(&pinctrl->lock, flags);
		
		val = readl(outen);
		val &= ~BIT(offset);
		writel(val, outen);
		
		val = readl(inen);
		val |= BIT(offset);
		writel(val, inen);
		
		spin_unlock_irqrestore(&pinctrl->lock, flags);
	}
}

static void caninos_raw_direction_device(struct caninos_gpio_chip *chip,
                                         unsigned offset)
{
	struct caninos_pinctrl *pinctrl = chip->pinctrl;
	unsigned long flags = 0;
	u32 val;
	
	void __iomem *base  = pinctrl->base + (chip->addr * 0xc);
	void __iomem *inen  = base + GPIO_AINEN;
	void __iomem *outen = base + GPIO_AOUTEN;
	
	if (chip->mask & BIT(offset))
	{
		spin_lock_irqsave(&pinctrl->lock, flags);
		
		val = readl(outen);
		val &= ~BIT(offset);
		writel(val, outen);
		
		val = readl(inen);
		val &= ~BIT(offset);
		writel(val, inen);
		
		spin_unlock_irqrestore(&pinctrl->lock, flags);
	}
}

static int caninos_raw_gpio_get(struct caninos_gpio_chip *chip, unsigned offset)
{
	u32 val = 0;
	void __iomem *dat = chip->pinctrl->base + (chip->addr * 0xc) + GPIO_ADAT;
	
	if (chip->mask & BIT(offset)) {
		val = readl(dat);
	}
	
	return !!(val & BIT(offset));
}

static void caninos_raw_gpio_set(struct caninos_gpio_chip *chip,
                                 unsigned offset, int value)
{
	u32 val;
	void __iomem *dat = chip->pinctrl->base + (chip->addr * 0xc) + GPIO_ADAT;
	
	if (chip->mask & BIT(offset))
	{
		val = readl(dat);
		
		if (value) {
			val |= BIT(offset);
		}
		else {
			val &= ~BIT(offset);
		}
		
		writel(val, dat);
	}
}

static int caninos_raw_get_direction(struct caninos_gpio_chip *chip,
                                     unsigned offset)
{
	struct caninos_pinctrl *pinctrl = chip->pinctrl;
	unsigned long flags = 0;
	u32 val;
	
	void __iomem *inen = pinctrl->base + (chip->addr * 0xc) + GPIO_AINEN;
	
	if (chip->mask & BIT(offset))
	{
		spin_lock_irqsave(&pinctrl->lock, flags);
		val = readl(inen);
		spin_unlock_irqrestore(&pinctrl->lock, flags);
		
		return !!(val & BIT(offset));
	}
	return 1;
}

static int caninos_pmx_set_mux(struct pinctrl_dev *pctldev,
                               unsigned func_selector, unsigned group_selector)
{
	struct caninos_pinctrl *pinctrl = to_caninos_pinctrl(pctldev);
	unsigned num_pins, i;
	const unsigned *pins;
	u32 addr, offset;
	
	caninos_get_group_pins(pctldev, group_selector, &pins, &num_pins);
	
	for (i = 0; i < num_pins; i++)
	{
		addr = pins[i] / GPIO_PER_BANK;
		offset = pins[i] % GPIO_PER_BANK;
		
		caninos_raw_direction_device(&pinctrl->banks[addr], offset);
	}
	
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

static int caninos_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct caninos_gpio_chip *bank = to_caninos_gpio_chip(chip);
	return caninos_raw_gpio_get(bank, offset);
}

static void caninos_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct caninos_gpio_chip *bank = to_caninos_gpio_chip(chip);
	caninos_raw_gpio_set(bank, offset, value);
}

static int caninos_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct caninos_gpio_chip *bank = to_caninos_gpio_chip(chip);
	caninos_raw_direction_input(bank, offset);
	return 0;
}

static int caninos_gpio_direction_output(struct gpio_chip *chip,
                                         unsigned offset, int value)
{
	struct caninos_gpio_chip *bank = to_caninos_gpio_chip(chip);
	caninos_raw_direction_output(bank, offset, value);
	return 0;
}

static int caninos_gpio_request(struct gpio_chip *chip, unsigned int offset)
{
	struct caninos_gpio_chip *bank = to_caninos_gpio_chip(chip);
	int ret;
	
	if ((ret = gpiochip_generic_request(chip, offset)) < 0) {
		return ret;
	}
	
	caninos_raw_direction_input(bank, offset);
	return 0;
}

static void caninos_gpio_free(struct gpio_chip *chip, unsigned int offset)
{
	struct caninos_gpio_chip *bank = to_caninos_gpio_chip(chip);
	
	caninos_raw_direction_device(bank, offset);
	gpiochip_generic_free(chip, offset);
}

static int caninos_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	struct caninos_gpio_chip *bank = to_caninos_gpio_chip(chip);
	return caninos_raw_get_direction(bank, offset);
}

static struct pinctrl_ops caninos_pctrl_ops = {
	.get_groups_count = caninos_get_groups_count,
	.get_group_name = caninos_get_group_name,
	.get_group_pins = caninos_get_group_pins,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_all,
	.dt_free_map = pinctrl_utils_free_map,
};

static struct pinmux_ops caninos_pmxops = {
	.get_functions_count = caninos_pmx_get_functions_count,
	.get_function_name = caninos_pmx_get_fname,
	.get_function_groups = caninos_pmx_get_fgroups,
	.set_mux = caninos_pmx_set_mux,
};

static const struct pinconf_ops caninos_pconf_ops = {
	.is_generic = true,
	.pin_config_get = caninos_pin_config_get,
	.pin_config_set = caninos_pin_config_set,
};

static int caninos_gpiolib_register_banks(struct caninos_pinctrl *pctl)
{
	struct caninos_gpio_chip *bank;
	struct device *dev = pctl->dev;
	int i, ret;
	
	for (i = 0; i < pctl->nbanks; i++)
	{
		bank = &pctl->banks[i];
		
		if ((ret = devm_gpiochip_add_data(dev, &bank->gpio_chip, bank)) < 0) {
			dev_err(dev, "could not add gpio bank %s\n", bank->label);
			return ret;
		}
		dev_info(dev, "gpio bank %s added\n", bank->label);
	}
	
	return 0;
}

static int caninos_gpiolib_parse_bank(struct caninos_pinctrl *pctl,
                                      struct device_node *np)
{
	struct caninos_gpio_chip *bank = &pctl->banks[pctl->nbanks];
	struct device *dev = pctl->dev;
	int i = 0, ret, bank_nr, npins;
	struct of_phandle_args args;
	const char *label;
	u32 mask;
		
	if (!of_parse_phandle_with_fixed_args(np, "gpio-ranges", 3, i, &args))
	{
		bank_nr = args.args[1] / GPIO_PER_BANK;
		
		bank->gpio_chip.base = args.args[1];
		npins = args.args[0] + args.args[2];
		
		while (!of_parse_phandle_with_fixed_args(np, "gpio-ranges", 3,
		                                         ++i, &args)) {
			npins = max(npins, (int)(args.args[0] + args.args[2]));
		}
	}
	else {
		dev_err(dev, "missing gpio-ranges property on bank %s\n",
		        np->full_name);
		return -ENODEV;
	}
	if (npins > GPIO_PER_BANK) {
		dev_err(dev, "number of gpios on bank %s is too large %i\n",
		        np->full_name, npins);
		return ret;
	}
	if ((ret = of_property_read_string(np, "gpio-label", &label)) < 0)
	{
		dev_err(dev, "missing gpio-label property on bank %s\n", np->full_name);
		return ret;
	}
	if ((ret = of_property_read_u32(np, "gpio-mask", &mask)) < 0)
	{
		dev_err(dev, "missing gpio-mask property on bank %s\n", np->full_name);
		return ret;
	}
	
	strlcpy(bank->label, label, BANK_LABEL_LEN);
	
	bank->pinctrl = pctl;
	bank->addr = bank_nr;
	bank->npins = npins;
	bank->mask = mask;
	
	memset(&bank->gpio_chip, 0, sizeof(bank->gpio_chip));
	
	bank->gpio_chip.label = bank->label;
	bank->gpio_chip.parent = dev;
	bank->gpio_chip.of_node = np;
	
	bank->gpio_chip.request = caninos_gpio_request;
	bank->gpio_chip.free = caninos_gpio_free;
	bank->gpio_chip.get_direction = caninos_gpio_get_direction;
	bank->gpio_chip.direction_input = caninos_gpio_direction_input;
	bank->gpio_chip.direction_output = caninos_gpio_direction_output;
	bank->gpio_chip.get = caninos_gpio_get;
	bank->gpio_chip.set = caninos_gpio_set;
	
	bank->gpio_chip.ngpio = npins;
	bank->gpio_chip.base = bank_nr * GPIO_PER_BANK;
	bank->gpio_chip.owner = THIS_MODULE;
	bank->gpio_chip.can_sleep = false;
	
	return 0;
}

static int caninos_pinctrl_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	//const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct caninos_pinctrl *pctl;
	struct device_node *child;
	struct resource res;
	int ret, nbanks = 0;
	
	if (!np) {
		dev_err(dev, "missing device OF node\n");
		return -EINVAL;
	}
	
	/*match = of_match_device(dev->driver->of_match_table, dev);
	
	if (!match || !match->data)
	{
		////
		return -EINVAL;
	}*/
	
	pctl = devm_kzalloc(dev, sizeof(*pctl), GFP_KERNEL);
	
	if (!pctl) {
		dev_err(dev, "pinctrl memory allocation failed\n");
		return -ENOMEM;
	}
	
	platform_set_drvdata(pdev, pctl);
	
	pctl->dev = dev;
	pctl->nbanks = 0;
	pctl->pctl_desc.name = dev_name(dev);
	pctl->pctl_desc.owner = THIS_MODULE;
	pctl->pctl_desc.pins = caninos_pins;
	pctl->pctl_desc.npins = ARRAY_SIZE(caninos_pins);
	pctl->pctl_desc.confops = &caninos_pconf_ops;
	pctl->pctl_desc.pctlops = &caninos_pctrl_ops;
	pctl->pctl_desc.pmxops = &caninos_pmxops;
	
	spin_lock_init(&pctl->lock);
	
	for_each_available_child_of_node(np, child)
		if (of_property_read_bool(child, "gpio-controller"))
			nbanks++;
	
	if (!nbanks) {
		dev_err(dev, "at least one GPIO bank is required\n");
		return -EINVAL;
	}
	
	pctl->banks = devm_kcalloc(dev, nbanks, sizeof(*pctl->banks), GFP_KERNEL);
	
	if (!pctl->banks) {
		dev_err(dev, "gpio banks memory allocation failed\n");
		return -ENOMEM;
	}
	
	for_each_available_child_of_node(np, child)
	{
		if (of_property_read_bool(child, "gpio-controller"))
		{
			if ((ret = caninos_gpiolib_parse_bank(pctl, child)) < 0)
				return ret;
			
			pctl->nbanks++;
		}
	}
	
	if (!of_address_to_resource(np, 0, &res))
		pctl->base = devm_ioremap(dev, res.start, resource_size(&res));
	
	if (!pctl->base) {
		dev_err(dev, "could not map memory\n");
		return -ENOMEM;
	}
	
	pctl->clk = devm_clk_get(dev, NULL);
	
	if (IS_ERR(pctl->clk)) {
		dev_err(dev, "could not get clock\n");
		return PTR_ERR(pctl->clk);
	}
	
	if ((ret = clk_prepare_enable(pctl->clk)) < 0) {
		dev_err(dev, "could not enable clock\n");
		return ret;
	}
	
	writel(0x00000000, pctl->base + MFP_CTL0);
	writel(0x2e400060, pctl->base + MFP_CTL1);
	writel(0x10000600, pctl->base + MFP_CTL2);
	writel(0x40000008, pctl->base + MFP_CTL3);
	writel(0x00000000, pctl->base + PAD_PULLCTL0);
	writel(0x0003e001, pctl->base + PAD_PULLCTL1);
	writel(0x00000000, pctl->base + PAD_PULLCTL2);
	writel(0x40401880, pctl->base + PAD_ST0);
	writel(0x00000140, pctl->base + PAD_ST1);
	writel(0x00000002, pctl->base + PAD_CTL);
	writel(0x2ffeeaaa, pctl->base + PAD_DRV0);
	writel(0xaacf0800, pctl->base + PAD_DRV1);
	writel(0xa9482008, pctl->base + PAD_DRV2);
	
	pctl->pctl_dev = devm_pinctrl_register(dev, &pctl->pctl_desc, pctl);
	
	if (IS_ERR(pctl->pctl_dev)) {
		dev_err(dev, "could not register pinctrl device\n");
		return PTR_ERR(pctl->pctl_dev);
	}
	
	if ((ret = caninos_gpiolib_register_banks(pctl)) < 0)
		return ret;
	
	dev_err(dev, "probe finished\n");
	return 0;
}

static const struct of_device_id caninos_pinctrl_dt_ids[] = {
	{ .compatible = "caninos,k7-pinctrl", .data = (void*)0, },
	{ .compatible = "caninos,k5-pinctrl", .data = (void*)0, },
	{ /* sentinel */ },
};

static struct platform_driver caninos_pinctrl_driver = {
	.driver	= {
		.name = "pinctrl-caninos",
		.of_match_table = caninos_pinctrl_dt_ids,
		.owner = THIS_MODULE,
		.suppress_bind_attrs = true,
	},
	.probe = caninos_pinctrl_probe,
};

static int __init caninos_pinctrl_init(void)
{
	return platform_driver_register(&caninos_pinctrl_driver);
}
arch_initcall(caninos_pinctrl_init);

