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
#include <../../drivers/gpu/drm/caninos/hdmi-regs.h>
#include <../../drivers/gpu/drm/caninos/hdmi.h>

#define I2S_CTL 0x00
#define I2SPM  11 // pin mode
#define I2SRCS 10 // rx clock select
#define I2SREN 1  // rx enable
#define I2STEN 0  // tx enable

#define I2S_FIFOCTL 0x04
#define I2STFSS 18 // tx fifo source select
#define I2SRFR  9  // rx fifo reset
#define I2STFR  0  // tx fifo reset

#define I2STX_DAT 0x08
#define I2SRX_DAT 0x0C

#define SPDIF_HDMI_CTL 0x10
#define HDMFSS 14 // hdmi fifo source select
#define	HDMFR  1  // hdmi fifo reset

#define I2STX_SPDIF_HDMI_CTL 0x2C
#define VASS 0 // Virtual address select

#define I2STX_SPDIF_HDMI_DAT 0x30

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

struct hdmi_audio
{
	struct soc_audio_device *soc;
	
	int (*probe)(struct soc_audio_device *soc);
	int (*remove)(void);
	int (*playback)(void);
	int (*hdmi_playback_mute)(int mute);
	int (*hdmi_playback_volume_set)(int left, int right);
	int (*hdmi_playback_volume_get)(int *left, int *right);
	struct caninos_hdmi *hdmi;
};

#endif

