#ifndef _HDMI_H_
#define _HDMI_H_

#include <linux/reset.h>
#include <linux/clk.h>

/* horizontal and vertical sync active high */
#define DSS_SYNC_HOR_HIGH_ACT		(1 << 0)
#define DSS_SYNC_VERT_HIGH_ACT		(1 << 1)

struct videomode
{
	int xres; /* visible resolution */
	int yres;
	int refresh; /* vertical refresh rate in hz */
	/*
	 * Timing: All values in pixclocks, except pixclock
	 */
	int pixclock; /* pixel clock in ps (pico seconds) */
	int hfp; /* horizontal front porch */
	int hbp; /* horizontal back porch */
	int vfp; /* vertical front porch */
	int vbp; /* vertical back porch */
	int hsw; /* horizontal synchronization pulse width */
	int vsw; /* vertical synchronization pulse width */
	int sync; /* see DSS_SYNC_* */
	int vmode; /* see DSS_VMODE_* */
};

enum SRC_SEL {
	VITD = 0,
	DE,
	SRC_MAX
};

enum hdmi_core_hdmi_dvi {
	HDMI_DVI = 0,
	HDMI_HDMI = 1,
};

enum hdmi_core_packet_mode {
	HDMI_PACKETMODERESERVEDVALUE = 0,
	HDMI_PACKETMODE24BITPERPIXEL = 4,
	HDMI_PACKETMODE30BITPERPIXEL = 5,
	HDMI_PACKETMODE36BITPERPIXEL = 6,
	HDMI_PACKETMODE48BITPERPIXEL = 7
};

enum hdmi_packing_mode {
	HDMI_PACK_10b_RGB_YUV444 = 0,
	HDMI_PACK_24b_RGB_YUV444_YUV422 = 1,
	HDMI_PACK_20b_YUV422 = 2,
	HDMI_PACK_ALREADYPACKED = 7
};

enum hdmi_pixel_format {
	RGB444 = 0,
	YUV444 = 2
};

enum hdmi_deep_color {
	color_mode_24bit = 0,
	color_mode_30bit = 1,
	color_mode_36bit = 2,
	color_mode_48bit = 3
};

enum hdmi_3d_mode {
	HDMI_2D,
	HDMI_3D_LR_HALF,
	HDMI_3D_TB_HALF,
	HDMI_3D_FRAME,
};

/*
 * a configuration structure to convet HDMI resolutoins
 * between vid and video_timings.
 * vid is some fix number defined by HDMI spec,
 * are used in EDID etc.
 */

enum hdmi_vid_table {
	VID640x480P_60_4VS3 = 1,
	VID720x480P_60_4VS3,
	VID1280x720P_60_16VS9 = 4,
	VID1920x1080I_60_16VS9,
	VID720x480I_60_4VS3,
	VID1920x1080P_60_16VS9 = 16,
	VID720x576P_50_4VS3,
	VID1280x720P_50_16VS9 = 19,
	VID1920x1080I_50_16VS9,
	VID720x576I_50_4VS3,
	VID1440x576P_50_4VS3 = 29,
	VID1920x1080P_50_16VS9 = 31,
	VID1920x1080P_24_16VS9,

	
};

enum hdmi_packet_type {
	PACKET_AVI_SLOT		= 0,
	PACKET_AUDIO_SLOT	= 1,
	PACKET_SPD_SLOT		= 2,
	PACKET_GBD_SLOT		= 3,
	PACKET_VS_SLOT		= 4,
	PACKET_HFVS_SLOT	= 5,
	PACKET_MAX,
};

struct hdmi_ip;

struct hdmi_ip_ops
{
	int  (*init)(struct hdmi_ip *ip);
	void (*exit)(struct hdmi_ip *ip);
	
	void (*power_off)(struct hdmi_ip *ip);
	int (*power_on)(struct hdmi_ip *ip);
	
	void (*hpd_enable)(struct hdmi_ip *ip);
	void (*hpd_disable)(struct hdmi_ip *ip);
	bool (*hpd_is_pending)(struct hdmi_ip *ip);
	void (*hpd_clear_pending)(struct hdmi_ip *ip);
	
	bool (*cable_status)(struct hdmi_ip *ip);
	
	int  (*video_enable)(struct hdmi_ip *ip);
	void (*video_disable)(struct hdmi_ip *ip);
	bool (*is_video_enabled)(struct hdmi_ip *ip);
	
	int  (*packet_generate)(struct hdmi_ip *ip, uint32_t no, uint8_t *pkt);
	int  (*packet_send)(struct hdmi_ip *ip, uint32_t no, int period);
	
	int  (*audio_enable)(struct hdmi_ip *ip);
	int  (*audio_disable)(struct hdmi_ip *ip);
};

struct hdmi_ip_settings
{
	int hdmi_src;
	int vitd_color;
	int pixel_encoding;
	int color_xvycc;
	int deep_color;
	int hdmi_mode;
	int mode_3d;
	int prelines;
	int channel_invert;
	int bit_invert;
};

struct hdmi_ip_hwdiff
{
	int	hp_start;
	int	hp_end;
	int	vp_start;
	int vp_end;
	int	mode_start;
	int	mode_end;
	
	uint32_t pll_reg;
	int	pll_24m_en;
	int	pll_en;
	
	uint32_t pll_debug0_reg;
	uint32_t pll_debug1_reg;
};

struct hdmi_ip
{
	/* register address */
	void __iomem *base;
	void __iomem *cmu_base;
	
	struct reset_control *hdmi_rst;
	struct clk *hdmi_dev_clk;
	
	struct hdmi_ip_settings	settings;
	int vid; /* video mode */
	
	struct videomode mode;
	
	bool interlace;
	int	vstart;	/* vsync start line */
	bool repeat;
	
	/* used for registers setting */
	uint32_t pll_val;
	uint32_t pll_debug0_val;
	uint32_t pll_debug1_val;
	uint32_t tx_1;
	uint32_t tx_2;
	uint32_t phyctrl_1;
	uint32_t phyctrl_2;
	
	/* ip functions */
	const struct hdmi_ip_ops *ops;
	
	/* used for hardware specific configurations */
	const struct hdmi_ip_hwdiff *hwdiff;
};

struct hdmi_ip_init_data
{
	void __iomem *base;
	void __iomem *cmu_base;
	struct reset_control *hdmi_rst;
	struct clk *hdmi_dev_clk;
};

extern struct hdmi_ip* hdmic_init(struct hdmi_ip_init_data *data);

#endif

