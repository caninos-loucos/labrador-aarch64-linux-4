/*
 * Copyright (c) 2019 LSI-TEC - Caninos Loucos
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
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/info.h>
#include <sound/dmaengine_pcm.h>

#include "hdmi.h"
#include "hdmi-regs.h"


#define SPDIF_HDMI_CTL 0x10
#define	HDMFR  1  // hdmi fifo reset
#define HDMFDEN 8 // fifo drq enable

#define HDMI_DAT 0x20;

struct snd_hdmi_caninos
{
	void __iomem *base;
	
	struct caninos_hdmi *hdmi;
	
	struct snd_card *card;
	struct snd_pcm *pcm;
	
	struct dma_chan *txchan;
	struct dma_chan *rxchan;

	struct clk *tx_clk;
	struct clk *pll_clk;

	int volume_left;
	int volume_right;
	
	struct device *dev;
	
	phys_addr_t phys_base;
};

static void caninos_set_rate(struct snd_hdmi_caninos *chip)
{
	unsigned long reg_val;
	int rate;
	
	rate = 48000;
	reg_val = 49152000;
	
	clk_set_rate(chip->pll_clk, reg_val);
	clk_prepare_enable(chip->pll_clk);
	clk_set_rate(chip->tx_clk, rate << 7);
	clk_prepare_enable(chip->tx_clk);
}

static void caninos_audio_enable(struct snd_hdmi_caninos *chip)
{
	writel(readl(chip->base) | BIT(HDMFR) | BIT(HDMFDEN), chip->base + SPDIF_HDMI_CTL);
	chip->hdmi->ops.audio_enable(chip->hdmi);
}

static void caninos_audio_disable(struct snd_hdmi_caninos *chip)
{
	writel(readl(chip->base) & ~BIT(HDMFR) & ~BIT(HDMFDEN), chip->base + SPDIF_HDMI_CTL);
	chip->hdmi->ops.audio_disable(chip->hdmi);
}

static int caninos_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_hdmi_caninos *chip = snd_pcm_chip(substream->pcm);
	int err;
	
	switch (cmd)
	{
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			caninos_audio_enable(chip);
		}
		break;
		
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {			
			caninos_audio_disable(chip);
		}
		break;
	}
	
	err = snd_dmaengine_pcm_trigger(substream, cmd);
	
	if (err)
	{
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			caninos_audio_disable(chip);
		}
		pr_err("%s: snd_dmaengine_pcm_trigger returned %d\n", __func__, err);
		return err;
	}
	
	return 0;
}

static int caninos_pcm_hw_params
	(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_hdmi_caninos *chip = snd_pcm_chip(substream->pcm);
	struct dma_slave_config slave_config;
	struct dma_chan *chan;

	int err;
	
	memset(&slave_config, 0, sizeof(slave_config));
	
	chan = chip->txchan;
	slave_config.dst_addr = chip->phys_base + HDMI_DAT;
	slave_config.direction = DMA_MEM_TO_DEV;
	slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	slave_config.device_fc = false;
	
	err = dmaengine_slave_config(chan, &slave_config);
	
	if (err) {
		pr_err("%s: dmaengine_slave_config returned %d\n", __func__, err);
		return err;
	}
	
	err = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
	
	if (err < 0) {
		pr_err("%s: snd_pcm_lib_malloc_pages returned %d\n", __func__, err);
		return err;
	}
	
	return 0;
}


static int caninos_pcm_set_runtime_hwparams(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_pcm_hardware hw;

	memset(&hw, 0, sizeof(hw));
	
	hw.info = SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
	          SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_INTERLEAVED |
	          SNDRV_PCM_INFO_RESUME;
	
	hw.periods_min = 1;
	hw.periods_max = 1024;
	
	hw.period_bytes_min = 64;
	hw.period_bytes_max = 64*1024;
	hw.buffer_bytes_max = 64*1024;
	
	hw.fifo_size = 0;
	
	hw.formats = SNDRV_PCM_FMTBIT_S32_LE;
	
	hw.rates = SNDRV_PCM_RATE_48000;
	
	hw.rate_min = 48000;
	hw.rate_max = 48000;
	
	hw.channels_min = 2;
	hw.channels_max = 2;
	
	runtime->hw = hw;

	return 0;
}

static int caninos_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_hdmi_caninos *chip = snd_pcm_chip(substream->pcm);
	int err;

	caninos_pcm_set_runtime_hwparams(substream);
	chip->hdmi->ops.set_audio_interface(chip->hdmi);
		
	err = snd_dmaengine_pcm_open(substream, chip->txchan);
	
	if (err) {
		pr_err("%s: snd_dmaengine_pcm_open returned %d\n", __func__, err);
		return err;
	}
	
	return 0;
}

static int caninos_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static struct snd_pcm_ops caninos_pcm_ops = {
	.open =	caninos_pcm_open,
	.close = snd_dmaengine_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = caninos_pcm_hw_params,
	.hw_free = snd_pcm_lib_free_pages,
	.trigger = caninos_pcm_trigger,
	.pointer = snd_dmaengine_pcm_pointer,
	.prepare = caninos_pcm_prepare,
};

static int caninos_hdmi_mixer_playback_volume_info
	(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 190;
	uinfo->value.integer.step = 1;
	return 0;
}

static int caninos_hdmi_mixer_playback_volume_get
	(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_hdmi_caninos *chip = snd_kcontrol_chip(kcontrol);
	
	ucontrol->value.integer.value[0] = chip->volume_left;
	ucontrol->value.integer.value[1] = chip->volume_right;
	return 0;
}

static int caninos_hdmi_mixer_playback_volume_put
	(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_hdmi_caninos *chip = snd_kcontrol_chip(kcontrol);
	
	chip->volume_left = ucontrol->value.integer.value[0];
	chip->volume_right = ucontrol->value.integer.value[1];	

	return 0;
}

static const DECLARE_TLV_DB_MINMAX(db_scale_playback_volume, -7200, 0);

static struct snd_kcontrol_new caninos_playback_volume = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ,
	.name = "Playback Volume",
	.index = 0,
	.info = caninos_hdmi_mixer_playback_volume_info,
	.get = caninos_hdmi_mixer_playback_volume_get,
	.put = caninos_hdmi_mixer_playback_volume_put,
	.tlv = {.p = db_scale_playback_volume}
};

static int snd_caninos_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct platform_device *hdmi_pdev;
	struct device_node *hdmi_np;
	struct snd_card *card;
	struct snd_hdmi_caninos *chip;
	struct snd_pcm *pcm;
	struct resource *res;
	int err;
	
	err = snd_card_new(dev, 0, "Caninos HDMI Soundcard",
	                   THIS_MODULE, sizeof(*chip), &card);
	
	if (err < 0) {
		return err;
	}
	
	chip = card->private_data;
	chip->card = card;
	chip->dev = dev;
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	
	if (!res)
	{
		dev_err(dev, "could not get memory resource\n");
		snd_card_free(card);
		return -ENODEV;
	}
	
	chip->phys_base = res->start;
	
	chip->base = devm_ioremap(dev, res->start, resource_size(res));
	
	if (IS_ERR(chip->base))
	{
		dev_err(dev, "could not get ioremap memory region\n");
		snd_card_free(card);
		return PTR_ERR(chip->base);
	}

	chip->pll_clk = devm_clk_get(dev, "audio-pll");
	
	if (IS_ERR_OR_NULL(chip->pll_clk))
	{
		dev_err(dev, "could not get audio-pll clock\n");
		snd_card_free(card);
		return -ENODEV;
	}
	
	chip->tx_clk = devm_clk_get(dev, "hdmia");
	
	if (IS_ERR_OR_NULL(chip->tx_clk))
	{
		dev_err(dev, "could not get hdmia clock\n");
		snd_card_free(card);
		return -ENODEV;
	}
	
	
	hdmi_np = of_parse_phandle(dev->of_node, "hdmi", 0);
	
	if (!hdmi_np)
	{
		dev_err(dev, "could not find hdmi\n");
		snd_card_free(card);
		return -ENXIO;
	}
	
	hdmi_pdev = of_find_device_by_node(hdmi_np);
	
	if (hdmi_pdev) {
		chip->hdmi = platform_get_drvdata(hdmi_pdev);
	}
	
	of_node_put(hdmi_np);
	
	if (!hdmi_pdev || !chip->hdmi)
	{
		dev_err(dev, "hdmi is not ready\n");
		snd_card_free(card);
		return -EPROBE_DEFER;
	}

	chip->txchan = dma_request_slave_channel(dev, "hdmi-tx");
	
	if (!chip->txchan)
	{
		dev_err(dev, "could not request tx dma channel");
		snd_card_free(card);
		return -ENODEV;
	}
	
	strcpy(card->driver, "Caninos HDMI Soundcard");
	strcpy(card->shortname, "Caninos HDMI Soundcard");
	strcpy(card->longname, "Caninos HDMI Soundcard");
	
	/* Create a playback and capture pcm device */
	err = snd_pcm_new(chip->card, "Caninos HDMI PCM", 0, 1, 0, &pcm);
	
	if (err < 0)
	{
		dev_err(dev, "snd_pcm_new() failed");
		snd_card_free(card);
		return err;
	}
	
	strcpy(pcm->name, "Caninos HDMI PCM");
	pcm->private_data = chip;
	pcm->info_flags = 0;
	chip->pcm = pcm;
	
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &caninos_pcm_ops);

	caninos_set_rate(chip);
	
	/* Setup Playback Substream DMA channel */
	snd_pcm_lib_preallocate_pages
		(pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream,
		SNDRV_DMA_TYPE_DEV, chip->txchan->device->dev, 64*1024, 64*1024);
		
	strcpy(card->mixername, "Caninos HDMI Mixer");
	
	err = snd_ctl_add(card, snd_ctl_new1(&caninos_playback_volume, chip));
	
	if (err < 0)
	{
		dev_err(dev, "snd_ctl_add() failed");
		snd_card_free(card);
		return err;
	}
		
	err = snd_card_register(card);
	
	if (err == 0)
	{
		platform_set_drvdata(pdev, card);
		return 0;
	}
	else
	{
		snd_card_free(card);
		return err;
	}
}

static int snd_caninos_remove(struct platform_device *pdev)
{
	struct snd_card *card = platform_get_drvdata(pdev);
	
	if (card)
	{
		struct snd_hdmi_caninos *chip = card->private_data;
		snd_card_free(card);
	}
	
	return 0;
}

static const struct of_device_id sndcard_of_match[] = {
	{.compatible = "caninos,k7-hdmi-audio",},
	{.compatible = "caninos,k5-hdmi-audio",},
	{}
};
MODULE_DEVICE_TABLE(of, sndcard_of_match);

static struct platform_driver snd_caninos_driver = {
	.probe = snd_caninos_probe,
	.remove	= snd_caninos_remove,
	.driver	= {
		.name = "caninos-hdmi-audio",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sndcard_of_match),
	},
};

module_platform_driver(snd_caninos_driver);

MODULE_DESCRIPTION("Caninos HDMI Soundcard Driver");
MODULE_LICENSE("GPL");

