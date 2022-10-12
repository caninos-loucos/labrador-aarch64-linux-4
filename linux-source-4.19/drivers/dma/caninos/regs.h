/*
 * Caninos Labrador DMA
 *
 * Copyright (c) 2022 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2019 LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2012 Actions Semi Inc.
 * Author: Actions Semi, Inc.
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

#ifndef _CANINOS_DMA_REGS_H_
#define _CANINOS_DMA_REGS_H_

/* global register for dma controller */
#define DMA_IRQ_PD0							(0x00)
#define DMA_IRQ_PD1							(0x04)
#define DMA_IRQ_PD2							(0x08)
#define DMA_IRQ_PD3							(0x0C)
#define DMA_IRQ_EN0							(0x10)
#define DMA_IRQ_EN1							(0x14)
#define DMA_IRQ_EN2							(0x18)
#define DMA_IRQ_EN3							(0x1C)
#define DMA_SECURE_ACCESS_CTL 				(0x20)
#define DMA_NIC_QOS							(0x24)
#define DMA_DBGSEL							(0x28)
#define DMA_IDLE_STAT						(0x2C)

/* channel register */
#define DMA_CHAN_BASE(i) 					(0x100 + (i) * 0x100)
#define DMAX_MODE 							(0x00)
#define DMAX_SOURCE 						(0x04)
#define DMAX_DESTINATION 					(0x08)
#define DMAX_FRAME_LEN 						(0x0C)
#define DMAX_FRAME_CNT 						(0x10)
#define DMAX_REMAIN_FRAME_CNT 				(0x14)
#define DMAX_REMAIN_CNT						(0x18)
#define DMAX_SOURCE_STRIDE 					(0x1C)
#define DMAX_DESTINATION_STRIDE 			(0x20)
#define DMAX_START 							(0x24)
#define DMAX_PAUSE_K7_K9 					(0x28)
#define	DMAX_ACP_ATTR_K5 					(0x28)
#define DMAX_CHAINED_CTL 					(0x2C)
#define DMAX_CONSTANT 						(0x30)
#define DMAX_LINKLIST_CTL 					(0x34)
#define DMAX_NEXT_DESCRIPTOR 				(0x38)
#define DMAX_CURRENT_DESCRIPTOR_NUM			(0x3C)
#define DMAX_INT_CTL 						(0x40)
#define DMAX_INT_STATUS 					(0x44)
#define DMAX_CURRENT_SOURCE_POINTER 		(0x48)
#define DMAX_CURRENT_DESTINATION_POINTER 	(0x4C)

/* DMAX_MODE */
#define DMA_MODE_TS(x) 				(((x) & 0x3f) << 0)
#define DMA_MODE_ST(x) 				(((x) & 0x3) << 8)
#define	DMA_MODE_ST_DEV 			DMA_MODE_ST(0)
#define	DMA_MODE_ST_DCU 			DMA_MODE_ST(2)
#define	DMA_MODE_ST_SRAM 			DMA_MODE_ST(3)
#define DMA_MODE_DT(x) 				(((x) & 0x3) << 10)
#define DMA_MODE_DT_DEV 			DMA_MODE_DT(0)
#define DMA_MODE_DT_DCU 			DMA_MODE_DT(2)
#define	DMA_MODE_DT_SRAM 			DMA_MODE_DT(3)
#define DMA_MODE_SAM(x) 			(((x) & 0x3) << 16)
#define	DMA_MODE_SAM_CONST 			DMA_MODE_SAM(0)
#define	DMA_MODE_SAM_INC 			DMA_MODE_SAM(1)
#define	DMA_MODE_SAM_STRIDE 		DMA_MODE_SAM(2)
#define DMA_MODE_DAM(x) 			(((x) & 0x3) << 18)
#define	DMA_MODE_DAM_CONST 			DMA_MODE_DAM(0)
#define	DMA_MODE_DAM_INC 			DMA_MODE_DAM(1)
#define	DMA_MODE_DAM_STRIDE 		DMA_MODE_DAM(2)
#define DMA_MODE_PW(x) 				(((x) & 0x7) << 20)
#define DMA_MODE_CB					(0x1 << 23)
#define DMA_MODE_NDDBW(x)			(((x) & 0x1) << 28)
#define	DMA_MODE_NDDBW_32BIT 		DMA_MODE_NDDBW(0)
#define	DMA_MODE_NDDBW_8BIT 		DMA_MODE_NDDBW(1)
#define DMA_MODE_CFE 				(0x1 << 29)
#define DMA_MODE_LME 				(0x1 << 30)
#define DMA_MODE_CME 				(0x1 << 31)

/* DMAX_LINKLIST_CTL */
#define DMA_LLC_SAV(x) 				(((x) & 0x3) << 8)
#define	DMA_LLC_SAV_INC 			DMA_LLC_SAV(0)
#define	DMA_LLC_SAV_LOAD_NEXT 		DMA_LLC_SAV(1)
#define	DMA_LLC_SAV_LOAD_PREV 		DMA_LLC_SAV(2)
#define DMA_LLC_DAV(x) 				(((x) & 0x3) << 10)
#define	DMA_LLC_DAV_INC 			DMA_LLC_DAV(0)
#define	DMA_LLC_DAV_LOAD_NEXT 		DMA_LLC_DAV(1)
#define	DMA_LLC_DAV_LOAD_PREV 		DMA_LLC_DAV(2)
#define DMA_LLC_SUSPEND 			(0x1 << 16)

/* DMAX_INT_CTL */
#define DMA_INTCTL_BLOCK 			(0x1 << 0)
#define DMA_INTCTL_SUPER_BLOCK 		(0x1 << 1)
#define DMA_INTCTL_FRAME 			(0x1 << 2)
#define DMA_INTCTL_HALF_FRAME 		(0x1 << 3)
#define DMA_INTCTL_LAST_FRAME 		(0x1 << 4)

/* DMAX_INT_STATUS */
#define DMA_INTSTAT_BLOCK 			(0x1 << 0)
#define DMA_INTSTAT_SUPER_BLOCK 	(0x1 << 1)
#define DMA_INTSTAT_FRAME 			(0x1 << 2)
#define DMA_INTSTAT_HALF_FRAME 		(0x1 << 3)
#define DMA_INTSTAT_LAST_FRAME 		(0x1 << 4)

#endif

