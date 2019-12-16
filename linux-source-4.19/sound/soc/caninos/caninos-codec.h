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

#ifndef _CANINOS_CODEC_H_
#define _CANINOS_CODEC_H_

#include <linux/mfd/atc260x/atc260x.h>

#define I2S_CTL     0x0
#define I2S_FIFOCTL 0x4
#define I2STX_DAT   0x8
#define I2SRX_DAT   0xC

struct soc_audio_device
{
	void (*soc_writel)(struct soc_audio_device *soc, u32 value, int reg);
	u32  (*soc_readl)(struct soc_audio_device *soc, int reg);
};

struct atc260x_audio
{
	struct soc_audio_device *soc;
	
	int (*probe)(struct soc_audio_device *soc);
	int (*remove)(void);
	int (*playback)(void);
	int (*capture)(void);
	int (*dac_playback_mute)(int mute);
	int (*dac_playback_volume_set)(int left, int right);
	int (*dac_playback_volume_get)(int *left, int *right);
};

#endif

