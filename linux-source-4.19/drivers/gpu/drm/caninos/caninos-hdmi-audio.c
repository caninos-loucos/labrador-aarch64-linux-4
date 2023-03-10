// SPDX-License-Identifier: GPL-2.0
/*
 * DRM/KMS driver for Caninos Labrador
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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

static int caninos_hdmi_audio_probe(struct platform_device *pdev)
{
	return 0;
}

static int caninos_hdmi_audio_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id caninos_hdmi_audio_match[] = {
	{ .compatible = "caninos,k7-hdmi-audio" },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, caninos_hdmi_audio_match);

static struct platform_driver caninos_hdmi_audio_driver = {
	.probe = caninos_hdmi_audio_probe,
	.remove = caninos_hdmi_audio_remove,
	.driver = {
		.name = "caninos-hdmi-audio",
		.of_match_table = caninos_hdmi_audio_match,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(caninos_hdmi_audio_driver);

MODULE_AUTHOR("Edgar Bernardi Righi <edgar.righi@lsitec.org.br>");
MODULE_DESCRIPTION("Caninos HDMI Audio Driver");
MODULE_LICENSE("GPL v2");
