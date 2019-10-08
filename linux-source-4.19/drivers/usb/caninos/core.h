#ifndef _CORE_H_
#define _CORE_H_

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/reset.h>

#define DRIVER_DESC "Caninos USB controller driver"
#define DRIVER_NAME "caninos-hcd"

struct caninos_ctrl
{
	int id;
	int irq;
	
	struct reset_control *rst;
	struct device *dev;
	
	void __iomem *base;
	void __iomem *usbecs;
	
	struct clk *clk_usbh_pllen;
	struct clk *clk_usbh_phy;
	
	struct usb_hcd *hcd;
};

extern struct kmem_cache *td_cache;

#define USB_HCD_IN_MASK  0x00
#define USB_HCD_OUT_MASK 0x10

#define	USB2_ECS_VBUS_P0		10
#define	USB2_ECS_ID_P0			12
#define USB2_ECS_LS_P0_SHIFT	8
#define USB2_ECS_LS_P0_MASK		(0x3<<8)
#define USB2_ECS_DPPUEN_P0     3
#define USB2_ECS_DMPUEN_P0     2
#define USB2_ECS_DMPDDIS_P0    1
#define USB2_ECS_DPPDDIS_P0    0
#define USB2_ECS_SOFTIDEN_P0   (1<<26)
#define USB2_ECS_SOFTID_P0     27
#define USB2_ECS_SOFTVBUSEN_P0 (1<<24)
#define USB2_ECS_SOFTVBUS_P0   25
#define USB2_PLL_EN0           (1<<12)
#define USB2_PLL_EN1           (1<<13)

#define HCIN0MAXPCK          0x000001E0
#define HCEP0CTRL            0x000000C0
#define HCEP0BINTERVAL       0x00000200
#define HCOUT1BINTERVAL      0x00000288
#define HCIN1SPILITCS        0x0000020B
#define HCOUT1CON            0x0000000E
#define HCIN1CON             0x0000000A
#define HCOUT1CS             0x0000000F
#define HCIN1CS              0x0000000B
#define HCOUT1BCL            0x0000000C
#define HCIN1BCL             0x00000008
#define HCOUT1CTRL           0x000000C4
#define HCOUT1MAXPCKL        0x000003E2
#define HCIN1MAXPCKL         0x000001E2
#define HCIN1CTRL            0x000000C6
#define HCOUT1STADDR         0x00000344
#define HCIN1STADDR          0x00000304
#define HCOUT1ADDR           0x00000289
#define HCOUT1PORT           0x0000028A
#define HCIN1ADDR            0x00000209
#define HCIN1PORT            0x0000020A
#define HCOUT1SPILITCS       0x0000028B
#define HCEP0SPILITCS        0x00000203
#define HCEP0PORT            0x00000202
#define HCEP0ADDR            0x00000201
#define FNADDR               0x000001A6
#define HCTRAINTERVAL		 0x000001A8
#define HCIN1DMACOMPLETECNT	 0x0000081C
#define	HCOUT1DMACOMPLETECNT 0x0000091C
#define	HCIN1DMACTRL		 0x00000818
#define	HCOUT1DMACTRL	     0x00000918
#define	HCIN1DMACURADDR		 0x00000814
#define HCOUT1DMACURADDR	 0x00000914
#define HCINxDMAIRQ0         0x00000404
#define HCFRMNRL             0x000001AC
#define	HCIN1DMALINKADDR	 0x00000810
#define	HCOUT1DMALINKADDR	 0x00000910
#define EP0INDATA_W0         0x00000100
#define EP0OUTDATA_W0        0x00000140
#define	HCIN0BC              0x00000000
#define	HCOUT0BC             0x00000001
#define EP0CS                0x00000002
#define HCOUT0ERR            0x000000C1
#define HCOUT1ERR            0x000000C5
#define HCOUT2ERR            0x000000C9
#define HCOUT3ERR            0x000000CD
#define HCOUT4ERR            0x000000D1
#define HCOUT5ERR            0x000000D5
#define HCOUT6ERR            0x000000D9
#define HCOUT7ERR            0x000000DD
#define HCOUT8ERR            0x000000E1
#define HCOUT9ERR            0x000000E5
#define HCOUT10ERR           0x000000E9
#define HCOUT11ERR           0x000000ED
#define HCOUT12ERR           0x000000F1
#define HCOUT13ERR           0x000000F5
#define HCOUT14ERR           0x000000F9
#define HCOUT15ERR           0x000000FD
#define HCIN0ERR             0x000000C3
#define HCIN1ERR             0x000000C7
#define HCIN2ERR             0x000000CB
#define HCIN3ERR             0x000000CF
#define HCIN4ERR             0x000000D3
#define HCIN5ERR             0x000000D7
#define HCIN6ERR             0x000000DB
#define HCIN7ERR             0x000000DF
#define HCIN8ERR             0x000000E3
#define HCIN9ERR             0x000000E7
#define HCIN10ERR            0x000000EB
#define HCIN11ERR            0x000000EF
#define HCIN12ERR            0x000000F3
#define HCIN13ERR            0x000000F7
#define HCIN14ERR            0x000000FB
#define HCIN15ERR            0x000000FF
#define IVECT                0x000001A0
#define ENDPRST              0x000001A2
#define USBCS                0x000001A3
#define OTGCTRL              0x000001BE
#define OTGSTATE             0x000001BD
#define HCOUTxIRQ0           0x00000188
#define HCOUTxIRQ1           0x00000189
#define HCINxIRQ0            0x0000018A
#define HCINxIRQ1            0x0000018B
#define USBIRQ               0x0000018C
#define HCOUTxIEN0           0x00000194
#define HCOUTxIEN1           0x00000195
#define HCINxIEN0            0x00000196
#define HCINxIEN1            0x00000197
#define USBIEN               0x00000198
#define HCPORTCTRL           0x000001AB
#define HCINxERRIRQ0         0x000001B4
#define HCINxERRIRQ1         0x000001B5
#define HCOUTxERRIRQ0        0x000001B6
#define HCOUTxERRIRQ1        0x000001B7
#define HCINxERRIEN0         0x000001B8
#define HCINxERRIEN1         0x000001B9
#define HCOUTxERRIEN0        0x000001BA
#define HCOUTxERRIEN1        0x000001BB
#define OTGIRQ               0x000001BC
#define OTGIEN               0x000001C0
#define TAAIDLBDIS           0x000001C1
#define TAWAITBCON           0x000001C2
#define TBVBUSPLS            0x000001C3
#define TBVBUSDISPLS         0x000001C7
#define HCOUTxDMAIRQ0        0x00000484
#define HCOUTxDMAIRQ1        0x00000485
#define HCOUTxDMAIEN0        0x00000486
#define HCOUTxDMAIEN1        0x00000487
#define USBERESET            0x00000500
#define TA_BCON_COUNT        0x00000501
#define VBUSDBCTIMERL        0x00000502
#define VDCTRL               0x00000504
#define USBEIRQ              0x0000050A
#define USBEIEN              0x0000050B
#define BKDOOR               0x00000506
#define HCINxSHORTPCKIRQ0    0x00000510
#define HCINxSHORTPCKIRQ1    0x00000511
#define HCINxSHORTPCKIEN0    0x00000512
#define HCINxSHORTPCKIEN1    0x00000513
#define HCINxZEROPCKIRQ0     0x00000514
#define HCINxZEROPCKIRQ1     0x00000515
#define HCINxZEROPCKIEN0     0x00000516
#define HCINxZEROPCKIEN1     0x00000517
#define HCOUTxBUFEMPTYIRQ0   0x00000518
#define HCOUTxBUFEMPTYIRQ1   0x00000519
#define HCOUTxBUFEMPTYIEN0   0x0000051A
#define HCOUTxBUFEMPTYIEN1   0x0000051B
#define HCOUTxBUFEMPTYCTRL0  0x0000051C
#define HCOUTxBUFEMPTYCTRL1  0x0000051D
#define	HCDMABCKDOOR         0x00000800
#define	HCDMAxOVERFLOWIRQ    0x00000808
#define HCDMAxOVERFLOWIEN    0x0000080C

#define DMACTRL_DMACS	(1 << 0)
#define DMACTRL_DMACC	(1 << 1)

/*EP0CS*/
/*bit 7 reserved*/
#define EP0CS_HCSETTOOGLE           (1 << 6)
#define EP0CS_HCCLRTOOGLE           (1 << 5)
#define EP0CS_HCSET                 (1 << 4)
#define EP0CS_HCINBSY		        (1 << 3)
#define	EP0CS_HCOUTBSY	            (1 << 2)
#define EP0CS_OUTBSY	            (1 << 3)
#define	EP0CS_INBSY	                (1 << 2)
#define	EP0CS_HSNAK	                (1 << 1)
#define	EP0CS_STALL	                (1 << 0)

/*ENDPRST*/
#define ENDPRST_EPNUM_MASK  0xf
#define ENDPRST_EPX(x)      ((x) & 0xf)
#define ENDPRST_IO          (1 << 4)
#define ENDPRST_FIFORST     (1 << 5)
#define ENDPRST_TOGRST      (1 << 6)
#define ENDPRST_TOGRST_R    (0x3 << 6)
/*bit 7 reserved*/

/*USBIRQ*/
#define	USBIRQ_HS     (1 << 5)
#define	USBIRQ_URES   (1 << 4)
#define	USBIRQ_SUSP   (1 << 3)
#define	USBIRQ_SUTOK  (1 << 2)
#define	USBIRQ_SOF    (1 << 1)
#define	USBIRQ_SUDAV  (1 << 0)

/*bit 7:4 reserved*/
/*OTGCTRL*/
#define	OTGCTRL_FORCEBCONN   (1 << 7)
/*bit 6 reserved*/
#define	OTGCTRL_SRPDATDETEN  (1 << 5)
#define	OTGCTRL_SRPVBUSDETEN (1 << 4)
#define	OTGCTRL_BHNPEN       (1 << 3)
#define	OTGCTRL_ASETBHNPEN   (1 << 2)
#define	OTGCTRL_ABUSDROP     (1 << 1)
#define	OTGCTRL_BUSREQ       (1 << 0)

/*USBIEN*/
/*bit 7:6 reserved*/
#define USBIEN_HS    (1 << 5)
#define USBIEN_URES  (1 << 4)
#define	USBIEN_SUSP  (1 << 3)
#define USBIEN_SUTOK (1 << 2)
#define	USBIEN_SOF   (1 << 1)
#define	USBIEN_SUDAV (1 << 0)

/*OTGIEN*/
/*bit 7:5 reserved*/
#define	OTGIEN_PERIPH  (1 << 4)
#define	OTGIEN_VBUSEER (1 << 3)
#define	OTGIEN_LOCSOF  (1 << 2)
#define	OTGIEN_SRPDET  (1 << 1)
#define	OTGIEN_IDLE    (1 << 0)

/*OTGIRQ*/
#define	OTGIRQ_PERIPH	            (1<<4)
#define	OTGIRQ_VBUSEER		        (1<<3)
#define	OTGIRQ_LOCSOF	            (1<<2)
#define	OTGIRQ_SRPDET		        (1<<1)
#define	OTGIRQ_IDLE		            (1<<0)

/* OTGSTATE value. */
/* extra dual-role default-b states */
/* dual-role default-a */
#define AOTG_STATE_A_IDLE		0
#define AOTG_STATE_A_WAIT_VRISE		1
#define AOTG_STATE_A_WAIT_BCON		2
#define AOTG_STATE_A_HOST		3
#define AOTG_STATE_A_SUSPEND		4
#define AOTG_STATE_A_PERIPHERAL		5
#define AOTG_STATE_A_VBUS_ERR		6
#define AOTG_STATE_A_WAIT_VFALL		7
/* single-role peripheral, and dual-role default-b */
#define AOTG_STATE_B_IDLE		8
#define AOTG_STATE_B_PERIPHERAL		9
#define AOTG_STATE_B_WAIT_ACON		10
#define AOTG_STATE_B_HOST		11
#define AOTG_STATE_B_SRP_INIT		12
#define AOTG_STATE_UNDEFINED		17

/* OTG external Registers USBEIRQ, USBEIEN. */
#define USBEIRQ_USBIRQ          (0x1 << 7)
#define USBEIRQ_USBIEN          (0x1 << 7)
#define RESUME_IRQIEN           (0x1 << 1)
#define SUSPEND_IRQIEN          (0x1 << 4)

/* IVECT, USB Interrupt Vector. */
#define UIV_SUDAV           0x01
#define UIV_SOF             0x02
#define UIV_SUTOK           0x03
#define UIV_SUSPEND         0x04
#define UIV_USBRESET        0x05
#define UIV_HSPEED          0x06

/* otg status. */
#define UIV_IDLE            0x07
#define UIV_SRPDET          0x08
#define UIV_LOCSOF          0x09
#define UIV_VBUSERR         0x0a
#define UIV_PERIPH          0x0b

#define UIV_HCOUT0ERR       0x10
#define UIV_EP0IN           0x20
#define UIV_HCEP0OUT        0x20
#define UIV_IN0TOKEN        0x30
#define UIV_HCIN0ERR        0x40
#define UIV_EP0OUT          0x50
#define UIV_HCEP0IN         0x50
#define UIV_OUT0TOKEN       0x60
#define UIV_EP0PING         0x70

#define UIV_HCOUT1ERR       0x11
#define UIV_EP1IN           0x21
#define UIV_HCEP1OUT        0x21
#define UIV_IN0T1KEN        0x31
#define UIV_HCIN1ERR        0x41
#define UIV_EP1OUT          0x51
#define UIV_HCEP1IN         0x51
#define UIV_OUT1TOKEN       0x61
#define UIV_EP1PING         0x71

#define UIV_HCOUT2ERR       0x12
#define UIV_EP2IN           0x22
#define UIV_HCEP2OUT        0x22
#define UIV_IN2TOKEN        0x32
#define UIV_HCIN2ERR        0x42
#define UIV_EP2OUT          0x52
#define UIV_HCEP2IN         0x52
#define UIV_OUT2TOKEN       0x62
#define UIV_EP2PING         0x72

#define UIV_HCOUT3ERR       0x13
#define UIV_EP3IN           0x23
#define UIV_HCEP3OUT        0x23
#define UIV_IN3TOKEN        0x33
#define UIV_HCIN3ERR        0x43
#define UIV_EP3OUT          0x53
#define UIV_HCEP3IN         0x53
#define UIV_OUT3TOKEN       0x63
#define UIV_EP3PING         0x73

#define UIV_HCOUT4ERR       0x14
#define UIV_EP4IN           0x24
#define UIV_HCEP4OUT        0x24
#define UIV_IN4TOKEN        0x34
#define UIV_HCIN4ERR        0x44
#define UIV_EP4OUT          0x54
#define UIV_HCEP4IN         0x54
#define UIV_OUT4TOKEN       0x64
#define UIV_EP4PING         0x74

#define UIV_HCOUT5ERR       0x15
#define UIV_EP5IN           0x25
#define UIV_HCEP5OUT        0x25
#define UIV_IN5TOKEN        0x35
#define UIV_HCIN5ERR        0x45
#define UIV_EP5OUT          0x55
#define UIV_HCEP5IN         0x55
#define UIV_OUT5TOKEN       0x65
#define UIV_EP5PING         0x75

#define UIV_HCOUT6ERR       0x16
#define UIV_EP6IN           0x26
#define UIV_HCEP6OUT        0x26
#define UIV_IN6TOKEN        0x36
#define UIV_HCIN6ERR        0x46
#define UIV_EP6OUT          0x56
#define UIV_HCEP6IN         0x56
#define UIV_OUT6TOKEN       0x66
#define UIV_EP6PING         0x76

#define UIV_HCOUT7ERR       0x17
#define UIV_EP7IN           0x27
#define UIV_HCEP7OUT        0x27
#define UIV_IN7TOKEN        0x37
#define UIV_HCIN7ERR        0x47
#define UIV_EP7OUT          0x57
#define UIV_HCEP7IN         0x57
#define UIV_OUT7TOKEN       0x67
#define UIV_EP7PING         0x77

#define UIV_HCOUT8ERR       0x18
#define UIV_EP8IN           0x28
#define UIV_HCEP8OUT        0x28
#define UIV_IN8TOKEN        0x38
#define UIV_HCIN8ERR        0x48
#define UIV_EP8OUT          0x58
#define UIV_HCEP8IN         0x58
#define UIV_OUT8TOKEN       0x68
#define UIV_EP8PING         0x78

#define UIV_HCOUT9ERR       0x19
#define UIV_EP9IN           0x29
#define UIV_HCEP9OUT        0x29
#define UIV_IN9TOKEN        0x39
#define UIV_HCIN9ERR        0x49
#define UIV_EP9OUT          0x59
#define UIV_HCEP9IN         0x59
#define UIV_OUT9TOKEN       0x69
#define UIV_EP9PING         0x79

#define UIV_HCOUT10ERR       0x1a
#define UIV_EP10IN           0x2a
#define UIV_HCEP10OUT        0x2a
#define UIV_IN10TOKEN        0x3a
#define UIV_HCIN10ERR        0x4a
#define UIV_EP10OUT          0x5a
#define UIV_HCEP10IN         0x5a
#define UIV_OUT10TOKEN       0x6a
#define UIV_EP10PING         0x7a

#define UIV_HCOUT11ERR       0x1b
#define UIV_EP11IN           0x2b
#define UIV_HCEP11OUT        0x2b
#define UIV_IN11TOKEN        0x3b
#define UIV_HCIN11ERR        0x4b
#define UIV_EP11OUT          0x5b
#define UIV_HCEP11IN         0x5b
#define UIV_OUT11TOKEN       0x6b
#define UIV_EP11PING         0x7b

#define UIV_HCOUT12ERR       0x1c
#define UIV_EP12IN           0x2c
#define UIV_HCEP12OUT        0x2c
#define UIV_IN12TOKEN        0x3c
#define UIV_HCIN12ERR        0x4c
#define UIV_EP12OUT          0x5c
#define UIV_HCEP12IN         0x5c
#define UIV_OUT12TOKEN       0x6c
#define UIV_EP12PING         0x7c

#define UIV_HCOUT13ERR       0x1d
#define UIV_EP13IN           0x2d
#define UIV_HCEP13OUT        0x2d
#define UIV_IN13TOKEN        0x3d
#define UIV_HCIN13ERR        0x4d
#define UIV_EP13OUT          0x5d
#define UIV_HCEP13IN         0x5d
#define UIV_OUT13TOKEN       0x6d
#define UIV_EP13PING         0x7d

#define UIV_HCOUT14ERR       0x1e
#define UIV_EP14IN           0x2e
#define UIV_HCEP14OUT        0x2e
#define UIV_IN14TOKEN        0x3e
#define UIV_HCIN14ERR        0x4e
#define UIV_EP14OUT          0x5e
#define UIV_HCEP14IN         0x5e
#define UIV_OUT14TOKEN       0x6e
#define UIV_EP14PING         0x7e

#define UIV_HCOUT15ERR       0x1f
#define UIV_EP15IN           0x2f
#define UIV_HCEP15OUT        0x2f
#define UIV_IN15TOKEN        0x3f
#define UIV_HCIN15ERR        0x4f
#define UIV_EP15OUT          0x5f
#define UIV_HCEP15IN         0x5f
#define UIV_OUT15TOKEN       0x6f
#define UIV_EP15PING         0x7f

/* HCINxERR */
#define	HCINxERR_TYPE_MASK      (0xf << 2)
/*
 * 0000 šC reserved (no error)
 * 0001 šC CRC error
 * 0010 šC data toggle mismatch
 * 0011 šC endpoint sent STALL handshake
 * 0100 šC no endpoint handshake (timeout)
 * 0101 šC PID error (pid check=error or unknown PID)
 * 0110 šC Data Overrun (too long packet šC babble)
 * 0111 šC Data Underrun (packet shorter than MaxPacketSize)
 * 1xxx šC Spilit Transaction Error, reference to HCOUTxSPILITCS register
*/
#define	HCINxERR_NO_ERR			(0x0 << 2)
#define	HCINxERR_CRC_ERR		(0x1 << 2)
#define	HCINxERR_TOG_ERR		(0x2 << 2)
#define	HCINxERR_STALL			(0x3 << 2)
#define	HCINxERR_TIMEOUT		(0x4 << 2)
#define	HCINxERR_PID_ERR		(0x5 << 2)
#define	HCINxERR_OVER_RUN		(0x6 << 2)
#define	HCINxERR_UNDER_RUN		(0x7 << 2)
#define	HCINxERR_SPLIET			(0x8 << 2)
#define	HCINxERR_RESEND			(1 << 6)

/* USBCS */
/*bit 7 reserved*/
#define	USBCS_DISCONN		        (1 << 6)
#define	USBCS_SIGRSUME		        (1 << 5)
#define	USBCS_HFMODE                (1 << 1)
#define USBCS_LSMODE                (1 << 0)

/* USBERESET USBERES */
#define USBERES_USBRESET        (1 << 0)

/**
 * Different names, for compatibility!
 */
#define HCIN1STARTADDR             0x00000304	/* HCIN1STADDR */
#define HCOUT1STARTADDR            0x00000344	/* HCOUT1STADDR */
#define HCEP0CS                    0x00000002	/* EP0CS */
#define VDSTATE                    0x00000505	/* VDSTATUS */

/*EPXCON host & device*/
#define	EPCON_VAL	                (1 << 7)
#define	EPCON_STALL	                (1 << 6)
#define	EPCON_TYPE                  (0x3 << 2)
#define	EPCON_TYPE_INT		        (0x3 << 2)
#define	EPCON_TYPE_BULK	            (0x2 << 2)
#define	EPCON_TYPE_ISO		        (0x1 << 2)
#define	EPCON_BUF                   (0x03)
#define	EPCON_BUF_QUAD	            (0x03)
#define	EPCON_BUF_TRIPLE	        (0x02)
#define	EPCON_BUF_DOUBLE	        (0x01)
#define	EPCON_BUF_SINGLE	        (0x00)

static inline void usb_writeb(u8 val, void __iomem *reg)
{
	writeb(val, reg);
}

static inline void usb_writew(u16 val, void __iomem *reg)
{
	writew(val, reg);
}

static inline void usb_writel(u32 val,  void __iomem *reg)
{
	writel(val, reg);
}

static inline u8 usb_readb(void __iomem *reg)
{
	return readb(reg);
}

static inline u16 usb_readw(void __iomem *reg)
{
	return readw(reg);
}

static inline u32 usb_readl(void __iomem *reg)
{
	return readl(reg);
}

static inline void usb_setbitsb(u8 mask, void __iomem *mem)
{
	usb_writeb(usb_readb(mem) | mask, mem);
}

static inline void usb_setbitsw(u16 mask, void __iomem *mem)
{
	usb_writew(usb_readw(mem) | mask, mem);
}

static inline void usb_setbitsl(ulong mask, void __iomem *mem)
{
	usb_writel(usb_readl(mem) | mask, mem);
}

static inline void usb_clearbitsb(u8 mask, void __iomem *mem)
{
	usb_writeb(usb_readb(mem) & ~mask, mem);
}

static inline void usb_clearbitsw(u16 mask, void __iomem *mem)
{
	usb_writew(usb_readw(mem) & ~mask, mem);
}

static inline void usb_clearbitsl(ulong mask, void __iomem *mem)
{
	usb_writel(usb_readl(mem) & ~mask, mem);
}

#endif

