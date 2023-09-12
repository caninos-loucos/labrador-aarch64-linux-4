/*
    Filename: ethernet.c
    Module  : Caninos Labrador Ethernet Driver
    Author  : Edgar Bernardi Righi
    Revisor : Igor Ruschi
    Company : LSITEC
    Date    : June 2019
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <uapi/linux/mii.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#include "caninos-ethernet-k5.h"

#define DRIVER_NAME "labrador-ethernet"

#define INFO_MSG(fmt,...) pr_info(DRIVER_NAME ": " fmt, ##__VA_ARGS__)
#define ERR_MSG(fmt,...) pr_err(DRIVER_NAME ": " fmt, ##__VA_ARGS__)

typedef struct{
	int phy_reset_gpio;
	int phy_power_gpio;
}  phy_gpio;

// receive and transmit buffer descriptor

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
#define EC_RX_run_dsp  (0x7 <<17)
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
#define PHY_RTL8201F_RMSR_CLK_DIR_INPUT     (0x1<<12)
#define PHY_RTL8201F_RMSR_RMII_MODE         (0x1<<3)
#define PHY_RTL8201F_RMSR_RMII_RX_OFFSET    (0xF<<4)
#define PHY_RTL8201F_RMSR_RMII_TX_OFFSET    (0xF<<8)
#define PHY_RTL8201F_PIN_LINK_STATE_CHANGE  (0x1 <<4)

#define PHY_POLL_INTERVAL (500)

#define ETH_MAC_LEN 6
#define ETH_CRC_LEN 4

struct reset_control *eth_rst = NULL;

static void __iomem *base = NULL;

static char default_mac_addr[ETH_MAC_LEN] = {0x00, 0x18, 0xFE, 0x61, 0xD5, 0xD6};
static char mac_addr[ETH_MAC_LEN];

static struct clk * ethernet_clk = NULL;
static struct pinctrl * ethernet_ppc = NULL;
static struct net_device * ethernet_dev = NULL;

static struct ethernet_buffer_desc *ethernet_tx_buf = NULL;
static struct ethernet_buffer_desc *ethernet_rx_buf = NULL;

static volatile struct ethernet_buffer_desc *ethernet_cur_rx = NULL;
static volatile struct ethernet_buffer_desc *ethernet_cur_tx = NULL;
static volatile struct ethernet_buffer_desc *ethernet_dirty_tx = NULL;

static struct sk_buff *ethernet_rx_skb[RX_RING_SIZE]; // rx_bd buffers
static struct sk_buff *ethernet_tx_skb[TX_RING_SIZE]; // temp save transmited skb

static dma_addr_t ethernet_tx_buf_paddr;
static dma_addr_t ethernet_rx_buf_paddr;

static u32 ethernet_phy_addr;
static bool ethernet_phy_linked = false;
static bool ethernet_device_opened = false;

static int ethernet_skb_cur = 0;
static int ethernet_skb_dirty = 0;

static bool ethernet_tx_full = false;

static DEFINE_SPINLOCK(ethernet_lock);

static phy_gpio global_phy_gpio;

static void ethernet_phy_state_worker(struct work_struct *);

static DECLARE_DELAYED_WORK(ethernet_phy_state_work, ethernet_phy_state_worker);

static inline void act_writel(u32 val, u32 offset)
{
	writel(val, base + offset);
}

static inline u32 act_readl(u32 offset)
{
	return readl(base + offset);
}

static int ethernet_tx_buffer_alloc(void);

static void ethernet_tx_buffer_free(void);

static int ethernet_rx_buffer_alloc(void);

static void ethernet_rx_buffer_free(void);

#define COPY_MAC_ADDR(dest, mac)  do {\
    *(unsigned short *)(dest) = *(unsigned short *)(mac); \
    *(unsigned short *)((dest) + 4) = *(unsigned short *)((mac) + 2); \
    *(unsigned short *)((dest) + 8) = *(unsigned short *)((mac) + 4); \
    (dest) += 12; \
}while (0)

/* generate an random mac address */
static int mac_gen(void)
{
	int ret = 0, i = 0;
	unsigned int rand_num = 0;
	char *mac = (char *) mac_addr;
	
	while (i<6)
	{
		get_random_bytes(&rand_num, sizeof(char));
		
		if(i == 0) {
			mac[i] = 0x02; //localy administrated    
		}
		else{
 			mac[i] = rand_num;
		}
		i++;
	}
	return ret;
}

static int ethernet_set_pin_mux(struct platform_device * pdev)
{
	ethernet_ppc = pinctrl_get_select_default(&pdev->dev);
	
	if (IS_ERR(ethernet_ppc))
	{
		ethernet_ppc = NULL;
		return -ENODEV;
	}
	return 0;
}

static void ethernet_put_pin_mux(void)
{
	if (ethernet_ppc)
	{
		pinctrl_put(ethernet_ppc);
		ethernet_ppc = NULL;
	}
}

static int ethernet_get_clk(struct device *dev)
{
	unsigned long tfreq, freq;
	struct clk * clk;
	int ret;
	
	ethernet_clk = clk_get(dev, "ethernet");
	
	if (IS_ERR(ethernet_clk))
	{
		ethernet_clk = NULL;
		return -ENODEV;
	}
	
	ret = clk_prepare(ethernet_clk);
	
	if (ret)
	{
		clk_put(ethernet_clk);
		ethernet_clk = NULL;
		return ret;
	}
	
	clk = clk_get(dev, "rmii-ref");
	
	if (IS_ERR(clk))
	{
		clk_put(ethernet_clk);
		ethernet_clk = NULL;
		return -ENODEV;
	}
	
	tfreq = 50 * 1000 * 1000;
	
	freq = clk_round_rate(clk, tfreq);
	
	if (freq != tfreq)
	{
		clk_put(clk);
		clk_put(ethernet_clk);
		ethernet_clk = NULL;
		return -EINVAL;
	}
	
	ret = clk_set_rate(clk, freq);
	
	if (ret)
	{
		clk_put(clk);
		clk_put(ethernet_clk);
		ethernet_clk = NULL;
		return ret;
	}
	
	clk_put(clk);
	return 0;
}

static void ethernet_put_clk(void)
{
	if (ethernet_clk)
	{
		clk_put(ethernet_clk);
		ethernet_clk = NULL;
	}
}

static int ethernet_clock_enable(int reset)
{
	int ret;
	
	if(ethernet_clk)
	{
		ret = clk_enable(ethernet_clk);
		
		if (ret) {
			return ret;
		}
		
		udelay(100);
		
		if(reset){
			reset_control_assert(eth_rst);
			udelay(100);
			reset_control_deassert(eth_rst);
		}
		
		udelay(100);
	}
	return 0;
}

static int ethernet_write_phy_reg(u16 reg_addr, u16 val)
{
	u32 op_reg, phy_addr = ethernet_phy_addr;
	
	do {
		op_reg = act_readl(MAC_CSR10);
	} while (op_reg & MII_MNG_SB);
	
	act_writel(MII_MNG_SB | MII_MNG_OPCODE(MII_OP_WRITE) | MII_MNG_REGADD(reg_addr) | MII_MNG_PHYADD(phy_addr) | val, MAC_CSR10);
	
	do {
		op_reg = act_readl(MAC_CSR10);
	} while (op_reg & MII_MNG_SB);
	
	return 0;
}

static int ethernet_read_phy_reg(u16 reg_addr, u16 * val)
{
	u32 op_reg, phy_addr = ethernet_phy_addr;

	do {
		op_reg = act_readl(MAC_CSR10);
	} while (op_reg & MII_MNG_SB);

	act_writel(MII_MNG_SB | MII_MNG_OPCODE(MII_OP_READ) | MII_MNG_REGADD(reg_addr) | MII_MNG_PHYADD(phy_addr), MAC_CSR10);

	do {
		op_reg = act_readl(MAC_CSR10);
	} while (op_reg & MII_MNG_SB);
	
	*val = MII_MNG_DATA(op_reg);
	return 0;
}

static int ethernet_phy_reg_set_bits(u16 reg_addr, u16 bits)
{
	u16 reg_val;
	
	ethernet_read_phy_reg(reg_addr, &reg_val);
	
	reg_val |= bits;
	
	ethernet_write_phy_reg(reg_addr, reg_val);
	ethernet_read_phy_reg(reg_addr, &reg_val);
	return 0;
}

static int ethernet_phy_get_link(bool * linked)
{
    u16 bmsr;
    int ret;
    
    ret = ethernet_read_phy_reg(MII_BMSR, &bmsr);
    if (ret < 0) {
        return ret;
    }
    
    *linked = (bmsr & BMSR_LSTATUS) ? true : false;
    return 0;
}

static int ethernet_phy_read_status(bool * speed, bool * full)
{
    u16 lpa, adv, bmcr;
    
    // speed - 10M : false - 100M : true
    // full - half duplex : false - full duplex : true
    
    ethernet_read_phy_reg(MII_BMCR, &bmcr);
    ethernet_read_phy_reg(MII_LPA, &lpa);
    ethernet_read_phy_reg(MII_ADVERTISE, &adv);
    
    // there is a priority order according to ieee 802.3u, as follow
    // 100M-full, 100M-half, 10M-full and last 10M-half
    
    lpa &= adv;
    
    if (bmcr & BMCR_SPEED100)
    {
        if (bmcr & BMCR_FULLDPLX)
        {
            if (lpa & LPA_100FULL)
            {
                *speed = true;
                *full  = true;
            }
            else if (lpa & LPA_100HALF)
            {
                *speed = true;
                *full  = false;
            }
            else if (lpa & LPA_10FULL)
            {
                *speed = false;
                *full  = true;
            }
            else if (lpa & LPA_10HALF)
            {
                *speed = false;
                *full  = false;
            }
            else {
                return -EINVAL;
            }
        }
        else
        {
            if (lpa & LPA_100HALF)
            {
                *speed = true;
                *full  = false;
            }
            else if (lpa & LPA_10HALF)
            {
                *speed = false;
                *full  = false;
            }
            else {
                return -EINVAL;
            }
        }
    }
    else
    {
        if (bmcr & BMCR_FULLDPLX)
        {
            if (lpa & LPA_10FULL)
            {
                *speed = false;
                *full  = true;
            }
            else if (lpa & LPA_10HALF)
            {
                *speed = false;
                *full  = false;
            }
            else {
                return -EINVAL;
            }
        }
        else
        {
            if (lpa & LPA_10HALF)
            {
                *speed = false;
                *full  = false;
            }
            else {
                return -EINVAL;
            }
        }
    }
    
    return 0;
}

static int ethernet_phy_set_mode(bool speed, bool duplex)
{
    u16 bmcr, eval;
    int ret;

    ret = ethernet_read_phy_reg(MII_BMCR, &bmcr);
    
    if (ret) {
        return ret;
    }

    eval = bmcr & ~(BMCR_ANENABLE | BMCR_SPEED100 | BMCR_FULLDPLX);
    
    if (speed) {
        eval |= BMCR_SPEED100;
    }
    
    if (duplex) {
        eval |= BMCR_FULLDPLX;
    }
    
    if (eval != bmcr) {
        ret = ethernet_write_phy_reg(MII_BMCR, eval);
    }

    return ret;
}

static int ethernet_phy_start_autoneg(void)
{
    int ret;
    u16 adv;
    
    ret = ethernet_read_phy_reg(MII_ADVERTISE, &adv);
    
    if (ret) {
        return ret;
    }
    
    adv |= ADVERTISE_ALL | ADVERTISE_PAUSE_CAP;
    
    ret = ethernet_write_phy_reg(MII_ADVERTISE, adv);
    
    if (ret) {
        return ret;
    }
    
    return ethernet_write_phy_reg(MII_BMCR, BMCR_ANENABLE | BMCR_ANRESTART);
}

static inline void raw_rx_buffer_init(void)
{
    volatile struct ethernet_buffer_desc * rx_bds = ethernet_rx_buf;
    int i;

    for (i = 0; i < RX_RING_SIZE; i++)
    {
        rx_bds[i] = (struct ethernet_buffer_desc) {
            .status = RXBD_STAT_OWN,
            .control = RXBD_CTRL_RBS1(PKG_MAX_LEN),
            .buf_addr = 0,
            .reserved = 0
        };
    }
    
    rx_bds[i - 1].control |= RXBD_CTRL_RER;
}

static inline void raw_tx_buffer_init(void)
{
    volatile struct ethernet_buffer_desc * tx_bds = ethernet_tx_buf;
    int i;

    for (i = 0; i < TX_RING_SIZE; i++)
    {
        tx_bds[i] = (struct ethernet_buffer_desc) {
            .status = 0,
            .control = TXBD_CTRL_IC,
            .buf_addr = 0,
            .reserved = 0
        };
    }
    
    tx_bds[i - 1].control |= TXBD_CTRL_TER;
}

static inline void free_rxtx_skbs(struct sk_buff **array, int len)
{
    int i;

    for (i = 0; i < len; i++)
    {
        if (NULL != array[i])
        {
            dev_kfree_skb_any(array[i]);
            array[i] = NULL;
        }
    }
}

static struct sk_buff * get_skb_aligned(unsigned int len)
{
    int offset;
    struct sk_buff *skb = NULL;

    if (NULL == (skb = dev_alloc_skb(len))) {
        return NULL;
    }
    
    offset = (unsigned long) skb->data & EC_SKB_ALIGN_BITS_MASK;
    
    if (unlikely(offset)) {
        skb_reserve(skb, EC_SKB_ALIGNED - offset);
    }

    return skb;
}

static char * ethernet_build_setup_frame(char * buffer, int buf_len)
{
    char broadcast_mac[ETH_MAC_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    char * frame = buffer;
    char * mac;
    
    memset(frame, 0, SETUP_FRAME_LEN);


    mac = (char *) mac_addr;

    COPY_MAC_ADDR(frame, mac);
    
    mac = broadcast_mac;
    
    COPY_MAC_ADDR(frame, mac);

    return buffer;
}

static int ethernet_transmit_setup_frame(void)
{
    volatile struct ethernet_buffer_desc * buf_des = ethernet_cur_tx;
    struct sk_buff * skb = NULL;
    
    if (ethernet_tx_full) {
        return -ENOMEM;
    }
    
    // will build a setup-frame in a skb
    
    skb = get_skb_aligned(SETUP_FRAME_RESV_LEN);
    
    if (!skb) {
        return -ENOMEM;
    }

    skb_put(skb, SETUP_FRAME_LEN);
    
    if (!ethernet_build_setup_frame(skb->data, SETUP_FRAME_LEN))
    {
        dev_kfree_skb_any(skb);
        return -ENOMEM;
    }
    
    // send it out as normal packet
    
    ethernet_tx_skb[ethernet_skb_cur] = skb;
    ethernet_skb_cur = (ethernet_skb_cur + 1) & TX_RING_MOD_MASK;
    
    // Push the data cache so the NIC does not get stale memory data

    buf_des->buf_addr = dma_map_single(&ethernet_dev->dev, skb->data, PKG_MAX_LEN, DMA_TO_DEVICE);
    
    buf_des->control &= (TXBD_CTRL_TER | TXBD_CTRL_IC); // maintain these bits
    buf_des->control |= TXBD_CTRL_SET;
    buf_des->control |= TXBD_CTRL_TBS1(SETUP_FRAME_LEN);
    mb();
    
    buf_des->status = TXBD_STAT_OWN;
    mb();
    
    // when call the routine, TX and Rx should have already stopped
    
    act_writel(act_readl(MAC_CSR6) | EC_OPMODE_ST, MAC_CSR6);
    
    act_writel(EC_TXPOLL_ST, MAC_CSR1);

    if (buf_des->control & TXBD_CTRL_TER) {
        ethernet_cur_tx = ethernet_tx_buf;
    }
    else {
        ethernet_cur_tx++;
    }
    
    if (ethernet_cur_tx == ethernet_dirty_tx) {
        ethernet_tx_full = true;
    }

    // resume old status
    
    act_writel(act_readl(MAC_CSR6) & ~EC_OPMODE_ST, MAC_CSR6);
	
    return 0;
}

static void subisr_enet_tx(void)
{
    volatile struct ethernet_buffer_desc * bdp;
    volatile struct ethernet_buffer_desc * bdp_next;
    struct sk_buff *skb;
    unsigned long status;
    unsigned long flags;
    
    spin_lock_irqsave(&ethernet_lock, flags);
    
    bdp = ethernet_dirty_tx;
    
    // don't enable CSR11 interrupt mitigation
    
    if (0 == ((status = bdp->status) & TXBD_STAT_OWN))
    {
        skb = ethernet_tx_skb[ethernet_skb_dirty];
        
        dma_unmap_single(&ethernet_dev->dev, bdp->buf_addr, skb->len, DMA_TO_DEVICE);
        
        // check for errors
        
        if (status & TXBD_STAT_ES)
        {
            ethernet_dev->stats.tx_errors++;
            
            if (status & TXBD_STAT_UF) {
                ethernet_dev->stats.tx_fifo_errors++;
            }
            
            if (status & TXBD_STAT_EC) {
                ethernet_dev->stats.tx_aborted_errors++;
            }
            
            if (status & TXBD_STAT_LC) {
                ethernet_dev->stats.tx_window_errors++;
            }
            
            if (status & TXBD_STAT_NC) {
                ethernet_dev->stats.tx_heartbeat_errors++;
            }
            
            if (status & TXBD_STAT_LO) {
                ethernet_dev->stats.tx_carrier_errors++;
            }
        } 
		else {
            ethernet_dev->stats.tx_packets++;
        }

        // some collisions occurred, but sent packet ok eventually
        
        if (status & TXBD_STAT_DE) {
            ethernet_dev->stats.collisions++;
        }

        dev_kfree_skb_any(skb);

        ethernet_tx_skb[ethernet_skb_dirty] = NULL;
        ethernet_skb_dirty = (ethernet_skb_dirty + 1) & TX_RING_MOD_MASK;
        
        if (bdp->control & TXBD_CTRL_TER) {
            bdp = ethernet_tx_buf;
        }
        else {
            bdp++;
        }

        if (ethernet_tx_full)
        {
            ethernet_tx_full = false;
            
            if (netif_queue_stopped(ethernet_dev)) {
                netif_wake_queue(ethernet_dev);
            }
        }
        
        if (netif_queue_stopped(ethernet_dev)) {
	        netif_wake_queue(ethernet_dev);
	    }
    }
    else if (ethernet_tx_full)
    {
        // handle the case that bdp->status own bit not cleared by hw but the interrupt still comes
        
        if (bdp->control & TXBD_CTRL_TER) {
            bdp_next = ethernet_tx_buf;
        }
        else {
            bdp_next = bdp + 1;
        }

        while (bdp_next != bdp)
        {
            // when tx full, if we find that some bd(s) has own bit is 0, which
            // indicates that mac hw has transmitted it but the own bit left not cleared.
            
            if (!(bdp_next->status & TXBD_STAT_OWN))
            {
                bdp->status &= ~TXBD_STAT_OWN; // clear own bit
                
                skb = ethernet_tx_skb[ethernet_skb_dirty];
                
                dma_unmap_single(&ethernet_dev->dev, bdp->buf_addr, skb->len, DMA_TO_DEVICE);
                
                dev_kfree_skb_any(skb);
                
                ethernet_tx_skb[ethernet_skb_dirty] = NULL;
                
                ethernet_skb_dirty = (ethernet_skb_dirty + 1) & TX_RING_MOD_MASK;
                
                if (bdp->control & TXBD_CTRL_TER) {
                    bdp = ethernet_tx_buf;
                }
                else {
                    bdp++;
                }

                ethernet_tx_full = false;
                
                if (netif_queue_stopped(ethernet_dev)) {
                    netif_wake_queue(ethernet_dev);
                }
                
                break;
            }
            if (bdp_next->control & TXBD_CTRL_TER) {
                bdp_next = ethernet_tx_buf;
            }
            else {
                bdp_next++;
            }
        }
    }

    ethernet_dirty_tx = bdp;

    spin_unlock_irqrestore(&ethernet_lock, flags);
}

static void subisr_enet_rx(void)
{
	volatile struct ethernet_buffer_desc * bdp;
	
	struct sk_buff *new_skb;
	struct sk_buff *skb_to_upper;
	unsigned long status;
	unsigned int pkt_len;
	int index;
	
	unsigned long flags;
    
    spin_lock_irqsave(&ethernet_lock, flags);
	
#define RX_ERROR_CARED \
	(RXBD_STAT_DE | RXBD_STAT_RF | RXBD_STAT_TL | RXBD_STAT_CS \
	| RXBD_STAT_DB | RXBD_STAT_CE | RXBD_STAT_ZERO)
	
	bdp = ethernet_cur_rx;
	
	while (0 == ((status = bdp->status) & RXBD_STAT_OWN))
	{
	    if (!ethernet_phy_linked)
	    {
	        goto rx_done;
	    }
	
		if (status & RX_ERROR_CARED)
		{
			ethernet_dev->stats.rx_errors++;
			
			if (status & RXBD_STAT_TL) {
				ethernet_dev->stats.rx_length_errors++;
			}
				
			if (status & RXBD_STAT_CE) {
				ethernet_dev->stats.rx_crc_errors++;
			}
				
			if (status & (RXBD_STAT_RF | RXBD_STAT_DB)) {
				ethernet_dev->stats.rx_frame_errors++;
			}
				
			if (status & RXBD_STAT_ZERO) {
				ethernet_dev->stats.rx_fifo_errors++;
			}
				
			if (status & RXBD_STAT_DE) {
				ethernet_dev->stats.rx_over_errors++;
			}
				
			if (status & RXBD_STAT_CS) {
				ethernet_dev->stats.collisions++;
			}
				
			goto rx_done;
		}

		pkt_len = RXBD_STAT_FL(status);
		
		if (pkt_len > ETH_PKG_MAX) { // assure skb_put() not panic

			ethernet_dev->stats.rx_length_errors++;
			goto rx_done;
		}

		if (NULL == (new_skb = get_skb_aligned(PKG_MAX_LEN))) {
			ethernet_dev->stats.rx_dropped++;

			goto rx_done;
		}

		dma_unmap_single(&ethernet_dev->dev, bdp->buf_addr, PKG_MAX_LEN, DMA_FROM_DEVICE);

		ethernet_dev->stats.rx_packets++;
		ethernet_dev->stats.rx_bytes += pkt_len;

		index = bdp - ethernet_rx_buf;
		
		skb_to_upper = ethernet_rx_skb[index];
		
		ethernet_rx_skb[index] = new_skb;
		
		skb_put(skb_to_upper, pkt_len - ETH_CRC_LEN); // modify its data length, remove CRC
		
		skb_to_upper->protocol = eth_type_trans(skb_to_upper, ethernet_dev);
		
		netif_rx(skb_to_upper);
		
		bdp->buf_addr = dma_map_single(&ethernet_dev->dev, new_skb->data, PKG_MAX_LEN, DMA_FROM_DEVICE);
		
rx_done:
		// mark MAC AHB owns the buffer, and clear other status
		bdp->status = RXBD_STAT_OWN;

		if (bdp->control & RXBD_CTRL_RER) {
			bdp = ethernet_rx_buf;
		}
		else {
			bdp++;
		}
	}
	
	ethernet_cur_rx = bdp;
	
	spin_unlock_irqrestore(&ethernet_lock, flags);
}

static irqreturn_t ethernet_isr(int irq, void *cookie)
{
    unsigned long status = 0;
    unsigned long intr_bits = 0;
    
    static unsigned long tx_cnt = 0, rx_cnt = 0;
	unsigned long mac_status;
	
	int ru_cnt = 0;
	
	intr_bits = EC_STATUS_NIS | EC_STATUS_AIS;
	
    disable_irq_nosync(ethernet_dev->irq);
    
    while ((status = act_readl(MAC_CSR5)) & intr_bits)
    {
        act_writel(status, MAC_CSR5); // clear status
        
        if (status & (EC_STATUS_TI | EC_STATUS_ETI))
        {
            subisr_enet_tx();
            
			tx_cnt = 0;
			
			mac_status = status & EC_STATUS_RSM;
			
			if((mac_status == EC_RX_fetch_dsp)||(mac_status == EC_RX_run_dsp)||(mac_status == EC_RX_close_dsp)) {
			    rx_cnt++;
			}
        }

        // RI & RU may come at same time, if RI handled, then RU needn't handle.
        // If RU comes & RI not comes, then we must handle RU interrupt.
        
        if (status & EC_STATUS_RI)
        {
            subisr_enet_rx();
            
			rx_cnt = 0;
			
			mac_status = status & EC_STATUS_TSM;
			
			if((mac_status == EC_STATUS_TSM) || (mac_status == EC_TX_run_dsp)) {
				tx_cnt++;
			}
        }
        else if (status & EC_STATUS_RU)
        {
            ru_cnt++;
            
            // set RPD could help if rx suspended & bd available
            
            if (ru_cnt == 2) {
                act_writel(0x1, MAC_CSR2);
            }
            
            subisr_enet_rx();
            
            // guard against too many RU interrupts to avoid long time ISR handling
            
            if (ru_cnt > 3) {
                break;
            }
        }
    }
    
    enable_irq(ethernet_dev->irq);
	
    return IRQ_HANDLED;
}

static int ethernet_netdev_start_xmit(struct sk_buff * skb, 
                                      struct net_device * _unused)
{
    struct device * dev = &ethernet_dev->dev;
    unsigned long flags;
    dma_addr_t tmp;
    
    spin_lock_irqsave(&ethernet_lock, flags);
    
    if (ethernet_tx_full)
    {
		ERR_MSG("TX buffer list is full\n");
		dev_kfree_skb(skb);
        goto out;
    }
    
    if (!ethernet_phy_linked) // this is not an error
    {
        INFO_MSG("link is already down");
        dev_kfree_skb(skb);
        goto out;
    }
    
    if(skb->len > 1518)
    {
		ERR_MSG("Invalid TX length\n");
		dev_kfree_skb(skb);
        goto out;
	}
    
    tmp = dma_map_single(dev, skb->data, skb->len, DMA_TO_DEVICE);
    
    if (dma_mapping_error(dev, tmp))
    {
        ERR_MSG("DMA map single failed\n");
        dev_kfree_skb(skb);
        goto out;
    }
    
    ethernet_cur_tx->buf_addr = (u32)tmp;
    ethernet_cur_tx->status = 0;
    
    ethernet_cur_tx->control &= TXBD_CTRL_IC | TXBD_CTRL_TER;
    ethernet_cur_tx->control |= TXBD_CTRL_TBS1(skb->len);
    ethernet_cur_tx->control |= TXBD_CTRL_FS | TXBD_CTRL_LS;
    mb();
    
    ethernet_cur_tx->status = TXBD_STAT_OWN;
    mb();
    
    act_writel(EC_TXPOLL_ST, MAC_CSR1);

    ethernet_dev->stats.tx_bytes += skb->len;
    
    ethernet_tx_skb[ethernet_skb_cur] = skb;
    ethernet_skb_cur = (ethernet_skb_cur + 1) & TX_RING_MOD_MASK;
    
    if (!(act_readl(MAC_CSR5) & EC_STATUS_TSM)) {
        ERR_MSG("TX stopped\n");
    }
    
    if (ethernet_cur_tx->control & TXBD_CTRL_TER) {
        ethernet_cur_tx = ethernet_tx_buf;
    }
    else {
        ethernet_cur_tx++;
    }
    
    if (ethernet_cur_tx == ethernet_dirty_tx)
    {
        ERR_MSG("TX is full skb_dirty\n");
        ethernet_tx_full = true;
    }
    
    netif_stop_queue(ethernet_dev); 
    
out:    
    spin_unlock_irqrestore(&ethernet_lock, flags);
    
    return NETDEV_TX_OK;
}

static int ethernet_netdev_open(struct net_device * _unused)
{
    unsigned long flags;
    int ret = 0;
    
    INFO_MSG("Device opened\n");
    
    spin_lock_irqsave(&ethernet_lock, flags);
    
    if (ethernet_device_opened)
    {
        ret = -EBUSY;
        ERR_MSG("Device already opened\n");
        goto out;
    }
    
    ethernet_device_opened = true;
    
    netif_start_queue(ethernet_dev);
    
out:
    spin_unlock_irqrestore(&ethernet_lock, flags);
    
    return ret;
}

static int ethernet_netdev_close(struct net_device *dev)
{
    unsigned long flags;
    
    INFO_MSG("Device closed\n");
    
    spin_lock_irqsave(&ethernet_lock, flags);
    
    if (ethernet_device_opened)
    {
        ethernet_device_opened = false;
        netif_stop_queue(ethernet_dev);
    }
    
    spin_unlock_irqrestore(&ethernet_lock, flags);
    
    return 0;
}

static int ethernet_mac_setup(void)
{
	// hardware soft reset and set bus mode
	
	act_writel(act_readl(MAC_CSR0) | EC_BMODE_SWR, MAC_CSR0);
	
	do {
		udelay(10);
	} while (act_readl(MAC_CSR0) & EC_BMODE_SWR);
	
	// select clk input from external phy

	act_writel(act_readl(MAC_CTRL) | (0x1 << 1), MAC_CTRL);
	
	act_writel(0, MAC_CSR10);
	
	act_writel(0xcc000000, MAC_CSR10);
	
	// set flow control mode and force transmiter to pause about 100ms
	
	act_writel(EC_CACHETHR_CPTL(0x0) | EC_CACHETHR_CRTL(0x0) | 
	           EC_CACHETHR_PQT(0x4FFF), MAC_CSR18);
	
    act_writel(EC_FIFOTHR_FPTL(0x40) | EC_FIFOTHR_FRTL(0x10), MAC_CSR19);
    
    act_writel(EC_FLOWCTRL_ENALL, MAC_CSR20);
    
    // always set to work as full duplex because of a MAC bug
    
    act_writel(act_readl(MAC_CSR6) | EC_OPMODE_FD, MAC_CSR6); //Full-duplex
    
    act_writel(act_readl(MAC_CSR6) & ~EC_OPMODE_10M, MAC_CSR6); //100M
	
	act_writel(act_readl(MAC_CSR6) & (~EC_OPMODE_PR), MAC_CSR6);
	
	//interrupt mitigation control register
	//NRP =7, RT =1, CS=0
	
	act_writel(0x004e0000, MAC_CSR11);
	
	// set mac address
	
    act_writel(*(u32 *)(mac_addr), MAC_CSR16);
    act_writel(*(u16 *)(mac_addr + 4), MAC_CSR17);

    // stop tx and rx and disable interrupts
    
    act_writel(act_readl(MAC_CSR6) & (~(EC_OPMODE_ST | EC_OPMODE_SR)), MAC_CSR6);
    act_writel(0, MAC_CSR7);
    
	return 0;
}

static int ethernet_mac_start(bool speed, bool duplex)
{
	int ret;
	
	ret = ethernet_rx_buffer_alloc();
	
	if (ret)
	{
        ERR_MSG("Could not setup RX buffer\n");
        return ret;
    }
    
    ret = ethernet_tx_buffer_alloc();
    
    if (ret)
    {
        ERR_MSG("Could not setup TX buffer\n");
        return ret;
    }

	// hardware soft reset and set bus mode
	
	act_writel(act_readl(MAC_CSR0) | EC_BMODE_SWR, MAC_CSR0);
	
	do {
		udelay(10);
	} while (act_readl(MAC_CSR0) & EC_BMODE_SWR);
	
	// select clk input from external phy

	act_writel(act_readl(MAC_CTRL) | (0x1 << 1), MAC_CTRL);
	
	act_writel(0, MAC_CSR10);
	
	act_writel(0xcc000000, MAC_CSR10);
	
	// physical buffer address
	
	act_writel((u32)(ethernet_tx_buf_paddr), MAC_CSR4);
	act_writel((u32)(ethernet_rx_buf_paddr), MAC_CSR3);
	
	// set flow control mode and force transmiter to pause about 100ms
	
	act_writel(EC_CACHETHR_CPTL(0x0) | EC_CACHETHR_CRTL(0x0) | 
	           EC_CACHETHR_PQT(0x4FFF), MAC_CSR18);
	
    act_writel(EC_FIFOTHR_FPTL(0x40) | EC_FIFOTHR_FRTL(0x10), MAC_CSR19);
    
    act_writel(EC_FLOWCTRL_ENALL, MAC_CSR20);
    
    // always set to work as full duplex because of a MAC bug
    
    act_writel(act_readl(MAC_CSR6) | EC_OPMODE_FD, MAC_CSR6); //Full-duplex
    
    if (speed) {
	    act_writel(act_readl(MAC_CSR6) & ~EC_OPMODE_10M, MAC_CSR6); //100M
	}
	else {
	    act_writel(act_readl(MAC_CSR6) | EC_OPMODE_10M, MAC_CSR6); //10M
	}
	
	act_writel(act_readl(MAC_CSR6) & (~EC_OPMODE_PR), MAC_CSR6);
	
	//interrupt mitigation control register
	//NRP =7, RT =1, CS=0
	
	act_writel(0x004e0000, MAC_CSR11);
	
	// set mac address
    act_writel(*(u32 *)(mac_addr), MAC_CSR16);
    act_writel(*(u16 *)(mac_addr + 4), MAC_CSR17);
    
    // send out a mac setup frame
    
    ret = ethernet_transmit_setup_frame();
    
    if (ret)
    {
        ERR_MSG("Could not transmit setup frame\n");
        return ret;
    }
    
    // rquest irq
    
    ret = request_irq(ethernet_dev->irq, (irq_handler_t)ethernet_isr, 
                      IRQF_TRIGGER_HIGH, "ethernet-isr", ethernet_dev);
    
    if (ret)
    {
        ERR_MSG("Unable to request IRQ\n");
        return ret;
    }
    
    // enable interrupts and start to tx and rx packets
    act_writel(EC_IEN_ALL, MAC_CSR7);
    act_writel(act_readl(MAC_CSR6) | EC_OPMODE_ST | EC_OPMODE_SR, MAC_CSR6);
    
    INFO_MSG("MAC STARTED OK!");
	return 0;
}

static int ethernet_mac_stop(void)
{
	// stop tx and rx and disable interrupts
	act_writel(act_readl(MAC_CSR6) & (~(EC_OPMODE_ST | EC_OPMODE_SR)), MAC_CSR6);
	act_writel(0x0, MAC_CSR7);
	
	// free IRQ
	free_irq(ethernet_dev->irq, ethernet_dev);
	
	// free buffers
	ethernet_tx_buffer_free();
	ethernet_rx_buffer_free();
	return 0;
}

static int ethernet_phy_setup(void)
{
    u32 cnt, phy_id;
	u16 reg_val;
	ethernet_phy_linked = false;
	
	if (gpio_is_valid(global_phy_gpio.phy_power_gpio))
	{ 
		gpio_set_value(global_phy_gpio.phy_power_gpio, 1);
	}
	else {
		ERR_MSG("phy_power_gpio is no Valid");
		return -ENOMEM;
	}
	
	if (gpio_is_valid(global_phy_gpio.phy_reset_gpio))
	{
		gpio_set_value(global_phy_gpio.phy_reset_gpio, 1);
		mdelay(150);//time for power up
		gpio_set_value(global_phy_gpio.phy_reset_gpio, 0);
		mdelay(12);//time for reset
		gpio_set_value(global_phy_gpio.phy_reset_gpio, 1);
		
		INFO_MSG("Reset_Done");
	}
	else {
		ERR_MSG("phy_reset_gpio is no Valid");
		return -ENOMEM;
	}
	
	//time required to access registers
	mdelay(150);
	
	//soft reset is not necessary
	//ethernet_phy_reg_set_bits(MII_BMCR, BMCR_RESET);
	//mdelay(50);
	
	cnt = 0;
	
	INFO_MSG("ETHERNET POWERED UP");
	
	do {
		ethernet_read_phy_reg(MII_BMCR, &reg_val);
		//udelay(20);
		
		if(reg_val & BMCR_PDOWN) { //so power is off;
			//try to force the power on
			INFO_MSG("Power is off (Should not happen!");
			ethernet_phy_reg_set_bits(MII_BMCR, !BMCR_PDOWN);
			mdelay(20);
		}
		if(reg_val & BMCR_RESET) { //so RESET is enable
			//try to force out of reset state
			INFO_MSG("Device is in Reset(should not happen!)");
			ethernet_phy_reg_set_bits(MII_BMCR, BMCR_RESET);
			mdelay(20);
		}
		if (cnt++ > 60)
		{
			ERR_MSG("Ethernet phy BMCR_RESET timedout\n");
			return -ETIMEDOUT;
		}
		
	} while ((reg_val & BMCR_PDOWN) & (reg_val & BMCR_RESET));
	
	// get the phy ID
	
	ethernet_read_phy_reg(MII_PHYSID1, &reg_val);
	phy_id = ((u32)(reg_val) & 0xFFFF) << 16;
	ethernet_read_phy_reg(MII_PHYSID2, &reg_val);
	phy_id |= (u32)(reg_val) & 0xFFFF;
	phy_id &= PHY_ID_MASK;
	
	INFO_MSG("PHY ID = 0x%x\n", phy_id);
	
	if (phy_id != ETH_PHY_ID_RTL8201) {
	    ERR_MSG("Invalid phy ID\n");
	    return -ENODEV;
	}
	
	ethernet_write_phy_reg(PHY_RTL8201F_REG_PAGE_SELECT, 
	                       PHY_RTL8201F_REG_PAGE_SELECT_SEVEN);
	
	ethernet_write_phy_reg(PHY_RTL8201F_REG_INT_LED_FUNC, 
	                       PHY_RTL8201F_PIN_LINK_STATE_CHANGE);
	
	ethernet_write_phy_reg(PHY_RTL8201F_REG_RMSR, 
	                       PHY_RTL8201F_RMSR_CLK_DIR_INPUT | 
	                       PHY_RTL8201F_RMSR_RMII_MODE | 
	                       PHY_RTL8201F_RMSR_RMII_RX_OFFSET | 
	                       PHY_RTL8201F_RMSR_RMII_TX_OFFSET);
	
	ethernet_write_phy_reg(PHY_RTL8201F_REG_PAGE_SELECT, 
	                       PHY_RTL8201F_REG_PAGE_SELECT_ZERO);
	
	ethernet_phy_start_autoneg();
	
	INFO_MSG("Phy is not linked\n");
	    
	
	netif_carrier_off(ethernet_dev);
	
	schedule_delayed_work(&ethernet_phy_state_work, 
	                      msecs_to_jiffies(PHY_POLL_INTERVAL));
	
	return 0;
}

static void ethernet_tx_buffer_free(void)
{
    const u32 total = sizeof(struct ethernet_buffer_desc) * TX_RING_SIZE;
    dma_addr_t * paddr = &ethernet_tx_buf_paddr, tmp;
    struct device * dev = &ethernet_dev->dev;
    int i;
    
    if (!ethernet_tx_buf) {
        return;
    }
    
    for (i = 0; i < TX_RING_SIZE; i++)
    {
        if (ethernet_tx_buf[i].buf_addr)
        {
            tmp = (dma_addr_t)(ethernet_tx_buf[i].buf_addr);
            dma_unmap_single(dev, tmp, PKG_MAX_LEN, DMA_TO_DEVICE);
            ethernet_tx_buf[i].buf_addr = 0;
        }
        
        if (ethernet_tx_skb[i])
        {
            dev_kfree_skb_any(ethernet_tx_skb[i]);
            ethernet_tx_skb[i] = NULL;
        }
    }
    
    dma_free_coherent(NULL, total, ethernet_tx_buf, (*paddr));
    ethernet_tx_buf = NULL;
    (*paddr) = (dma_addr_t)(0);
}

static int ethernet_tx_buffer_alloc(void)
{
    const u32 total = sizeof(struct ethernet_buffer_desc) * TX_RING_SIZE;
    dma_addr_t * paddr = &ethernet_tx_buf_paddr;
    int i;
    
    for (i = 0; i < TX_RING_SIZE; i++) {
        ethernet_tx_skb[i] = NULL;
    }
    
    //GFP_ATOMIC
    //ethernet_tx_buf = dma_alloc_coherent(NULL, total, paddr, GFP_KERNEL);
    ethernet_tx_buf = dma_alloc_coherent(NULL, total, paddr, GFP_NOWAIT);
    if (!ethernet_tx_buf)
    {
        ethernet_tx_buf = NULL;
        *paddr = (dma_addr_t)(0);
        return -ENOMEM;
    }
    
    ethernet_cur_tx    = ethernet_tx_buf;
    ethernet_dirty_tx  = ethernet_tx_buf;
    ethernet_skb_cur   = 0;
    ethernet_skb_dirty = 0;
    ethernet_tx_full   = false;
    
    for (i = 0; i < TX_RING_SIZE; i++)
    {
        ethernet_tx_buf[i].status   = 0;
        ethernet_tx_buf[i].control  = TXBD_CTRL_IC;
        ethernet_tx_buf[i].buf_addr = 0;
        ethernet_tx_buf[i].reserved = 0;
    }
    
    ethernet_tx_buf[i - 1].control |= TXBD_CTRL_TER;
    return 0;
}

static void ethernet_rx_buffer_free(void)
{
    const u32 total = sizeof(struct ethernet_buffer_desc) * RX_RING_SIZE;
    dma_addr_t * paddr = &ethernet_rx_buf_paddr, tmp;
    struct device * dev = &ethernet_dev->dev;
    int i;
    
    if (!ethernet_rx_buf) {
        return;
    }
    
    for (i = 0; i < RX_RING_SIZE; i++)
    {
        if (ethernet_rx_buf[i].buf_addr)
        {
            tmp = (dma_addr_t)(ethernet_rx_buf[i].buf_addr);
            dma_unmap_single(dev, tmp, PKG_MAX_LEN, DMA_FROM_DEVICE);
            ethernet_rx_buf[i].buf_addr = 0;
        }
        
        if (ethernet_rx_skb[i])
        {
            dev_kfree_skb_any(ethernet_rx_skb[i]);
            ethernet_rx_skb[i] = NULL;
        }
    }
    
    dma_free_coherent(NULL, total, ethernet_rx_buf, (*paddr));
    ethernet_rx_buf = NULL;
    (*paddr) = (dma_addr_t)(0);
}

static int ethernet_rx_buffer_alloc(void)
{
    const u32 total = sizeof(struct ethernet_buffer_desc) * RX_RING_SIZE;
    dma_addr_t * paddr = &ethernet_rx_buf_paddr, tmp;
    struct device * dev = &ethernet_dev->dev;
    struct sk_buff * skb;
    int i;
    
    INFO_MSG("rx_buffer_alloc");
    
    for (i = 0; i < RX_RING_SIZE; i++) {
        ethernet_rx_skb[i] = NULL;
    }
    
    ethernet_rx_buf = dma_alloc_coherent(NULL, total, paddr, GFP_NOWAIT);
    
    if (!ethernet_rx_buf)
    {
        ethernet_rx_buf = NULL;
        *paddr = (dma_addr_t)(0);
        return -ENOMEM;
    }
    
    ethernet_cur_rx = ethernet_rx_buf;
    
    for (i = 0; i < RX_RING_SIZE; i++)
    {
        ethernet_rx_buf[i].status   = RXBD_STAT_OWN;
        ethernet_rx_buf[i].control  = RXBD_CTRL_RBS1(PKG_MAX_LEN);
        ethernet_rx_buf[i].buf_addr = 0;
        ethernet_rx_buf[i].reserved = 0;
    }
    
    ethernet_rx_buf[i - 1].control |= RXBD_CTRL_RER;
    
    for (i = 0; i < RX_RING_SIZE; i++)
    {
        ethernet_rx_skb[i] = skb = get_skb_aligned(PKG_MAX_LEN);
        
        if (skb)
        {
            tmp = dma_map_single(dev, skb->data, PKG_MAX_LEN, DMA_FROM_DEVICE);
            
            if (dma_mapping_error(dev, tmp))
            {
                ethernet_rx_buf[i].buf_addr = 0;
                ethernet_rx_buffer_free();
                return -ENOMEM;
            }
            
            ethernet_rx_buf[i].buf_addr = (u32)tmp;
        }
        else
        {
            ethernet_rx_buffer_free();
            return -ENOMEM;
        }
    }
    return 0;
}

static void ethernet_phy_state_worker(struct work_struct * _unused)
{
	bool linked, speed, full;
	unsigned long flags;
	int ret;
	
	spin_lock_irqsave(&ethernet_lock, flags);
	
	ret = ethernet_phy_get_link(&linked);
	
	if (ret) {
		ERR_MSG("Could not get phy link state\n");
		goto out;
	}
	
	if (linked != ethernet_phy_linked)
	{
		ethernet_phy_linked = linked;
		
		if (linked)
		{
			INFO_MSG("IS LINKED");
			ret = ethernet_phy_read_status(&speed, &full);
			
			if (ret)
			{
				ERR_MSG("Could not get phy aneg status\n");
				ethernet_phy_linked = false;
				goto out;
			}
			
            INFO_MSG("Phy is linked at %s %s-duplex\n", 
                     speed ? "100M" : "10M", full ? "full" : "half");
            
            ethernet_mac_start(speed, full);
            
            ethernet_phy_set_mode(speed, full);
            
            netif_carrier_on(ethernet_dev);
            INFO_MSG("phy_set_ok!"); 
            
        }
        else
        {

            INFO_MSG("Phy is not linked\n");
            
            netif_carrier_off(ethernet_dev);
            
            ethernet_mac_stop();
            
            ethernet_phy_start_autoneg();
        }
    }
    
out: 
    if(linked){//do nothing
    }
    else{
        schedule_delayed_work(&ethernet_phy_state_work, 
                          msecs_to_jiffies(PHY_POLL_INTERVAL));
    }
    spin_unlock_irqrestore(&ethernet_lock, flags);
}

static int ethernet_netdev_init(struct net_device * _unused)
{
	int ret;
	ret = ethernet_clock_enable(1);
	
	if (ret)
	{
		ERR_MSG("Could not enable ethernet clock\n");
		return ret;
	}
	
	ret = ethernet_mac_setup();
	
	if (ret)
	{
		ERR_MSG("Could not setup ethernet MAC\n");
		return ret;
	}
	
	ret = ethernet_phy_setup();
	
	if (ret)
	{
		ERR_MSG("Could not setup ethernet PHY\n");
		return ret;
	}
	return 0;
}

static void ethernet_netdev_uninit(struct net_device * _unused)
{
	return;
}

static int ethernet_netdev_set_mac_address(struct net_device *ndev, void *p)
{
	struct sockaddr *addr = p;
	char *mac = (char*)mac_addr; //pointer to global mac_address
	
	if (addr->sa_data)
	{
		memcpy(ndev->dev_addr, addr->sa_data, sizeof(default_mac_addr));
		
		if (!is_valid_ether_addr(ndev->dev_addr))
		{
			eth_random_addr(ndev->dev_addr);
			INFO_MSG("MAC Address is not valid, randomic was inserted instead!");
		}
		
		//update global mac_address variable
		memcpy(mac, ndev->dev_addr, sizeof(default_mac_addr));
		
		//set mac
		act_writel(*(u32 *)(mac_addr), MAC_CSR16);
		act_writel(*(u16 *)(mac_addr + 4), MAC_CSR17);
		
		//broke link
		ethernet_phy_start_autoneg();
		
		netif_carrier_off(ethernet_dev);
		
		INFO_MSG("Phy is not linked\n");
		INFO_MSG("New MAC: %X:%X:%X:%X:%X:%X", mac_addr[0], mac_addr[1], 
		         mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
		
		//get new link
		schedule_delayed_work(&ethernet_phy_state_work, 
		                      msecs_to_jiffies(PHY_POLL_INTERVAL));
	}
	else {
		INFO_MSG("MAC Address is NULL");
	}
	return 0;
}

static const struct net_device_ops ethernet_netdev_ops = {
	.ndo_init = ethernet_netdev_init,
	.ndo_uninit = ethernet_netdev_uninit,
	.ndo_open = ethernet_netdev_open,
	.ndo_stop = ethernet_netdev_close,
	.ndo_start_xmit = ethernet_netdev_start_xmit,
	.ndo_set_mac_address = ethernet_netdev_set_mac_address
};

static int eth_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret, irq;
	
	platform_set_drvdata(pdev, NULL);
	
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ethernet");
	
	if (!res) {
		dev_err(dev, "unable to get ioresource from dts\n");
		return -ENODEV;
	}
	
	base = devm_ioremap(dev, res->start, resource_size(res));
	
	if (!base) {
		dev_err(dev, "unable to remap io memory\n");
		return -ENOMEM;
	}
	
	eth_rst = devm_reset_control_get(dev, "ethernet");
	
	if (IS_ERR(eth_rst)) {
		dev_err(dev, "could not get ethernet reset control\n");
		return PTR_ERR(eth_rst);
	}
	
	// generate a random Mac Address to begin
	if(!mac_gen())
	{
		INFO_MSG("MAC ADDR: %X:%X:%X:%X:%X:%X", mac_addr[0], mac_addr[1],
		         mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);
	}
	
	irq = platform_get_irq(pdev, 0);
	
	if (irq < 0)
	{
		ERR_MSG("Could not get irq\n");
		ret = -ENODEV;
		goto out_error;
	}
	
	ret = ethernet_set_pin_mux(pdev);
	
	if (ret) {
		ERR_MSG("Could not set device pin mux\n");
		goto out_error;
	}
	
	ret = ethernet_get_clk(dev);
	
	if (ret) {
		ERR_MSG("Could not get device clock handler\n");
		goto out_error;
	}
	
	ethernet_dev = alloc_etherdev(0);
	
	if (!ethernet_dev) {
		ERR_MSG("Could not alloc ethernet device\n");
		ret = -ENOMEM;
		goto out_error;
	}
	
	if (of_property_read_u32(pdev->dev.of_node, "phy_addr", &ethernet_phy_addr))
	{
		ERR_MSG("Could not get phy addr from DTS\n");
		ret = -ENODEV;
		goto out_error;
	}
	
	global_phy_gpio.phy_reset_gpio = 
		of_get_named_gpio(dev->of_node, "phy-reset-gpio", 0);
	
	if (!gpio_is_valid(global_phy_gpio.phy_reset_gpio)) {
		dev_err(dev, "could not get reset gpio\n");
		return -ENODEV;
	}
	
	global_phy_gpio.phy_power_gpio = 
		of_get_named_gpio(dev->of_node, "phy-power-gpio", 0);
	
	if (!gpio_is_valid(global_phy_gpio.phy_power_gpio)) {
		dev_err(dev, "could not get power gpio\n");
		return -ENODEV;
	}
	
	ret = devm_gpio_request(dev, global_phy_gpio.phy_reset_gpio, "phy_reset");
	
	if (ret) {
		dev_err(dev, "could not request reset gpio\n");
		return ret;
	}
	
	ret = devm_gpio_request(dev, global_phy_gpio.phy_power_gpio, "phy_power");
	
	if (ret) {
		dev_err(dev, "could not request power gpio\n");
		return ret;
	}
	
	gpio_direction_output(global_phy_gpio.phy_reset_gpio, 0);
	gpio_direction_output(global_phy_gpio.phy_power_gpio, 0);
	
	gpio_set_value(global_phy_gpio.phy_power_gpio, 0);
	gpio_set_value(global_phy_gpio.phy_reset_gpio, 0);
	
	SET_NETDEV_DEV(ethernet_dev, &pdev->dev);
	
	sprintf(ethernet_dev->name, "eth0");
	
	ethernet_dev->irq = irq;
	ethernet_dev->watchdog_timeo = EC_TX_TIMEOUT;
	ethernet_dev->netdev_ops = &ethernet_netdev_ops;
	
	memcpy(ethernet_dev->dev_addr, mac_addr, sizeof(default_mac_addr));
	
	ret = register_netdev(ethernet_dev);
	
	if (ret) {
		ERR_MSG("Could not register netdev\n");
		goto out_error;
	}
	
	pm_runtime_dont_use_autosuspend(dev);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_get(dev);
	
	platform_set_drvdata(pdev, &global_phy_gpio);
	return 0;
	
out_error:
	
	if (ethernet_dev) {
		free_netdev(ethernet_dev);
		ethernet_dev = NULL;
	}
	
	if (global_phy_gpio.phy_power_gpio < 0) {
		gpio_free(global_phy_gpio.phy_power_gpio);
		global_phy_gpio.phy_power_gpio = -1;
	}
	
	if (global_phy_gpio.phy_reset_gpio < 0) {
		gpio_free(global_phy_gpio.phy_reset_gpio);
		global_phy_gpio.phy_reset_gpio = -1;
	}
	
	ethernet_put_clk();
	ethernet_put_pin_mux();
	return ret;
}

static int eth_driver_remove(struct platform_device *pdev)
{
	if (ethernet_dev)
	{
		unregister_netdev(ethernet_dev);
		free_netdev(ethernet_dev);
		ethernet_dev = NULL;
	}
	
	if (global_phy_gpio.phy_power_gpio < 0)
	{
		gpio_free(global_phy_gpio.phy_power_gpio);
		global_phy_gpio.phy_power_gpio = -1;
	}
	
	if (global_phy_gpio.phy_reset_gpio < 0)
	{
		gpio_free(global_phy_gpio.phy_reset_gpio);
		global_phy_gpio.phy_reset_gpio = -1;
	}
	
	ethernet_put_clk();
	ethernet_put_pin_mux();
	
	pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id eth_dt_match[]  = {
	{ .compatible = "caninos,ethernet-k5" },
	{ /* sentinel */ }
};

#ifdef CONFIG_PM_SLEEP
static int caninos_eth_suspend(struct device *dev)
{
	dev_info(dev, "eth suspend\n");
	return -EBUSY;
}

static int caninos_eth_resume(struct device *dev)
{
	dev_info(dev, "eth resume\n");
	return 0;
}
#else
#define caninos_eth_suspend NULL
#define caninos_eth_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(caninos_eth_pm_ops,
                         caninos_eth_suspend, caninos_eth_resume);

MODULE_DEVICE_TABLE(of, eth_dt_match);

static struct platform_driver __refdata eth_driver = {
	.probe = eth_driver_probe,
	.remove = eth_driver_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = eth_dt_match,
		.pm = &caninos_eth_pm_ops,
	},
};

static int __init caninos_module_init(void)
{
	return platform_driver_register(&eth_driver);
}

static void __exit caninos_module_fini(void)
{
	platform_driver_unregister(&eth_driver);
}

module_init(caninos_module_init);
module_exit(caninos_module_fini);

MODULE_LICENSE("GPL v2");
