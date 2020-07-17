#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/machine.h>
#include <linux/slab.h>
#include <asm/io.h>

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



#define GPIOA(x) (x)
#define GPIOB(x) (32+(x))
#define GPIOC(x) (64+(x))
#define GPIOD(x) (96+(x))
#define GPIOE(x) (128+(x))

/* from gpio-caninos.c */
extern int caninos_request_extio_group(const int *gpios, int num);
extern int caninos_release_extio_group(const int *gpios, int num);

void __iomem *regs = NULL;

struct clk *clk = NULL;

struct caninos_group {
	const char *name;
	unsigned *pins;
	unsigned num_pins;
};

const int pinctrl_to_gpio[] = {
	[0] = GPIOE(3),
	[1] = GPIOE(2),
	[2] = GPIOC(27),
	[3] = GPIOC(26),
	[4] = GPIOB(8),
};

const struct pinctrl_pin_desc caninos_pins[] = {
	PINCTRL_PIN(0, "EXTIO_3"),
	PINCTRL_PIN(1, "EXTIO_5"),
	PINCTRL_PIN(2, "EXTIO_8"),
	PINCTRL_PIN(3, "EXTIO_10"),
	PINCTRL_PIN(4, "EXTIO_12"),
};

static unsigned int spi1_pins[]  = { 0, 1, 2, 3 };
static unsigned int uart0_pins[] = { 2, 3 };
static unsigned int i2c2_pins[]  = { 0, 1 };
static unsigned int pwm_pins[]   = { 4 };

static struct caninos_group caninos_groups[] = {
	{
		.name = "spi1_grp",
		.pins = spi1_pins,
		.num_pins = ARRAY_SIZE(spi1_pins),
	},
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

static struct pinctrl_ops caninos_pctrl_ops = {
	.get_groups_count = caninos_get_groups_count,
	.get_group_name = caninos_get_group_name,
	.get_group_pins = caninos_get_group_pins,
	.dt_node_to_map = caninos_dt_node_to_map,
	.dt_free_map = caninos_dt_free_map,
};

struct caninos_pmx_func {
	const char *name;
	const char * const *groups;
	const unsigned num_groups;
};

static const char * const spi1_groups[]  = { "spi1_grp" };
static const char * const uart0_groups[] = { "uart0_grp"};
static const char * const i2c2_groups[]  = { "i2c2_grp" };
static const char * const pwm_groups[]   = { "pwm_grp"  };

static const struct caninos_pmx_func caninos_functions[] = {
	{
		.name = "spi1",
		.groups = spi1_groups,
		.num_groups = ARRAY_SIZE(spi1_groups),
	},
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

static int caninos_pmx_set_mux(struct pinctrl_dev *pctldev, unsigned func_selector, unsigned group_selector)
{
	pr_info("caninos_pmx_set_mux: function=%u group=%u\n",func_selector, group_selector);
	return 0;
}

static struct pinmux_ops caninos_pmxops = {
	.request = caninos_pmx_request,
	.free = caninos_pmx_free,
	.set_mux = caninos_pmx_set_mux,
	.get_functions_count = caninos_pmx_get_functions_count,
	.get_function_name = caninos_pmx_get_fname,
	.get_function_groups = caninos_pmx_get_fgroups,
};

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

static struct pinconf_ops caninos_pconf_ops = {
	.pin_config_get = caninos_pin_config_get,
	.pin_config_set = caninos_pin_config_set,
	.pin_config_group_get = caninos_pin_config_group_get,
	.pin_config_group_set = caninos_pin_config_group_set,
};

static struct pinctrl_desc caninos_desc = {
	.name = "pinctrl-caninos",
	.pins = caninos_pins,
	.npins = ARRAY_SIZE(caninos_pins),
	.pctlops = &caninos_pctrl_ops,
	.pmxops = &caninos_pmxops,
	.confops = &caninos_pconf_ops,
	.owner = THIS_MODULE,
};

static int caninos_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pinctrl_dev *pctl;
	
	platform_set_drvdata(pdev, NULL);
	
	regs = of_iomap(dev->of_node, 0);
	
	if (regs == NULL)
	{
		dev_err(dev, "of_iomap() error\n");
		return -ENOMEM;
	}
	
	/* get GPIO/MFP clock */
	clk = devm_clk_get(&pdev->dev, NULL);
	
	if (IS_ERR(clk))
	{
		dev_err(&pdev->dev, "could not get clock\n");
		return -ENODEV;
	}
	
	/* enable GPIO/MFP clock (just a formality, it is already enabled) */
	clk_prepare_enable(clk);
	
	/* This is a temporary solution (until all mux functions are implemented) */
	writel(0x00000000, regs + MFP_CTL0);
	writel(0x2e400000, regs + MFP_CTL1);
	writel(0x00000600, regs + MFP_CTL2);
	writel(0x40000008, regs + MFP_CTL3);
	
	writel(0x2aaaaaaa, regs + PAD_DRV0);
	writel(0xaacf0800, regs + PAD_DRV1);
	
	dev_info(&pdev->dev, "PAD_PULLCTL0: 0x%08x\n", readl(regs + PAD_PULLCTL0));
	dev_info(&pdev->dev, "PAD_PULLCTL1: 0x%08x\n", readl(regs + PAD_PULLCTL1));
	dev_info(&pdev->dev, "PAD_PULLCTL2: 0x%08x\n", readl(regs + PAD_PULLCTL2));
	dev_info(&pdev->dev, "PAD_ST0:      0x%08x\n", readl(regs + PAD_ST0));
	dev_info(&pdev->dev, "PAD_ST1:      0x%08x\n", readl(regs + PAD_ST1));
	dev_info(&pdev->dev, "PAD_CTL:      0x%08x\n", readl(regs + PAD_CTL));
	dev_info(&pdev->dev, "PAD_DRV0:     0x%08x\n", readl(regs + PAD_DRV0));
	dev_info(&pdev->dev, "PAD_DRV1:     0x%08x\n", readl(regs + PAD_DRV1));
	dev_info(&pdev->dev, "PAD_DRV2:     0x%08x\n", readl(regs + PAD_DRV2));
	
	/* i2c2 pull-up, level 1 drive
	 mmc pull-up level 3
	 */
	 
	pctl = pinctrl_register(&caninos_desc, &pdev->dev, NULL);
	
	if (IS_ERR(pctl))
	{
		dev_err(&pdev->dev, "could not register pin driver\n");
		return -EINVAL;
	}
	
	platform_set_drvdata(pdev, pctl);
	return 0;
}

static int caninos_pinctrl_remove(struct platform_device *pdev)
{
	struct pinctrl_dev *pctl = platform_get_drvdata(pdev);
	//may get here only at shutdown... do we really need to clear things up?
	pinctrl_unregister(pctl);
	iounmap(regs);
	//clk_disable_unprepare(clk); must never be disabled!
	return 0;
}

static const struct of_device_id caninos_pinctrl_dt_ids[] = {
	{ .compatible = "caninos,k7-pinctrl", },
	{ },
};
MODULE_DEVICE_TABLE(of, caninos_pinctrl_dt_ids);

static struct platform_driver caninos_pinctrl_driver = {
	.driver	= {
		.name = "pinctrl-caninos",
		.owner = THIS_MODULE,
		.of_match_table = caninos_pinctrl_dt_ids,
	},
	.probe = caninos_pinctrl_probe,
	.remove = caninos_pinctrl_remove,
};

static int __init caninos_pinctrl_init(void) {
	return platform_driver_register(&caninos_pinctrl_driver);
}

arch_initcall(caninos_pinctrl_init);

