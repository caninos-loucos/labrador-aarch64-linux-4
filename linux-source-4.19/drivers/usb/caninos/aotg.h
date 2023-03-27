#ifndef __LINUX_AOTG_H__
#define __LINUX_AOTG_H__

#include "aotg_regs.h"

#define TRB_ITE       (1 << 11)
#define TRB_CHN       (1 << 10)
#define TRB_CSP       (1 << 9)
#define TRB_COF       (1 << 8)
#define TRB_ICE       (1 << 7)
#define TRB_IZE       (1 << 6)
#define TRB_ISE       (1 << 5)
#define TRB_LT        (1 << 4)
#define AOTG_TRB_IOC  (1 << 3)
#define AOTG_TRB_IOZ  (1 << 2)
#define AOTG_TRB_IOS  (1 << 1)
#define TRB_OF        (1 << 0)

#define USE_SG

#ifdef USE_SG
#define NUM_TRBS (256)
#define RING_SIZE (NUM_TRBS * 16)
#else
#define NUM_TRBS (64)
#define RING_SIZE (NUM_TRBS * 16)
#endif

struct aotg_trb {
	u32 hw_buf_ptr;
	u32 hw_buf_len;
	u32 hw_buf_remain;
	u32 hw_token;
};

struct aotg_plat_data {
	struct device *dev;
	void __iomem *base;
	void __iomem *usbecs;
	resource_size_t rsrc_start;
	resource_size_t rsrc_len;
	struct clk *clk_usbh_pllen;
	struct clk *clk_usbh_phy;
	struct clk *clk_usbh_cce;
	int irq;
	int id;
};

#endif

