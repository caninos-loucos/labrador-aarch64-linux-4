/*
 * Pinctrl driver for Actions OWL SoCs
 *
 * Copyright (C) 2014 Actions Semi Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include "../core.h"
#include "pinctrl-owl.h"

struct owl_pinctrl {
	struct device		*dev;
	struct pinctrl_dev	*pctl;
	void __iomem		*membase;
	struct clk* clk;
	const struct owl_pinctrl_soc_info *info;
};

struct cfg_param {
	const char *property;
	enum owl_pinconf_param param;
};

static const struct cfg_param cfg_params[] = {
	{"actions,pull",	OWL_PINCONF_PARAM_PULL},
	{"actions,paddrv",	OWL_PINCONF_PARAM_PADDRV},
};

static unsigned int owl_pinctrl_readl(struct owl_pinctrl *apctl,
		unsigned int reg)
{
	unsigned int val;

	val = readl_relaxed(apctl->membase + reg);
	PINCTRL_DBG("%s: reg 0x%x, val 0x%x\n", __func__, reg, val);

	return val;
}

static void owl_pinctrl_writel(struct owl_pinctrl *apctl,
		unsigned int val, unsigned int reg)
{
	PINCTRL_DBG("%s: reg 0x%x, val 0x%x\n", __func__, reg, val);

	writel_relaxed(val, apctl->membase + reg);
}

/* part 1, pinctrl groups */

static int owl_pctlops_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct owl_pinctrl *apctl = pinctrl_dev_get_drvdata(pctldev);
	const struct owl_pinctrl_soc_info *info = apctl->info;

	PINCTRL_DBG("%s\n", __func__);

	return info->ngroups;
}

static const char *owl_pctlops_get_group_name(struct pinctrl_dev *pctldev,
		unsigned selector)
{
	struct owl_pinctrl *apctl = pinctrl_dev_get_drvdata(pctldev);
	const struct owl_pinctrl_soc_info *info = apctl->info;

	PINCTRL_DBG("%s(selector:%d)\n", __func__, selector);

	return info->groups[selector].name;
}

static int owl_pctlops_get_group_pins(struct pinctrl_dev *pctldev,
		unsigned selector, const unsigned **pins, unsigned *num_pins)
{
	struct owl_pinctrl *apctl = pinctrl_dev_get_drvdata(pctldev);
	const struct owl_pinctrl_soc_info *info = apctl->info;

	PINCTRL_DBG("%s(selector:%d)\n", __func__, selector);

	if (selector >= info->ngroups)
		return -EINVAL;

	*pins = info->groups[selector].pads;
	*num_pins = info->groups[selector].padcnt;

	return 0;
}

static void owl_pctlops_pin_dbg_show(struct pinctrl_dev *pctldev,
		struct seq_file *s, unsigned offset)
{
	struct owl_pinctrl *apctl = pinctrl_dev_get_drvdata(pctldev);

	seq_printf(s, "%s", dev_name(apctl->dev));
}

static int reserve_map(struct device *dev, struct pinctrl_map **map,
		unsigned *reserved_maps, unsigned *num_maps,
		unsigned reserve)
{
	unsigned old_num = *reserved_maps;
	unsigned new_num = *num_maps + reserve;
	struct pinctrl_map *new_map;

	if (old_num >= new_num)
		return 0;

	new_map = krealloc(*map, sizeof(*new_map) * new_num, GFP_KERNEL);
	if (!new_map) {
		dev_err(dev, "krealloc(map) failed\n");
		return -ENOMEM;
	}

	memset(new_map + old_num, 0, (new_num - old_num) * sizeof(*new_map));

	*map = new_map;
	*reserved_maps = new_num;

	return 0;
}

static int add_map_mux(struct pinctrl_map **map, unsigned *reserved_maps,
		unsigned *num_maps, const char *group,
		const char *function)
{
	if (WARN_ON(*num_maps == *reserved_maps))
		return -ENOSPC;

	(*map)[*num_maps].type = PIN_MAP_TYPE_MUX_GROUP;
	(*map)[*num_maps].data.mux.group = group;
	(*map)[*num_maps].data.mux.function = function;
	(*num_maps)++;

	return 0;
}

static int add_map_group_configs(struct device *dev, struct pinctrl_map **map,
		unsigned *reserved_maps, unsigned *num_maps,
		const char *group, unsigned long *configs,
		unsigned num_configs)
{
	unsigned long *dup_configs;

	if (WARN_ON(*num_maps == *reserved_maps))
		return -ENOSPC;

	dup_configs = kmemdup(configs, num_configs * sizeof(*dup_configs),
			      GFP_KERNEL);
	if (!dup_configs) {
		dev_err(dev, "kmemdup(configs) failed\n");
		return -ENOMEM;
	}

	(*map)[*num_maps].type = PIN_MAP_TYPE_CONFIGS_GROUP;
	(*map)[*num_maps].data.configs.group_or_pin = group;
	(*map)[*num_maps].data.configs.configs = dup_configs;
	(*map)[*num_maps].data.configs.num_configs = num_configs;
	(*num_maps)++;

	return 0;
}

static int add_map_pin_configs(struct device *dev, struct pinctrl_map **map,
		unsigned *reserved_maps, unsigned *num_maps,
		const char *pin, unsigned long *configs,
		unsigned num_configs)
{
	unsigned long *dup_configs;

	if (WARN_ON(*num_maps == *reserved_maps))
		return -ENOSPC;

	dup_configs = kmemdup(configs, num_configs * sizeof(*dup_configs),
			      GFP_KERNEL);
	if (!dup_configs) {
		dev_err(dev, "kmemdup(configs) failed\n");
		return -ENOMEM;
	}

	(*map)[*num_maps].type = PIN_MAP_TYPE_CONFIGS_PIN;
	(*map)[*num_maps].data.configs.group_or_pin = pin;
	(*map)[*num_maps].data.configs.configs = dup_configs;
	(*map)[*num_maps].data.configs.num_configs = num_configs;
	(*num_maps)++;

	return 0;
}

static int add_config(struct device *dev, unsigned long **configs,
		unsigned *num_configs, unsigned long config)
{
	unsigned old_num = *num_configs;
	unsigned new_num = old_num + 1;
	unsigned long *new_configs;

	new_configs = krealloc(*configs, sizeof(*new_configs) * new_num,
		GFP_KERNEL);
	if (!new_configs) {
		dev_err(dev, "krealloc(configs) failed\n");
		return -ENOMEM;
	}

	new_configs[old_num] = config;

	*configs = new_configs;
	*num_configs = new_num;

	return 0;
}

static void owl_pinctrl_dt_free_map(struct pinctrl_dev *pctldev,
		struct pinctrl_map *map,
		unsigned num_maps)
{
	int i;

	for (i = 0; i < num_maps; i++)
		if (map[i].type == PIN_MAP_TYPE_CONFIGS_GROUP ||
				map[i].type == PIN_MAP_TYPE_CONFIGS_PIN)
			kfree(map[i].data.configs.configs);

	kfree(map);
}

static int owl_pinctrl_dt_subnode_to_map(struct device *dev,
		struct device_node *np,
		struct pinctrl_map **map,
		unsigned *reserved_maps,
		unsigned *num_maps)
{
	int ret, i;
	const char *function;
	u32 val;
	unsigned long config;
	unsigned long *configs = NULL;
	unsigned num_configs = 0;
	unsigned reserve;
	struct property *prop;
	int groups_prop_num;
	int pins_prop_num;
	int groups_or_pins_prop_num;

	ret = of_property_read_string(np, "actions,function", &function);
	if (ret < 0) {
		/* EINVAL=missing, which is fine since it's optional */
		if (ret != -EINVAL)
			dev_err(dev,
				"could not parse property actions,function\n");
		function = NULL;
	}

	for (i = 0; i < ARRAY_SIZE(cfg_params); i++) {
		ret = of_property_read_u32(np, cfg_params[i].property, &val);
		if (!ret) {
			config = OWL_PINCONF_PACK(cfg_params[i].param, val);
			ret = add_config(dev, &configs, &num_configs, config);
			if (ret < 0)
				goto exit;
		/* EINVAL=missing, which is fine since it's optional */
		} else if (ret != -EINVAL) {
			dev_err(dev, "could not parse property %s\n",
				cfg_params[i].property);
		}
	}

	reserve = 0;
	if (function != NULL)
		reserve++;
	if (num_configs)
		reserve++;

	ret = of_property_count_strings(np, "actions,pins");
	if (ret < 0) {
		if (ret != -EINVAL)
			dev_err(dev, "could not parse property actions,pins\n");

		pins_prop_num = 0;
	} else {
		pins_prop_num = ret;
	}

	if (pins_prop_num > 0 && function != NULL) {
		dev_err(dev, "could not assign actions,pins to function\n");
		goto exit;
	}

	ret = of_property_count_strings(np, "actions,groups");
	if (ret < 0) {
		if (ret != -EINVAL)
			dev_err(dev, "could not parse property actions,groups\n");

		groups_prop_num = 0;
	} else {
		groups_prop_num = ret;
	}

	groups_or_pins_prop_num = groups_prop_num + pins_prop_num;
	if (groups_or_pins_prop_num == 0) {
		dev_err(dev, "no property asoc,pins or asoc,groups\n");
		goto exit;
	}

	reserve *= groups_or_pins_prop_num;

	ret = reserve_map(dev, map, reserved_maps, num_maps, reserve);
	if (ret < 0)
		goto exit;

	if (groups_prop_num > 0) {
		const char *group;
		of_property_for_each_string(np, "actions,groups", prop, group) {
			if (function) {
				ret = add_map_mux(map, reserved_maps, num_maps,
						group, function);
				if (ret < 0)
					goto exit;
			}

			if (num_configs) {
				ret = add_map_group_configs(dev, map,
					reserved_maps, num_maps, group,
					configs, num_configs);
				if (ret < 0)
					goto exit;
			}
		}
	}

	if (pins_prop_num > 0) {
		const char *pin;
		of_property_for_each_string(np, "actions,pins", prop, pin) {
			if (num_configs) {
				ret = add_map_pin_configs(dev, map,
					reserved_maps, num_maps, pin,
					configs, num_configs);
				if (ret < 0)
					goto exit;
			}
		}
	}

	ret = 0;

exit:
	kfree(configs);
	return ret;
}

static int owl_pinctrl_dt_node_to_map(struct pinctrl_dev *pctldev,
		struct device_node *np_config,
		struct pinctrl_map **map,
		unsigned *num_maps)
{
	unsigned reserved_maps;
	struct device_node *np;
	int ret;

	reserved_maps = 0;
	*map = NULL;
	*num_maps = 0;

	for_each_child_of_node(np_config, np) {
		ret = owl_pinctrl_dt_subnode_to_map(pctldev->dev, np, map,
				&reserved_maps, num_maps);
		if (ret < 0) {
			owl_pinctrl_dt_free_map(pctldev, *map, *num_maps);
			return ret;
		}
	}

	return 0;
}

static struct pinctrl_ops owl_pctlops_ops = {
	.get_groups_count = owl_pctlops_get_groups_count,
	.get_group_name = owl_pctlops_get_group_name,
	.get_group_pins = owl_pctlops_get_group_pins,
	.pin_dbg_show = owl_pctlops_pin_dbg_show,
	.dt_node_to_map = owl_pinctrl_dt_node_to_map,
	.dt_free_map = owl_pinctrl_dt_free_map,

};

/* part 2, pinctrl pinmux */
static inline int get_group_mfp_mask_val(const struct owl_group *g,
		int function, u32 *mask, u32 *val)
{
	int i;
	u32 option_num;
	u32 option_mask;

	for (i = 0; i < g->nfuncs; i++) {
		if (g->funcs[i] == function)
			break;
	}
	if (WARN_ON(i == g->nfuncs))
		return -EINVAL;

	option_num = (1 << g->mfpctl_width);
	if (i > option_num)
		i -= option_num;

	option_mask = option_num - 1;
	*mask = (option_mask  << g->mfpctl_shift);
	*val = (i << g->mfpctl_shift);

	return 0;
}

/*gpio/owl_gpio.c*/
extern int owl_gpio_check_dir_by_pinctrl(unsigned int gpio);
static int pinmux_request_check_gpio(struct pinctrl_dev *pctldev, u32 pin)
{
	struct pin_desc *desc;
	struct owl_pinctrl *apctl = pinctrl_dev_get_drvdata(pctldev);
	struct pinctrl_gpio_range *range = apctl->info->gpio_ranges;

	desc = pin_desc_get(pctldev, pin);
	pr_info("%s,pin=%d\n", __func__, pin);
	if (desc->gpio_owner) {
		PINCTRL_ERR("%s\n", __func__);
		PINCTRL_ERR("error:%s has already been requested by %s",
				desc->name, desc->gpio_owner);
	} else if ( pin < range->npins)  { // check gpio reg
		/* Convert to the gpio controllers number space */
		u32 gpio   =  pin + range->base - range->pin_base;
		if (owl_gpio_check_dir_by_pinctrl(gpio)) {
			PINCTRL_ERR("%s\n", __func__);
			PINCTRL_ERR("error:%s has already used by %d\n",
				desc->name,  gpio);
		}
	}

	return 0;
}

static int gpio_request_check_pinmux(struct pinctrl_dev *pctldev, u32 pin)
{
	struct pin_desc *desc;
	desc = pin_desc_get(pctldev, pin);

	if (desc->mux_owner) {
		PINCTRL_ERR("%s\n", __func__);
		PINCTRL_ERR("error:%s has already been requested by %s\n",
				desc->name, desc->mux_owner);
	}

	return 0;
}

static int owl_pmxops_request(struct pinctrl_dev *pctldev, unsigned pin)
{
	return pinmux_request_check_gpio(pctldev, pin);
}

static int owl_pmxops_set_mux(struct pinctrl_dev *pctldev,
	unsigned function, unsigned group)
{
	struct owl_pinctrl *apctl = pinctrl_dev_get_drvdata(pctldev);
	const struct owl_pinctrl_soc_info *info = apctl->info;
	const struct owl_group *g;
	u32 g_val, g_mask;

	PINCTRL_DBG("%s function:%d '%s', group:%d '%s'\n", __func__,
		function, info->functions[function].name,
		group, info->groups[group].name);

	g = &info->groups[group];

	/*
	 * TODO: disable GPIOs function
	 */

	if (g->mfpctl_reg >= 0) {
		u32 mfpval;

		if (get_group_mfp_mask_val(g, function, &g_mask, &g_val))
			return -EINVAL;

		/*
		 * we've done all the checkings. From now on ,we will set
		 * hardware. No more errors should happen, otherwise it will
		 * be hard to roll back
		 */
		mfpval = owl_pinctrl_readl(apctl, g->mfpctl_reg);
		PINCTRL_DBG("read mfpval = 0x%x\n", mfpval);
		mfpval &= (~g_mask);
		mfpval |= g_val;
		PINCTRL_DBG("write mfpval = 0x%x\n", mfpval);
		owl_pinctrl_writel(apctl, mfpval, g->mfpctl_reg);
		PINCTRL_DBG("read mfpval again = 0x%x\n",
				owl_pinctrl_readl(apctl, g->mfpctl_reg));

	}

	return 0;
}



static int owl_pmxops_get_funcs_count(struct pinctrl_dev *pctldev)
{
	struct owl_pinctrl *apctl = pinctrl_dev_get_drvdata(pctldev);
	const struct owl_pinctrl_soc_info *info = apctl->info;

	PINCTRL_DBG("%s\n", __func__);

	return info->nfunctions;
}

static const char *owl_pmxops_get_func_name(struct pinctrl_dev *pctldev,
		unsigned selector)
{
	struct owl_pinctrl *apctl = pinctrl_dev_get_drvdata(pctldev);
	const struct owl_pinctrl_soc_info *info = apctl->info;

	PINCTRL_DBG("%s(selector:%d) name %s\n", __func__, selector,
		info->functions[selector].name);

	return info->functions[selector].name;
}

static int owl_pmxops_get_groups(struct pinctrl_dev *pctldev,
		unsigned selector, const char * const **groups,
		unsigned * const num_groups)
{
	struct owl_pinctrl *apctl = pinctrl_dev_get_drvdata(pctldev);
	const struct owl_pinctrl_soc_info *info = apctl->info;

	PINCTRL_DBG("%s(selector:%d)\n", __func__, selector);

	*groups = info->functions[selector].groups;
	*num_groups = info->functions[selector].ngroups;

	return 0;
}

static int owl_pmxops_gpio_request_enable(struct pinctrl_dev *pctldev,
		struct pinctrl_gpio_range *range, unsigned offset)
{
	return gpio_request_check_pinmux(pctldev, offset);
}

static void owl_pmxops_gpio_disable_free(struct pinctrl_dev *pctldev,
	struct pinctrl_gpio_range *range, unsigned offset)
{
	PINCTRL_DBG("%s() disable pin %u as GPIO\n", __func__, offset);
	/* Set the pin to some default state, GPIO is usually default */
}

static struct pinmux_ops owl_pmxops_ops = {
	.get_functions_count = owl_pmxops_get_funcs_count,
	.get_function_name = owl_pmxops_get_func_name,
	.get_function_groups = owl_pmxops_get_groups,
	
	.set_mux = owl_pmxops_set_mux,
	
	.request = owl_pmxops_request,
	
	.gpio_request_enable = owl_pmxops_gpio_request_enable,
	.gpio_disable_free = owl_pmxops_gpio_disable_free,

};

/* part 3, pinctrl pinconfs */
static int owl_group_pinconf_reg(const struct owl_group *g,
		enum owl_pinconf_param param,
		u32 *reg, u32 *bit, u32 *width)
{
	switch (param) {
	case OWL_PINCONF_PARAM_PADDRV:
		if (g->paddrv_reg < 0)
			return -EINVAL;

		*reg = g->paddrv_reg;
		*bit = g->paddrv_shift;
		*width = g->paddrv_width;
		break;
	default:
		return -EINVAL;
	}

	return 0;

}

static int owl_group_pinconf_arg2val(const struct owl_group *g,
		enum owl_pinconf_param param,
		u32 arg, u32 *val)
{
	switch (param) {
	case OWL_PINCONF_PARAM_PADDRV:
		*val = arg;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int owl_group_pinconf_val2arg(const struct owl_group *g,
		enum owl_pinconf_param param,
		u32 val, u32 *arg)
{
	switch (param) {
	case OWL_PINCONF_PARAM_PADDRV:
		*arg = val;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int owl_pad_pinconf_reg(const struct owl_pinconf_pad_info *pad,
		enum owl_pinconf_param param,
		u32 *reg, u32 *bit, u32 *width)
{
	switch (param) {
	case OWL_PINCONF_PARAM_PULL:
		if ((!pad->pull) || (pad->pull->reg < 0))
			return -EINVAL;

		*reg = pad->pull->reg;
		*bit = pad->pull->shift;
		*width = pad->pull->width;
		break;
	case OWL_PINCONF_PARAM_SCHMITT:
		PINCTRL_ERR("Cannot configure pad schmitt yet!\n");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int owl_pad_pinconf_arg2val(const struct owl_pinconf_pad_info *pad,
		enum owl_pinconf_param param,
		u32 arg, u32 *val)
{
	switch (param) {
	case OWL_PINCONF_PARAM_PULL:
		switch (arg) {
		case OWL_PINCONF_PULL_NONE:
			*val = 0;
			break;
		case OWL_PINCONF_PULL_DOWN:
			if (pad->pull->pulldown)
				*val = pad->pull->pulldown;
			else
				return -EINVAL;
			break;
		case OWL_PINCONF_PULL_UP:
			if (pad->pull->pullup)
				*val = pad->pull->pullup;
			else
				return -EINVAL;
			break;
		default:
			return -EINVAL;
		}

		break;

	case OWL_PINCONF_PARAM_SCHMITT:
		PINCTRL_ERR("Cannot configure pad schmitt yet!\n");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int owl_pad_pinconf_val2arg(const struct owl_pinconf_pad_info *pad,
		enum owl_pinconf_param param,
		u32 val, u32 *arg)
{
	switch (param) {
	case OWL_PINCONF_PARAM_PULL:
		if (pad->pull->pulldown && (val == pad->pull->pulldown))
			*arg = OWL_PINCONF_PULL_DOWN;
		else if (pad->pull->pullup && (val == pad->pull->pullup))
			*arg = OWL_PINCONF_PULL_UP;
		else if (val == 0)
			*arg = OWL_PINCONF_PULL_NONE;
		else
			return -EINVAL;

		break;

	case OWL_PINCONF_PARAM_SCHMITT:
		PINCTRL_ERR("Cannot configure pad schmitt yet!\n");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int owl_confops_pin_config_get(struct pinctrl_dev *pctldev,
		unsigned pin, unsigned long *config)
{
	int ret = 0;
	struct owl_pinctrl *apctl;
	const struct owl_pinctrl_soc_info *info;
	const struct owl_pinconf_pad_info *pad_tab;
	u32 reg, bit, width;
	u32 val, mask;
	u32 tmp;
	u32 arg = 0;
	enum owl_pinconf_param param = OWL_PINCONF_UNPACK_PARAM(*config);

	PINCTRL_DBG("%s(pin:%d)\n", __func__, pin);

	apctl = pinctrl_dev_get_drvdata(pctldev);
	info = apctl->info;
	pad_tab = &info->padinfo[pin];
	PINCTRL_DBG("%s(pin:%d)\n", __func__, pin);
	/* We get config for those pins we CAN get it for and that's it */

	ret = owl_pad_pinconf_reg(pad_tab, param, &reg, &bit, &width);
	if (ret)
		return ret;

	tmp = owl_pinctrl_readl(apctl, reg);
	mask = (1 << width) - 1;
	val = (tmp >> bit) & mask;

	ret = owl_pad_pinconf_val2arg(pad_tab, param, val, &arg);
	if (ret)
		return ret;

	*config = OWL_PINCONF_PACK(param, arg);

	return ret;
}

static int owl_confops_pin_config_set(struct pinctrl_dev *pctldev,
		unsigned pin, unsigned long *configs,  unsigned num_configs)
{
	int ret = 0;
	struct owl_pinctrl *apctl;
	const struct owl_pinctrl_soc_info *info;
	const struct owl_pinconf_pad_info *pad_tab;
	u32 reg, bit, width;
	u32 val = 0, mask = 0;
	u32 tmp;
	
	enum owl_pinconf_param param = OWL_PINCONF_UNPACK_PARAM(*configs);
	
	u32 arg = OWL_PINCONF_UNPACK_ARG(*configs);
	
	PINCTRL_DBG("%s(pin:%d, config:%ld)\n", __func__, pin, *configs);

	apctl = pinctrl_dev_get_drvdata(pctldev);
	info = apctl->info;
	pad_tab = &info->padinfo[pin];
	PINCTRL_DBG("%s(pin:%d)\n", __func__, pin);


	ret = owl_pad_pinconf_reg(pad_tab, param, &reg, &bit, &width);
	if (ret)
		return ret;

	ret = owl_pad_pinconf_arg2val(pad_tab, param, arg, &val);
	if (ret)
		return ret;

	/* Update register */
	mask = (1 << width) - 1;
	mask = mask << bit;
	tmp = owl_pinctrl_readl(apctl, reg);
	tmp &= ~mask;
	tmp |= val << bit;
	owl_pinctrl_writel(apctl, tmp, reg);

	return ret;
}

static int owl_confops_group_config_get(struct pinctrl_dev *pctldev,
		unsigned group, unsigned long *config)
{
	int ret = 0;
	const struct owl_group *g;
	struct owl_pinctrl *apctl = pinctrl_dev_get_drvdata(pctldev);
	const struct owl_pinctrl_soc_info *info = apctl->info;
	u32 reg, bit, width;
	u32 val, mask;
	u32 tmp;
	u32 arg = 0;
	enum owl_pinconf_param param = OWL_PINCONF_UNPACK_PARAM(*config);

	g = &info->groups[group];

	ret = owl_group_pinconf_reg(g, param, &reg, &bit, &width);
	if (ret)
		return ret;

	tmp = owl_pinctrl_readl(apctl, reg);
	mask = (1 << width) - 1;
	val = (tmp >> bit) & mask;

	ret = owl_group_pinconf_val2arg(g, param, val, &arg);
	if (ret)
		return ret;

	*config = OWL_PINCONF_PACK(param, arg);

	return ret;

}

static int owl_confops_group_config_set(struct pinctrl_dev *pctldev,
		unsigned group, unsigned long *configs, unsigned num_configs)
{
	int ret = 0;
	const struct owl_group *g;
	struct owl_pinctrl *apctl = pinctrl_dev_get_drvdata(pctldev);
	const struct owl_pinctrl_soc_info *info = apctl->info;
	u32 reg, bit, width;
	u32 val, mask;
	u32 tmp;
	enum owl_pinconf_param param = OWL_PINCONF_UNPACK_PARAM(*configs);
	u32 arg = OWL_PINCONF_UNPACK_ARG(*configs);

	g = &info->groups[group];
	ret = owl_group_pinconf_reg(g, param, &reg, &bit, &width);
	if (ret)
		return ret;

	ret = owl_group_pinconf_arg2val(g, param, arg, &val);
	if (ret)
		return ret;

	/* Update register */
	mask = (1 << width) - 1;
	mask = mask << bit;
	tmp = owl_pinctrl_readl(apctl, reg);
	tmp &= ~mask;
	tmp |= val << bit;
	owl_pinctrl_writel(apctl, tmp, reg);

	return ret;
}

static struct pinconf_ops owl_confops_ops = {
	.pin_config_get = owl_confops_pin_config_get,
	.pin_config_set = owl_confops_pin_config_set,
	.pin_config_group_get = owl_confops_group_config_get,
	.pin_config_group_set = owl_confops_group_config_set,
};

/* platform device */
static struct pinctrl_desc owl_pinctrl_desc = {
	.name = NULL,
	.pins = NULL,
	.npins = 0,
	.pctlops = &owl_pctlops_ops,
	.pmxops = &owl_pmxops_ops,
	.confops = &owl_confops_ops,
	.owner = THIS_MODULE,
};

int owl_pinctrl_probe(struct platform_device *pdev,
		struct owl_pinctrl_soc_info *info)
{
	struct resource *res;
	struct owl_pinctrl *apctl;
	int i;

	pr_info("[OWL] pinctrl initialization\n");

	if (!info || !info->pins || !info->npins) {
		dev_err(&pdev->dev, "wrong pinctrl info\n");
		return -EINVAL;
	}

	owl_pinctrl_desc.name = dev_name(&pdev->dev);
	owl_pinctrl_desc.pins = info->pins;
	owl_pinctrl_desc.npins = info->npins;

	/* Create state holders etc for this driver */
	apctl = devm_kzalloc(&pdev->dev, sizeof(*apctl), GFP_KERNEL);
	if (!apctl)
		return -ENOMEM;

	apctl->info = info;
	apctl->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	apctl->membase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(apctl->membase))
		return PTR_ERR(apctl->membase);

	/* enable GPIO/MFP clock */
	apctl->clk = devm_clk_get(&pdev->dev, NULL);
	
	if (IS_ERR(apctl->clk))
	{
		dev_err(&pdev->dev, "no clock defined\n");
		return -ENODEV;
	}
	
	clk_prepare_enable(apctl->clk);

	dev_info(&pdev->dev, "nfunctions %d, ngroups %d\n",
		info->nfunctions, info->ngroups);

	apctl->pctl = pinctrl_register(&owl_pinctrl_desc, &pdev->dev, apctl);
	if (!apctl->pctl) {
		dev_err(&pdev->dev, "could not register Actions SOC pinmux driver\n");
		return -EINVAL;
	}

	/* We will handle a range of GPIO pins */
	for (i = 0; i < info->gpio_num_ranges; i++)
		pinctrl_add_gpio_range(apctl->pctl, &info->gpio_ranges[i]);

	platform_set_drvdata(pdev, apctl);

	dev_dbg(&pdev->dev, "initialized Actions SOC pin control driver\n");

	return 0;
}

int owl_pinctrl_remove(struct platform_device *pdev)
{
	struct owl_pinctrl *apctl = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < apctl->info->gpio_num_ranges; i++) {
		pinctrl_remove_gpio_range(apctl->pctl,
			&apctl->info->gpio_ranges[i]);
	}

	pinctrl_unregister(apctl->pctl);
	clk_disable_unprepare(apctl->clk);

	platform_set_drvdata(pdev, NULL);

	return 0;
}
