/*
 * Copyright (C) 2010-2012 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file owl_gpu_platform.h
 * Platform specific Mali driver functions
 */

#ifndef __OWL_GPU_PLATFORM_H__
#define __OWL_GPU_PLATFORM_H__

#include "mali_osk.h"

#ifdef __cplusplus
extern "C" {
#endif

struct owl_clk {
	struct clk *clk;
	char *name;
};

#define GPU_MHZ                         1000000

_mali_osk_errcode_t owl_mali_os_resume(void);
_mali_osk_errcode_t owl_mali_os_suspend(void);
_mali_osk_errcode_t owl_mali_runtime_resume(void);
_mali_osk_errcode_t owl_mali_runtime_suspend(void);

int owl_get_freqtable(struct mali_gpu_clk_item **freqtable);

/** @brief Platform specific setup and initialisation of MALI
 *
 * This is called from the entrypoint of the driver to initialize the platform
 *
 * @return _MALI_OSK_ERR_OK on success otherwise, a suitable _mali_osk_errcode_t error.
 */
_mali_osk_errcode_t mali_platform_init(void);

/** @brief Platform specific deinitialisation of MALI
 *
 * This is called on the exit of the driver to terminate the platform
 *
 * @return _MALI_OSK_ERR_OK on success otherwise, a suitable _mali_osk_errcode_t error.
 */
_mali_osk_errcode_t mali_platform_deinit(void);

#if defined(CONFIG_REGULATOR)
void mali_regulator_disable(void);
void mali_regulator_enable(void);
void mali_regulator_set_voltage(int min_uV, int max_uV);
#endif

mali_bool mali_clk_get(void);
int owl_set_clockspeed(unsigned long rate);

mali_bool init_mali_dvfs_status(void);
void deinit_mali_dvfs_status(void);

typedef enum
{
    GPUFREQ_POLICY_POWERSAVE = 0,
    GPUFREQ_POLICY_NORMAL,
    GPUFREQ_POLICY_PERFORMANCE,
    GPUFREQ_POLICY_USERSPACE,

    GPUFREQ_NUMBER_OF_POLICY,
} gpufreq_policy;

struct owl_gpu_t {
	struct kobject kobj;
	int policy;
	unsigned long user_freq;
	int cur_step;
	mali_bool user_freq_changed;
	mali_bool lock;
};

struct gpu_attribute {
	struct attribute attr;
	ssize_t (*show)(struct owl_gpu_t *, char *);
	ssize_t	(*store)(struct owl_gpu_t *, const char *, size_t);
};

ssize_t gpu_clock_show(struct owl_gpu_t *gpu, char *buf);
ssize_t gpu_clock_store(struct owl_gpu_t *gpu,const char *buf, size_t count);

ssize_t clock_fun_show(struct owl_gpu_t *gpu, char *buf);
ssize_t clock_fun_store(struct owl_gpu_t *gpu,const char *buf, size_t count);

ssize_t lock_show(struct owl_gpu_t *gpu, char *buf);
ssize_t lock_store(struct owl_gpu_t *gpu,const char *buf, size_t count);

int owl_gpu_fun_add_attr(struct kobject *dev_kobj);

mali_bool owl_gpu_fun_active(int policy);

#ifdef __cplusplus
}
#endif
#endif
