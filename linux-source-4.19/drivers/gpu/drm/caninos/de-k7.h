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

#ifndef _DE_K7_H_
#define _DE_K7_H_

#define DE_SCALE_CONST_VALUE (8192)

#define DE_OUTPUT_CON_HDMI (0x0)

#define DSS_PATH1   (0x0)
#define DSS_PATH2   (0x1)
#define DSS_PLANE1  (0x0)
#define DSS_PLANE2  (0x1)
#define DSS_PLANE3  (0x2)
#define DSS_PLANE4  (0x3)
#define DSS_LAYER1  (0x0)
#define DSS_LAYER2  (0x1)
#define DSS_LAYER3  (0x2)
#define DSS_LAYER4  (0x3)
#define DSS_SCALER1 (0x0)
#define DSS_SCALER2 (0x1)

#define DE_IRQENABLE        (0x0000)
#define DE_IRQSTATUS        (0x0004)
#define DE_MMU_EN           (0x0008)
#define DE_MMU_BASE         (0x000c)
#define DE_QOS              (0x0014)
#define DE_IF_CON           (0x0018)
#define DE_PATH_BASE        (0x0100)
#define DE_PLANE_BASE       (0x0400)
#define DE_SCALER_BASE      (0x0C00)
#define DE_OUTPUT_CON       (0x1000)
#define DE_OUTPUT_STAT      (0x1004)
#define DE_PATH_CTL(x)      (DE_PATH_BASE   + (x) * 0x100 + 0x00)
#define DE_PATH_FCR(x)      (DE_PATH_BASE   + (x) * 0x100 + 0x04)
#define DE_PATH_EN(x)       (DE_PATH_BASE   + (x) * 0x100 + 0x08)
#define DE_PATH_BK(x)       (DE_PATH_BASE   + (x) * 0x100 + 0x0c)
#define DE_PATH_SIZE(x)     (DE_PATH_BASE   + (x) * 0x100 + 0x10)
#define DE_PATH_COOR(x,y)   (DE_PATH_BASE   + (x) * 0x100 + (y) * 0x4 + 0x20)
#define DE_PLANE_CFG(y)     (DE_PLANE_BASE  + (y) * 0x200 + 0x00)
#define DE_PLANE_ISIZE(y)   (DE_PLANE_BASE  + (y) * 0x200 + 0x04)
#define DE_PLANE_CSC(y)     (DE_PLANE_BASE  + (y) * 0x200 + 0x14)
#define DE_PLANE_BK(y)      (DE_PLANE_BASE  + (y) * 0x200 + 0x1c)
#define DE_LAYER_CFG(y,z)   (DE_PLANE_BASE  + (y) * 0x200 + 0x20 + (z) * 0x80)
#define DE_LAYER_COOR(y,z)  (DE_PLANE_BASE  + (y) * 0x200 + 0x28 + (z) * 0x80)
#define DE_LAYER_FB(y,z)    (DE_PLANE_BASE  + (y) * 0x200 + 0x2c + (z) * 0x80)
#define DE_LAYER_STR(y,z)   (DE_PLANE_BASE  + (y) * 0x200 + 0x44 + (z) * 0x80)
#define DE_LAYER_CROP(y,z)  (DE_PLANE_BASE  + (y) * 0x200 + 0x48 + (z) * 0x80)
#define DE_SCALER_CFG(x)    (DE_SCALER_BASE + (x) * 0x080 + 0x00)
#define DE_SCALER_OSIZE(x)  (DE_SCALER_BASE + (x) * 0x080 + 0x04)
#define DE_SCALER_HSR(x)    (DE_SCALER_BASE + (x) * 0x080 + 0x08)
#define DE_SCALER_VSR(x)    (DE_SCALER_BASE + (x) * 0x080 + 0x0C)
#define DE_SCALER_SCOEF0(x) (DE_SCALER_BASE + (x) * 0x080 + 0x20)
#define DE_SCALER_SCOEF1(x) (DE_SCALER_BASE + (x) * 0x080 + 0x24)
#define DE_SCALER_SCOEF2(x) (DE_SCALER_BASE + (x) * 0x080 + 0x28)
#define DE_SCALER_SCOEF3(x) (DE_SCALER_BASE + (x) * 0x080 + 0x2C)
#define DE_SCALER_SCOEF4(x) (DE_SCALER_BASE + (x) * 0x080 + 0x30)
#define DE_SCALER_SCOEF5(x) (DE_SCALER_BASE + (x) * 0x080 + 0x34)
#define DE_SCALER_SCOEF6(x) (DE_SCALER_BASE + (x) * 0x080 + 0x38)
#define DE_SCALER_SCOEF7(x) (DE_SCALER_BASE + (x) * 0x080 + 0x3C)

#define DE_PATH_CTL_PLANE_EN_MSK    (GENMASK(3, 0))
#define DE_PATH_CTL_PLANE_EN(n)     (BIT(n) & DE_PATH_CTL_PLANE_EN_MSK)
#define DE_PATH_CTL_ILACE_EN        (BIT(9))
#define DE_PATH_CTL_YUV_EN          (BIT(13))
#define DE_PATH_EN_ENABLE           (BIT(0))
#define DE_PATH_FCR_SET_GO          (BIT(0))
#define DE_PLANE_CFG_LAYER_EN_MSK   (GENMASK(3, 0))
#define DE_PLANE_CFG_LAYER_EN(n)    (BIT(n))
#define DE_LAYER_CFG_FLIP_MSK       (GENMASK(21, 20))
#define DE_LAYER_CFG_FLIP(n)        ((n << 20) & DE_LAYER_CFG_FLIP_MSK)
#define DE_LAYER_CFG_FMT_MSK        (GENMASK(4, 0))
#define DE_LAYER_CFG_FMT(n)         ((n) & DE_LAYER_CFG_FMT_MSK)
#define DE_LAYER_CFG_DATA_MODE      (BIT(16))
#define DE_LAYER_CFG_DATA_ALPHA_MSK (GENMASK(15, 8))
#define DE_LAYER_CFG_DATA_ALPHA(n)  (((n) << 8) & DE_LAYER_CFG_DATA_ALPHA_MSK)
#define DE_SCALER_CFG_ENABLE        (BIT(0))
#define DE_SCALER_CFG_4K_SINGLE     (BIT(2))
#define DE_SCALER_CFG_RGB_LINE      (BIT(3))
#define DE_SCALER_CFG_PLANE_MSK     (GENMASK(5, 4))
#define DE_SCALER_CFG_PLANE(n)      ((n << 4) & DE_SCALER_CFG_PLANE_MSK)
#define DE_OUTPUT_CON_PATH1_DEV_MSK (GENMASK(1, 0))
#define DE_OUTPUT_CON_PATH1_DEV(x)  ((x) & DE_OUTPUT_CON_PATH1_DEV_MSK)
#define DE_OUTPUT_CON_PATH2_DEV_MSK (GENMASK(5, 4))
#define DE_OUTPUT_CON_PATH2_DEV(x)  (((x) << 4) & DE_OUTPUT_CON_PATH2_DEV_MSK)

#endif
