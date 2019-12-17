/*
 * Actions OWL SoCs usb2.0 controller driver
 *
 * Copyright (c) 2015 Actions Semiconductor Co., ltd.
 * dengtaiping <dengtaiping@actions-semi.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LINUX_USB_AOTG_DEBUG_H__
#define __LINUX_USB_AOTG_DEBUG_H__

/*#define DEBUG_HCD*/
/*#define DEBUG_HUB*/
/*#define DEBUG_SETUP_DATA*/
/*#define DEBUG_EP_CONFIG*/
/*#define IN_PROCESS_DEBUG*/
/*#define DEBUG_IN_DATA*/
/*#define OUT_PROCESS_DEBUG*/
/*#define DEBUG_OUT_DATA*/
/*#define DEBUG_DMA*/
#define AOTG_REG_DUMP
/*#define AOTG_DEBUG_RECORD_URB*/
#define AOTG_DEBUG_FILE
#define DEBUG_LINKLIST_DMA

#define ACT_HCD_ERR		pr_warn("%s:%d, err!\n", __func__, __LINE__);
#define ACT_HCD_DBG		pr_warn("%s:%d, dbg!\n", __func__, __LINE__);

#ifdef ERR
#undef ERR
#endif
#define ERR(fmt, args...) pr_err(fmt, ## args)

#ifdef DEBUG_HUB
#define HUB_DEBUG(fmt, args...) pr_err(fmt, ## args)
#else
#define HUB_DEBUG(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_HCD
#define HCD_DEBUG(fmt, args...) pr_err(fmt, ## args)
#else
#define HCD_DEBUG(fmt, args...) do {} while (0)
#endif

#ifdef IN_PROCESS_DEBUG
#define IN_DEBUG(fmt, args...) pr_err(fmt, ## args)
#else
#define IN_DEBUG(fmt, args...) do {} while (0)
#endif

#ifdef OUT_PROCESS_DEBUG
#define OUT_DEBUG(fmt, args...) pr_err(fmt, ## args)
#else
#define OUT_DEBUG(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_DMA
#define ACT_DMA_DEBUG(fmt, args...) pr_err(fmt, ## args)
#else
#define ACT_DMA_DEBUG(fmt, args...) do {} while (0)
#endif

#ifdef DEBUG_LINKLIST_DMA
#define ACT_LINKLIST_DMA_DEBUG(fmt, args...) pr_err(fmt, ## args)
#else
#define ACT_LINKLIST_DMA_DEBUG(fmt, args...) do {} while (0)
#endif

#define HCD_WARN(fmt, args...) pr_warn(fmt, ## args)
#define HCD_WARNING(fmt, args...) pr_warn(fmt, ## args)

extern unsigned int aotg_trace_onff;

void aotg_dbg_output_info(void);

void aotg_dbg_put_q(struct aotg_queue *q, unsigned int num, unsigned int type, unsigned int len);
void aotg_dbg_finish_q(struct aotg_queue *q);

void aotg_dbg_proc_output_ep(void);
int aotg_dbg_proc_output_ep_state(struct aotg_hcd *acthcd);

void create_debug_file(struct aotg_hcd *acthcd);
void remove_debug_file(struct aotg_hcd *acthcd);

void aotg_dbg_regs(struct aotg_hcd *acthcd);

void create_acts_hcd_proc(void);
void remove_acts_hcd_proc(void);
void aotg_dump_ep_reg(struct aotg_hcd *acthcd, int ep_index, int is_out);

#endif /* __LINUX_USB_AOTG_DEBUG_H__ */
