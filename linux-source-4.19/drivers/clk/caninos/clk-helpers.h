// SPDX-License-Identifier: GPL-2.0
/*
 * Clock Controller Driver for Caninos Labrador
 *
 * Copyright (c) 2022-2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2018-2020 LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2014 Actions Semi Inc.
 * Author: David Liu <liuwei@actions-semi.com>
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

#ifndef __CLK_HELPERS_H
#define __CLK_HELPERS_H

static inline u32 MASK(u32 width, u32 shift) {
	return ((u32)((((1ULL << width) - 1ULL) << shift) & 0xFFFFFFFF));
}

static inline bool GET_BIT(u32 value, u32 bit_idx) {
	return ((value & (1U << bit_idx)) != 0U);
}

static inline u32 SET_BIT(u32 raw, u32 bit_idx) {
	return (raw | (1U << bit_idx));
}

static inline u32 CLEAR_BIT(u32 raw, u32 bit_idx) {
	return (raw & ~SET_BIT(0U, bit_idx));
}

static inline u32 CLEAR_VALUE(u32 raw, u32 width, u32 shift) {
	return (raw & ~MASK(width, shift));
}

static inline u32 GET_VALUE(u32 raw, u32 width, u32 shift) {
	return ((raw >> shift) & MASK(width, 0U));
}

static inline u32 SET_VALUE(u32 raw, u32 val, u32 width, u32 shift)
{
	val = (val << shift) & MASK(width, shift);
	return (CLEAR_VALUE(raw, width, shift) | val);
}

static inline u8 GET_MUX(u32 raw, u32 width, u32 shift) {
	return ((u8)(GET_VALUE(raw, width, shift) & 0xFF));
}

static inline u32 SET_MUX(u32 raw, u8 index, u32 width, u32 shift) {
	return SET_VALUE(raw, ((u32)(index)) & 0xFF, width, shift);
}

#endif

