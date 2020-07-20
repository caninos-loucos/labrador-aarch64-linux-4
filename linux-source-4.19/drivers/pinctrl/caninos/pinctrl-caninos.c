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

static int caninos_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	return 0;
}

static void caninos_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	////
}

static int caninos_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	return 0;
}

static int caninos_gpio_direction_output(struct gpio_chip *chip,
                                         unsigned offset, int value)
{
	return 0;
}

static int caninos_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	return 0;
}

static int caninos_gpio_request_enable(struct pinctrl_dev *pctldev,
                                       struct pinctrl_gpio_range *range,
                                       unsigned offset)
{
	return 0;
}

static int caninos_gpio_set_direction(struct pinctrl_dev *pctldev,
                                      struct pinctrl_gpio_range *range,
                                      unsigned offset, bool input)
{
	return 0;
}

static struct pinctrl_ops caninos_pctrl_ops = {
	.get_groups_count = caninos_get_groups_count,
	.get_group_name = caninos_get_group_name,
	.get_group_pins = caninos_get_group_pins,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
	.dt_free_map = pinctrl_utils_free_map,
};

static struct pinmux_ops caninos_pmxops = {
	.get_functions_count = caninos_pmx_get_functions_count,
	.get_function_name = caninos_pmx_get_fname,
	.get_function_groups = caninos_pmx_get_fgroups,
	.set_mux = caninos_pmx_set_mux,
	.gpio_request_enable = caninos_gpio_request_enable,
	.gpio_set_direction = caninos_gpio_set_direction,
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

#define GPIO_BANK(_bank, _npins)                               \
	{								                           \
		.gpio_chip = {                                         \
			.label = "GPIO" #_bank,                            \
			.request = gpiochip_generic_request,               \
			.free = gpiochip_generic_free,                     \
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
	}

static struct caninos_gpio_bank caninos_gpio_banks[] = {
	GPIO_BANK(0, 32),
	GPIO_BANK(1, 32),
	GPIO_BANK(2, 32),
	GPIO_BANK(3, 32),
	GPIO_BANK(4, 8),
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
	struct device_node *np = pdev->dev.of_node;
	struct caninos_gpio_bank *bank;
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
	
	return devm_gpiochip_add_data(&pdev->dev, &(bank->gpio_chip), bank);
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
	&caninos_gpio_driver,
	&caninos_pinctrl_driver,
};

static int __init caninos_pinctrl_init(void)
{
	return platform_register_drivers(drivers, ARRAY_SIZE(drivers));
}
arch_initcall(caninos_pinctrl_init);

