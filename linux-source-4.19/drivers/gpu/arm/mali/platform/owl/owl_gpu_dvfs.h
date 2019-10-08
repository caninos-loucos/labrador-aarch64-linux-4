/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file owl_gpu_dvfs.h
 * Platform specific Mali driver dvfs functions
 */

#ifndef __OWL_GPU_DVFS_H__
#define __OWL_GPU_DVFS_H__

#include <linux/mali/mali_utgard.h>
#define RELATION_L 0 /* highest frequency below or at target */
#define RELATION_H 1 /* lowest frequency at or above target */

enum gpugreq_thermal_stat
{
	GPUFREQ_THERMAL_NORMAL = 0,
	GPUFREQ_THERMAL_HOT,
	GPUFREQ_THERMAL_CRITICAL,
	NUM_GPUFREQ_THERMAL_STAT,
};

enum gpufreq_governor
{
	GPUFREQ_GOVERNOR_UNKNOWN = -1,
	GPUFREQ_GOVERNOR_POWERSAVE = 0,
	GPUFREQ_GOVERNOR_PERFORMANCE = 1,
	GPUFREQ_GOVERNOR_CONSERVATIVE = 2,
	GPUFREQ_GOVERNOR_USER = 3,

	NUM_GPUFREQ_GOVERNOR,
};

struct gpufreq_governor_t
{
    enum gpufreq_governor governor;
    int (*get_target_level)(void);
};

mali_bool gpu_dvfs_set_governor(enum gpufreq_governor governor);
mali_bool gpu_dvfs_set_step(int step);
int gpu_dvfs_get_current_step(void);
int gpu_dvfs_get_maximum_step(void);
int gpu_dvfs_get_step(unsigned long freq);

/* Function that platfrom report it's clock info which driver can set, needed when CONFIG_MALI_DVFS enabled */
void mali_get_clock_info(struct mali_gpu_clock **data);
/* Fuction that platform callback for freq setting, needed when CONFIG_MALI_DVFS enabled */
mali_bool mali_set_freq_step(int setting_clock_step);
/* Function that get the current clock info, needed when CONFIG_MALI_DVFS enabled */
int mali_get_freq_step(void);

#if defined(CONFIG_THERMAL)
mali_bool mali_gpu_dvfs_set_thermal_stat(enum gpugreq_thermal_stat stat);
int thermal_notifier_callback(struct notifier_block *nb,
                                     unsigned long action, void *data);
#endif

#endif /* __OWL_GPU_DVFS_H__ */
