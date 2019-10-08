#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <dt-bindings/clock/caninos-clk.h>

#define DRIVER_NAME "caninos-cmu"
#define DRIVER_DESC "Caninos Labrador Clock Management Unit Driver."

#define S700_DEVPLL    0x04
#define S700_SD0CLK    0x50
#define S700_SD1CLK    0x54
#define S700_SD2CLK    0x58
#define S700_UART0CLK  0x5C
#define S700_UART1CLK  0x60
#define S700_UART2CLK  0x64
#define S700_UART3CLK  0x68
#define S700_UART4CLK  0x6C
#define S700_UART5CLK  0x70
#define S700_UART6CLK  0x74
#define S700_DEVCLKEN0 0xA0
#define S700_DEVCLKEN1 0xA4
#define S700_USBPLL    0xB0

#define HOSC_CLKRATE_HZ 24000000

enum
{
	DEVICE_DEVPLL,
	DEVICE_UART,
	DEVICE_SDC,
	DEVICE_HDCP2,
	DEVICE_CLK_GATE,
};

struct caninos_cmu_clock_reg_data
{
	int device_type;
	
	u32 enable_offset;
	u32 enable_clear_mask;
	u32 enable_set_mask;
	
	u32 disable_offset;
	u32 disable_clear_mask;
	u32 disable_set_mask;
	
	u32 value_offset;
	u32 value_mask;
};

struct caninos_clk
{
	struct clk_hw hw;
	void __iomem *cmu_base;
	unsigned long devpll_rate;
	int clkid;
	const struct caninos_cmu_clock_reg_data *data;
};

static struct caninos_clk *to_caninos_clk(struct clk_hw *hw)
{
	return container_of(hw, struct caninos_clk, hw);
}

static void caninos_clk_enable(struct caninos_clk *clk, bool enable)
{
	int id = clk->clkid;
	u32 val;
	
	if (enable)
	{
		val = readl(clk->cmu_base + clk->data[id].enable_offset);
		
		val &= ~(clk->data[id].enable_clear_mask);
		val |= (clk->data[id].enable_set_mask);
		
		writel(val, clk->cmu_base + clk->data[id].enable_offset);
	}
	else
	{
		val = readl(clk->cmu_base + clk->data[id].disable_offset);
		
		val &= ~(clk->data[id].disable_clear_mask);
		val |= (clk->data[id].disable_set_mask);
		
		writel(val, clk->cmu_base + clk->data[id].disable_offset);
	}
}

static unsigned long sdc_calc_clk_div(long rate, long parent_rate)
{
	unsigned long div, div128;
	
	if (rate < 187500) {
		rate = 187500;
	}
	
	if (rate > 200000000) {
		rate = 200000000;
	}
	
	rate *= 2;
	div = (parent_rate / rate);
	
	if (div >= 128)
	{
		div128 = div;
		div = div / 128;
			
		if (div128 % 128) {
			div++;
		}
			
		div--;
		div |= 0x100;
	}
	else
	{
		if (parent_rate % rate) {
			div++;
		}
		div--;
	}
	
	return div;
}

static long sdc_calc_clk_rate(unsigned long div, long parent_rate)
{
	long rate;
	
	if (div & 0x100)
	{
		div &= ~(0x100);
		div++;
		div *= 128;
	}
	else {
		div++;
	}
	
	rate = parent_rate / div;
	rate /= 2;
	return rate;
}

static long caninos_clk_round_rate(struct caninos_clk *clk, unsigned long rate)
{
	unsigned long div, parent_rate;
	int id = clk->clkid;
	
	rate = (rate == 0) ? (1) : (rate);
	
	if (clk->data[id].device_type == DEVICE_UART)
	{
		div  = (HOSC_CLKRATE_HZ / rate);
		div  = (div > 312) ? (624) : (div);
		rate = (HOSC_CLKRATE_HZ / div);
	}
	else if (clk->data[id].device_type == DEVICE_SDC)
	{
		parent_rate = clk->devpll_rate;
		div = sdc_calc_clk_div(rate, parent_rate);
		rate = sdc_calc_clk_rate(div, parent_rate);
	}
	else if (clk->data[id].device_type == DEVICE_DEVPLL)
	{
		rate = clk->devpll_rate;
	}
	
	return rate;
}

static void caninos_clk_set_rate(struct caninos_clk *clk, unsigned long rate)
{
	unsigned long parent_rate, div;
	int id = clk->clkid;
	u32 val;
	
	if (clk->data[id].device_type == DEVICE_UART)
	{
		rate = caninos_clk_round_rate(clk, rate);
		
		val = readl(clk->cmu_base + clk->data[id].value_offset);
		
		val &= ~(clk->data[id].value_mask);
		val |= ((HOSC_CLKRATE_HZ / rate) - 1);
		
		writel(val, clk->cmu_base + clk->data[id].value_offset);
	}
	else if (clk->data[id].device_type == DEVICE_SDC)
	{
		parent_rate = clk->devpll_rate;
		
		div = sdc_calc_clk_div(rate, parent_rate);
		
		val = readl(clk->cmu_base + clk->data[id].value_offset);
		
		val &= ~(clk->data[id].value_mask);
		val |= div;
		
		writel(val, clk->cmu_base + clk->data[id].value_offset);
	}
}

static long caninos_clk_get_rate(struct caninos_clk *clk)
{
	unsigned long parent_rate, div;
	int id = clk->clkid;
	long rate = 0;
	
	if (clk->data[id].device_type == DEVICE_UART)
	{
		div = readl(clk->cmu_base + clk->data[id].value_offset);
		div &= clk->data[id].value_mask;
		div  = (div == 312) ? (624) : (div + 1);
		rate = HOSC_CLKRATE_HZ / div;
	}
	else if (clk->data[id].device_type == DEVICE_SDC)
	{
		parent_rate = clk->devpll_rate;
		
		div = readl(clk->cmu_base + clk->data[id].value_offset);
		div &= clk->data[id].value_mask;
		
		rate = sdc_calc_clk_rate(div, parent_rate);
	}
	else if (clk->data[id].device_type == DEVICE_DEVPLL)
	{
		rate = clk->devpll_rate;
	}
	
	return rate;
}

static int clk_ops_enable(struct clk_hw *hw)
{
	struct caninos_clk *clk = to_caninos_clk(hw);
	caninos_clk_enable(clk, true);
	return 0;
}

static void clk_ops_disable(struct clk_hw *hw)
{
	struct caninos_clk *clk = to_caninos_clk(hw);
	caninos_clk_enable(clk, false);
}

static int clk_ops_set_rate
(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
	struct caninos_clk *clk = container_of(hw, struct caninos_clk, hw);
	caninos_clk_set_rate(clk, rate);
	return 0;
}

static long clk_ops_round_rate
(struct clk_hw *hw, unsigned long rate, unsigned long *parent_rate)
{
	struct caninos_clk *clk = container_of(hw, struct caninos_clk, hw);
	return caninos_clk_round_rate(clk, rate);
}

static unsigned long clk_ops_recalc_rate
(struct clk_hw *hw, unsigned long parent_rate)
{
	struct caninos_clk *clk = container_of(hw, struct caninos_clk, hw);
	return caninos_clk_get_rate(clk);
}

const static struct clk_ops caninos_clk_ops = {
	.enable      = clk_ops_enable,
	.disable     = clk_ops_disable,
	.set_rate    = clk_ops_set_rate,
	.round_rate  = clk_ops_round_rate,
	.recalc_rate = clk_ops_recalc_rate,
};

static struct clk_hw caninos_clk_hw[] = {
	[CLK_DEVPLL] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-devpll", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_UART0] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-uart0", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_UART1] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-uart1", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_UART2] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-uart2", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_UART3] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-uart3", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_UART4] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-uart4", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_UART5] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-uart5", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_UART6] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-uart6", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_SDC0] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-sdc0", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_SDC1] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-sdc1", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_SDC2] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-sdc2", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_HDCP2] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-hdcp2", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_USB2H0_PLLEN] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-usb2h0-pllen", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_USB2H0_PHY] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-usb2h0-phy", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_USB2H0_CCE] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-usb2h0-cce", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_USB2H1_PLLEN] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-usb2h1-pllen", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_USB2H1_PHY] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-usb2h1-phy", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_USB2H1_CCE] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-usb2h1-cce", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
	[CLK_GPIO] = {
		.init = CLK_HW_INIT_NO_PARENT("clk-gpio", &caninos_clk_ops, CLK_IGNORE_UNUSED),
	},
};

static struct caninos_cmu_clock_reg_data k7_reg_data[] = {
	[CLK_DEVPLL] = {
		.device_type        = DEVICE_DEVPLL,
		.value_offset       = S700_DEVPLL,
		.value_mask         = 0x7F,
	},
	[CLK_UART0] = {
		.device_type        = DEVICE_UART,
		.enable_offset      = S700_DEVCLKEN1,
		.enable_clear_mask  = BIT(8),
		.enable_set_mask    = BIT(8),
		.disable_offset     = S700_DEVCLKEN1,
		.disable_clear_mask = BIT(8),
		.disable_set_mask   = 0x0,
		.value_offset       = S700_UART0CLK,
		.value_mask         = 0x1FF,
	},
	[CLK_UART1] = {
		.device_type        = DEVICE_UART,
		.enable_offset      = S700_DEVCLKEN1,
		.enable_clear_mask  = BIT(9),
		.enable_set_mask    = BIT(9),
		.disable_offset     = S700_DEVCLKEN1,
		.disable_clear_mask = BIT(9),
		.disable_set_mask   = 0x0,
		.value_offset       = S700_UART1CLK,
		.value_mask         = 0x1FF,
	},
	[CLK_UART2] = {
		.device_type        = DEVICE_UART,
		.enable_offset      = S700_DEVCLKEN1,
		.enable_clear_mask  = BIT(10),
		.enable_set_mask    = BIT(10),
		.disable_offset     = S700_DEVCLKEN1,
		.disable_clear_mask = BIT(10),
		.disable_set_mask   = 0x0,
		.value_offset       = S700_UART2CLK,
		.value_mask         = 0x1FF,
	},
	[CLK_UART3] = {
		.device_type        = DEVICE_UART,
		.enable_offset      = S700_DEVCLKEN1,
		.enable_clear_mask  = BIT(11),
		.enable_set_mask    = BIT(11),
		.disable_offset     = S700_DEVCLKEN1,
		.disable_clear_mask = BIT(11),
		.disable_set_mask   = 0x0,
		.value_offset       = S700_UART3CLK,
		.value_mask         = 0x1FF,
	},
	[CLK_UART4] = {
		.device_type        = DEVICE_UART,
		.enable_offset      = S700_DEVCLKEN1,
		.enable_clear_mask  = BIT(12),
		.enable_set_mask    = BIT(12),
		.disable_offset     = S700_DEVCLKEN1,
		.disable_clear_mask = BIT(12),
		.disable_set_mask   = 0x0,
		.value_offset       = S700_UART4CLK,
		.value_mask         = 0x1FF,
	},
	[CLK_UART5] = {
		.device_type        = DEVICE_UART,
		.enable_offset      = S700_DEVCLKEN1,
		.enable_clear_mask  = BIT(13),
		.enable_set_mask    = BIT(13),
		.disable_offset     = S700_DEVCLKEN1,
		.disable_clear_mask = BIT(13),
		.disable_set_mask   = 0x0,
		.value_offset       = S700_UART5CLK,
		.value_mask         = 0x1FF,
	},
	[CLK_UART6] = {
		.device_type        = DEVICE_UART,
		.enable_offset      = S700_DEVCLKEN1,
		.enable_clear_mask  = BIT(14),
		.enable_set_mask    = BIT(14),
		.disable_offset     = S700_DEVCLKEN1,
		.disable_clear_mask = BIT(14),
		.disable_set_mask   = 0x0,
		.value_offset       = S700_UART6CLK,
		.value_mask         = 0x1FF,
	},
	[CLK_SDC0] = {
		.device_type        = DEVICE_SDC,
		.enable_offset      = S700_DEVCLKEN0,
		.enable_clear_mask  = BIT(22),
		.enable_set_mask    = BIT(22),
		.disable_offset     = S700_DEVCLKEN0,
		.disable_clear_mask = BIT(22),
		.disable_set_mask   = 0x0,
		.value_offset       = S700_SD0CLK,
		.value_mask         = ~(0xFFFFFCE0),
	},
	[CLK_SDC1] = {
		.device_type        = DEVICE_SDC,
		.enable_offset      = S700_DEVCLKEN0,
		.enable_clear_mask  = BIT(23),
		.enable_set_mask    = BIT(23),
		.disable_offset     = S700_DEVCLKEN0,
		.disable_clear_mask = BIT(23),
		.disable_set_mask   = 0x0,
		.value_offset       = S700_SD1CLK,
		.value_mask         = ~(0xFFFFFCE0),
	},
	[CLK_SDC2] = {
		.device_type        = DEVICE_SDC,
		.enable_offset      = S700_DEVCLKEN0,
		.enable_clear_mask  = BIT(24),
		.enable_set_mask    = BIT(24),
		.disable_offset     = S700_DEVCLKEN0,
		.disable_clear_mask = BIT(24),
		.disable_set_mask   = 0x0,
		.value_offset       = S700_SD2CLK,
		.value_mask         = ~(0xFFFFFCE0),
	},
	[CLK_HDCP2] = {
		.device_type        = DEVICE_HDCP2,
		.enable_offset      = S700_DEVCLKEN0,
		.enable_clear_mask  = BIT(6),
		.enable_set_mask    = BIT(6),
		.disable_offset     = S700_DEVCLKEN0,
		.disable_clear_mask = BIT(6),
		.disable_set_mask   = 0x0,
	},
	[CLK_USB2H0_PLLEN] = {
		.device_type        = DEVICE_CLK_GATE,
		.enable_offset      = S700_USBPLL,
		.enable_clear_mask  = BIT(12),
		.enable_set_mask    = BIT(12),
		.disable_offset     = S700_USBPLL,
		.disable_clear_mask = BIT(12),
		.disable_set_mask   = 0x0,
	},
	[CLK_USB2H0_PHY] = {
		.device_type        = DEVICE_CLK_GATE,
		.enable_offset      = S700_USBPLL,
		.enable_clear_mask  = BIT(10),
		.enable_set_mask    = BIT(10),
		.disable_offset     = S700_USBPLL,
		.disable_clear_mask = BIT(10),
		.disable_set_mask   = 0x0,
	},
	[CLK_USB2H0_CCE] = {
		.device_type        = DEVICE_CLK_GATE,
		.enable_offset      = S700_DEVCLKEN0,
		.enable_clear_mask  = BIT(26),
		.enable_set_mask    = BIT(26),
		.disable_offset     = S700_DEVCLKEN0,
		.disable_clear_mask = BIT(26),
		.disable_set_mask   = 0x0,
	},
	[CLK_USB2H1_PLLEN] = {
		.device_type        = DEVICE_CLK_GATE,
		.enable_offset      = S700_USBPLL,
		.enable_clear_mask  = BIT(13),
		.enable_set_mask    = BIT(13),
		.disable_offset     = S700_USBPLL,
		.disable_clear_mask = BIT(13),
		.disable_set_mask   = 0x0,
	},
	[CLK_USB2H1_PHY] = {
		.device_type        = DEVICE_CLK_GATE,
		.enable_offset      = S700_USBPLL,
		.enable_clear_mask  = BIT(11),
		.enable_set_mask    = BIT(11),
		.disable_offset     = S700_USBPLL,
		.disable_clear_mask = BIT(11),
		.disable_set_mask   = 0x0,
	},
	[CLK_USB2H1_CCE] = {
		.device_type        = DEVICE_CLK_GATE,
		.enable_offset      = S700_DEVCLKEN0,
		.enable_clear_mask  = BIT(27),
		.enable_set_mask    = BIT(27),
		.disable_offset     = S700_DEVCLKEN0,
		.disable_clear_mask = BIT(27),
		.disable_set_mask   = 0x0,
	},
	[CLK_GPIO] = {
		.device_type        = DEVICE_CLK_GATE,
		.enable_offset      = S700_DEVCLKEN1,
		.enable_clear_mask  = BIT(25),
		.enable_set_mask    = BIT(25),
		.disable_offset     = S700_DEVCLKEN1,
		.disable_clear_mask = BIT(25),
		.disable_set_mask   = 0x0,
	},
};

static const struct of_device_id caninos_cmu_clock_dt_ids[] = {
	{ .compatible = "caninos,k7-cmu", .data = &k7_reg_data },
	{ }
};
MODULE_DEVICE_TABLE(of, caninos_cmu_clock_dt_ids);

static int caninos_cmu_clock_probe(struct platform_device *pdev)
{
	const struct caninos_cmu_clock_reg_data *data;
	struct clk_hw_onecell_data *hw_clks;
	const struct of_device_id *of_id;
	struct device *dev = &pdev->dev;
	unsigned long devpll_rate;
	struct caninos_clk *clocks;
	void __iomem *cmu_base;
	struct resource *res;
	int i;
	
	of_id = of_match_node(caninos_cmu_clock_dt_ids, dev->of_node);
	
	if (!of_id)
	{
		dev_err(dev, "could not match device type.\n");
		return -ENODEV;
	}
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res)
	{
		dev_err(dev, "could not get register base from DTS.\n");
		return -ENOMEM;
	}
	
	hw_clks = devm_kzalloc(dev, struct_size(hw_clks, hws, NR_CLOCKS), GFP_KERNEL);
	
	if (IS_ERR(hw_clks))
	{
		dev_err(dev, "could not allocate clock onecell data.\n");
		return PTR_ERR(hw_clks);
	}
	
	cmu_base = devm_ioremap_resource(dev, res);
	
	if (IS_ERR(cmu_base))
	{
		dev_err(dev, "could not map cmu-base registers.\n");
		return PTR_ERR(cmu_base);
	}
	
	data = of_id->data;
	
	devpll_rate = readl(cmu_base + data[CLK_DEVPLL].value_offset);
	devpll_rate &= data[CLK_DEVPLL].value_mask;
	devpll_rate *= 6000000;
	
	clocks = devm_kzalloc(dev, sizeof(*clocks) * NR_CLOCKS, GFP_KERNEL);
	
	if (IS_ERR(clocks))
	{
		dev_err(dev, "could not allocate clock data.\n");
		return PTR_ERR(cmu_base);
	}
	
	hw_clks->num = NR_CLOCKS;
	
	for (i = 0; i < hw_clks->num; i++)
	{
		clocks[i].hw          = caninos_clk_hw[i];
		clocks[i].cmu_base    = cmu_base;
		clocks[i].devpll_rate = devpll_rate;
		clocks[i].clkid       = i;
		clocks[i].data        = data;
		
		hw_clks->hws[i] = &clocks[i].hw;
		
		if (devm_clk_hw_register(dev, hw_clks->hws[i]))
		{
			dev_err(dev, "could not register clock %s.\n", caninos_clk_hw[i].init->name);
			return -ENOMEM;
		}
	}
	
	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get, hw_clks);
}

static struct platform_driver caninos_cmu_clock_driver = {
	.probe = caninos_cmu_clock_probe,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = caninos_cmu_clock_dt_ids,
	},
};
module_platform_driver(caninos_cmu_clock_driver);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
