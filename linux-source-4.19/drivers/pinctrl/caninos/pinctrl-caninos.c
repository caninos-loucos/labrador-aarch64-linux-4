// SPDX-License-Identifier: GPL-2.0
/*
 * Pinctrl/GPIO driver for Caninos Labrador
 *
 * Copyright (c) 2022-2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2018-2020 LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
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
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio/driver.h>
#include "pinctrl-caninos.h"
#include "../pinctrl-utils.h"

static int caninos_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct caninos_pinctrl *pctl = to_caninos_pinctrl(pctldev);
	return pctl->ngroups;
}

int caninos_pmx_get_functions_count(struct pinctrl_dev *pctldev)
{
	struct caninos_pinctrl *pctl = to_caninos_pinctrl(pctldev);
	return pctl->nfuncs;
}

static const char *caninos_get_group_name(struct pinctrl_dev *pctldev,
                                          unsigned selector)
{
	struct caninos_pinctrl *pctl = to_caninos_pinctrl(pctldev);
	return pctl->groups[selector].name;
}

static int caninos_get_group_pins(struct pinctrl_dev *pctldev,
                                  unsigned selector, const unsigned **pins,
                                  unsigned *num_pins)
{
	struct caninos_pinctrl *pctl = to_caninos_pinctrl(pctldev);
	*pins = pctl->groups[selector].pins;
	*num_pins = pctl->groups[selector].num_pins;
	return 0;
}

const char *caninos_pmx_get_fname(struct pinctrl_dev *pctldev,
                                  unsigned selector)
{
	struct caninos_pinctrl *pctl = to_caninos_pinctrl(pctldev);
	return pctl->functions[selector].name;
}

static int caninos_pmx_get_fgroups(struct pinctrl_dev *pctldev,
                                   unsigned selector,
                                   const char * const **groups,
                                   unsigned * num_groups)
{
	struct caninos_pinctrl *pctl = to_caninos_pinctrl(pctldev);
	*groups = pctl->functions[selector].groups;
	*num_groups = pctl->functions[selector].num_groups;
	return 0;
}

static void caninos_raw_direction_output(struct caninos_gpio_chip *chip,
                                         unsigned offset, int value)
{
	struct caninos_pinctrl *pinctrl = chip->pinctrl;
	unsigned long flags;
	
	void __iomem *base  = pinctrl->base + GPIO_REGBASE(chip->addr);
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
	unsigned long flags;
	u32 val;
	
	void __iomem *base  = pinctrl->base + GPIO_REGBASE(chip->addr);
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
	unsigned long flags;
	u32 val;
	
	void __iomem *base  = pinctrl->base + GPIO_REGBASE(chip->addr);
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

static int caninos_raw_gpio_get(struct caninos_gpio_chip *chip,
                                unsigned offset)
{
	void __iomem *base = chip->pinctrl->base + GPIO_REGBASE(chip->addr);
	u32 val = 0;
	
	if (chip->mask & BIT(offset)) {
		val = readl(base + GPIO_ADAT);
	}
	
	return !!(val & BIT(offset));
}

static void caninos_raw_gpio_set(struct caninos_gpio_chip *chip,
                                 unsigned offset, int value)
{
	void __iomem *base = chip->pinctrl->base + GPIO_REGBASE(chip->addr);
	u32 val;
	
	if (chip->mask & BIT(offset))
	{
		val = readl(base + GPIO_ADAT);
		
		if (value) {
			val |= BIT(offset);
		}
		else {
			val &= ~BIT(offset);
		}
		
		writel(val, base + GPIO_ADAT);
	}
}

static int caninos_raw_get_direction(struct caninos_gpio_chip *chip,
                                     unsigned offset)
{
	struct caninos_pinctrl *pinctrl = chip->pinctrl;
	unsigned long flags;
	u32 val;
	
	void __iomem *base = pinctrl->base + GPIO_REGBASE(chip->addr);
	
	if (chip->mask & BIT(offset))
	{
		spin_lock_irqsave(&pinctrl->lock, flags);
		val = readl(base + GPIO_AINEN);
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

static void caninos_gpio_irq_ack(struct irq_data *data) 
{
	struct caninos_gpio_chip *bank = irq_data_get_irq_chip_data(data);
	u32 val;
	unsigned long type, flags;
	int line, hwirq;

	raw_spin_lock_irqsave(bank->lock, flags);

	pr_err("here?");
	val = readl(bank->pinctrl->base + INTC_GPIOCTL);
	hwirq = irq_linear_revmap(data->domain, data->irq);
	line = data->irq - hwirq;

	// check if interrupt type is edge-based
	if (line >= 16) {
		type = readl(bank->pinctrl->base + INTC_GPIOX_TYPE0(bank->addr));
		type &= 0x3 << 2*(line - 16);
		type = (type >> (30 - 2*(line/2))); // justify type
	} else {
		type = readl(bank->pinctrl->base + INTC_GPIOX_TYPE1(bank->addr));
		type &= (0x3 << (30 - 2*(15 - line)));
		type = (type >> (2*line - 30));
	}

	// if edge-based, clear pendinig bit
	if (type && 0x2)
	{
		val &= ~INTC_GPIOCTL_GPIOX_PD(bank->addr);
		// add reserved values?
		writel(val, bank->pinctrl->base + INTC_GPIOCTL);
	}
	raw_spin_unlock_irqrestore(bank->lock, flags);
}

static void caninos_gpio_irq_enable(struct irq_data *data) 
{
	struct caninos_gpio_chip *bank = irq_data_get_irq_chip_data(data);
	u32 val;
	unsigned long flags;

	raw_spin_lock_irqsave(bank->lock, flags);
	val = readl(bank->pinctrl->base + INTC_GPIOCTL);
	val |= INTC_GPIOCTL_GPIOX_EN(bank->addr);

	writel(val, bank->pinctrl->base + INTC_GPIOCTL);
	raw_spin_unlock_irqrestore(bank->lock, flags);
}

static void caninos_gpio_irq_disable(struct irq_data *data) 
{
	struct caninos_gpio_chip *bank = irq_data_get_irq_chip_data(data);
	u32 val;
	unsigned long flags;

	raw_spin_lock_irqsave(bank->lock, flags);
	val = readl(bank->pinctrl->base + INTC_GPIOCTL);
	val &= ~INTC_GPIOCTL_GPIOX_EN((int)bank->addr);

	writel(val, bank->pinctrl->base + INTC_GPIOCTL);
	raw_spin_unlock_irqrestore(bank->lock, flags);
}

static int caninos_gpio_irq_set_type(struct irq_data *data, unsigned int flow_type) 
{
	struct caninos_gpio_chip *bank = irq_data_get_irq_chip_data(data);
	u32 val0, val1;
	unsigned long flags, type;
	int line, hwirq;

	raw_spin_lock_irqsave(bank->lock, flags);	
	val0 = readl(bank->pinctrl->base + INTC_GPIOX_TYPE0(bank->addr));
	val1 = readl(bank->pinctrl->base + INTC_GPIOX_TYPE1(bank->addr));

	hwirq = irq_linear_revmap(data->domain, data->irq);
	line = data->irq - hwirq;

	pr_info("LABEL %s, irq = %d, hwirq = %d, line = %d", bank->label, data->irq, hwirq, line);

	switch(flow_type)
	{
		case IRQ_TYPE_LEVEL_HIGH:
			type = GPIO_INT_TYPE_HIGH;
		case IRQ_TYPE_LEVEL_LOW:
			type = GPIO_INT_TYPE_LOW;
		case IRQ_TYPE_EDGE_RISING:
			type = GPIO_INT_TYPE_RISING;
		case IRQ_TYPE_EDGE_FALLING:
			type = GPIO_INT_TYPE_FALLING;
		default:
			dev_err(bank->pinctrl->dev, "unsupported external interrupt type\n");
			return -EINVAL;
	}

	if (line >= 16) {
		val0 &= ~(0x3 << 2*(line - 16));
		val0 |= (type << 2*(line - 16));
	} else {
		val1 &= ~(0x3 << (30 - 2*(15 - line)));
		val1 |= (type << (30 - 2*(15 - line)));
	}

	writel(val0, bank->pinctrl->base + INTC_GPIOX_TYPE0(bank->addr));
	writel(val1, bank->pinctrl->base + INTC_GPIOX_TYPE1(bank->addr));
	raw_spin_unlock_irqrestore(bank->lock, flags);

	return 0;
}

static void caninos_gpio_irq_mask(struct irq_data *data)
{
	struct caninos_gpio_chip *bank = irq_data_get_irq_chip_data(data);
	u32 val;
	unsigned long flags;
	int line, hwirq;

	raw_spin_lock_irqsave(bank->lock, flags);
	val = readl(bank->pinctrl->base + INTC_GPIOX_MSK(bank->addr));

	hwirq = irq_linear_revmap(data->domain, data->irq);
	line = data->irq - hwirq;

	pr_info("LABEL %s, irq = %d, hwirq = %d, line = %d", bank->label, data->irq, hwirq, line);

	val |= (1 << line);

	// mask reserved values?
	writel(val, bank->pinctrl->base + INTC_GPIOX_MSK(bank->addr));
	raw_spin_unlock_irqrestore(bank->lock, flags);
}

static void caninos_gpio_irq_unmask(struct irq_data *data) 
{
	struct caninos_gpio_chip *bank = irq_data_get_irq_chip_data(data);
	u32 val;
	unsigned long flags;
	int line, hwirq;

	raw_spin_lock_irqsave(bank->lock, flags);
	val = readl(bank->pinctrl->base + INTC_GPIOX_MSK(bank->addr));

	hwirq = irq_linear_revmap(data->domain, data->irq);
	line = data->irq - hwirq;

	pr_info("LABEL %s, irq = %d, hwirq = %d, line = %d", bank->label, data->irq, hwirq, line);

	val &= ~(1 << line);

	// mask reserved values?
	writel(val, bank->pinctrl->base + INTC_GPIOX_MSK(bank->addr));
	raw_spin_unlock_irqrestore(bank->lock, flags);
}

//https://elixir.bootlin.com/linux/v4.20.17/source/drivers/gpio/gpio-rcar.c#L501
static irqreturn_t caninos_handle_irq(int irq, void *data)
{
	struct caninos_gpio_chip *bank = data;
	u32 val;
	int gpio;
	
	val = readl(INTC_GPIOX_PD(bank->addr));
	val &= readl(INTC_GPIOX_MSK(bank->addr));

	// check parsing direction
	for_each_set_bit(gpio, &val, bank->gpio_chip->ngpio) {
		generic_handle_irq(irq_find_mapping(bank->gpio_chip->irq.domain, gpio));
	}
	
	return IRQ_HANDLED;
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
	struct gpio_chip *gpio_chip;
	struct irq_chip *irq_chip;
	int i, ret;
	
	for (i = 0; i < pctl->nbanks; i++)
	{
		bank = &pctl->banks[i];
		gpio_chip = &bank->gpio_chip;
		irq_chip = &bank->irq_chip;
		
		ret = gpiochip_add_data(gpio_chip, bank);
		
		if (ret < 0) {
			dev_err(dev, "could not add gpio bank %s\n", bank->label);
			return ret;
		}
		
		ret = gpiochip_irqchip_add(gpio_chip, irq_chip, 0, handle_edge_irq,
		                           IRQ_TYPE_NONE);
		
		if (ret < 0) {
			dev_err(dev, "failed to add irq chip for bank %s\n", bank->label);
			return ret;
		}
		
		ret = request_irq(bank->irq, caninos_handle_irq, IRQF_SHARED,
		                  bank->label, bank);
		
		if (ret < 0) {
			dev_err(dev, "failed to add irq chip for bank %s\n", bank->label);
			return ret;
		}
		
		dev_info(dev, "gpio bank %s added\n", bank->label);
	}
	
	return 0;
}

static int caninos_gpiolib_parse_bank(struct caninos_pinctrl *pctl,
                                      struct device_node *np, int bank_n)
{
	struct caninos_gpio_chip *bank = &pctl->banks[bank_n];
	struct device *dev = pctl->dev;
	int i = 0, ret, bank_nr, npins;
	struct of_phandle_args args;
	const char *label;
	unsigned int irq;
	u32 mask;
	
	if (!of_parse_phandle_with_fixed_args(np, "gpio-ranges", 3, i, &args))
	{
		bank_nr = args.args[1] / GPIO_PER_BANK;
		
		if (bank_nr != bank_n) {
			dev_err(dev, "gpio bank number mismatch between OF and driver");
			return -EINVAL;
		}
		
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

	if ((ret = of_property_read_string(np, "gpio-label", &label)) < 0) {
		dev_err(dev, "missing gpio-label property on bank %s\n", np->full_name);
		return ret;
	}

	if ((ret = of_property_read_u32(np, "gpio-mask", &mask)) < 0) {
		dev_err(dev, "missing gpio-mask property on bank %s\n", np->full_name);
		return ret;
	}
	
	irq = irq_of_parse_and_map(np, 0);
	
	if (!irq) {
		dev_err(dev, "missing irq for bank %s\n", np->full_name);
		return -EINVAL;
	}
	
	strlcpy(bank->label, label, BANK_LABEL_LEN);
	
	bank->pinctrl = pctl;
	bank->addr = bank_n;
	bank->npins = npins;
	bank->mask = mask;
	bank->irq = irq;
	
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
	
	memset(&bank->irq_chip, 0, sizeof(bank->irq_chip));
	
	bank->irq_chip.irq_ack = caninos_gpio_irq_ack,
	bank->irq_chip.irq_enable = caninos_gpio_irq_enable,
	bank->irq_chip.irq_disable = caninos_gpio_irq_disable,
	bank->irq_chip.irq_set_type = caninos_gpio_irq_set_type,
	bank->irq_chip.irq_mask = caninos_gpio_irq_mask,
	bank->irq_chip.irq_unmask = caninos_gpio_irq_unmask,
	bank->irq_chip.name = bank->label;
	
	return 0;
}

static int caninos_pinctrl_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct caninos_pinctrl_hwdiff *pinctrl_hw;
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct caninos_pinctrl *pctl;
	struct device_node *child;
	struct resource res;
	int ret, i = 0;
	
	if (!np) {
		dev_err(dev, "missing device OF node\n");
		return -ENODEV;
	}
	
	match = of_match_device(dev->driver->of_match_table, dev);
	
	if (!match || !match->data) {
		dev_err(dev, "could not get hardware specific data\n");
		return -EINVAL;
	}
	
	pinctrl_hw = (const struct caninos_pinctrl_hwdiff*)(match->data);
	
	pctl = devm_kzalloc(dev, sizeof(*pctl), GFP_KERNEL);
	
	if (!pctl) {
		dev_err(dev, "pinctrl memory allocation failed\n");
		return -ENOMEM;
	}
	
	platform_set_drvdata(pdev, pctl);
	
	pctl->dev = dev;
	pctl->nbanks = 0;
	
	pctl->functions = pinctrl_hw->functions;
	pctl->nfuncs = pinctrl_hw->nfuncs;
	
	pctl->groups = pinctrl_hw->groups;
	pctl->ngroups = pinctrl_hw->ngroups;
	
	pctl->pctl_desc.name = dev_name(dev);
	pctl->pctl_desc.owner = THIS_MODULE;
	pctl->pctl_desc.pins = pinctrl_hw->pins;
	pctl->pctl_desc.npins = pinctrl_hw->npins;
	pctl->pctl_desc.confops = &caninos_pconf_ops;
	pctl->pctl_desc.pctlops = &caninos_pctrl_ops;
	pctl->pctl_desc.pmxops = &caninos_pmxops;
	
	spin_lock_init(&pctl->lock);
	
	if (!of_address_to_resource(np, 0, &res))
	{
		pctl->base = devm_ioremap(dev, res.start, resource_size(&res));
	}
	
	if (!pctl->base) {
		dev_err(dev, "could not map memory\n");
		return -ENOMEM;
	}
	
	for_each_available_child_of_node(np, child)
	{
		if (!of_property_read_bool(child, "gpio-controller")) {
			continue;
		}
		
		ret = caninos_gpiolib_parse_bank(pctl, child, pctl->nbanks);
		
		if (ret < 0) {
			return ret;
		}
		
		pctl->nbanks++;
	}
	
	if (!pctl->nbanks) {
		dev_err(dev, "at least one GPIO bank is required\n");
		return -EINVAL;
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
	
	if (pinctrl_hw->hwinit) {
		if ((ret = pinctrl_hw->hwinit(pctl)) < 0)
			return ret;
	}
	
	pctl->pctl_dev = devm_pinctrl_register(dev, &pctl->pctl_desc, pctl);
	
	if (IS_ERR(pctl->pctl_dev)) {
		dev_err(dev, "could not register pinctrl device\n");
		return PTR_ERR(pctl->pctl_dev);
	}
	
	if ((ret = caninos_gpiolib_register_banks(pctl)) < 0)
		return ret;
	
	dev_info(dev, "probe finished\n");
	return 0;
}

static const struct of_device_id caninos_pinctrl_dt_ids[] = {
	{ .compatible = "caninos,k7-pinctrl", .data = (void*) &k7_pinctrl_hw },
	{ .compatible = "caninos,k5-pinctrl", .data = (void*) &k5_pinctrl_hw },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, caninos_pinctrl_dt_ids);

static struct platform_driver caninos_pinctrl_driver = {
	.driver = {
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

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("Caninos Pinctrl/GPIO Driver");
MODULE_LICENSE("GPL v2");
