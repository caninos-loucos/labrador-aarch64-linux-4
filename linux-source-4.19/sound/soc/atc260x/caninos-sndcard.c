#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/time.h>
#include <linux/delay.h>

#include <linux/mfd/atc260x/atc260x.h>

#define DRIVER_NAME "caninos-snd-card"

static int caninos_hw_params(struct snd_pcm_substream *substream,
                             struct snd_pcm_hw_params *params)
{
	return 0;
}

static int dailink_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static struct snd_soc_ops dailink_ops = {
	.hw_params = caninos_hw_params,
};

static struct snd_soc_dai_link caninos_dailink = {
	.name = "ATC2603C",
	.stream_name = "ATC2603C",
	.platform_name = DRIVER_NAME,
	.cpu_dai_name = "caninos-i2s",
	.codec_dai_name = "atc2603c-hifi",
	.codec_name = "atc260c-audio",
	.dai_fmt = SND_SOC_DAIFMT_I2S 
		| SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS,
	.init = dailink_init,
	.ops = &dailink_ops,
};

static const struct snd_soc_dapm_widget caninos_widgets[] = {
	SND_SOC_DAPM_HP("Headphones", NULL),
	SND_SOC_DAPM_MIC("IntMic", NULL),
};

static const struct snd_soc_dapm_route caninos_routes[] = {
	{"Headphones", NULL, "HOUTL"},
	{"Headphones", NULL, "HOUTR"},
	{"MICL", NULL, "IntMic"},
	{"MICR", NULL, "IntMic"},
};

static const struct snd_kcontrol_new caninos_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphones"),
	SOC_DAPM_PIN_SWITCH("IntMic"),
};

static struct snd_soc_card snd_soc_caninos = {
	.name = "sndcard",
	.owner = THIS_MODULE,
	.dai_link = &caninos_dailink,
	.num_links = 1,
	.dapm_widgets = caninos_widgets,
	.num_dapm_widgets = ARRAY_SIZE(caninos_widgets),
	.dapm_routes = caninos_routes,
	.num_dapm_routes = ARRAY_SIZE(caninos_routes),
	.controls = caninos_controls,
	.num_controls = ARRAY_SIZE(caninos_controls),
};

static const struct of_device_id caninos_snd_card_of_match[] = {
	{.compatible = "caninos,snd-card",},
	{},
};
MODULE_DEVICE_TABLE(of, caninos_snd_card_of_match);

static int caninos_snd_card_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	int ret;
	
	card = &snd_soc_caninos;
	snd_soc_caninos.dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	
	ret = devm_snd_soc_register_card(&pdev->dev, &snd_soc_caninos);
	
	if (ret)
	{
		dev_err(&pdev->dev, "snd card register failed\n");
		return ret;
	}
	return 0;
}

static struct platform_driver caninos_snd_card = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = caninos_snd_card_of_match,
	},
	.probe = caninos_snd_card_probe,
};

module_platform_driver(caninos_snd_card);

MODULE_LICENSE("GPL");

