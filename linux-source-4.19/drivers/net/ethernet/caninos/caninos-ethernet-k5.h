// SPDX-License-Identifier: GPL-2.0
/*
 * Ethernet Driver for Caninos Labrador
 *
 * Copyright (c) 2022-2023 ITEX - LSITEC - Caninos Loucos
 * Author: Ana Clara Forcelli <ana.forcelli@lsitec.org.br>
 *
 * Copyright (c) 2019 LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2019 LSITEC - Caninos Loucos
 * Author: Igor Ruschi
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

typedef struct{
	int phy_reset_gpio;
	int phy_power_gpio;
}  phy_gpio;

struct ethernet_buffer_desc {
    u32 status;
    u32 control;
    u32 buf_addr;
    u32 reserved;
};

#define EC_TX_TIMEOUT (0.05*HZ)

#define RX_RING_SIZE 64
#define TX_RING_SIZE 128 

#define ETH_PKG_MIN 64
#define ETH_PKG_MAX 1518

#define SETUP_FRAME_LEN 192
#define SETUP_FRAME_PAD 16

// 0xD0 reserve 16 bytes for align

#define SETUP_FRAME_RESV_LEN  (SETUP_FRAME_LEN + SETUP_FRAME_PAD)

// reserve 18 bytes for align

#define PKG_RESERVE 18
#define PKG_MIN_LEN (ETH_PKG_MIN)
#define PKG_MAX_LEN (ETH_PKG_MAX + PKG_RESERVE)

#define EC_SKB_ALIGN_BITS_MASK  0x3
#define EC_SKB_ALIGNED 0x4

#define TXBD_STAT_OWN (0x1 << 31)
#define TXBD_CTRL_TBS1(x)  ((x) & 0x7FF) // buf1 size

#define RXBD_STAT_OWN (0x1 << 31)
#define RXBD_CTRL_RBS1(x) ((x) & 0x7FF) // buffer1 size
#define RXBD_CTRL_RER (0x1 << 25) // receive end of ring

#define TXBD_CTRL_IC (0x1 << 31) // interrupt on completion
#define TXBD_CTRL_TER (0x1 << 25) // transmit end of ring
#define TXBD_CTRL_SET (0x1 << 27) // setup packet

#define TXBD_CTRL_LS (0x1 << 30) // last descriptor
#define TXBD_CTRL_FS (0x1 << 29) // first descriptor

#define TX_RING_MOD_MASK (TX_RING_SIZE - 1)

#define EC_STATUS_NIS  (0x1 << 16) // normal interrupt summary
#define EC_STATUS_AIS  (0x1 << 15) // abnormal interrupt summary
#define EC_STATUS_TI   (0x1) //transmit interrupt
#define EC_STATUS_ETI  (0x1 << 10)//early transmit interrupt
#define EC_STATUS_RSM  (0x7 << 17)      //receive process state
#define EC_STATUS_TSM  (0x7 << 20)      //transmit process state
#define EC_STATUS_RU   (0x1 << 7)       //receive buffer unavailable
#define EC_STATUS_RI   (0x1 << 6)       //receive interrupt

#define TXBD_STAT_ES  (0x1 << 15)       // error summary
#define TXBD_STAT_LO  (0x1 << 11)       // loss of carrier
#define TXBD_STAT_NC  (0x1 << 10)       // no carrier
#define TXBD_STAT_LC  (0x1 << 9)        // late collision
#define TXBD_STAT_EC  (0x1 << 8)        // excessive collision
#define TXBD_STAT_CC(x)  (((x) >> 3) & 0xF)
#define TXBD_STAT_UF  (0x1 << 1)        // underflow error
#define TXBD_STAT_DE  (0x1)         // deferred

#define RXBD_STAT_OWN (0x1 << 31)
#define RXBD_STAT_FF  (0x1 << 30)       //filtering fail
#define RXBD_STAT_FL(x) (((x) >> 16) & 0x3FFF)  // frame leng
#define RXBD_STAT_ES  (0x1 << 15)       // error summary
#define RXBD_STAT_DE  (0x1 << 14)       // descriptor error
#define RXBD_STAT_RF  (0x1 << 11)       // runt frame
#define RXBD_STAT_MF  (0x1 << 10)       // multicast frame
#define RXBD_STAT_FS  (0x1 << 9)        // first descriptor
#define RXBD_STAT_LS  (0x1 << 8)        // last descriptor
#define RXBD_STAT_TL  (0x1 << 7)        // frame too long
#define RXBD_STAT_CS  (0x1 << 6)        // collision
#define RXBD_STAT_FT  (0x1 << 5)        // frame type
#define RXBD_STAT_RE  (0x1 << 3)        // mii error
#define RXBD_STAT_DB  (0x1 << 2)        // byte not aligned
#define RXBD_STAT_CE  (0x1 << 1)        // crc error
#define RXBD_STAT_ZERO  (0x1)

#define RXBD_CTRL_RER (0x1 << 25)       // receive end of ring
#define RXBD_CTRL_RCH (0x1 << 24)       // using second buffer, not used here
#define RXBD_CTRL_RBS1(x) ((x) & 0x7FF) // buffer1 size

#define EC_RX_fetch_dsp (0x1 <<17)
#define EC_RX_close_dsp (0x5 <<17)
#define EC_RX_run_dsp 	(0x7 <<17)
#define EC_TX_run_dsp  (0x3 << 20)

#define EC_BMODE_SWR (0x1) // software resetcani
#define EC_CACHETHR_CPTL(x) (((x) & 0xFF) << 24) // cache pause threshold level
#define EC_CACHETHR_CRTL(x) (((x) & 0xFF) << 16) // cache restart threshold level
#define EC_CACHETHR_PQT(x)  ((x) & 0xFFFF) // flow control pause quanta time
#define EC_FIFOTHR_FPTL(x) (((x) & 0xFFFF) << 16) // fifo pause threshold level
#define EC_FIFOTHR_FRTL(x) ((x) & 0xFFFF) // fifo restart threshold level
#define EC_FLOWCTRL_ENALL (0x1F << 27)
#define EC_OPMODE_FD  (0x1 << 9) // full duplex mode
#define EC_OPMODE_SPEED(x) (((x) & 0x3) << 16) // eth speed selection
#define EC_OPMODE_PR (0x1 << 6) // promiscuous mode
#define EC_OPMODE_ST (0x1 << 13) // start or stop transmit command
#define EC_OPMODE_SR (0x1 << 1) // start or stop receive command
#define EC_OPMODE_10M (0x1 << 17) // set when work on 10M, otherwise 100M
#define EC_IEN_ALL (0x1CDE3) // TU interrupt disabled
#define EC_TXPOLL_ST (0x1) // leave suspended mode to running mode to start xmit
#define EC_RXPOLL_SR (0x1) // leave suspended to running mode

#define ETH_PHY_ID_RTL8201 0x001cc810
#define PHY_ID_MASK 0xfffffff0

#define MII_MNG_SB (0x1 << 31) // start transfer or busy
#define MII_MNG_OPCODE(x) (((x) & 0x3) << 26) // operation mode
#define MII_MNG_PHYADD(x) (((x) & 0x1F) << 21)  // physical layer address
#define MII_MNG_REGADD(x) (((x) & 0x1F) << 16)  // register address
#define MII_MNG_DATAM (0xFFFF) //register data mask
#define MII_MNG_DATA(x) ((MII_MNG_DATAM) & (x)) // data to write
#define MII_OP_WRITE 0x1
#define MII_OP_READ 0x2
#define MII_OP_CDS 0x3

#define PHY_RTL8201F_REG_RMSR           0x10
#define PHY_RTL8201F_REG_INT_LED_FUNC   0x13
#define PHY_RTL8201F_REG_PAGE_SELECT    0x1F
#define PHY_RTL8201F_REG_INT_SNR        0x1E

#define PHY_RTL8201F_REG_PAGE_SELECT_SEVEN    0x7
#define PHY_RTL8201F_REG_PAGE_SELECT_ZERO     0x0

#define PHY_RTL8201F_LINK_STATUS_CHANGE     (0x1<<11)
#define PHY_RTL8201F_RMSR_CLK_DIR_INPUT	    (0x1<<12)
#define PHY_RTL8201F_RMSR_RMII_MODE	        (0x1<<3)
#define PHY_RTL8201F_RMSR_RMII_RX_OFFSET    (0xF<<4)
#define PHY_RTL8201F_RMSR_RMII_TX_OFFSET    (0xF<<8)
#define PHY_RTL8201F_PIN_LINK_STATE_CHANGE  (0x1 <<4)

#define PHY_POLL_INTERVAL (500)

#define ETH_MAC_LEN 6
#define ETH_CRC_LEN 4

static inline void eth_writel(struct caninos_eth *eth, u32 data, int idx)
{
    writel(data, eth->base + idx);
}

static inline u32 eth_readl(struct caninos_eth *eth, int idx)
{
    return readl(eth->base + idx);
}