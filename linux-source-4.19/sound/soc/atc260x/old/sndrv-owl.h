#ifndef __S900_SNDRV_H__
#define __S900_SNDRV_H__
#include <sound/pcm.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <linux/atomic.h>
#include <linux/dmaengine.h>
#include <linux/pinctrl/consumer.h>

#define PMU_NOT_USED	-1
#define IC_NOT_USED		-1

#define S900_AIF_I2S        1
#define S900_AIF_HDMI       2
#define S900_AIF_SPDIF      3
#define S900_AIF_PCM        4
#define S900_AIF_I2S_TDM    5
#define S900_AIF_SPI        6

#define SND_SOC_DAIFMT_I2S_6WIRE		   8 /* I2S mode  see soc-dai.h*/

static int error_switch = 1;
static int debug_switch = 1;

#define SND_DEBUG
#ifdef SND_DEBUG
#define snd_err(fmt, args...) \
	if (error_switch) \
		printk(KERN_ERR"[SNDRV]:[%s] "fmt"\n", __func__, ##args)

#define snd_dbg(fmt, args...) \
	if (debug_switch) \
		printk(KERN_DEBUG"[SNDRV]:[%s] "fmt"\n", __func__, ##args)
#endif

enum {
	O_MODE_I2S          = 0x1,
	O_MODE_HDMI         = 0x2,
	O_MODE_SPDIF        = 0x4,
	O_MODE_PCM          = 0x8,
	O_MODE_I2S_TDM      = 0x10,
	O_MODE_SPI          = 0x20,
};

enum {
	SPEAKER_ON = 0,
	HEADSET_MIC = 1,
	HEADSET_NO_MIC = 2,
};

/* add ic type of platform in S500/S700/S900 */
enum {
	IC_PLATFM_3605, 	 /* 3605 */
	IC_PLATFM_S500,      /* S500 */
	IC_PLATFM_S700,
	IC_PLATFM_S900,
};


typedef struct {
	short sample_rate;	/* 真实采样率除以1000 */
	char index[2];		/* 对应硬件寄存器的索引值 */
} fs_t;

typedef struct {
	unsigned int earphone_gpios;
	unsigned int speaker_gpios;
	unsigned int earphone_output_mode;
	unsigned int mic_num;
	unsigned int mic0_gain[2];
	unsigned int speaker_gain[2];
	unsigned int earphone_gain[2];
	unsigned int speaker_volume;
	unsigned int earphone_volume;
	unsigned int earphone_detect_mode;
	unsigned int mic_mode;
	unsigned int earphone_detect_method;
	unsigned int adc_plugin_threshold;
	unsigned int adc_level;
	unsigned int adc_num;
	unsigned int record_source;
	unsigned int i2s_mode;
} audio_hw_cfg_t;

/*extern audio_hw_cfg_t audio_hw_cfg;*/

struct s900_pcm_priv {
	int input_mode;
	int output_mode;
	struct pinctrl *pc;
	struct pinctrl_state *ps;
};

enum{
I2S_SPDIF_NUM = 0,
GPIO_MFP_NUM,
HDMI_NUM,
CMU_NUM,
MFP_NUM,
PCM_NUM,
SPI_NUM,
MAX_RES_NUM
};

struct s900_pcm_dma_params {
	struct dma_chan *dma_chan;		/* the DMA request channel to use */
	u32 dma_addr;			        /* device physical address for DMA */
};

void set_dai_reg_base(int num);
u32 snd_dai_readl(u32 reg);
void snd_dai_writel(u32 val, u32 reg);

void snd_dai_i2s_clk_disable(void);
void snd_dai_i2s_clk_enable(void);
void __iomem *get_dai_reg_base(int num);

extern void earphone_detect_cancel(void);
extern void earphone_detect_work(void);

#endif /* ifndef __S900_SNDRV_H__ */
