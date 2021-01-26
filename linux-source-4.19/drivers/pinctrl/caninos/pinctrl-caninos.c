#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
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

static struct caninos_gpio_bank caninos_gpio_banks[];

#define to_caninos_gpio_bank(x) \
	container_of(x, struct caninos_gpio_bank, gpio_chip)

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

static void caninos_raw_direction_output(struct caninos_gpio_bank *bank,
                                         unsigned offset, int value)
{
	unsigned long flags = 0;
	u32 val;
	
	if (bank->valid_mask & BIT(offset))
	{
		spin_lock_irqsave(&bank->lock, flags);
		
		val = readl(bank->base + bank->inen);
		val &= ~GPIO_BIT(offset);
		writel(val, bank->base + bank->inen);
		
		val = readl(bank->base + bank->outen);
		val |= GPIO_BIT(offset);
		writel(val, bank->base + bank->outen);
		
		val = readl(bank->base + bank->dat);
		
		if (value) {
			val |= GPIO_BIT(offset);
		}
		else {
			val &= ~GPIO_BIT(offset);
		}
		
		writel(val, bank->base + bank->dat);
		
		spin_unlock_irqrestore(&bank->lock, flags);
	}
}

static void caninos_raw_direction_input(struct caninos_gpio_bank *bank,
                                        unsigned offset)
{
	unsigned long flags = 0;
	u32 val;
	
	if (bank->valid_mask & BIT(offset))
	{
		spin_lock_irqsave(&bank->lock, flags);
		
		val = readl(bank->base + bank->outen);
		val &= ~GPIO_BIT(offset);
		writel(val, bank->base + bank->outen);
		
		val = readl(bank->base + bank->inen);
		val |= GPIO_BIT(offset);
		writel(val, bank->base + bank->inen);
		
		spin_unlock_irqrestore(&bank->lock, flags);
	}
}

static void caninos_raw_direction_device(struct caninos_gpio_bank *bank,
                                         unsigned offset)
{
	unsigned long flags = 0;
	u32 val;
	
	if (bank->valid_mask & BIT(offset))
	{
		spin_lock_irqsave(&bank->lock, flags);
		
		val = readl(bank->base + bank->outen);
		val &= ~GPIO_BIT(offset);
		writel(val, bank->base + bank->outen);
		
		val = readl(bank->base + bank->inen);
		val &= ~GPIO_BIT(offset);
		writel(val, bank->base + bank->inen);
		
		spin_unlock_irqrestore(&bank->lock, flags);
	}
}

static int caninos_raw_gpio_get(struct caninos_gpio_bank *bank, unsigned offset)
{
	u32 val = 0;
	
	if (bank->valid_mask & BIT(offset)) {
		val = readl(bank->base + bank->dat);
	}
	return !!(val & GPIO_BIT(offset));
}

static void caninos_raw_gpio_set(struct caninos_gpio_bank *bank,
                                 unsigned offset, int value)
{
	u32 val;
	
	if (bank->valid_mask & BIT(offset))
	{
		val = readl(bank->base + bank->dat);
		
		if (value) {
			val |= GPIO_BIT(offset);
		}
		else {
			val &= ~GPIO_BIT(offset);
		}
		
		writel(val, bank->base + bank->dat);
	}
}

static int caninos_raw_get_direction(struct caninos_gpio_bank *bank,
                                     unsigned offset)
{
	unsigned long flags = 0;
	u32 val;
	
	if (bank->valid_mask & BIT(offset))
	{
		spin_lock_irqsave(&bank->lock, flags);
		val = readl(bank->base + bank->inen);
		spin_unlock_irqrestore(&bank->lock, flags);
		return !!(val & GPIO_BIT(offset));
	}
	return 1;
}

static int caninos_pmx_set_mux(struct pinctrl_dev *pctldev,
                               unsigned func_selector, unsigned group_selector)
{
	struct caninos_gpio_bank *bank;
	unsigned num_pins, i;
	const unsigned *pins;
	
	caninos_get_group_pins(pctldev, group_selector, &pins, &num_pins);
	
	for (i = 0; i < num_pins; i++)
	{
		bank = &caninos_gpio_banks[pins[i] / GPIO_PER_BANK];
		caninos_raw_direction_device(bank, pins[i] % GPIO_PER_BANK);
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
	struct caninos_gpio_bank *bank = to_caninos_gpio_bank(chip);
	return caninos_raw_gpio_get(bank, offset);
}

static void caninos_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct caninos_gpio_bank *bank = to_caninos_gpio_bank(chip);
	caninos_raw_gpio_set(bank, offset, value);
}

static int caninos_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct caninos_gpio_bank *bank = to_caninos_gpio_bank(chip);
	caninos_raw_direction_input(bank, offset);
	return 0;
}

static int caninos_gpio_direction_output(struct gpio_chip *chip,
                                         unsigned offset, int value)
{
	struct caninos_gpio_bank *bank = to_caninos_gpio_bank(chip);
	caninos_raw_direction_output(bank, offset, value);
	return 0;
}

static int caninos_gpio_request(struct gpio_chip *chip, unsigned int offset)
{
	struct caninos_gpio_bank *bank = to_caninos_gpio_bank(chip);
	int ret;
	
	if ((ret = gpiochip_generic_request(chip, offset)) < 0) {
		return ret;
	}
	
	caninos_raw_direction_input(bank, offset);
	return 0;
}

static void caninos_gpio_free(struct gpio_chip *chip, unsigned int offset)
{
	struct caninos_gpio_bank *bank = to_caninos_gpio_bank(chip);
	
	caninos_raw_direction_device(bank, offset);
	gpiochip_generic_free(chip, offset);
}

static int caninos_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	struct caninos_gpio_bank *bank = to_caninos_gpio_bank(chip);
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

static struct pinctrl_desc caninos_desc = {
	.pins = caninos_pins,
	.npins = ARRAY_SIZE(caninos_pins),
	.pctlops = &caninos_pctrl_ops,
	.pmxops = &caninos_pmxops,
	.confops = &caninos_pconf_ops,
	.owner = THIS_MODULE,
};

#define GPIO_BANK(_bank, _npins, _outen, _inen, _dat, _valid)  \
	{								                           \
		.gpio_chip = {                                         \
			.label = "GPIO" #_bank,                            \
			.request = caninos_gpio_request,                   \
			.free = caninos_gpio_free,                         \
			.get_direction = caninos_gpio_get_direction,       \
			.direction_input = caninos_gpio_direction_input,   \
			.direction_output = caninos_gpio_direction_output, \
			.get = caninos_gpio_get,                           \
			.set = caninos_gpio_set,                           \
			.ngpio = _npins,                                   \
			.base = GPIO_BANK_START(_bank),                    \
			.owner = THIS_MODULE,                              \
			.can_sleep = 0,                                    \
		},                                                     \
		.outen = _outen,                                       \
		.inen = _inen,                                         \
		.dat = _dat,                                           \
		.valid_mask = _valid,                                  \
	}

static struct caninos_gpio_bank caninos_gpio_banks[] = {
	GPIO_BANK(0, 32, GPIO_AOUTEN, GPIO_AINEN, GPIO_ADAT, 0xFFFFF000),
	GPIO_BANK(1, 32, GPIO_BOUTEN, GPIO_BINEN, GPIO_BDAT, 0xFFFFFFFF),
	GPIO_BANK(2, 32, GPIO_COUTEN, GPIO_CINEN, GPIO_CDAT, 0xBDBFFFFF),
	GPIO_BANK(3, 32, GPIO_DOUTEN, GPIO_DINEN, GPIO_DDAT, 0xF3FDB400),
	GPIO_BANK(4,  8, GPIO_EOUTEN, GPIO_EINEN, GPIO_EDAT, 0x000000FF),
};

static int caninos_pinctrl_probe(struct platform_device *pdev)
{
	struct caninos_pinctrl *data;
	int ret;
	
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	
	if (!data) {
		return -ENOMEM;
	}
	
	platform_set_drvdata(pdev, data);
	
	data->base = of_iomap(pdev->dev.of_node, 0);
	
	if (!data->base)
	{
		dev_err(&pdev->dev, "could not map memory\n");
		return -ENOMEM;
	}
	
	/* get GPIO/MFP clock */
	data->clk = devm_clk_get(&pdev->dev, NULL);
	
	if (IS_ERR(data->clk))
	{
		dev_err(&pdev->dev, "could not get clock\n");
		iounmap(data->base);
		return PTR_ERR(data->clk);
	}
	
	/* enable GPIO/MFP clock (just a formality, it is already enabled) */
	ret = clk_prepare_enable(data->clk);
	
	if (ret)
	{
		dev_err(&pdev->dev, "could not enable clock\n");
		iounmap(data->base);
		return ret;
	}
	
	spin_lock_init(&data->lock);
	data->dev = &pdev->dev;
	
	caninos_desc.name = dev_name(&pdev->dev);
	
	/* This is a temporary solution (until all mux functions are implemented) */
	writel(0x00000000, data->base + MFP_CTL0);
	writel(0x2e400060, data->base + MFP_CTL1);
	writel(0x10000600, data->base + MFP_CTL2);
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
	
	data->pinctrl = devm_pinctrl_register(&pdev->dev, &caninos_desc, data);
	
	if (IS_ERR(data->pinctrl))
	{
		dev_err(&pdev->dev, "could not register pin driver\n");
		iounmap(data->base);
		return PTR_ERR(data->pinctrl);
	}
	
	return 0;
}

static int caninos_gpio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct caninos_gpio_bank *bank;
	int ret;
	u32 id;
	
	if (of_property_read_u32(np, "caninos,gpio-bank", &id))
	{
		dev_err(&pdev->dev, "caninos,gpio-bank property not found\n");
		return -EINVAL;
	}
	
	if (id >= ARRAY_SIZE(caninos_gpio_banks))
	{
		dev_err(&pdev->dev, "invalid caninos,gpio-bank property\n");
		return -EINVAL;
	}
	
	bank = &caninos_gpio_banks[id];
	bank->gpio_chip.parent = &pdev->dev;
	bank->gpio_chip.of_node = np;
	spin_lock_init(&bank->lock);
	
	bank->base = of_iomap(np, 0);
	
	if (!bank->base)
	{
		dev_err(&pdev->dev, "could not map memory\n");
		return -ENOMEM;
	}
	
	ret = devm_gpiochip_add_data(&pdev->dev, &(bank->gpio_chip), bank);
	
	if (ret < 0)
	{
		dev_err(&pdev->dev, "could not add gpiochip\n");
		iounmap(bank->base);
		return ret;
	}
	
	return 0;
}

static const struct of_device_id caninos_pinctrl_dt_ids[] = {
	{ .compatible = "caninos,k7-pinctrl", },
	{ },
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

static const struct of_device_id caninos_gpio_dt_ids[] = {
	{ .compatible = "caninos,k7-gpio", },
	{ },
};

static struct platform_driver caninos_gpio_driver = {
	.driver	= {
		.name = "gpio-caninos",
		.of_match_table = caninos_gpio_dt_ids,
		.owner = THIS_MODULE,
		.suppress_bind_attrs = true,
	},
	.probe = caninos_gpio_probe,
};

static struct platform_driver * const drivers[] = {
	&caninos_gpio_driver,
	&caninos_pinctrl_driver,
};

static int __init caninos_pinctrl_init(void)
{
	return platform_register_drivers(drivers, ARRAY_SIZE(drivers));
}
arch_initcall(caninos_pinctrl_init);

