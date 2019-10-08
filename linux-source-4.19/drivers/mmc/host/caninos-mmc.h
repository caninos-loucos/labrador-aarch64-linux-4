#ifndef _CANINOS_MMC_H_
#define _CANINOS_MMC_H_

enum {
	PURE_CMD,
	DATA_CMD,
};

#define MMC_CMD_COMPLETE		1

#define CMD_OK						BIT(0)
#define CMD_RSP_ERR				BIT(1)
#define CMD_RSP_BUSY				BIT(2)
#define CMD_RSP_CRC_ERR			BIT(3)
#define CMD_TS_TIMEOUT			BIT(4)
#define CMD_DATA_TIMEOUT			BIT(5)
#define HW_TIMEOUT_ERR			BIT(6)
#define DATA_WR_CRC_ERR			BIT(7)
#define DATA_RD_CRC_ERR			BIT(8)
#define DATA0_BUSY_ERR				BIT(9)

#define HW_TIMEOUT					0xe4000000


/* SDC registers */
#define SD_EN_OFFSET         0x0000
#define SD_CTL_OFFSET        0x0004
#define SD_STATE_OFFSET      0x0008
#define SD_CMD_OFFSET        0x000c
#define SD_ARG_OFFSET        0x0010
#define SD_RSPBUF0_OFFSET    0x0014
#define SD_RSPBUF1_OFFSET    0x0018
#define SD_RSPBUF2_OFFSET    0x001c
#define SD_RSPBUF3_OFFSET    0x0020
#define SD_RSPBUF4_OFFSET    0x0024
#define SD_DAT_OFFSET        0x0028
#define SD_BLK_SIZE_OFFSET   0x002c
#define SD_BLK_NUM_OFFSET    0x0030
#define SD_BUF_SIZE_OFFSET   0x0034

/* SDC register access helpers */
#define HOST_EN(h)           ((h)->base + SD_EN_OFFSET)
#define HOST_CTL(h)          ((h)->base + SD_CTL_OFFSET)
#define HOST_STATE(h)        ((h)->base + SD_STATE_OFFSET)
#define HOST_CMD(h)          ((h)->base + SD_CMD_OFFSET)
#define HOST_ARG(h)          ((h)->base + SD_ARG_OFFSET)
#define HOST_RSPBUF0(h)      ((h)->base + SD_RSPBUF0_OFFSET)
#define HOST_RSPBUF1(h)      ((h)->base + SD_RSPBUF1_OFFSET)
#define HOST_RSPBUF2(h)      ((h)->base + SD_RSPBUF2_OFFSET)
#define HOST_RSPBUF3(h)      ((h)->base + SD_RSPBUF3_OFFSET)
#define HOST_RSPBUF4(h)      ((h)->base + SD_RSPBUF4_OFFSET)
#define HOST_DAT(h)          ((h)->base + SD_DAT_OFFSET)
#define HOST_BLK_SIZE(h)     ((h)->base + SD_BLK_SIZE_OFFSET)
#define HOST_BLK_NUM(h)      ((h)->base + SD_BLK_NUM_OFFSET)
#define HOST_BUF_SIZE(h)     ((h)->base + SD_BUF_SIZE_OFFSET)

/* Register SD_EN */
#define SD_EN_RANE           (1 << 31)
/* bit 30 reserved */
#define SD_EN_RAN_SEED(x)    (((x) & 0x3f) << 24)
/* bit 23~13 reserved */
#define SD_EN_S18EN          (1 << 12)
/* bit 11 reserved */
#define SD_EN_RESE           (1 << 10)
#define SD_EN_DAT1_S         (1 << 9)
#define SD_EN_CLK_S          (1 << 8)
#define SD_ENABLE            (1 << 7)
#define SD_EN_BSEL           (1 << 6)
/* bit 5~4 reserved */
#define SD_EN_SDIOEN         (1 << 3)
#define SD_EN_DDREN          (1 << 2)
#define SD_EN_DATAWID(x)     (((x) & 0x3) << 0)

/* Register SD_CTL */
#define SD_CTL_TOUTEN        (1 << 31)
#define SD_CTL_TOUTCNT(x)    (((x) & 0x7f) << 24)
#define SD_CTL_RDELAY(x)     (((x) & 0xf) << 20)
#define SD_CTL_WDELAY(x)     (((x) & 0xf) << 16)
/* bit 15~14 reserved */
#define SD_CTL_CMDLEN        (1 << 13)
#define SD_CTL_SCC           (1 << 12)
#define SD_CTL_TCN(x)        (((x) & 0xf) << 8)
#define SD_CTL_TS            (1 << 7)
#define SD_CTL_LBE           (1 << 6)
#define SD_CTL_C7EN          (1 << 5)
/* bit 4 reserved */
#define SD_CTL_TM(x)         (((x) & 0xf) << 0)

/* Register SD_STATE */
/* bit 31~19 reserved */
#define SD_STATE_DAT1BS      (1 << 18)
#define SD_STATE_SDIOB_P     (1 << 17)
#define SD_STATE_SDIOB_EN    (1 << 16)
#define SD_STATE_TOUTE       (1 << 15)
#define SD_STATE_BAEP        (1 << 14)
/* bit 13 reserved */
#define SD_STATE_MEMRDY      (1 << 12)
#define SD_STATE_CMDS        (1 << 11)
#define SD_STATE_DAT1AS      (1 << 10)
#define SD_STATE_SDIOA_P     (1 << 9)
#define SD_STATE_SDIOA_EN    (1 << 8)
#define SD_STATE_DAT0S       (1 << 7)
#define SD_STATE_TEIE        (1 << 6)
#define SD_STATE_TEI         (1 << 5)
#define SD_STATE_CLNR        (1 << 4)
#define SD_STATE_CLC         (1 << 3)
#define SD_STATE_WC16ER	     (1 << 2)
#define SD_STATE_RC16ER      (1 << 1)
#define SD_STATE_CRC7ER	     (1 << 0)

/* SDC0 delays */
#define SDC0_WDELAY_LOW_CLK  (0xf)
#define SDC0_WDELAY_MID_CLK  (0xa)
#define SDC0_WDELAY_HIGH_CLK (0x9)
#define SDC0_RDELAY_LOW_CLK  (0xf)
#define SDC0_RDELAY_MID_CLK  (0xa)
#define SDC0_RDELAY_HIGH_CLK (0x8)

/* SDC1 delays */
#define SDC1_WDELAY_LOW_CLK  (0xf)
#define SDC1_WDELAY_MID_CLK  (0xa)
#define SDC1_WDELAY_HIGH_CLK (0x8)
#define SDC1_RDELAY_LOW_CLK  (0xf)
#define SDC1_RDELAY_MID_CLK  (0xa)
#define SDC1_RDELAY_HIGH_CLK (0x8)

/* SDC2 delays */
#define SDC2_WDELAY_LOW_CLK  (0xf)
#define SDC2_WDELAY_MID_CLK  (0xa)
#define SDC2_WDELAY_HIGH_CLK (0x8)
#define SDC2_RDELAY_LOW_CLK  (0xf)
#define SDC2_RDELAY_MID_CLK  (0xa)
#define SDC2_RDELAY_HIGH_CLK (0x8)

/* SDC3 delays */
#define SDC3_WDELAY_LOW_CLK  (0xf)
#define SDC3_WDELAY_MID_CLK  (0xa)
#define SDC3_WDELAY_HIGH_CLK (0x8)
#define SDC3_RDELAY_LOW_CLK  (0xf)
#define SDC3_RDELAY_MID_CLK  (0xa)
#define SDC3_RDELAY_HIGH_CLK (0x8)

#endif

