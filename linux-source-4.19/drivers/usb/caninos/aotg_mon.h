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

#ifndef __AOTG_UHOST_MON_H__
#define __AOTG_UHOST_MON_H__

extern struct aotg_uhost_mon_t *aotg_uhost_mon[2];
void aotg_dev_plugout_msg(int id);
void aotg_uhost_mon_init(int id);
void aotg_uhost_mon_exit(void);

#endif /* __AOTG_UHOST_MON_H__ */

