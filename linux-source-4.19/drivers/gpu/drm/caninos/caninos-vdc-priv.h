// SPDX-License-Identifier: GPL-2.0
/*
 * Video Display Controller Driver for Caninos Labrador
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

#ifndef _CANINOS_VDC_PRIV_H_
#define _CANINOS_VDC_PRIV_H_

#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/reset.h>
#include <linux/clk.h>

#include "caninos-drm.h"

enum de_hw_model {
	DE_HW_K7 = 0xAA55,
	DE_HW_K5 = 0x55AA,
};

struct de_hw_ops {
	void (*fini)(struct caninos_vdc*);
	int  (*init)(struct caninos_vdc*);
	void (*enable_irqs)(struct caninos_vdc*);
	bool (*handle_irqs)(struct caninos_vdc*);
	bool (*is_enabled)(struct caninos_vdc*);
	void (*disable)(struct caninos_vdc*);
	void (*enable)(struct caninos_vdc*);
	void (*disable_irqs)(struct caninos_vdc*);
	void (*set_size)(struct caninos_vdc*, u32, u32);
	bool (*set_go)(struct caninos_vdc*);
	void (*set_format)(struct caninos_vdc*, u32);
	void (*set_stride)(struct caninos_vdc*, u32);
	void (*set_framebuffer)(struct caninos_vdc*, u32);
};

extern const struct de_hw_ops de_k5_ops;
extern const struct de_hw_ops de_k7_ops;

struct caninos_vdc {
	struct device *dev;
	enum de_hw_model model;
	void __iomem *base;
	struct reset_control *rst;
	struct clk *clk;
	int irq;
	
	void *mem_virt;
	phys_addr_t mem_phys;
	size_t mem_size;
	
	const struct de_hw_ops *ops;
	spinlock_t lock;
	
	struct caninos_vdc_mode mode;
	u32 fbaddr;
	
	struct {
		struct caninos_vdc_mode mode;
		u32 fbaddr;
	} next;
};

static inline u32 de_calc_stride(u32 width, u32 bits_per_pixel) {
	return ((((width * bits_per_pixel) + 31U) & ~31U) >> 3);
}

static inline void de_enable_irqs(struct caninos_vdc *priv) {
	priv->ops->enable_irqs(priv);
}

static inline bool de_handle_irqs(struct caninos_vdc *priv) {
	return priv->ops->handle_irqs(priv);
}

static inline void de_disable_irqs(struct caninos_vdc *priv) {
	priv->ops->disable_irqs(priv);
}

static inline bool de_is_enabled(struct caninos_vdc *priv) {
	return priv->ops->is_enabled(priv);
}

static inline void de_disable(struct caninos_vdc *priv) {
	priv->ops->disable(priv);
}

static inline void de_enable(struct caninos_vdc *priv) {
	priv->ops->enable(priv);
}

static inline int de_init(struct caninos_vdc *priv) {
	return priv->ops->init(priv);
}

static inline void de_fini(struct caninos_vdc *priv) {
	priv->ops->fini(priv);
}

static inline void de_set_size(struct caninos_vdc *priv, u32 w, u32 h) {
	priv->ops->set_size(priv, w, h);
}

static inline bool de_set_go(struct caninos_vdc *priv) {
	return priv->ops->set_go(priv);
}

static inline void de_set_stride(struct caninos_vdc *priv, u32 stride) {
	priv->ops->set_stride(priv, stride);
}

static inline void de_set_framebuffer(struct caninos_vdc *priv, u32 fb) {
	priv->ops->set_framebuffer(priv, fb);
}

static inline void de_set_format(struct caninos_vdc *priv, u32 format) {
	priv->ops->set_format(priv, format);
}

#endif /* _CANINOS_VDC_PRIV_H_ */

