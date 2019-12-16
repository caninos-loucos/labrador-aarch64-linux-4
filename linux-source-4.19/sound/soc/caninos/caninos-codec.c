/*
 * Copyright (c) 2019 LSI-TEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 * 
 * Copyright (C) 2009 Actions Semi Inc 
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
#include <sound/soc.h>
#include <linux/dma-mapping.h>
#include "atc2603c-audio-regs.h"
#include "caninos-codec.h"

#define AUDIOINOUT_CTL    0x0
#define AUDIO_DEBUGOUTCTL 0x1
#define DAC_DIGITALCTL    0x2
#define DAC_VOLUMECTL0    0x3
#define DAC_ANALOG0       0x4
#define DAC_ANALOG1       0x5
#define DAC_ANALOG2       0x6
#define DAC_ANALOG3       0x7
#define ADC_DIGITALCTL    0x8
#define ADC_HPFCTL        0x9
#define ADC_CTL           0xa
#define AGC_CTL0          0xb
#define AGC_CTL1          0xc
#define AGC_CTL2          0xd
#define ADC_ANALOG0       0xe
#define ADC_ANALOG1       0xf

static int adc_num = 2;

struct codec_local_data
{
	struct atc260x_dev *pmic;
	struct atc260x_audio *codec;
	struct delayed_work dwork;
	bool hw_init_flag;
	int playback_volume;
	spinlock_t lock;
};

static struct codec_local_data codec_local_data = {
	.pmic = NULL,
	.codec = NULL,
	.hw_init_flag = false,
	.playback_volume = 0,
};

static int pmu_update_bits(int reg, u16 mask, u16 value)
{
	struct atc260x_dev *pmic = codec_local_data.pmic;
	u16 reg_value, new_value;
	int ret;
	
	if (!pmic) {
		return -ENODEV;
	}
	
	ret = atc260x_reg_read(pmic, reg);
	
	if (ret < 0) {
		return ret;
	}
	
	reg_value = ret & 0xFFFF;
	new_value = (reg_value & ~mask) | (value & mask);
	
	if (reg_value != new_value)
	{
		ret = atc260x_reg_write(pmic, reg, new_value);
		
		if (ret < 0) {
			return ret;
		}
	}
	return 0;
}

static int codec_write(int reg, u16 value)
{
	struct atc260x_dev *pmic = codec_local_data.pmic;
	
	if (pmic) {
		return atc260x_reg_write(pmic, ATC2603C_AUDIO_OUT_BASE + reg, value);
	}
	else {
		return -ENODEV;
	}
}

static int codec_read(int reg)
{
	struct atc260x_dev *pmic = codec_local_data.pmic;
	
	if (pmic) {
		return atc260x_reg_read(pmic, ATC2603C_AUDIO_OUT_BASE + reg);
	}
	else {
		return -ENODEV;
	}
}

static int codec_update_bits(int reg, u16 mask, u16 value)
{
	return pmu_update_bits(ATC2603C_AUDIO_OUT_BASE + reg, mask, value);
}

static void reenable_audio_block(struct atc260x_audio *codec)
{
	/* audio block reset */
	pmu_update_bits(ATC2603C_CMU_DEVRST, 0x1 << 4, 0);
	/* SCLK to Audio Clock Enable Control */
	pmu_update_bits(ATC2603C_CMU_DEVRST, 0x1 << 10, 0x1 << 10);
	pmu_update_bits(ATC2603C_CMU_DEVRST, 0x1 << 4, 0x1 << 4);
}

static int atc2603c_dac_playback_mute(int mute)
{
	codec_update_bits(DAC_ANALOG1, 0x1<<10, 0x0);
	
	if (!mute) {
		codec_update_bits(DAC_ANALOG1, 0x1<<10, 0x1<<10);
	}
	
	return 0;
} 

static int atc2603c_playback_set_controls(void)
{
    codec_update_bits(DAC_ANALOG3, 0x1 << DAC_ANALOG3_PAEN_FR_FL_SFT, 0x01 << DAC_ANALOG3_PAEN_FR_FL_SFT);
    codec_update_bits(DAC_ANALOG3, 0x1 << DAC_ANALOG3_PAOSEN_FR_FL_SFT, 0x01 << DAC_ANALOG3_PAOSEN_FR_FL_SFT);
    codec_update_bits(DAC_DIGITALCTL, 0x03 << DAC_DIGITALCTL_DEFL_SFT, 0x03 << DAC_DIGITALCTL_DEFL_SFT);
    codec_update_bits(DAC_ANALOG1, 0x01 << DAC_ANALOG1_PASW_SFT, 0x01 << DAC_ANALOG1_PASW_SFT);
    codec_update_bits(DAC_ANALOG3, 0x03 << 0, 0x03);
    
	/* I2S input en */
	codec_update_bits(AUDIOINOUT_CTL, 0x01 << 1, 0x01 << 1);
	
	/* DAC FL&FR playback mute */
	codec_update_bits(DAC_ANALOG1, 0x1<<10, 0x0);
	
	/* DAC FL&FR ANALOG enable */
	codec_update_bits(DAC_ANALOG3, 0x03, 0x03);
    
    return 0;
}

static int atc2603c_capture_set_controls(void)
{
	/* MIC mute (clear bit 15) */
	codec_update_bits(DAC_ANALOG1, 0x1 << 15, 0x0 << 15);
	/* FM mute (clear bit 14) */
	codec_update_bits(DAC_ANALOG1, 0x1 << 14, 0x0 << 14);
	
	codec_write(ADC_DIGITALCTL, 0);
	codec_write(ADC_HPFCTL, 0);
	
	codec_update_bits(ADC_CTL, 0x3 << 13, 0x0 << 13);
	
	codec_update_bits(ADC_CTL, 0x1f << 0, 0x1b << 0);
	
	/* Set MIC to fully differential mode (clear bit 5) */
	codec_update_bits(ADC_CTL, 0x1 << 5, 0x0 << 5);
	
	/* Enable MIC0 input L Channel (set bit 7) */
	codec_update_bits(ADC_CTL, 0x1 << 7, 0x1 << 7);
	
	/* Enable MIC0 input R Channel (set bit 6) */
	codec_update_bits(ADC_CTL, 0x1 << 6, 0x1 << 6);
	
	/* Enable ADC0 input R Channel (set bit 3) */
	codec_update_bits(ADC_CTL, 0x1 << 3, 0x1 << 3);
	
	/* Mix ADC0 R and L channels (set bit 11) */
	codec_update_bits(ADC_DIGITALCTL, 0x1 << 11, 0x1 << 11);
	
	/* Disable Internal MIC Power Controlled by MIC Plug (clear bit 7) */
	codec_update_bits(AGC_CTL0, 0x1 << 5, 0x0 << 5);
	/* Enable Internal MIC Power */
	codec_update_bits(AGC_CTL0, 0x1 << 3, 0x1 << 3);
	/* Enable VMICEXT */
	codec_update_bits(AGC_CTL0, 0x1 << 6, 0x1 << 6);
	/* Set voltage to 2.9V */
	codec_update_bits(AGC_CTL0, 0x3 << 4, 0x1 << 4);
	
	/* Set FMINL pin to MICINLP function (pin 63) */
	pmu_update_bits(ATC2603C_MFP_CTL, 0x3 << 9 , 0x2 << 9);
	
	/* Set MICINL pin to MICINLN function (pin 58) */
	pmu_update_bits(ATC2603C_MFP_CTL, 0x3 << 0 , 0x1 << 0);
	
	/* I2S output en */
	codec_update_bits(AUDIOINOUT_CTL, 0x1 << 8, 0x1 << 8);
	
	return 0;
}

static void ramp_undirect(struct atc260x_audio *codec, u32 begv, u32 endv)
{
	u32 val = 0;
	int count = 0;
	
	while (endv < begv)
	{
		count++;
		val = codec->soc->soc_readl(codec->soc, I2S_FIFOCTL);
		
		while ((val & (0x1 << 8)) != 0) {
			val = codec->soc->soc_readl(codec->soc, I2S_FIFOCTL);
		}
		
		codec->soc->soc_writel(codec->soc, endv, I2STX_DAT);
		endv -= 0x36000;
	}
	while (begv <= endv)
	{
		count++;
		val = codec->soc->soc_readl(codec->soc, I2S_FIFOCTL);
		
		while ((val & (0x1 << 8)) != 0) {
			val = codec->soc->soc_readl(codec->soc, I2S_FIFOCTL);
		}
		
		codec->soc->soc_writel(codec->soc, endv, I2STX_DAT);
		endv -= 0x36000;
	}
}

static void pa_up(struct atc260x_audio *codec)
{
	/* i2s rx/tx fifo en */
	codec->soc->soc_writel(codec->soc, codec->soc->soc_readl(codec->soc, I2S_FIFOCTL) | 0x160b, I2S_FIFOCTL);
	
	/* 4-wire mode, i2s rx clk select I2S_CLK1, i2s rx/tx en */
	codec->soc->soc_writel(codec->soc, codec->soc->soc_readl(codec->soc, I2S_CTL) | 0xc03, I2S_CTL);
	
	/* DAC FL&FR volume contrl */
	codec_write(DAC_VOLUMECTL0, 0xbebe);
	codec_write(DAC_ANALOG0, 0);
	codec_write(DAC_ANALOG1, 0);
	codec_write(DAC_ANALOG2, 0);
	codec_write(DAC_ANALOG3, 0);
	
	/* 2.0-Channel Mode */
	codec->soc->soc_writel(codec->soc, codec->soc->soc_readl(codec->soc, I2S_CTL) & ~(0x7 << 4), I2S_CTL);
	
	/* I2S input en */
	codec_update_bits(AUDIOINOUT_CTL, 0x01 << 1, 0x01 << 1);
	/* 4WIRE MODE */
	codec_update_bits(AUDIOINOUT_CTL, 0x03 << 5, 0x01 << 5);
	/* DAC FL&FR digital enable */
	codec_update_bits(DAC_DIGITALCTL, 0x3, 0x3);
	
	/* DAC PA BIAS */
	codec_write(DAC_ANALOG0, 0x26b3);
	/* */
	codec_write(DAC_ANALOG2, 0x03);
	/* MIC mute */
	codec_update_bits(DAC_ANALOG1, 0x1<<15, 0);
	/* FM mute */
	codec_update_bits(DAC_ANALOG1, 0x1<<14, 0);
	/* DAC FL&FR playback mute */
	codec_update_bits(DAC_ANALOG1, 0x1<<10, 0);
	/* headphone PA VOLUME=0 */
	codec_update_bits(DAC_ANALOG1, 0x3f, 0);
	
	/* da_a  EN,PA EN,all bias en */
	codec_write(DAC_ANALOG3, 0x4b7);
	
	/* da_a  EN,PA EN,OUTPSTAGE DIS,all bias en,loop2 en*/
	codec_write(DAC_ANALOG3, 0x6b7);
	
	/* 4-wire mode */
	codec->soc->soc_writel(codec->soc, ((codec->soc->soc_readl(codec->soc, I2S_CTL) & ~(0x3 << 11)) | (0x1 << 11)), I2S_CTL);
	
	/* non direct mode */
	msleep(50);
		
	/* write high volume data */
	{
		unsigned int val = 0;
		int  i = 0;
			
		while (i < 900) {
			val = codec->soc->soc_readl(codec->soc, I2S_FIFOCTL);
			while ((val & (0x1 << 8)) != 0) {
				val = codec->soc->soc_readl(codec->soc, I2S_FIFOCTL);
			}
			codec->soc->soc_writel(codec->soc, 0x7fffffff, I2STX_DAT);
			i++;
		}

		mdelay(20);
	}
		
	/* PA RAMP2 CONNECT */
	codec_update_bits(DAC_ANALOG2 , 0x1<<9, 0x1<<9);
	mdelay(50);

	/* write ramp data for antipop */
	ramp_undirect(codec, 0x80000000, 0x7ffffe00);
	msleep(300);
	
	/* non direct mode */
	/* enable PA FR&FL output stage */
	codec_update_bits(DAC_ANALOG3, 0x1<<3, 0x1<<3);
	/* disable antipop2 LOOP2 */
	codec_update_bits(DAC_ANALOG3, 0x1<<9, 0);
	/* mute DAC playback */
	codec_update_bits(DAC_ANALOG1, 0x1<<10, 0);
	/* set volume 111111*/
	codec_update_bits(DAC_ANALOG1, 0x3f, 0x3f);
	/* disable ramp connect */
	codec_update_bits(DAC_ANALOG2, 0x1<<9, 0);
	
	msleep(20);
}

static void atc2603c_pa_up_all(struct atc260x_audio *codec)
{
	static int classd_flag = 1;
	int ret;
	
	/* EXTIRQ pad enable */
	pmu_update_bits(ATC2603C_PAD_EN, 0x73, 0x73);
	
	/* I2S: 2.0 Channel, SEL 4WIRE MODE */
	codec_update_bits(AUDIOINOUT_CTL, 0x122, 0);
	codec_update_bits(AUDIOINOUT_CTL, 0x20, 0x20);
	
	/* MICINL&MICINR differential */
	pmu_update_bits(ATC2603C_MFP_CTL, 0x3<<9 , 0x2<<9);
	pmu_update_bits(ATC2603C_MFP_CTL, 0x01 , 0x01);
	
	/* for atc2603c those burn Efuse */
	/* bit4:0 should not be changed, otherwise VREF isn't accurate */
	ret = pmu_update_bits(ATC2603C_PMU_BDG_CTL, 0x01 << 6, 0x01 << 6);
	ret = pmu_update_bits(ATC2603C_PMU_BDG_CTL, 0x01 << 5, 0);
	
	pa_up(codec);
	
	if (classd_flag == 1)
	{
		/* DAC&PA Bias enable */
		codec_update_bits(DAC_ANALOG3, 0x1 << 10, 0x1 << 10);
		/* pa zero cross enable */
		codec_update_bits(DAC_ANALOG2, 0x1 << 15, 0x1 << 15);
		codec_update_bits(DAC_ANALOG3, 0x1 << 13, 0x1 << 13);
	}
	
	/* after pa_up, the regs status should be the same as outstandby? */
	codec_update_bits(DAC_ANALOG1, DAC_ANALOG1_DACFL_FRMUTE, 0);/* mute */
	codec_update_bits(AUDIOINOUT_CTL, 0x01 << 1, 0);/* I2S input disable */
	codec_update_bits(DAC_ANALOG1, DAC_ANALOG1_PASW, DAC_ANALOG1_PASW);
	codec_update_bits(DAC_DIGITALCTL, 0x3<<4, 0x2<<4);
	codec_update_bits(DAC_ANALOG1, 0x3f, 40);
	codec_write(DAC_VOLUMECTL0, 0xbebe);
	
	codec->soc->soc_writel(codec->soc, codec->soc->soc_readl(codec->soc, I2S_CTL) & ~0x3, I2S_CTL);
	codec->soc->soc_writel(codec->soc, codec->soc->soc_readl(codec->soc, I2S_FIFOCTL) & ~(0x3 << 9) & ~0x3, I2S_FIFOCTL);
	
	codec_update_bits(ADC_CTL, 0x1 << 6, 0x1 << 6);
	codec_update_bits(ADC_CTL, 0x1 << 7, 0x1 << 7);
	
	//set capture params
	
	/* FML input gain control */
	codec_update_bits(AGC_CTL0, 0xf << 12, 0xd << 12);
	/* FMR input gain control */
    codec_update_bits(AGC_CTL0, 0xf << 8, 0xd << 8);
    codec_update_bits(ADC_DIGITALCTL, 0xF << ADC_DIGITALCTL_ADGC0_SFT, 0x0 << ADC_DIGITALCTL_ADGC0_SFT);
    codec_update_bits(AGC_CTL0, 0x7 << AGC_CTL0_AMP0GR1_SET, 0x0 << AGC_CTL0_AMP0GR1_SET);
	codec_update_bits(ADC_CTL, 0x1f << 0, 0x2 << 0);
	
	//set playback params
	codec_update_bits(DAC_ANALOG1, 0x3f << DAC_ANALOG1_VOLUME_SFT, 0x28 << DAC_ANALOG1_VOLUME_SFT);
    codec_update_bits(DAC_VOLUMECTL0, 0xff << DAC_VOLUMECTL0_DACFL_VOLUME_SFT, 0xbe << DAC_VOLUMECTL0_DACFL_VOLUME_SFT);
    codec_update_bits(DAC_VOLUMECTL0, 0xff << DAC_VOLUMECTL0_DACFR_VOLUME_SFT, 0xbe << DAC_VOLUMECTL0_DACFR_VOLUME_SFT);
}

static void atc2603c_pa_down(struct atc260x_audio *codec)
{
	/* delay for antipop before pa down */
	/* close PA OUTPSTAGE EN */
	codec_update_bits(DAC_ANALOG3, 0x1<<3, 0x0);
	mdelay(100);
	
	if (codec_local_data.hw_init_flag == true)
	{
		
		codec_write(DAC_VOLUMECTL0, 0);
		
		codec_local_data.hw_init_flag = false;
	}
}

static void atc2603c_adckeypad_config(struct atc260x_audio *codec)
{
	/* external mic power enable */
	codec_update_bits(AGC_CTL0, 0x1 << 6, 0x1 << 6);
	codec_update_bits(AGC_CTL0, 0x3 << 4, 0x3 << 4);
	
	switch(adc_num)
	{
	case 0:
		pmu_update_bits(ATC2603C_PMU_AUXADC_CTL0, 0x1 << 14, 0x1 << 14);
		break;
	case 1:
		pmu_update_bits(ATC2603C_PMU_AUXADC_CTL0, 0x1 << 13, 0x1 << 13);
		break;
	case 2:
	default:
		pmu_update_bits(ATC2603C_PMU_AUXADC_CTL0, 0x1 << 12, 0x1 << 12);
		break;
	}
}

static int atc2603c_dac_playback_volume_set(int left, int right)
{
	unsigned long flags;
	int new_value, change;
	
	if (left < 0) {
		left = 0;
	}
	if (left > 190) {
		left = 190;
	}
	if (right < 0) {
		right = 0;
	}
	if (right > 190) {
		right = 190;
	}
	
	new_value = (left & 0xFF) | ((right << 8) & 0xFF00);
	
	spin_lock_irqsave(&codec_local_data.lock, flags);
	
	if (new_value != codec_local_data.playback_volume)
	{
		codec_local_data.playback_volume = new_value;
		change = 1;
	}
	else {
		change = 0;
	}
	
	spin_unlock_irqrestore(&codec_local_data.lock, flags);
	
	return change;
}

static int atc2603c_dac_playback_volume_get(int *left, int *right)
{
	unsigned long flags;
	int value;
	
	spin_lock_irqsave(&codec_local_data.lock, flags);
	value = codec_local_data.playback_volume;
	spin_unlock_irqrestore(&codec_local_data.lock, flags);
	
	*left = value & 0xFF;
	*right = (value >> 8) & 0xFF;
	
	return 0;
}

static int atc2603c_codec_probe(struct soc_audio_device *soc)
{
	struct atc260x_audio *codec = codec_local_data.codec;
	unsigned long flags;
	int volume_dev;
	
	codec->soc = soc;
	codec_local_data.hw_init_flag = true;
	
	reenable_audio_block(codec);
	
	atc2603c_pa_up_all(codec);
	
	/* config adc detect */
	atc2603c_adckeypad_config(codec);
	
	
	/* Set I2S input mode to 2.0 channel mode */
	codec_update_bits(AUDIOINOUT_CTL, 0x3 << 2, 0);
	
	volume_dev = codec_read(DAC_VOLUMECTL0);
	
	spin_lock_irqsave(&codec_local_data.lock, flags);
	codec_local_data.playback_volume = volume_dev;
	spin_unlock_irqrestore(&codec_local_data.lock, flags);
	
	schedule_delayed_work(&codec_local_data.dwork, msecs_to_jiffies(500));
	return 0;
}

static int atc2603c_codec_remove(void)
{
	struct atc260x_audio *codec = codec_local_data.codec;
	unsigned long flags;
	int volume_dev;
	
	cancel_delayed_work_sync(&codec_local_data.dwork);
	
	atc2603c_pa_down(codec);
	
	volume_dev = codec_read(DAC_VOLUMECTL0);
	
	spin_lock_irqsave(&codec_local_data.lock, flags);
	codec_local_data.playback_volume = volume_dev;
	spin_unlock_irqrestore(&codec_local_data.lock, flags);
	return 0;
}

static void atc2603c_state_poll(struct work_struct *work)
{
	int volume_local, volume_dev;
	unsigned long flags;
	
	volume_dev = codec_read(DAC_VOLUMECTL0);
	
	spin_lock_irqsave(&codec_local_data.lock, flags);
	volume_local = codec_local_data.playback_volume;
	spin_unlock_irqrestore(&codec_local_data.lock, flags);
	
	if (volume_dev != volume_local) {
		codec_write(DAC_VOLUMECTL0, volume_local);
	}
	
	schedule_delayed_work(&codec_local_data.dwork, msecs_to_jiffies(500));
}

static int atc2603c_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct atc260x_audio *codec;
	
	pdev->dev.init_name = "atc2603c-audio";
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32); 
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	
	codec_local_data.pmic = dev_get_drvdata(dev->parent);
	
	if (!codec_local_data.pmic) {
		return -ENODEV;
	}
	
	codec = devm_kzalloc(dev, sizeof(*codec), GFP_KERNEL);
	
	if (!codec) {
		return -ENOMEM;
	}
	
	codec_local_data.codec = codec;
	
	codec->probe = atc2603c_codec_probe;
	codec->remove = atc2603c_codec_remove;
	codec->playback = atc2603c_playback_set_controls;
	codec->capture = atc2603c_capture_set_controls;
	codec->dac_playback_mute = atc2603c_dac_playback_mute;
	codec->dac_playback_volume_get = atc2603c_dac_playback_volume_get;
	codec->dac_playback_volume_set = atc2603c_dac_playback_volume_set;
	codec->soc = NULL;
	
	INIT_DELAYED_WORK(&codec_local_data.dwork, atc2603c_state_poll);
	
	spin_lock_init(&codec_local_data.lock);
	
	dev_set_drvdata(dev, codec);
	
	return 0;
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

