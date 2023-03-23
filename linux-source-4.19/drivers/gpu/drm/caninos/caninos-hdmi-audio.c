// SPDX-License-Identifier: GPL-2.0
/*
 * DRM/KMS driver for Caninos Labrador
 *
 * Copyright (c) 2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 * 
 * Copyright (c) 2023 ITEX - LSITEC - Caninos Loucos
 * Author: Ana Clara Forcelli <ana.forcelli@lsitec.org.br>
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
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "hdmi.h"
#include "hdmi-regs.h"

#define SPDIF_HDMI_CTL 0x10
#define HDMFSS 14 // hdmi fifo source select
#define	HDMFR  1  // hdmi fifo reset

#define HDMI_DAT 0x20;

static int caninos_hdmi_audio_enable(struct caninos_hdmi *hdmi)
{
	u32 val = caninos_hdmi_readl(hdmi, HDMI_ICR);
	val |= (1 << 25);
	// caninos_hdmi_writel(hdmi, HDMI_ICR, val);
	return 0;
}

static int caninos_hdmi_audio_disable(struct caninos_hdmi *hdmi)
{
	u32 val = caninos_hdmi_readl(hdmi, HDMI_ICR);
	val &= ~(1 << 25);
	//caninos_hdmi_writel(hdmi, HDMI_ICR, val);
	return 0;
}


static inline struct caninos_hdmi_audio *dai_to_hdmi_audio(struct snd_soc_dai *dai)
{
	struct snd_soc_card *card = snd_soc_dai_get_drvdata(dai);

	return snd_soc_card_get_drvdata(card);
}

static void caninos_hdmi_audio_reset(struct caninos_hdmi *hdmi)
{
	caninos_hdmi_audio_disable(hdmi);
	caninos_hdmi_audio_enable(hdmi);
}


static int caninos_hdmi_audio_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct caninos_hdmi_audio *hdmi_audio = dai_to_hdmi_audio(dai);

	if (hdmi_audio->substream && hdmi_audio->substream != substream)
		return -EINVAL;

	hdmi_audio->substream = substream;

	caninos_hdmi_audio_enable(hdmi_audio->hdmi);

	return 0;
}

static int caninos_hdmi_audio_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	return 0;
}

static void caninos_hdmi_audio_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct caninos_hdmi_audio *hdmi_audio = dai_to_hdmi_audio(dai);

	if (substream != hdmi_audio->substream)
		return;

	caninos_hdmi_audio_disable(hdmi_audio->hdmi);

	hdmi_audio->substream = NULL;
}

/* HDMI audio codec callbacks */
static int caninos_hdmi_audio_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct caninos_hdmi_audio *hdmi_audio = dai_to_hdmi_audio(dai);
	struct caninos_hdmi *hdmi = hdmi_audio->hdmi;
	struct device *dev = hdmi_audio->dev;
	u32 tmp03, tmp47, CRP_N = 0;
	u32 ASPCR = 0;
	u32 ACACR = 0;
	
	if (substream != hdmi_audio->substream)
		return -EINVAL;

	dev_info(dev, "%s: %u Hz, %d bit, %d channels\n", __func__,
		params_rate(params), params_width(params),
		params_channels(params));

	hdmi_audio->channels = params_channels(params);
	hdmi_audio->samplerate = params_rate(params);
/* 
	caninos_hdmi_writel(hdmi, HDMI_ACRPCR, caninos_hdmi_readl(hdmi, HDMI_ACRPCR) | BIT(31));
	caninos_hdmi_readl(hdmi, HDMI_ACRPCR); // flush write buffer

	tmp03 = caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE0TO3);
	tmp03 &= ~(0xf << 24);

	tmp47 = caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE4TO7);
	tmp47 &= ~(0xf << 4);
	tmp47 |= 0xb;

	/* assume 48KHz samplerate TODO OTHER SAMPLERATES *//* 
	tmp03 |= 0x2 << 24;
	tmp47 |= 0xd << 4;
	CRP_N = 6144;

	caninos_hdmi_writel(hdmi, HDMI_ACRPCR, CRP_N | (0x1 << 31));

	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE0TO3, tmp03);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE4TO7, tmp47);

	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE8TO11, 0x0);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE12TO15, 0x0);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE16TO19, 0x0);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE20TO23, 0x0); */

	// assume 2 channels: channels 1 and 2
	// caninos_hdmi_writel(hdmi, HDMI_AICHSTASCN, 0x21);

	//Sample size = 24b */
	/* 
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE4TO7, (caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE4TO7) & ~0xf));
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE4TO7, caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE4TO7) | 0xb); */

	// Assume audio is in IEC-60958 format, 2 channels
	ASPCR = 0x3;
	ACACR = 0xfac688;

	/* enable Audio FIFO_FILL  disable wait cycle */
	/* caninos_hdmi_writel(hdmi, HDMI_CR, caninos_hdmi_readl(hdmi, HDMI_CR) | 0x50);

	caninos_hdmi_writel(hdmi, HDMI_ASPCR, ASPCR);
	caninos_hdmi_writel(hdmi, HDMI_ACACR, ACACR);
 */
    /* Uncompressed format 23~30 bits write 0
    * If for compressed streams,
    then the bit[1:0] of HDMI_AICHSTABYTE0TO3 = 0x2 (5005 new addition);
    * If for linear PCM streams, then HDMI_AICHSTABYTE0TO3
    bit[1:0]=0x0 (same as 227A);
    */
	// caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE0TO3, caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE0TO3) & ~0x3);

    /* If for compressed streams, then
    bit[30:23] of HDMI_ASPCR = 0xff (5005 new addition);
     * If for linear PCM streams,
     then bit[30:23]=0x0 of HDMI_ASPCR (same as 227A);
     */
	// caninos_hdmi_writel(hdmi, HDMI_ASPCR, caninos_hdmi_readl(hdmi, HDMI_ASPCR) & ~(0xff << 23));

	//hdmi_packet_gen_infoframe(hdmi);

    /* enable CRP */
	/*caninos_hdmi_writel(hdmi, HDMI_ACRPCR, caninos_hdmi_readl(hdmi, HDMI_ACRPCR) & ~(0x1 << 31));*/

	return 0; 
}

static int caninos_hdmi_audio_trigger(struct snd_pcm_substream *substream, int cmd,
				  struct snd_soc_dai *dai)
{
	struct caninos_hdmi_audio *hdmi_audio = dai_to_hdmi_audio(dai);
	struct caninos_hdmi *hdmi = hdmi_audio->hdmi;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		caninos_hdmi_audio_enable(hdmi);
		// infoframe
		
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		caninos_hdmi_audio_disable(hdmi);

		break;
	default:
		break;
	}

	return 0;
}

static inline struct caninos_hdmi_audio *
snd_component_to_hdmi_audio(struct snd_soc_component *component)
{
	struct snd_soc_card *card = snd_soc_component_get_drvdata(component);

	return snd_soc_card_get_drvdata(card);
}

static int caninos_hdmi_audio_ctl_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct caninos_hdmi_audio *hdmi = snd_component_to_hdmi_audio(component);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = sizeof(hdmi->ctl_data);

	return 0;
}

static int caninos_hdmi_audio_ctl_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct caninos_hdmi_audio *hdmi = snd_component_to_hdmi_audio(component);

	memcpy(ucontrol->value.bytes.data, hdmi->ctl_data,
	       sizeof(hdmi->ctl_data));

	return 0;
}

static const struct snd_kcontrol_new caninos_hdmi_audio_controls[] = {
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "IDK",
		.info = caninos_hdmi_audio_ctl_info,
		.get = caninos_hdmi_audio_ctl_get,
	},
};

static const struct snd_soc_dapm_widget caninos_hdmi_audio_widgets[] = {
	SND_SOC_DAPM_OUTPUT("TX"),
};

static const struct snd_soc_dapm_route caninos_hdmi_audio_routes[] = {
	{ "TX", NULL, "Playback" },
};

static const struct snd_soc_component_driver caninos_hdmi_audio_codec_component_drv = {
	.name 				= "hdmi-audio-codec-drv",
	.controls			= caninos_hdmi_audio_controls,
	.num_controls		= ARRAY_SIZE(caninos_hdmi_audio_controls),
	.dapm_widgets		= caninos_hdmi_audio_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(caninos_hdmi_audio_widgets),
	.dapm_routes		= caninos_hdmi_audio_routes,
	.num_dapm_routes	= ARRAY_SIZE(caninos_hdmi_audio_routes),
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming = 0,
};

static struct snd_soc_dai_driver caninos_hdmi_audio_codec_dai_drv = {
	.name = "hdmi-codec-dai",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S24_LE,
	},
};

static const struct snd_soc_dai_ops caninos_hdmi_audio_dai_ops = {
	.startup = caninos_hdmi_audio_startup,
	.shutdown = caninos_hdmi_audio_shutdown,
	.hw_params = caninos_hdmi_audio_hw_params,
	.set_fmt = caninos_hdmi_audio_set_fmt,
	.trigger = caninos_hdmi_audio_trigger,
};

static const struct snd_soc_component_driver caninos_hdmi_audio_cpu_dai_comp = {
	.name = "hdmi-cpu-dai-comp",
};
 
static int caninos_hdmi_audio_cpu_dai_probe(struct snd_soc_dai *dai)
{
	struct caninos_hdmi_audio *hdmi_audio = dai_to_hdmi_audio(dai);

	snd_soc_dai_init_dma_data(dai, &hdmi_audio->dma_data, NULL);

	return 0;
}

static struct snd_soc_dai_driver caninos_hdmi_audio_cpu_dai_drv = {
	.name = "hdmi-cpu-dai",
	.probe  = caninos_hdmi_audio_cpu_dai_probe,
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000 ,
		.formats = SNDRV_PCM_FMTBIT_S24_LE,
	},
	.ops = &caninos_hdmi_audio_dai_ops,
};

static const struct snd_dmaengine_pcm_config pcm_conf = {
	.chan_names[SNDRV_PCM_STREAM_PLAYBACK] = "audio-tx",
	.prepare_slave_config = snd_dmaengine_pcm_prepare_slave_config,
};

static int caninos_hdmi_audio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct caninos_hdmi_audio *hdmi_audio;
	struct snd_soc_dai_link *dai_link;
	struct snd_soc_card *card;

 	struct device_node *hdmi_np;
	struct platform_device *hdmi_pdev;

	struct resource *res;
	int ret;

	hdmi_audio = devm_kzalloc(dev, sizeof(*hdmi_audio), GFP_KERNEL);
	hdmi_audio->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res)
	{
		dev_err(dev, "could not get memory resource\n");
		return -ENODEV;
	}
	
	hdmi_audio->phys_base = res->start;
	hdmi_audio->base = devm_ioremap(dev, res->start, resource_size(res));
	
	if (IS_ERR(hdmi_audio->base)) {
		dev_err(dev, "could not get ioremap memory region\n");
		return PTR_ERR(hdmi_audio->base);
	}

 	hdmi_np = of_parse_phandle(dev->of_node, "hdmi", 0);

	if (!hdmi_np) {
		dev_err(dev, "could not find HDMI");
		return -ENXIO;
	}

	hdmi_pdev = of_find_device_by_node(hdmi_np);

	if (hdmi_pdev)
		hdmi_audio->hdmi = platform_get_drvdata(hdmi_pdev);

	if(!hdmi_pdev || !hdmi_audio->hdmi) {
		dev_err(dev, "HDMI is not ready");
		return -EPROBE_DEFER;
	}

	of_node_put(hdmi_np);
	hdmi_audio->hdmi->audio = hdmi_audio;
	
	if (!of_find_property(dev->of_node, "dmas", NULL)) {
		dev_warn(dev,
			 "'dmas' DT property is missing, no HDMI audio\n");
		return -ENXIO;
	}

	hdmi_audio->dma_data.addr = hdmi_audio->phys_base + HDMI_DAT;
	hdmi_audio->dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	hdmi_audio->dma_data.maxburst = 2;

	ret = devm_snd_dmaengine_pcm_register(dev, &pcm_conf, 0);
	if (ret) {
		dev_err(dev, "Could not register PCM component: %d\n", ret);
		return ret;
	}

	ret = devm_snd_soc_register_component(dev, &caninos_hdmi_audio_cpu_dai_comp,
					      &caninos_hdmi_audio_cpu_dai_drv, 1);
	if (ret) {
		dev_err(dev, "Could not register CPU DAI: %d\n", ret);
		return ret;
	}

	ret = devm_snd_soc_register_component(dev, &caninos_hdmi_audio_codec_component_drv,
				     &caninos_hdmi_audio_codec_dai_drv, 1);
	if (ret) {
		dev_err(dev, "Could not register component: %d\n", ret);
		return ret;
	}

	dai_link = &hdmi_audio->link;
	dai_link->name = "Caninos HDMI Link";
	dai_link->stream_name = "Caninos HDMI PCM";
	dai_link->codec_dai_name = caninos_hdmi_audio_codec_dai_drv.name;
	dai_link->cpu_dai_name = dev_name(dev);
	dai_link->codec_name = dev_name(dev);
	dai_link->platform_name = dev_name(dev);

	card = &hdmi_audio->card;
	card->dai_link = dai_link;
	card->num_links = 1;
	card->name = "Caninos HDMI Sound Card";
	card->dev = dev;
	card->owner = THIS_MODULE;

	ret = devm_snd_soc_register_card(dev, card);
	if (ret) {
		dev_err(dev, "Could not register sound card: %d\n", ret);
		return ret;
	}

	snd_soc_card_set_drvdata(card, hdmi_audio);

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

MODULE_AUTHOR("Ana Clara Forcelli <ana.forcelli@lsitec.org.br>");
MODULE_DESCRIPTION("Caninos HDMI Audio Driver");
MODULE_LICENSE("GPL v2");
