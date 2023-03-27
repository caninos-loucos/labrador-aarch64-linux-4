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
#include <linux/pinctrl/consumer.h>
#include <linux/of_platform.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/info.h>
#include <sound/dmaengine_pcm.h>

#include "caninos-codec.h"

struct snd_caninos
{
	void __iomem *base;
	struct pinctrl *pcl;
	struct clk *tx_clk;
	struct clk *rx_clk;
	struct clk *pll_clk;
	struct reset_control *rst_audio;
	
	struct atc260x_audio *codec;
	struct soc_audio_device soc;
	
	struct snd_card *card;
	struct snd_pcm *pcm;
	
	struct dma_chan *txchan;
	struct dma_chan *rxchan;
	
	struct device *dev;
	
	phys_addr_t phys_base;
};



static void snd_caninos_set_rate(struct snd_caninos *chip)
{
	unsigned long reg_val;
	int rate;
	
	rate = 48000;
	reg_val = 49152000;
	
	clk_set_rate(chip->pll_clk, reg_val);
	clk_set_rate(chip->rx_clk, rate << 8);
	clk_set_rate(chip->tx_clk, rate << 8);
}

static void snd_caninos_antipop_clk_set(struct snd_caninos *chip)
{
	unsigned long reg_val;
	int rate;
	
	rate = 44100;
	reg_val = 45158400;
	
	clk_prepare_enable(chip->pll_clk);
	clk_set_rate(chip->pll_clk, reg_val);
	
	clk_prepare_enable(chip->rx_clk);
	clk_prepare_enable(chip->tx_clk);
	
	clk_set_rate(chip->rx_clk, rate << 8);
	clk_set_rate(chip->tx_clk, rate << 8);
	
	reset_control_assert(chip->rst_audio);
	udelay(20);
	reset_control_deassert(chip->rst_audio);
}

static void snd_caninos_soc_writel(struct soc_audio_device *soc,
                                   u32 value, int reg)
{
	struct snd_caninos *chip = container_of(soc, struct snd_caninos, soc);
	writel(value, chip->base + reg);
}

static u32 snd_caninos_soc_readl(struct soc_audio_device *soc, int reg)
{
	struct snd_caninos *chip = container_of(soc, struct snd_caninos, soc);
	return readl(chip->base + reg);
}

static void snd_caninos_playback_capture_remove(struct snd_caninos *chip)
{
	void __iomem *base = chip->base;
	writel(readl(base + I2S_CTL) & ~(0x3 << 0), base + I2S_CTL);
}

static void snd_caninos_playback_capture_setup(struct snd_caninos *chip)
{ 
	void __iomem *base = chip->base;

	/* disable i2s tx&rx */
	writel(readl(base + I2S_CTL) & ~(0x3 << 0), base + I2S_CTL);
	
	/* reset i2s rx&&tx fifo, avoid left & right channel wrong */
	writel(readl(base + I2S_FIFOCTL) & ~(0x3 << 9) & ~0x3, base + I2S_FIFOCTL);
	writel(readl(base + I2S_FIFOCTL) | (0x3 << 9) | 0x3, base + I2S_FIFOCTL);

	
	/* this should before enable rx/tx,
	or after suspend, data may be corrupt */
	writel(readl(base + I2S_CTL) & ~(0x3 << 11), base + I2S_CTL);
	writel(readl(base + I2S_CTL) | (0x1 << 11), base + I2S_CTL);
	
	/* set i2s mode I2S_RX_ClkSel==1 */
	writel(readl(base + I2S_CTL) | (0x1 << 10), base + I2S_CTL);
	
	/* enable i2s rx/tx at the same time */
	writel(readl(base + I2S_CTL) | 0x3, base + I2S_CTL);
	
	/* i2s rx 00: 2.0-Channel Mode */
	writel(readl(base + I2S_CTL) & ~(0x3 << 8), base + I2S_CTL);
	writel(readl(base + I2S_CTL) & ~(0x7 << 4), base + I2S_CTL);
	
	writel(0x0, base + I2STX_DAT);
	writel(0x0, base + I2STX_DAT);
} 

static int caninos_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_caninos *chip = snd_pcm_chip(substream->pcm);
	int err;
	
	switch (cmd)
	{
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			chip->codec->dac_playback_mute(0);
		}
		break;
		
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			chip->codec->dac_playback_mute(1);
		}
		break;
	}
	
	err = snd_dmaengine_pcm_trigger(substream, cmd);
	
	if (err)
	{
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			chip->codec->dac_playback_mute(1);
		}
		pr_err("%s: snd_dmaengine_pcm_trigger returned %d\n", __func__, err);
		return err;
	}
	
	return 0;
}

static int caninos_pcm_hw_params
	(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_caninos *chip = snd_pcm_chip(substream->pcm);
	struct dma_slave_config slave_config;
	struct dma_chan *chan;

	int err;
	
	memset(&slave_config, 0, sizeof(slave_config));
	
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
		chan = chip->txchan;
		slave_config.dst_addr = chip->phys_base + I2STX_DAT;
		slave_config.direction = DMA_MEM_TO_DEV;
		slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		slave_config.device_fc = false;
	}
	else
	{
		chan = chip->rxchan;
		slave_config.src_addr = chip->phys_base + I2SRX_DAT;
		slave_config.direction = DMA_DEV_TO_MEM;
		slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		slave_config.device_fc = false;
	}
	
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
	struct snd_caninos *chip = snd_pcm_chip(substream->pcm);
	struct dma_chan *chan;
	int err;
	
	err = caninos_pcm_set_runtime_hwparams(substream);
	
	if (err) {
		pr_err("%s: caninos_pcm_set_runtime_hwparams returned %d\n",
		       __func__, err);
		return err;
	}
	
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		chan = chip->txchan;
	}
	else {
		chan = chip->rxchan;
	}
	
	err = snd_dmaengine_pcm_open(substream, chan);
	
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

static int labrador_mixer_playback_volume_info
	(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 190;
	uinfo->value.integer.step = 1;
	return 0;
}

static int labrador_mixer_playback_volume_get
	(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_caninos *chip = snd_kcontrol_chip(kcontrol);
	int left, right, ret;
	
	ret = chip->codec->dac_playback_volume_get(&left, &right);
	
	if (ret < 0) {
		return ret;
	}
	else
	{
		ucontrol->value.integer.value[0] = left;
		ucontrol->value.integer.value[1] = right;
		return 0;
	}
}

static int labrador_mixer_playback_volume_put
	(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_caninos *chip = snd_kcontrol_chip(kcontrol);
	int left, right;
	
	left = ucontrol->value.integer.value[0];
	right = ucontrol->value.integer.value[1];
	
	return chip->codec->dac_playback_volume_set(left, right);
}

static const DECLARE_TLV_DB_MINMAX(db_scale_playback_volume, -7200, 0);

static struct snd_kcontrol_new caninos_playback_volume = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ,
	.name = "Playback Volume",
	.index = 0,
	.info = labrador_mixer_playback_volume_info,
	.get = labrador_mixer_playback_volume_get,
	.put = labrador_mixer_playback_volume_put,
	.tlv = {.p = db_scale_playback_volume}
};

static int snd_caninos_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct platform_device *codec_pdev;
	struct device_node *codec_np;
	struct snd_card *card;
	struct snd_caninos *chip;
	struct snd_pcm *pcm;
	struct resource *res;
	int err;
	
	err = snd_card_new(dev, 1, "Caninos Soundcard",
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
	
	//chip->pcl = pinctrl_get_select_default(dev);
	
	//if (IS_ERR(chip->pcl))
	//{
	//	dev_err(dev, "could not get pinctrl device\n");
	//	snd_card_free(card);
	//	return PTR_ERR(chip->pcl);
	//}
	
	chip->pll_clk = devm_clk_get(dev, "audio_pll");
	
	if (IS_ERR_OR_NULL(chip->pll_clk))
	{
		dev_err(dev, "could not get audio_pll clock\n");
		snd_card_free(card);
		return -ENODEV;
	}
	
	chip->tx_clk = devm_clk_get(dev, "i2stx");
	
	if (IS_ERR_OR_NULL(chip->tx_clk))
	{
		dev_err(dev, "could not get i2stx clock\n");
		snd_card_free(card);
		return -ENODEV;
	}
	
	chip->rx_clk = devm_clk_get(dev, "i2srx");
	
	if (IS_ERR_OR_NULL(chip->rx_clk))
	{
		dev_err(dev, "could not get i2srx clock\n");
		snd_card_free(card);
		return -ENODEV;
	}
	
	chip->rst_audio = devm_reset_control_get(dev, "audio_rst");
	
	if (IS_ERR_OR_NULL(chip->rst_audio))
	{
		dev_err(dev, "could not get audio reset control\n");
		snd_card_free(card);
		return -ENODEV;
	}
	
	codec_np = of_parse_phandle(dev->of_node, "codec", 0);
	
	if (!codec_np)
	{
		dev_err(dev, "could not find codec\n");
		snd_card_free(card);
		return -ENXIO;
	}
	
	codec_pdev = of_find_device_by_node(codec_np);
	
	if (codec_pdev) {
		chip->codec = platform_get_drvdata(codec_pdev);
	}
	
	of_node_put(codec_np);
	
	if (!codec_pdev || !chip->codec)
	{
		dev_err(dev, "codec is not ready\n");
		snd_card_free(card);
		return -EPROBE_DEFER;
	}

	chip->txchan = dma_request_slave_channel(dev, "tx");
	
	if (!chip->txchan)
	{
		dev_err(dev, "could not request tx dma channel");
		snd_card_free(card);
		return -ENODEV;
	}
	
	chip->rxchan = dma_request_slave_channel(dev, "rx");

	if (!chip->rxchan)
	{
		dev_err(dev, "could not request rx dma channel");
		snd_card_free(card);
		return -ENODEV;
	}
	
	strcpy(card->driver, "Caninos Soundcard");
	strcpy(card->shortname, "Caninos Soundcard");
	strcpy(card->longname, "Caninos Labrador Soundcard Driver");
	
	/* Create a playback and capture pcm device */
	err = snd_pcm_new(chip->card, "Caninos PCM", 0, 1, 1, &pcm);
	
	if (err < 0)
	{
		dev_err(dev, "snd_pcm_new() failed");
		snd_card_free(card);
		return err;
	}
	
	strcpy(pcm->name, "Caninos PCM");
	pcm->private_data = chip;
	pcm->info_flags = 0;
	chip->pcm = pcm;
	
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &caninos_pcm_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &caninos_pcm_ops);
	
	/* Setup Playback Substream DMA channel */
	snd_pcm_lib_preallocate_pages
		(pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream,
		SNDRV_DMA_TYPE_DEV, chip->txchan->device->dev, 64*1024, 64*1024);
	
	/* Setup Capture Substream DMA channel */
	snd_pcm_lib_preallocate_pages
		(pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream,
		SNDRV_DMA_TYPE_DEV, chip->rxchan->device->dev, 64*1024, 64*1024);
	
	strcpy(card->mixername, "Caninos Mixer");
	
	err = snd_ctl_add(card, snd_ctl_new1(&caninos_playback_volume, chip));
	
	if (err < 0)
	{
		dev_err(dev, "snd_ctl_add() failed");
		snd_card_free(card);
		return err;
	}
	
	chip->soc.soc_writel = snd_caninos_soc_writel;
	chip->soc.soc_readl = snd_caninos_soc_readl;
	
	/* set i2s tx/rx clk 11.2896MHz for antipop */
	snd_caninos_antipop_clk_set(chip);
	
	/* start codec configuration */
	chip->codec->probe(&chip->soc);
	
	/* set rate to 48000 */
	snd_caninos_set_rate(chip);
	
	/* setup codec playback and capture modes */
	chip->codec->playback();
	chip->codec->capture();
	
	/* setup soc playback and capture modes */
	snd_caninos_playback_capture_setup(chip);
	
	/* all is good, register our new card! */
	err = snd_card_register(card);
	
	if (err == 0)
	{
		platform_set_drvdata(pdev, card);
		return 0;
	}
	else
	{
		chip->codec->remove();
		snd_caninos_playback_capture_remove(chip);
		snd_card_free(card);
		return err;
	}
}

static int snd_caninos_remove(struct platform_device *pdev)
{
	struct snd_card *card = platform_get_drvdata(pdev);
	
	if (card)
	{
		struct snd_caninos *chip = card->private_data;
		chip->codec->remove();
		snd_caninos_playback_capture_remove(chip);
		snd_card_free(card);
	}
	
	return 0;
}

static const struct of_device_id sndcard_of_match[] = {
	{.compatible = "caninos,sndcard",},
	{}
};
MODULE_DEVICE_TABLE(of, sndcard_of_match);

static struct platform_driver snd_caninos_driver = {
	.probe = snd_caninos_probe,
	.remove	= snd_caninos_remove,
	.driver	= {
		.name = "caninos-sndcard",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sndcard_of_match),
	},
};

module_platform_driver(snd_caninos_driver);

MODULE_DESCRIPTION("Caninos Labrador Soundcard Driver");
MODULE_LICENSE("GPL");

