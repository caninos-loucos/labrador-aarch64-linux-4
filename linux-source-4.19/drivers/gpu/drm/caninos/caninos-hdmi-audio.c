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
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <sound/hdmi-codec.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm_drm_eld.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "hdmi.h"
#include "hdmi-regs.h"

static int caninos_hdmi_audio_enable(struct caninos_hdmi *hdmi)
{
	u32 val = caninos_hdmi_readl(hdmi, HDMI_ICR);
	val |= (1 << 25);
	caninos_hdmi_writel(hdmi, HDMI_ICR, val);
	return 0;
}

static int caninos_hdmi_audio_disable(struct caninos_hdmi *hdmi)
{
	u32 val = caninos_hdmi_readl(hdmi, HDMI_ICR);
	val &= ~(1 << 25);
	caninos_hdmi_writel(hdmi, HDMI_ICR, val);
	return 0;
}


static inline struct caninos_hdmi *dai_to_hdmi(struct snd_soc_dai *dai)
{
	struct snd_soc_card *card = snd_soc_dai_get_drvdata(dai);

	return snd_soc_card_get_drvdata(card);
}

static int caninos_hdmi_audio_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct caninos_hdmi *hdmi = dai_to_hdmi(dai);
	int ret;

	if (hdmi->audio.substream && hdmi->audio.substream != substream)
		return -EINVAL;

	hdmi->audio.substream = substream;

	if (ret)
		return ret;

	return 0;
}

static int caninos_hdmi_audio_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	return 0;
}

static void caninos_hdmi_audio_reset(struct caninos_hdmi *hdmi)
{
	caninos_hdmi_audio_disable(hdmi);
	caninos_hdmi_audio_enable(hdmi);
}

static void caninos_hdmi_audio_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct caninos_hdmi *hdmi = dai_to_hdmi(dai);

	if (substream != hdmi->audio.substream)
		return;

	caninos_hdmi_audio_reset(hdmi);

	hdmi->audio.substream = NULL;
}

/* HDMI audio codec callbacks */
static int caninos_hdmi_audio_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct caninos_hdmi *hdmi = dai_to_hdmi(dai);
	struct device *dev = hdmi->dev;

	if (substream != hdmi->audio.substream)
		return -EINVAL;

	dev_info(dev, "%s: %u Hz, %d bit, %d channels\n", __func__,
		params_rate(params), params_width(params),
		params_channels(params));

	hdmi->audio.channels = params_channels(params);
	hdmi->audio.samplerate = params_rate(params);

		u32 tmp03, tmp47;
	u32 CRP_N = 0;
	u32 ASPCR = 0;
	u32 ACACR = 0;
	
	caninos_hdmi_writel(hdmi, HDMI_ACRPCR, caninos_hdmi_readl(hdmi, HDMI_ACRPCR) | BIT(31));
	caninos_hdmi_readl(hdmi, HDMI_ACRPCR); // flush write buffer

	tmp03 = caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE0TO3);
	tmp03 &= (~(0xf << 24));

	tmp47 = caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE4TO7);
	tmp47 &= (~(0xf << 4));
	tmp47 |= 0xb;

	/* assume 48KHz samplerate */
	tmp03 |= (0x2 << 24);
	tmp47 |= (0xd << 4);
	CRP_N = 6144;

	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE0TO3, tmp03);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE4TO7, tmp47);

	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE8TO11, 0x0);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE12TO15, 0x0);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE16TO19, 0x0);
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE20TO23, 0x0);

	/* assume 2 channels */
	caninos_hdmi_writel(hdmi, HDMI_AICHSTASCN, 0x20001);

	/* TODO samplesize 16bit, 20bit */
	/* 24 bit */
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE4TO7, (caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE4TO7) & ~0xf));
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE4TO7, caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE4TO7) | 0xb);

	// Assume audio is in IEC-60958 format, 2 channels
	ASPCR = 0x00000011;
	ACACR = 0xfac688;

	/* enable Audio FIFO_FILL  disable wait cycle */
	caninos_hdmi_writel(hdmi, HDMI_CR, caninos_hdmi_readl(hdmi, HDMI_CR) | 0x50);

	caninos_hdmi_writel(hdmi, HDMI_ASPCR, ASPCR);
	caninos_hdmi_writel(hdmi, HDMI_ACACR, ACACR);

    /* Uncompressed format 23~30 bits write 0
    * If for compressed streams,
    then the bit[1:0] of HDMI_AICHSTABYTE0TO3 = 0x2 (5005 new addition);
    * If for linear PCM streams, then HDMI_AICHSTABYTE0TO3
    bit[1:0]=0x0 (same as 227A);
    */
	caninos_hdmi_writel(hdmi, HDMI_AICHSTABYTE0TO3, caninos_hdmi_readl(hdmi, HDMI_AICHSTABYTE0TO3) & ~0x3);

    /* If for compressed streams, then
    bit[30:23] of HDMI_ASPCR = 0xff (5005 new addition);
     * If for linear PCM streams,
     then bit[30:23]=0x0 of HDMI_ASPCR (same as 227A);
     */
	caninos_hdmi_writel(hdmi, HDMI_ASPCR, caninos_hdmi_readl(hdmi, HDMI_ASPCR) & ~(0xff << 23));

	caninos_hdmi_writel(hdmi, HDMI_ACRPCR, CRP_N | (0x1 << 31));
	//hdmi_packet_gen_infoframe(hdmi);

    /* enable CRP */
	caninos_hdmi_writel(hdmi, HDMI_ACRPCR, caninos_hdmi_readl(hdmi, HDMI_ACRPCR) & ~(0x1 << 31));

	return 0;
}

static int caninos_hdmi_audio_trigger(struct snd_pcm_substream *substream, int cmd,
				  struct snd_soc_dai *dai)
{
	struct caninos_hdmi *hdmi = dai_to_hdmi(dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	
		break;
	default:
		break;
	}

	return 0;
}

static inline struct caninos_hdmi *
snd_component_to_hdmi(struct snd_soc_component *component)
{
	struct snd_soc_card *card = snd_soc_component_get_drvdata(component);

	return snd_soc_card_get_drvdata(card);
}

static int vc4_hdmi_audio_eld_ctl_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct caninos_hdmi *hdmi = snd_component_to_hdmi(component);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = sizeof(hdmi->connector->eld);

	return 0;
}

static int vc4_hdmi_audio_eld_ctl_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct caninos_hdmi *hdmi = snd_component_to_hdmi(component);

	memcpy(ucontrol->value.bytes.data, hdmi->connector->eld,
	       sizeof(hdmi->connector->eld));

	return 0;
}

static const struct snd_kcontrol_new vc4_hdmi_audio_controls[] = {
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "ELD",
		.info = vc4_hdmi_audio_eld_ctl_info,
		.get = vc4_hdmi_audio_eld_ctl_get,
	},
};

static const struct snd_soc_dapm_widget vc4_hdmi_audio_widgets[] = {
	SND_SOC_DAPM_OUTPUT("TX"),
};

static const struct snd_soc_dapm_route vc4_hdmi_audio_routes[] = {
	{ "TX", NULL, "Playback" },
};

static const struct snd_soc_component_driver caninos_hdmi_audio_component_drv = {
	.controls		= vc4_hdmi_audio_controls,
	.num_controls		= ARRAY_SIZE(vc4_hdmi_audio_controls),
	.dapm_widgets		= vc4_hdmi_audio_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(vc4_hdmi_audio_widgets),
	.dapm_routes		= vc4_hdmi_audio_routes,
	.num_dapm_routes	= ARRAY_SIZE(vc4_hdmi_audio_routes),
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

static const struct snd_soc_dai_ops caninos_hdmi_audio_dai_ops = {
	.startup = caninos_hdmi_audio_startup,
	.shutdown = caninos_hdmi_audio_shutdown,
	.hw_params = caninos_hdmi_audio_hw_params,
	.set_fmt = caninos_hdmi_audio_set_fmt,
	.trigger = caninos_hdmi_audio_trigger,
};

static struct snd_soc_dai_driver caninos_hdmi_audio_codec_dai_drv = {
	.name = "caninos-hdmi-audio",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_48000 ,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	},
};

static const struct snd_soc_component_driver caninos_hdmi_audio_cpu_dai_comp = {
	.name = "caninos-hdmi-cpu-dai-component",
};

static int vc4_hdmi_audio_cpu_dai_probe(struct snd_soc_dai *dai)
{
	struct caninos_hdmi *hdmi = dai_to_hdmi(dai);

	snd_soc_dai_init_dma_data(dai, &hdmi->audio.dma_data, NULL);

	return 0;
}

static struct snd_soc_dai_driver vc4_hdmi_audio_cpu_dai_drv = {
	.name = "vc4-hdmi-cpu-dai",
	.probe  = vc4_hdmi_audio_cpu_dai_probe,
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_48000 ,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &vc4_hdmi_audio_dai_ops,
};

static const struct snd_dmaengine_pcm_config pcm_conf = {
	.chan_names[SNDRV_PCM_STREAM_PLAYBACK] = "audio-tx",
	.prepare_slave_config = snd_dmaengine_pcm_prepare_slave_config,
};

static int vc4_hdmi_audio_init(struct caninos_hdmi *hdmi)
{
	struct snd_soc_dai_link *dai_link = &hdmi->audio.link;
	struct snd_soc_card *card = &hdmi->audio.card;
	struct device *dev = &hdmi->dev;
	const __be32 *addr;
	int ret;

	if (!of_find_property(dev->of_node, "dmas", NULL)) {
		dev_warn(dev,
			 "'dmas' DT property is missing, no HDMI audio\n");
		return 0;
	}

	/*
	 * Get the physical address of VC4_HD_MAI_DATA. We need to retrieve
	 * the bus address specified in the DT, because the physical address
	 * (the one returned by platform_get_resource()) is not appropriate
	 * for DMA transfers.
	 * This VC/MMU should probably be exposed to avoid this kind of hacks.
	 */
	addr = of_get_address(dev->of_node, 1, NULL, NULL);
	hdmi->audio.dma_data.addr = be32_to_cpup(addr) + VC4_HD_MAI_DATA;
	hdmi->audio.dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	hdmi->audio.dma_data.maxburst = 2;

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

	/* register component and codec dai */
	ret = devm_snd_soc_register_component(dev, &caninos_hdmi_audio_component_drv,
				     &caninos_hdmi_audio_codec_dai_drv, 1);
	if (ret) {
		dev_err(dev, "Could not register component: %d\n", ret);
		return ret;
	}

	dai_link->name = "Caninos HDMI Link";
	dai_link->stream_name = "Caninos HDMI PCM";
	dai_link->codec_dai_name = caninos_hdmi_audio_codec_dai_drv.name;
	dai_link->cpu_dai_name = dev_name(dev);
	dai_link->codec_name = dev_name(dev);
	dai_link->platform_name = dev_name(dev);

	card->dai_link = dai_link;
	card->num_links = 1;
	card->name = "caninos-hdmi-snd";
	card->dev = dev;
	card->owner = THIS_MODULE;

	/*
	 * Be careful, snd_soc_register_card() calls dev_set_drvdata() and
	 * stores a pointer to the snd card object in dev->driver_data. This
	 * means we cannot use it for something else. The hdmi back-pointer is
	 * now stored in card->drvdata and should be retrieved with
	 * snd_soc_card_get_drvdata() if needed.
	 */
	snd_soc_card_set_drvdata(card, hdmi);
	ret = devm_snd_soc_register_card(dev, card);
	if (ret)
		dev_err(dev, "Could not register sound card: %d\n", ret);

	return ret;

}


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
