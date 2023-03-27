// SPDX-License-Identifier: GPL-2.0
/*
 * Caninos SPS driver
 *
 * Copyright (c) 2023 ITEX - LSITEC - Caninos Loucos
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

#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <dt-bindings/power/caninos-power.h>

#define SPS_PG_CTL 0x0

static int caninos_sps_set_pg(void __iomem *base, int pwr_bit,
                              int ack_bit, bool enable)
{
	u32 pwr_mask, ack_mask, val;
	int timeout = 5000;
	bool ack;
	
	if (ack_bit < 0) {
		ack_bit = pwr_bit;
	}
	
	pwr_mask = BIT(pwr_bit);
	ack_mask = BIT(ack_bit);
	
	val = readl(base + SPS_PG_CTL);
	ack = (val & ack_mask) == ack_mask;
	
	if (ack == enable) {
		return 0;
	}
	if (enable) {
		val |= pwr_mask;
	}
	else {
		val &= ~pwr_mask;
	}
	
	writel(val, base + SPS_PG_CTL);
	
	while (timeout > 0)
	{
		val = readl(base + SPS_PG_CTL);
		ack = (val & ack_mask) == ack_mask;
		
		if (ack == enable) {
			break;
		}
		
		udelay(50);
		timeout -= 50;
	}
	
	if (timeout <= 0) {
		return -ETIMEDOUT;
	}
	
	udelay(10);
	return 0;
}

struct caninos_sps_domain_info {
	const char *name;
	int pwr_bit;
	int ack_bit;
	unsigned int genpd_flags;
	bool is_off;
};

struct caninos_sps_info {
	unsigned num_domains;
	const struct caninos_sps_domain_info *domains;
};

struct caninos_sps {
	struct device *dev;
	const struct caninos_sps_info *info;
	void __iomem *base;
	struct genpd_onecell_data genpd_data;
	struct generic_pm_domain *domains[];
};

#define to_caninos_pd(gpd) container_of(gpd, struct caninos_sps_domain, genpd)

struct caninos_sps_domain {
	struct generic_pm_domain genpd;
	const struct caninos_sps_domain_info *info;
	struct caninos_sps *sps;
};

static int caninos_sps_set_power(struct caninos_sps_domain *pd, bool enable)
{
	int pwr_bit, ack_bit;
	
	pwr_bit = pd->info->pwr_bit;
	ack_bit = pd->info->ack_bit;
	
	return caninos_sps_set_pg(pd->sps->base, pwr_bit, ack_bit, enable);
}

static int caninos_sps_power_on(struct generic_pm_domain *domain)
{
	struct caninos_sps_domain *pd = to_caninos_pd(domain);
	dev_info(pd->sps->dev, "%s power on.\n", pd->info->name);
	return caninos_sps_set_power(pd, true);
}

static int caninos_sps_power_off(struct generic_pm_domain *domain)
{
	struct caninos_sps_domain *pd = to_caninos_pd(domain);
	dev_info(pd->sps->dev, "%s power off.\n", pd->info->name);
	return caninos_sps_set_power(pd, false);
}

static int caninos_sps_init_domain(struct caninos_sps *sps, int index)
{
	struct caninos_sps_domain *pd;
	int ret;
	
	pd = devm_kzalloc(sps->dev, sizeof(*pd), GFP_KERNEL);
	
	if (!pd) {
		return -ENOMEM;
	}
	
	pd->info = &sps->info->domains[index];
	pd->sps = sps;
	
	pd->genpd.name = pd->info->name;
	pd->genpd.power_on = caninos_sps_power_on;
	pd->genpd.power_off = caninos_sps_power_off;
	pd->genpd.flags = pd->info->genpd_flags;
	
	if (pd->info->is_off) {
		ret = caninos_sps_power_off(&pd->genpd);
	}
	else {
		ret = caninos_sps_power_on(&pd->genpd);
	}
	if (ret) {
		return ret;
	}
	
	pm_genpd_init(&pd->genpd, NULL, pd->info->is_off);
	sps->genpd_data.domains[index] = &pd->genpd;
	return 0;
}

static int caninos_sps_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct caninos_sps_info *sps_info;
	struct caninos_sps *sps;
	int i, ret;
	
	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "no device node\n");
		return -ENODEV;
	}
	
	match = of_match_device(pdev->dev.driver->of_match_table, &pdev->dev);
	
	if (!match || !match->data) {
		dev_err(&pdev->dev, "unknown compatible or missing data\n");
		return -EINVAL;
	}
	
	sps_info = match->data;
	
	sps = devm_kzalloc(&pdev->dev,
	                   struct_size(sps, domains, sps_info->num_domains),
	                   GFP_KERNEL);
	if (!sps) {
		dev_err(&pdev->dev, "memory allocation failed\n");
		return -ENOMEM;
	}
	
	sps->base = of_iomap(pdev->dev.of_node, 0);
	
	if (IS_ERR(sps->base)) {
		dev_err(&pdev->dev, "failed to map sps registers\n");
		return PTR_ERR(sps->base);
	}
	
	sps->dev = &pdev->dev;
	sps->info = sps_info;
	sps->genpd_data.domains = sps->domains;
	sps->genpd_data.num_domains = sps_info->num_domains;
	
	for (i = 0; i < sps_info->num_domains; i++) {
		ret = caninos_sps_init_domain(sps, i);
		if (ret)
			return ret;
	}
	
	ret = of_genpd_add_provider_onecell(pdev->dev.of_node, &sps->genpd_data);
	if (ret) {
		dev_err(&pdev->dev, "failed to add provider (%d)\n", ret);
		return ret;
	}
	
	dev_info(&pdev->dev, "probe finished\n");
	return 0;
}

static const struct caninos_sps_domain_info k7_sps_domains[] = {
	[PD_USB3] = {
		.name = "USB3",
		.pwr_bit = 10,
		.ack_bit = -1,
		.genpd_flags = 0,
		.is_off = false,
	},
	[PD_DMAC] = {
		.name = "DMAC",
		.pwr_bit = 8,
		.ack_bit = -1,
		.genpd_flags = GENPD_FLAG_ALWAYS_ON,
		.is_off = false,
	},
	[PD_USB2H0] = {
		.name = "USB2H0",
		.pwr_bit = 11,
		.ack_bit = -1,
		.genpd_flags = GENPD_FLAG_ALWAYS_ON,
		.is_off = false,
	},
	[PD_USB2H1] = {
		.name = "USB2H1",
		.pwr_bit = 2,
		.ack_bit = -1,
		.genpd_flags = GENPD_FLAG_ALWAYS_ON,
		.is_off = false,
	},
};

static const struct caninos_sps_domain_info k5_sps_domains[] = {
	[PD_USB3] = {
		.name = "USB3",
		.pwr_bit = 10,
		.ack_bit = 14,
		.genpd_flags = 0,
		.is_off = false,
	},
	[PD_DMAC] = {
		.name = "DMAC",
		.pwr_bit = 8,
		.ack_bit = 12,
		.genpd_flags = GENPD_FLAG_ALWAYS_ON,
		.is_off = false,
	},
	[PD_USB2H0] = {
		.name = "USB2H0",
		.pwr_bit = 11,
		.ack_bit = 15,
		.genpd_flags = GENPD_FLAG_ALWAYS_ON,
		.is_off = false,
	},
	[PD_USB2H1] = {
		.name = "USB2H1",
		.pwr_bit = 2,
		.ack_bit = 18,
		.genpd_flags = GENPD_FLAG_ALWAYS_ON,
		.is_off = false,
	},
};

static const struct caninos_sps_info k7_sps_info = {
	.num_domains = PD_NUM,
	.domains = k7_sps_domains,
};

static const struct caninos_sps_info k5_sps_info = {
	.num_domains = PD_NUM,
	.domains = k5_sps_domains,
};

static const struct of_device_id caninos_sps_of_matches[] = {
	{ .compatible = "caninos,k7-sps", .data = &k7_sps_info },
	{ .compatible = "caninos,k5-sps", .data = &k5_sps_info },
	{ /* sentinel */ }
};

static struct platform_driver caninos_sps_platform_driver = {
	.probe = caninos_sps_probe,
	.driver = {
		.name = "caninos-sps",
		.of_match_table = caninos_sps_of_matches,
		.suppress_bind_attrs = true,
	},
};

static int __init caninos_sps_init(void)
{
	return platform_driver_register(&caninos_sps_platform_driver);
}

postcore_initcall(caninos_sps_init);

