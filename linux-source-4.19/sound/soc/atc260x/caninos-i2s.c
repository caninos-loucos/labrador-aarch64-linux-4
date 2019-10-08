#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/io.h>
#include <linux/ioport.h>

#include "common-regs-owl.h"

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>

#include <linux/mfd/atc260x/atc260x.h>

#define DRIVER_NAME "caninos-audio-i2s"

struct caninos_i2s_dev
{
	struct device *dev;
	struct snd_dmaengine_dai_dma_data dma_data[2];
	bool clk_prepared;
	
    void __iomem *base;
    phys_addr_t i2s_reg_base;
    
    struct clk *i2stx_clk;
    struct clk *i2srx_clk;
	struct clk *clk;
};

u32 snd_dai_readl(struct caninos_i2s_dev *dev, u32 reg)
{
	u32 res;
	res = readl(dev->base + reg);
	return res;
}

void snd_dai_writel(struct caninos_i2s_dev *dev, u32 val, u32 reg)
{
	u32 reg_val;
	writel(val, dev->base + reg);
	reg_val = readl(dev->base + reg);
}

typedef struct {
	short sample_rate;
	char index[2];
} fs_t;

static int get_sf_index(int sample_rate)
{
	int i = 0;
	char fs = sample_rate / 1000;
	
	static fs_t fs_list[] = {
		{ 192, { 0,  1} },
		{ 176, { 16, 17} },
		{ 96,  { 1,  3} },
		{ 88,  { 17, 19} },
		{ 64,  { 2, -1} },
		{ 48,  { 3,  5} },
		{ 44,  { 19, 21} },
		{ 32,  { 4,  6} },
		{ 24,  { 5, -1} },
		{ 22,  {21, -1} },
		{ 16,  { 6, -1} },
		{ 12,  { 7, -1} },
		{ 11,  {23, -1} },
		{ 8,   { 8, -1} },
		{ -1,  {-1, -1} }
	};

	while ((fs_list[i].sample_rate > 0) && (fs_list[i].sample_rate != fs))
		i++;
	return fs_list[i].index[0];
}

static void caninos_i2s_start_clock(struct caninos_i2s_dev *dev)
{
	if (dev->clk_prepared) {
		return;
	}
	clk_prepare_enable(dev->i2srx_clk);
	clk_prepare_enable(dev->i2stx_clk);
	clk_prepare_enable(dev->clk);
	dev->clk_prepared = true;
}

static void caninos_i2s_stop_clock(struct caninos_i2s_dev *dev)
{
	if (dev->clk_prepared)
	{
		//clk_disable_unprepare(dev->i2stx_clk);
		//clk_disable_unprepare(dev->i2srx_clk);
		//clk_disable_unprepare(dev->clk);
	}
	//dev->clk_prepared = false;
}

static int s900_dai_clk_set(struct caninos_i2s_dev *dev, int rate)
{
	unsigned long reg_val;
	int sf_index, ret;
	
	if (dev->clk_prepared == false) {
		return -1;
	}
	
	sf_index = get_sf_index(rate);
	
	if (sf_index & 0x10) {
		reg_val = 45158400;
	}
	else {
		reg_val = 49152000;
	}
	
	ret = clk_set_rate(dev->clk, reg_val);
	
	if (ret < 0)
	{
		dev_err(dev->dev, "clk set rate error\n");
		return ret;
	}
	
	ret = clk_set_rate(dev->i2stx_clk, rate << 8);
	
	if (ret < 0)
	{
		dev_err(dev->dev, "tx clk set rate error\n");
		return ret;
	}
	
	ret = clk_set_rate(dev->i2srx_clk, rate << 8);
	
	if (ret < 0)
	{
		dev_err(dev->dev, "rx clk set rate error\n");
		return ret;
	}
	
	return 0;
}

static int s900_dai_mode_set(struct caninos_i2s_dev *dev,
                             struct snd_soc_dai *dai)
{
	int ret = 0;
	
	/* disable i2s tx&rx */
	snd_dai_writel(dev, snd_dai_readl(dev, I2S_CTL) & ~(0x3 << 0), I2S_CTL);
	
	/* reset i2s rx&&tx fifo, avoid left & right channel wrong */
	snd_dai_writel(dev, snd_dai_readl(dev, I2S_FIFOCTL) & ~(0x3 << 9) & ~0x3,
	               I2S_FIFOCTL);
	snd_dai_writel(dev, snd_dai_readl(dev, I2S_FIFOCTL) | (0x3 << 9) | 0x3,
	               I2S_FIFOCTL);
	
	/* this should before enable rx/tx,
	or after suspend, data may be corrupt */
	snd_dai_writel(dev, snd_dai_readl(dev, I2S_CTL) & ~(0x3 << 11), I2S_CTL);
	snd_dai_writel(dev, snd_dai_readl(dev, I2S_CTL) | (0x1 << 11), I2S_CTL);
	
	/* set i2s mode I2S_RX_ClkSel==1 */
	snd_dai_writel(dev, snd_dai_readl(dev, I2S_CTL) | (0x1 << 10), I2S_CTL);
	
	/* enable i2s rx/tx at the same time */
	snd_dai_writel(dev, snd_dai_readl(dev, I2S_CTL) | 0x3, I2S_CTL);
	
	/* i2s rx 00: 2.0-Channel Mode */
	snd_dai_writel(dev, snd_dai_readl(dev, I2S_CTL) & ~(0x3 << 8), I2S_CTL);
	snd_dai_writel(dev, snd_dai_readl(dev, I2S_CTL) & ~(0x7 << 4), I2S_CTL);
	snd_dai_writel(dev, 0x0, I2STX_DAT);
	snd_dai_writel(dev, 0x0, I2STX_DAT);
	return ret;
}

static int caninos_i2s_hw_free(struct snd_pcm_substream *substream,
                               struct snd_soc_dai *dai)
{
	struct caninos_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	
	if (SNDRV_PCM_STREAM_CAPTURE == substream->stream)
	{
		dev_info(dev->dev, "capture stream free\n");
	}

	if (SNDRV_PCM_STREAM_PLAYBACK == substream->stream)
	{
		dev_info(dev->dev, "playback stream free\n");
	}
	return 0;
}

static int caninos_i2s_hw_params(struct snd_pcm_substream *substream,
                                 struct snd_pcm_hw_params *params,
                                 struct snd_soc_dai *dai)
{
	struct caninos_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	
	switch (params_format(params))
	{
	case SNDRV_PCM_FORMAT_S32_LE:
		break;
	default:
		dev_err(dev->dev, "stream hw invalid format %d\n", params_format(params));
		return -EINVAL;
	}
	
	if (SNDRV_PCM_STREAM_CAPTURE == substream->stream)
	{
		s900_dai_clk_set(dev, params_rate(params));
		s900_dai_mode_set(dev, dai);
		dev_info(dev->dev, "capture stream hw params\n");
	}
	
	if (SNDRV_PCM_STREAM_PLAYBACK == substream->stream)
	{
		s900_dai_clk_set(dev, params_rate(params));
		s900_dai_mode_set(dev, dai);
		dev_info(dev->dev, "playback stream hw params\n");
	}
	return 0;
}

static int s900_dai_mode_unset(struct caninos_i2s_dev *dev)
{
	snd_dai_writel(dev, snd_dai_readl(dev, I2S_CTL) & ~0x3,
		           I2S_CTL);
	snd_dai_writel(dev, snd_dai_readl(dev, I2S_FIFOCTL) & ~0x3,
		           I2S_FIFOCTL);
	snd_dai_writel(dev, snd_dai_readl(dev, I2S_FIFOCTL) & ~(0x3 << 9),
		           I2S_FIFOCTL);
	return 0;
}

static void caninos_i2s_shutdown(struct snd_pcm_substream *substream,
                                 struct snd_soc_dai *dai)
{
	struct caninos_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	
	if (SNDRV_PCM_STREAM_CAPTURE == substream->stream)
	{
		s900_dai_mode_unset(dev);
		
		dev_info(dev->dev, "capture stream shutdown\n");
	}
	if (SNDRV_PCM_STREAM_PLAYBACK == substream->stream)
	{
		s900_dai_mode_unset(dev);
		
		dev_info(dev->dev, "playback stream shutdown\n");
	}
	
	if (dai->active) {
		return;
	}
	
	caninos_i2s_stop_clock(dev);
}

static int caninos_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
	struct snd_soc_dai *dai)
{
	struct caninos_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	
	dev_info(dev->dev, "i2s trigger called\n");
	
	switch (cmd)
	{
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		caninos_i2s_start_clock(dev);
		s900_dai_mode_unset(dev);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		s900_dai_mode_unset(dev);
		caninos_i2s_stop_clock(dev);
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}

static int caninos_i2s_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct caninos_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	
	if (dai->active) {
		return 0;
	}
	
	caninos_i2s_start_clock(dev);
	
	s900_dai_mode_unset(dev);
	
	dev_info(dev->dev, "i2s startup called\n");
	
	return 0;
}

static int caninos_i2s_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct caninos_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	
	dev_info(dev->dev, "i2s prepare called\n");
	
	s900_dai_mode_unset(dev);
	return 0;
}

static const struct snd_soc_dai_ops caninos_i2s_dai_ops = {
	.startup   = caninos_i2s_startup,
	.shutdown  = caninos_i2s_shutdown,
	.prepare   = caninos_i2s_prepare,
	.trigger   = caninos_i2s_trigger,
	.hw_params = caninos_i2s_hw_params,
	.hw_free   = caninos_i2s_hw_free,
};

static int caninos_i2s_dai_probe(struct snd_soc_dai *dai)
{
	struct caninos_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	
	dev_info(dev->dev, "i2s dai probe\n");
	
	snd_soc_dai_init_dma_data(dai,
		&dev->dma_data[SNDRV_PCM_STREAM_PLAYBACK],
		&dev->dma_data[SNDRV_PCM_STREAM_CAPTURE]);
	
	return 0;
}

struct snd_soc_dai_driver caninos_dai = {
	.name = "caninos-i2s",
	.probe = caninos_i2s_dai_probe,
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &caninos_i2s_dai_ops,
	.symmetric_rates = 1,
	.symmetric_samplebits = 1,
};

static const struct snd_soc_component_driver caninos_component = {
	.name = "caninos-i2s-comp",
};

static int caninos_i2s_probe(struct platform_device *pdev)
{
	struct caninos_i2s_dev *dev;
	struct resource *res;
	int ret;
	
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32); 
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	
	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	
	if (!dev)
	{
		dev_err(&pdev->dev, "could not alloc private i2s data.\n");
		return -ENOMEM;
	}
	
	dev->clk_prepared = false;
	
	dev->clk = devm_clk_get(&pdev->dev, "audio_pll");
	
	if (IS_ERR(dev->clk))
	{
		dev_err(&pdev->dev, "no audio clock defined.\n");
		return PTR_ERR(dev->clk);
	}
	
	dev->i2stx_clk = devm_clk_get(&pdev->dev, "i2stx");
	
	if (IS_ERR(dev->i2stx_clk))
	{
		dev_err(&pdev->dev, "no i2stx clock defined.\n");
		return PTR_ERR(dev->i2stx_clk);
	}
	
	dev->i2srx_clk = devm_clk_get(&pdev->dev, "i2srx");
	
	if (IS_ERR(dev->i2srx_clk))
	{
		dev_err(&pdev->dev, "no i2srx clock defined.\n");
		return PTR_ERR(dev->i2srx_clk);
	}
	
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "regs");
	
	if (!res)
	{
		dev_err(&pdev->dev, "could not get i2s memory resource.\n");
		return -ENODEV;
	}
	
	dev->i2s_reg_base = res->start;
	
	dev->base = devm_ioremap_resource(&pdev->dev, res);
	
	if (!dev->base)
	{
		dev_err(&pdev->dev, "unable to ioremap i2s resources.\n");
		return -ENXIO;
	}
	
	dev->dma_data[SNDRV_PCM_STREAM_PLAYBACK].addr =
		dev->i2s_reg_base + I2STX_DAT;
	dev->dma_data[SNDRV_PCM_STREAM_CAPTURE].addr =
		dev->i2s_reg_base + I2SRX_DAT;
	
	dev->dma_data[SNDRV_PCM_STREAM_PLAYBACK].addr_width =
		DMA_SLAVE_BUSWIDTH_4_BYTES;
	dev->dma_data[SNDRV_PCM_STREAM_CAPTURE].addr_width =
		DMA_SLAVE_BUSWIDTH_4_BYTES;
		
	dev->dma_data[SNDRV_PCM_STREAM_PLAYBACK].maxburst = 2;
	dev->dma_data[SNDRV_PCM_STREAM_CAPTURE].maxburst = 2;
	
	dev->dma_data[SNDRV_PCM_STREAM_PLAYBACK].flags =
		SND_DMAENGINE_PCM_DAI_FLAG_PACK;
	dev->dma_data[SNDRV_PCM_STREAM_CAPTURE].flags =
		SND_DMAENGINE_PCM_DAI_FLAG_PACK;
	
	dev->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, dev);
	
	ret = devm_snd_soc_register_component(&pdev->dev, 
		&caninos_component, &caninos_dai, 1);
	
	if (ret)
	{
		dev_err(&pdev->dev, "could not register dai.\n");
		return ret;
	}
	
	ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL, 0);
	
	if (ret)
	{
		dev_err(&pdev->dev, "could not register pcm.\n");
		return ret;
	}
	
	return 0;
}

static const struct of_device_id caninos_i2s_of_match[] = {
	{.compatible = "caninos,k7-audio-i2s",},
	{},
};
MODULE_DEVICE_TABLE(of, caninos_i2s_of_match);

static struct platform_driver caninos_i2s_driver = {
	.probe  = caninos_i2s_probe,
	.driver = {
		.name  = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = caninos_i2s_of_match,
	},
};

module_platform_driver(caninos_i2s_driver);

MODULE_LICENSE("GPL");

