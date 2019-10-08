/*
 * Pinctrl definitions for Actions SOC
 *
 * Copyright (C) 2012 Actions Semi Inc.
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

#ifndef __PINCTRL_OWL_H__
#define __PINCTRL_OWL_H__

#undef DEBUG_PINCTRL

#ifdef DEBUG_PINCTRL
#define PINCTRL_DBG(format, ...) \
	pr_info("[OWL] pinctrl: " format, ## __VA_ARGS__)
#else
#define PINCTRL_DBG(format, ...)
#endif

#define PINCTRL_ERR(format, ...) \
	pr_err("[OWL] pinctrl: " format, ## __VA_ARGS__)

enum owl_pinconf_param {
	OWL_PINCONF_PARAM_PULL,
	OWL_PINCONF_PARAM_PADDRV,
	OWL_PINCONF_PARAM_SCHMITT,
};

enum owl_pinconf_pull {
	OWL_PINCONF_PULL_NONE,
	OWL_PINCONF_PULL_DOWN,
	OWL_PINCONF_PULL_UP,
	OWL_PINCONF_PULL_HOLD,
};

#define OWL_PINCONF_PACK(_param_, _arg_)				\
		((_param_) << 16 | ((_arg_) & 0xffff))
#define OWL_PINCONF_UNPACK_PARAM(_conf_)	((_conf_) >> 16)
#define OWL_PINCONF_UNPACK_ARG(_conf_)		((_conf_) & 0xffff)

/**
 * struct owl_pinmux_group - describes a Actions SOC pin group
 * @name: the name of this specific pin group
 * @pads: an array of discrete physical pins, ie, named pads in ic spec,
 *      used in this group, defined in driver-local pin enumeration space
 * @padcnt: the number of pins in this group array, i.e. the number of
 *	elements in .pads so we can iterate over that array
 * @mfpctl: fragment of mfp code
 */
struct owl_group {
	const char *name;
	unsigned int *pads;
	unsigned int padcnt;
	unsigned int *funcs;
	unsigned int nfuncs;

	int mfpctl_reg;
	unsigned int mfpctl_shift;
	unsigned int mfpctl_width;

	int paddrv_reg;
	unsigned int paddrv_shift;
	unsigned int paddrv_width;
};

/**
 * struct owl_pinmux_func - Actions SOC pinctrl mux functions
 * @name: The name of the function, exported to pinctrl core.
 * @groups: An array of pin groups that may select this function.
 * @ngroups: The number of entries in @groups.
 */
struct owl_pinmux_func {
	const char *name;
	const char * const *groups;
	unsigned ngroups;
};

/**
 * struct owl_pinconf_reg_pull - Actions SOC pinctrl pull up/down regs
 * @reg: The index of PAD_PULLCTL regs.
 * @mask: The bit mask of PAD_PULLCTL fragment.
 * @pullup: The pullup value of PAD_PULLCTL fragment.
 * @pulldown: The pulldown value of PAD_PULLCTL fragment.
 */
struct owl_pinconf_reg_pull {
	int reg;
	unsigned int shift;
	unsigned int width;
	unsigned int pullup;
	unsigned int pulldown;
};

/**
 * struct owl_pinconf_schimtt - Actions SOC pinctrl PAD_ST regs
 * @reg: The index of PAD_ST regs.
 * @mask: The bit mask of PAD_ST fragment.
 */
struct owl_pinconf_schimtt {
	unsigned int *schimtt_funcs;
	unsigned int num_schimtt_funcs;
	int reg;
	unsigned int shift;
};

/**
 * struct owl_pinconf_pad_info - Actions SOC pinctrl pad info
 * @pad: The pin, in soc, the pad code of the silicon.
 * @gpio: The gpio number of the pad.
 * @pull: pull up/down reg, mask, and value.
 * @paddrv: pad drive strength info.
 * @schimtt: schimtt triger info.
 */
struct owl_pinconf_pad_info {
	int  pad;
	int  gpio;
	struct owl_pinconf_reg_pull *pull;
	struct owl_pinconf_reg_paddrv *paddrv;
	struct owl_pinconf_schimtt *schimtt;
};

/**
 * this struct is identical to pinctrl_pin_desc.
 * struct pinctrl_pin_desc - boards/machines provide information on their
 * pins, pads or other muxable units in this struct
 * @number: unique pin number from the global pin number space
 * @name: a name for this pin
 */
struct owl_pinctrl_pin_desc {
	unsigned number;
	const char *name;
};

/**
 * struct owl_pinctrl_soc_data - Actions SOC pin controller per-SoC configuration
 * @gpio_ranges: An array of GPIO ranges for this SoC
 * @gpio_num_ranges: The number of GPIO ranges for this SoC
 * @pins:	An array describing all pins the pin controller affects.
 *		All pins which are also GPIOs must be listed first within the
 *		array, and be numbered identically to the GPIO controller's
 *		numbering.
 * @npins:	The number of entries in @pins.
 * @functions:	The functions supported on this SoC.
 * @nfunction:	The number of entries in @functions.
 * @groups:	An array describing all pin groups the pin SoC supports.
 * @ngroups:	The number of entries in @groups.
 */
struct owl_pinctrl_soc_info {
	struct device *dev;
	struct pinctrl_gpio_range *gpio_ranges;
	unsigned gpio_num_ranges;
	const struct owl_pinconf_pad_info *padinfo;
	const struct pinctrl_pin_desc *pins;
	unsigned npins;
	const struct owl_pinmux_func *functions;
	unsigned nfunctions;
	const struct owl_group *groups;
	unsigned ngroups;
};

int owl_pinctrl_probe(struct platform_device *pdev,
		struct owl_pinctrl_soc_info *info);
int owl_pinctrl_remove(struct platform_device *pdev);

#endif /* __PINCTRL_OWL_H__ */
