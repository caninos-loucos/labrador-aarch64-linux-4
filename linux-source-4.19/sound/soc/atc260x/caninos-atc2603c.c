#include <linux/module.h>
#include <linux/mfd/atc260x/atc260x.h>
#include <sound/soc.h>
#include <linux/dma-mapping.h>
#include "atc2603c-audio-regs.h"

#define AUDIOINOUT_CTL    (0x0)
#define AUDIO_DEBUGOUTCTL (0x1)
#define DAC_DIGITALCTL    (0x2)
#define DAC_VOLUMECTL0    (0x3)
#define DAC_ANALOG0       (0x4)
#define DAC_ANALOG1       (0x5)
#define DAC_ANALOG2       (0x6)
#define DAC_ANALOG3       (0x7)
#define ADC_DIGITALCTL    (0x8)
#define ADC_HPFCTL        (0x9)
#define ADC_CTL           (0xa)
#define AGC_CTL0          (0xb)
#define AGC_CTL1          (0xc)
#define AGC_CTL2          (0xd)
#define ADC_ANALOG0       (0xe)
#define ADC_ANALOG1       (0xf)

/* codec private data */
struct atc2603c_priv
{
	struct device *dev;
	struct atc260x_dev *pmic;
	bool hwinit;
	int opencount;
};

static int atc2603c_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai);

static int snd_soc_update_bits_pmic(struct atc2603c_priv *dev,
	unsigned short reg, unsigned int mask, unsigned int value)
{
	unsigned int old, new;
	bool change;
	int ret;
	
	ret = atc260x_reg_read(dev->pmic, reg);
	
	if (ret < 0) {
		return ret;
	}
	
	old = ret;
	new = (old & ~mask) | (value & mask);
	change = old != new;
	
	if (change) {
		ret = atc260x_reg_write(dev->pmic, reg, new);
	}
	
	if (ret < 0) {
		return ret;
	}
	return change;
}

static int snd_soc_update_bits(struct atc2603c_priv *dev, unsigned short reg,
	unsigned int mask, unsigned int value)
{
	return snd_soc_update_bits_pmic(dev,
		reg + ATC2603C_AUDIO_OUT_BASE, mask, value);
}

static int snd_soc_write(struct atc2603c_priv *dev, u16 reg, u16 val)
{
	int ret;
	reg += ATC2603C_AUDIO_OUT_BASE;
	return atc260x_reg_write(dev->pmic, reg, val);
}



static int atc2603c_playback_set_controls(struct atc2603c_priv *dev)
{
	dev_info(dev->dev, "playback set controls\n");

	/* unmute output */
    snd_soc_update_bits(dev, DAC_ANALOG3, 0x1 << DAC_ANALOG3_PAEN_FR_FL_SFT, 0x01 << DAC_ANALOG3_PAEN_FR_FL_SFT);
    
    /* enable pa */
    snd_soc_update_bits(dev, DAC_ANALOG3, 0x1 << DAC_ANALOG3_PAOSEN_FR_FL_SFT, 0x01 << DAC_ANALOG3_PAOSEN_FR_FL_SFT);
    
	/* dacfl and dacfr enable */
    snd_soc_update_bits(dev, DAC_DIGITALCTL, 0x03 << DAC_DIGITALCTL_DEFL_SFT, 0x03 << DAC_DIGITALCTL_DEFL_SFT);
    
    /* 1.6Vpp swing */
    snd_soc_update_bits(dev, DAC_ANALOG1, 0x01 << DAC_ANALOG1_PASW_SFT, 0x01 << DAC_ANALOG1_PASW_SFT);
    
    snd_soc_update_bits(dev, DAC_ANALOG3, 0x03 << 0, 0x03);
    return 0;
}

static int atc2603c_capture_set_controls(struct atc2603c_priv *dev)
{
	dev_info(dev->dev, "capture set controls\n");
	
	/* unmute mic */
    snd_soc_update_bits(dev, DAC_ANALOG1, 0x1 << DAC_ANALOG1_DACMICMUTE_SFT, 0x0 << DAC_ANALOG1_DACMICMUTE_SFT);
    
    /* mute fm */
    snd_soc_update_bits(dev, DAC_ANALOG1, 0x1 << DAC_ANALOG1_DACFMMUTE_SFT, 0);
    
    snd_soc_update_bits(dev, AGC_CTL0, 0x1 << AGC_CTL0_VMICINEN_SFT, 0x1 << AGC_CTL0_VMICINEN_SFT);
    snd_soc_update_bits(dev, AGC_CTL0, 0x1 << AGC_CTL0_VMICEXEN_SFT, 0x1 << AGC_CTL0_VMICEXEN_SFT);
    snd_soc_update_bits(dev, AGC_CTL0, 0x3 << AGC_CTL0_VMICEXST_SFT, 0x1 << AGC_CTL0_VMICEXST_SFT);
    
    snd_soc_update_bits(dev, ADC_CTL, 0x1 << ADC_CTL_MIC0FDSE_SFT, 0x0 << ADC_CTL_MIC0FDSE_SFT);
    
    //snd_soc_update_bits_pmic(dev, ATC2603C_MFP_CTL, 0x3<<9 , 0x2<<9); //MICINL&MICINR
   // snd_soc_update_bits_pmic(dev, ATC2603C_MFP_CTL, 0x01 , 0x01); //MICINL&MICINR
    return 0;
}

static void atc2603c_pa_down(struct atc2603c_priv *dev)
{
	/* delay for antipop before pa down */
	/* close PA OUTPSTAGE EN */
	snd_soc_update_bits(dev, DAC_ANALOG3, 0x1<<3, 0x0);
	mdelay(100);
	
	if (dev->hwinit == true)
	{
		//snd_soc_write(dev, DAC_VOLUMECTL0, 0);
		dev->hwinit = false;
	}
}





static int atc2603c_hw_free
	(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct atc2603c_priv *dev = dev_get_drvdata(dai->dev);
	
	dev_info(dev->dev, "hw free called\n");
	
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
		/* disable i2s input (clear bit 1) */
		snd_soc_update_bits(dev, AUDIOINOUT_CTL, (0x01 << 1), 0);
	}
	else
	{
		/* disable i2s output (clear bit 8) */ 
		snd_soc_update_bits(dev, AUDIOINOUT_CTL, (0x01 << 8), 0);
	}
	
	if(dev->opencount > 0) {
    	dev->opencount--;
	}
	return 0;
}

static int atc2603c_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct atc2603c_priv *dev = dev_get_drvdata(dai->dev);
	
	dev_info(dev->dev, "set dai fmt called\n");
	
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK)
	{
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}
	
	switch (fmt & SND_SOC_DAIFMT_INV_MASK)
	{
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		return -EINVAL;
	}
	
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK)
	{
	case SND_SOC_DAIFMT_I2S:
		break;
	default:
		return -EINVAL;
	}
	
	/* set output sample rate as MCLK/4 */
	snd_soc_update_bits(dev, DAC_DIGITALCTL, (0x3 << 4), (0x2 << 4));
	
	dev_info(dev->dev, "set dai fmt ok\n");
	return 0;
}

static int atc2603c_set_dai_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct atc2603c_priv *dev = dev_get_drvdata(dai->dev);
	
	dev_info(dev->dev, "set digital mute called\n");
	
	if (mute) {
		snd_soc_update_bits(dev, DAC_ANALOG1, DAC_ANALOG1_DACFL_FRMUTE, 0);
	}
	else {
		snd_soc_update_bits(dev, DAC_ANALOG1, DAC_ANALOG1_DACFL_FRMUTE,
		                    DAC_ANALOG1_DACFL_FRMUTE);
	}
	return 0;
}

static const struct snd_soc_dai_ops atc2603c_dai_ops = {
	.hw_params = atc2603c_hw_params,
	.hw_free = atc2603c_hw_free,
	.set_fmt = atc2603c_set_dai_fmt,
	.digital_mute = atc2603c_set_dai_digital_mute,
};

static struct snd_soc_dai_driver atc2603c_dai = {
	.name = "atc2603c-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops = &atc2603c_dai_ops,
};

static const struct snd_kcontrol_new atc2603c_snd_controls[] = {
	SOC_SINGLE_TLV("AMP1 Gain boost Range select",
		AGC_CTL0,
		AGC_CTL0_AMP0GR1_SET,
		0x7,
		0,
		NULL),
	SOC_SINGLE_TLV("ADC0 Digital Gain control",
		ADC_DIGITALCTL,
		ADC_DIGITALCTL_ADGC0_SFT,
		0xF,
		0,
		NULL),
	SOC_SINGLE_TLV("DAC PA Volume",
		DAC_ANALOG1,
		DAC_ANALOG1_VOLUME_SFT,
		0x28,
		0,
		NULL),
	SOC_SINGLE_TLV("DAC FL Gain",
		DAC_VOLUMECTL0,
		DAC_VOLUMECTL0_DACFL_VOLUME_SFT,
		0xFF,
		0,
		NULL),
	SOC_SINGLE_TLV("DAC FR Gain",
		DAC_VOLUMECTL0,
		DAC_VOLUMECTL0_DACFR_VOLUME_SFT,
		0xFF,
		0,
		NULL),
	SOC_DOUBLE("DAC Digital FL FR Switch",
		DAC_DIGITALCTL,
		DAC_DIGITALCTL_DEFL_SFT,
		DAC_DIGITALCTL_DEFR_SFT,
		1,
		0),
};

static const struct snd_soc_dapm_widget atc2603c_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("MICR"),
	SND_SOC_DAPM_INPUT("MICL"),
	SND_SOC_DAPM_OUTPUT("HOUTR"),
	SND_SOC_DAPM_OUTPUT("HOUTL"),
};

static const struct snd_soc_dapm_route atc2603c_dapm_routes[] = {
	{ "Capture", NULL, "MICR" },
	{ "Capture", NULL, "MICL" },
	{ "HOUTR", NULL, "Playback" },
	{ "HOUTL", NULL, "Playback" },
};

static unsigned int comp_read(struct snd_soc_component *comp, unsigned int reg)
{
	struct atc2603c_priv *dev = dev_get_drvdata(comp->dev);
	int ret = 0;
	
	reg += ATC2603C_AUDIO_OUT_BASE;
	
	ret = atc260x_reg_read(dev->pmic, reg);
	
	if (ret < 0)
	{
	   dev_err(dev->dev, "read reg = %x, ret = %x failed\n", reg, ret);
	   return 0;
	}
	else
	{
		ret = (unsigned int)(ret) & 0xFFFF;
		dev_info(dev->dev, "read reg = %x, ret = %x ok\n", reg, ret);
	}
	
	return ret;
}

static int comp_write
	(struct snd_soc_component *comp, unsigned int reg, unsigned int val)
{
	struct atc2603c_priv *dev = dev_get_drvdata(comp->dev);
	int ret = 0;
	
	reg += ATC2603C_AUDIO_OUT_BASE;
	ret = atc260x_reg_write(dev->pmic, reg, val);
	
	if (ret < 0) {
	   dev_err(dev->dev, "write reg = %x, val = %x failed\n", reg, val);
	}
	else
	{
		dev_info(dev->dev, "write reg = %x, val = %x ok\n", reg, val);
	}
	
	return ret;
}









static int reset_audio_codec(struct atc2603c_priv *dev)
{
	int ret;
	
	dev_info(dev->dev, "reset audio codec called\n");
	
	/* audio block reset */
	ret = snd_soc_update_bits_pmic(dev, ATC2603C_CMU_DEVRST, 0x1 << 4, 0);
	
	if (ret < 0)
	{
		dev_err(dev->dev, "could not reset pmic audio block.\n");
		return ret;
	}
	
	/* SCLK to Audio Clock Enable Control */
	ret = snd_soc_update_bits_pmic(dev, ATC2603C_CMU_DEVRST,
	                               (0x1 << 10) | (0x1 << 4),
	                               (0x1 << 10) | (0x1 << 4));
	if (ret < 0)
	{
		dev_err(dev->dev, "could not enable audio block clock.\n");
		return ret;
	}
	return 0;
}

static int setup_audio_codec(struct atc2603c_priv *dev)
{
	int ret;
	
	dev_info(dev->dev, "setup audio codec called\n");
	
	/*
	enable P_EXTIRQ pad    (set bit 0)
	enable P_I2S_DOUT pad  (set bit 1)
	enable P_I2S_DIN pad   (set bit 4)
	enable P_I2S_LRCLK pad (set bit 5)
	enable P_I2S_MCLK pad  (set bit 6)
	*/
	ret = snd_soc_update_bits_pmic(dev, ATC2603C_PAD_EN,
	                               (0x1 << 0) | (0x1 << 1) | (0x1 << 4)  |
	                               (0x1 << 5) | (0x1 << 6),
	                               (0x1 << 0) | (0x1 << 1) | (0x1 << 4)  |
	                               (0x1 << 5) | (0x1 << 6));
	if (ret < 0)
	{
		dev_err(dev->dev, "could not configure codec pads.\n");
		return ret;
	}
	
	/*
	enable differential mic input pads
	choose MICINLP (set bits 1~0 to 01b)
	choose MICINLN (set bits 12~11 to 01b)
	*/
	ret = snd_soc_update_bits_pmic(dev, ATC2603C_PAD_EN, (0x3 << 11) |
	                               (0x3 << 0), (0x1 << 11) | (0x1 << 0));
	
	if (ret < 0)
	{
		dev_err(dev->dev, "could not configure mfp.\n");
		return ret;
	}
	
	/*
	disable i2s output (clear bit 8)
	disable i2s input  (clear bit 1)
	*/
	ret = snd_soc_update_bits(dev, AUDIOINOUT_CTL, (0x1 << 8) | (0x1 << 1), 0);
	
	if (ret < 0)
	{
		dev_err(dev->dev, "could not disable i2s output/input.\n");
		return ret;
	}
	
	/* select i2s 4-wire mode (set bits 6~5 to 0x1) */
	ret = snd_soc_update_bits(dev, AUDIOINOUT_CTL, (0x3 << 5), (0x1 << 5));
	
	if (ret < 0)
	{
		dev_err(dev->dev, "could not select i2s 4-wire mode.\n");
		return ret;
	}
	
	/*
	bit4:0 should not be changed, otherwise VREF isn't accurate
	this is not docummented!!! not sure what it does..
	*/
	snd_soc_update_bits_pmic(dev, ATC2603C_PMU_BDG_CTL, (0x1 << 6), (0x1 << 6));
	snd_soc_update_bits_pmic(dev, ATC2603C_PMU_BDG_CTL, (0x1 << 5), (0x0 << 5));
	
	/* set volume of DAC-FL (bits 7~0) and DAC-FR (bits 15~8) to +0db (0xbf) */
	ret = snd_soc_write(dev, DAC_VOLUMECTL0, 0xbfbf);
	
	if (ret < 0)
	{
		dev_err(dev->dev, "could not set dac output volume.\n");
		return ret;
	}
	
	/* setup DAC PA BIAS and disable karaoke mode */
	ret = snd_soc_write(dev, DAC_ANALOG0, 0x26b3);
	
	if (ret < 0)
	{
		dev_err(dev->dev, "could not setup dac analog0 register.\n");
		return ret;
	}
	
	/*
	MIC mute                      (clear bit 15)
	FM mute                       (clear bit 14)
	DAC FL&FR playback mute       (clear bit 10)
	set headphone PA VOLUME=0     (clear bits 5~0)
	set PA output swing to 1.6Vpp (set bit 6)
	*/
	ret = snd_soc_write(dev, DAC_ANALOG1, (0x1 << 6));
	
	if (ret < 0)
	{
		dev_err(dev->dev, "could not setup dac analog1 register.\n");
		return ret;
	}
	
	/* set internal DAC_OPVRO output stage IQ */
	ret = snd_soc_write(dev, DAC_ANALOG2, 0x3);
	
	if (ret < 0)
	{
		dev_err(dev->dev, "could not setup dac analog2 register.\n");
		return ret;
	}
	
	/*
	enable FR DAC       (set bit 0)
	enable FRL DAC      (set bit 1)
	enable FR and FL PA (set bit 2)
	enable output stage (set bit 3)
	enable all bias
	*/
	ret = snd_soc_write(dev, DAC_ANALOG3, 0x4bf);
	
	if (ret < 0)
	{
		dev_err(dev->dev, "could not setup dac analog3 register.\n");
		return ret;
	}
	
	/* enable DAC_OPVRO (set bit 4) */
	ret = snd_soc_update_bits(dev, DAC_ANALOG2 , (0x1 << 4), (0x1 << 4));
	
	if (ret < 0)
	{
		dev_err(dev->dev, "could not enable DAC_OPVRO.\n");
		return ret;
	}
	
	/* enable volume change delay (set bit 13) */
	ret = snd_soc_update_bits(dev, DAC_ANALOG3, (0x1 << 13), (0x1 << 13));
	
	if (ret < 0)
	{
		dev_err(dev->dev, "could not enable volume change delay.\n");
		return ret;
	}
	
	
	snd_soc_update_bits(dev, DAC_ANALOG1, DAC_ANALOG1_PASW, DAC_ANALOG1_PASW);
	snd_soc_update_bits(dev, DAC_DIGITALCTL, 0x3<<4, 0x2<<4);
	snd_soc_update_bits(dev, DAC_ANALOG1, 0x3f, 40);
	
	snd_soc_write(dev, DAC_VOLUMECTL0, 0xbebe);
	
	snd_soc_update_bits(dev, ADC_CTL, 0x1 << 6, 0x1 << 6);
	snd_soc_update_bits(dev, ADC_CTL, 0x1 << 7, 0x1 << 7);
	
	snd_soc_update_bits(dev, AUDIOINOUT_CTL, 0x1 << 10, 0x1 << 10);
	
	/*  External MIC Power VMIC enabled */
	snd_soc_update_bits(dev, AGC_CTL0, 0x1 << 6, 0x1 << 6);
	snd_soc_update_bits(dev, AGC_CTL0, 0x1 << 7, 0x1 << 7);
	snd_soc_update_bits(dev, ADC_ANALOG1, 0x1 << 8, 0x1 << 8);
	
	/* set capture params */
	snd_soc_update_bits(dev, AGC_CTL0, 0xf << AGC_CTL0_AMP1G0L_SFT, 0xd << AGC_CTL0_AMP1G0L_SFT);
    snd_soc_update_bits(dev, AGC_CTL0, 0xf << AGC_CTL0_AMP1G0R_SFT, 0xd << AGC_CTL0_AMP1G0R_SFT);
    snd_soc_update_bits(dev, ADC_DIGITALCTL, 0xF << ADC_DIGITALCTL_ADGC0_SFT, 0x0 << ADC_DIGITALCTL_ADGC0_SFT);
    snd_soc_update_bits(dev, AGC_CTL0, 0x7 << AGC_CTL0_AMP0GR1_SET, 0x0 << AGC_CTL0_AMP0GR1_SET);
    snd_soc_update_bits(dev, ADC_CTL, 0x1f << 0, 0x2 << 0);
    
	/* set playback params */
	snd_soc_update_bits(dev, DAC_ANALOG1, 0x3f << DAC_ANALOG1_VOLUME_SFT, 0x28 << DAC_ANALOG1_VOLUME_SFT);
    snd_soc_update_bits(dev, DAC_VOLUMECTL0, 0xff << DAC_VOLUMECTL0_DACFL_VOLUME_SFT, 0xbe << DAC_VOLUMECTL0_DACFL_VOLUME_SFT);
    snd_soc_update_bits(dev, DAC_VOLUMECTL0, 0xff << DAC_VOLUMECTL0_DACFR_VOLUME_SFT, 0xbe << DAC_VOLUMECTL0_DACFR_VOLUME_SFT);

	return 0;
}

static int atc2603c_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct atc2603c_priv *dev = dev_get_drvdata(dai->dev);
	
	if (dev->hwinit == false)
	{
		reset_audio_codec(dev);
		setup_audio_codec(dev);
		dev->hwinit = true;
	}
	
	dev_info(dev->dev, "hw params called\n");
	
	/* 4WIRE MODE */
	snd_soc_update_bits(dev, AUDIOINOUT_CTL, 0x03 << 5, 0x01 << 5);
	
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
		/* enable i2s input (set bit 0) */
		snd_soc_update_bits(dev, AUDIOINOUT_CTL, (0x1 << 1), (0x1 << 1));
		
		/* DAC FL&FR playback mute */
		snd_soc_update_bits(dev, DAC_ANALOG1, 0x1<<10, 0x0);
		
		/* DAC FL&FR ANOLOG enable */
		snd_soc_update_bits(dev, DAC_ANALOG3, 0x03, 0x03);
		
        atc2603c_playback_set_controls(dev);
	}
	else
	{
		snd_soc_update_bits(dev, ADC_HPFCTL, 0x3 << 0, 0x3 << 0);
		
		/* enable i2s output (set bit 8) */ 
		snd_soc_update_bits(dev, AUDIOINOUT_CTL, (0x1 << 8), (0x1 << 8));
		
        atc2603c_capture_set_controls(dev);
	}
	
	dev->opencount++;
	return 0;
}

static int atc2603c_probe(struct snd_soc_component *comp)
{
	struct atc2603c_priv *dev = dev_get_drvdata(comp->dev);
	
	dev_info(comp->dev, "atc2603c probe called\n");
	
	reset_audio_codec(dev);
	setup_audio_codec(dev);
	
	dev->hwinit = false;
	dev->opencount = 0;
	
	snd_soc_update_bits(dev, AUDIOINOUT_CTL, 0x1 << 2, 0);
	snd_soc_update_bits(dev, AUDIOINOUT_CTL, 0x1 << 3, 0);
	return 0;
}

static void atc2603c_remove(struct snd_soc_component *comp)
{
	struct atc2603c_priv *dev = dev_get_drvdata(comp->dev);
	//atc2603c_pa_down(dev);
}




static const struct snd_soc_component_driver atc2603c_component = {
	.probe = atc2603c_probe,
	.remove = atc2603c_remove,
	
	.controls = atc2603c_snd_controls,
	.num_controls = ARRAY_SIZE(atc2603c_snd_controls),
	
	.dapm_widgets = atc2603c_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(atc2603c_dapm_widgets),
	
	.dapm_routes = atc2603c_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(atc2603c_dapm_routes),
	
	.read = comp_read,
	.write = comp_write,
	
	.non_legacy_dai_naming = 1,
};

static int atc2603c_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct atc2603c_priv *atc2603c;
	
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32); 
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	
	atc2603c = devm_kzalloc(dev, sizeof(*atc2603c), GFP_KERNEL);
	
	if (atc2603c == NULL)
	{
		dev_err(dev, "could not alloc private codec data.\n");
		return -ENOMEM;
	}
	
	atc2603c->dev = dev;
	atc2603c->pmic = dev_get_drvdata(dev->parent);
	
	pdev->dev.init_name = "atc260c-audio";
	
	dev_set_drvdata(dev, atc2603c);
	
	return devm_snd_soc_register_component(dev, &atc2603c_component, &atc2603c_dai, 1);
}

static int atc2603c_platform_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id atc2603c_audio_of_match[] = {
	{.compatible = "actions,atc2603c-audio",},
	{}
};
MODULE_DEVICE_TABLE(of, atc2603c_audio_of_match);

static struct platform_driver atc2603c_platform_driver = {
	.probe = atc2603c_platform_probe,
	.remove = atc2603c_platform_remove,
	.driver = {
		.name = "atc2603c-audio",
		.owner = THIS_MODULE,
		.of_match_table = atc2603c_audio_of_match,
	},
};

static int __init atc2603c_init(void)
{
	return platform_driver_register(&atc2603c_platform_driver);
}

static void __exit atc2603c_exit(void)
{
	platform_driver_unregister(&atc2603c_platform_driver);
}

module_init(atc2603c_init);
module_exit(atc2603c_exit);

MODULE_DESCRIPTION("ATC2603C audio CODEC driver");
MODULE_LICENSE("GPL");

