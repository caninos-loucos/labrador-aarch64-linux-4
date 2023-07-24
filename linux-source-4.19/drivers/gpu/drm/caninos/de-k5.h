// SPDX-License-Identifier: GPL-2.0
/*
 * DRM/KMS driver for Caninos Labrador
 *
 * Copyright (c) 2022-2023 ITEX - LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
 *
 * Copyright (c) 2018-2020 LSITEC - Caninos Loucos
 * Author: Edgar Bernardi Righi <edgar.righi@lsitec.org.br>
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

#ifndef _DE_K5_H_
#define _DE_K5_H_

#define DE_SCALE_CONST_VALUE (8192)

#define DE_OUTPUT_CON_HDMI (0x2)

#define DSS_PATH1  (0x0)
#define DSS_PATH2  (0x1)
#define DSS_PLANE1 (0x0)
#define DSS_PLANE2 (0x1)
#define DSS_PLANE3 (0x2)
#define DSS_PLANE4 (0x3)

#define DE_IRQENABLE          (0x0000)
#define DE_IRQSTATUS          (0x0004)
#define DE_IF_CON             (0x000c)
#define DE_MMU_EN             (0x0010)
#define DE_MMU_BASE           (0x0014)
#define DE_PATH_BASE          (0x0100)
#define DE_PATH_DITHER        (0x0150)
#define DE_PLANE_BASE         (0x0400)
#define DE_OUTPUT_CON         (0x1000)
#define DE_OUTPUT_STAT        (0x100c)
#define DE_WB_CON             (0x1004)
#define DE_WB_ADDR            (0x1008)
#define DE_PATH_CTL(x)        (DE_PATH_BASE  + (x) * 0x100 + 0x00)
#define DE_PATH_EN(x)         (DE_PATH_BASE  + (x) * 0x100 + 0x00)
#define DE_PATH_FCR(x)        (DE_PATH_BASE  + (x) * 0x100 + 0x00)
#define DE_PATH_BK(x)         (DE_PATH_BASE  + (x) * 0x100 + 0x20)
#define DE_PATH_SIZE(x)       (DE_PATH_BASE  + (x) * 0x100 + 0x24)
#define DE_PLANE_CFG(y)       (DE_PLANE_BASE + (y) * 0x100 + 0x00)
#define DE_PLANE_ISIZE(y)     (DE_PLANE_BASE + (y) * 0x100 + 0x04)
#define DE_PLANE_OSIZE(y)     (DE_PLANE_BASE + (y) * 0x100 + 0x08)
#define DE_PLANE_SR(y)        (DE_PLANE_BASE + (y) * 0x100 + 0x0c)
#define DE_PLANE_SCOEF0(y)    (DE_PLANE_BASE + (y) * 0x100 + 0x10)
#define DE_PLANE_SCOEF1(y)    (DE_PLANE_BASE + (y) * 0x100 + 0x14)
#define DE_PLANE_SCOEF2(y)    (DE_PLANE_BASE + (y) * 0x100 + 0x18)
#define DE_PLANE_SCOEF3(y)    (DE_PLANE_BASE + (y) * 0x100 + 0x1c)
#define DE_PLANE_SCOEF4(y)    (DE_PLANE_BASE + (y) * 0x100 + 0x20)
#define DE_PLANE_SCOEF5(y)    (DE_PLANE_BASE + (y) * 0x100 + 0x24)
#define DE_PLANE_SCOEF6(y)    (DE_PLANE_BASE + (y) * 0x100 + 0x28)
#define DE_PLANE_SCOEF7(y)    (DE_PLANE_BASE + (y) * 0x100 + 0x2c)
#define DE_PLANE_BA0(y)       (DE_PLANE_BASE + (y) * 0x100 + 0x30)
#define DE_PLANE_ALPHA_CFG(y) (DE_PLANE_BASE + (y) * 0x100 + 0x58)
#define DE_PLANE_ALPHA_EN(y)  (DE_PLANE_BASE + (y) * 0x100 + 0x64)
#define DE_PLANE_COOR(y)      (DE_PLANE_BASE + (y) * 0x100 + 0x54)
#define DE_PLANE_STR(y)       (DE_PLANE_BASE + (y) * 0x100 + 0x48)

#define DE_PATH_CTL_PLANE_EN_MSK    (GENMASK(23, 20))
#define DE_PATH_CTL_PLANE_EN(n)     ((BIT(n) << 20) & DE_PATH_CTL_PLANE_EN_MSK)
#define DE_PATH_CTL_ILACE_EN        (BIT(11))
#define DE_PATH_CTL_YUV_EN          (BIT(16))
#define DE_PATH_EN_ENABLE           (BIT(28))
#define DE_PATH_FCR_SET_GO          (BIT(29))
#define DE_PLANE_CFG_FLIP_MSK       (GENMASK(21, 20))
#define DE_PLANE_CFG_FLIP(n)        ((n << 20) & DE_PLANE_CFG_FLIP_MSK)
#define DE_PLANE_CFG_FMT_MSK        (GENMASK(2, 0))
#define DE_PLANE_CFG_FMT(n)         ((n) & DE_PLANE_CFG_FMT_MSK)
#define DE_PLANE_CFG_FMT_BYTES_MSK  (GENMASK(31, 28))
#define DE_PLANE_CFG_FMT_BYTES(n)   (((n) << 28) & DE_PLANE_CFG_FMT_BYTES_MSK)
#define DE_OUTPUT_CON_PATH1_DEV_MSK (GENMASK(2, 0))
#define DE_OUTPUT_CON_PATH1_DEV(x)  ((x) & DE_OUTPUT_CON_PATH1_DEV_MSK)
#define DE_OUTPUT_CON_PATH2_DEV_MSK (GENMASK(6, 4))
#define DE_OUTPUT_CON_PATH2_DEV(x)  (((x) << 4) & DE_OUTPUT_CON_PATH2_DEV_MSK)

#endif
