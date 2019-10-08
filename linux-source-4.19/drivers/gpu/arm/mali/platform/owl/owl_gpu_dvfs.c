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
 * @file owl_gpu_dvfs.c
 * Platform specific Mali driver dvfs functions
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/workqueue.h>

#include <asm/io.h>
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "owl_gpu_platform.h"
#include "owl_gpu_dvfs.h"

#if defined(CONFIG_OWL_THERMAL)
#include <linux/cpu_cooling.h>
#endif


struct gpufreq_dvfs
{
    struct gpufreq_governor_t *governor;

    int cur_step;
    int pre_step;
    int max_step_lock;
    int min_step_lock;
#if defined(CONFIG_THERMAL)
    int thermal_step_lock;
#endif

    struct mali_gpu_clk_item *freq_table;
    int num_steps;

    struct rw_semaphore dvfs_rwsem;
    struct mutex dvfs_enable_mutex;
    int stat;
};

static struct gpufreq_dvfs gpudvfs = {
    .governor = NULL,
};

static struct gpufreq_governor_t dvfs_governor[NUM_GPUFREQ_GOVERNOR] = {
    {
        .governor = GPUFREQ_GOVERNOR_POWERSAVE,
        .get_target_level = NULL,
    },
    {
        .governor = GPUFREQ_GOVERNOR_PERFORMANCE,
        .get_target_level = NULL,
    },
    {
        .governor = GPUFREQ_GOVERNOR_CONSERVATIVE,
        .get_target_level = NULL,
    },
    {
        .governor = GPUFREQ_GOVERNOR_USER,
        .get_target_level = NULL,
    },
};

extern unsigned long mali_default_clk;

static void mali_dvfs_work_handler(struct work_struct *w);
static struct workqueue_struct *mali_dvfs_wq = 0;
static DECLARE_WORK(mali_dvfs_work, mali_dvfs_work_handler);

static int mali_dvfs_step = 0;

mali_bool mali_dvfs_set_step(int setting_clock_step)
{
    int cur_step = 0;

    /*decide next step*/
    cur_step = gpu_dvfs_get_current_step();

    MALI_DEBUG_PRINT(3,
            ("setting_clock_step %d, gpudvfs.cur_step %d, thermal stat = %d.\n", setting_clock_step, cur_step, gpudvfs.stat));

    if (gpudvfs.stat == GPUFREQ_THERMAL_CRITICAL) {
        setting_clock_step = 0;
    }
    /*if next status is same with current status, don't change anything*/
    if ((cur_step != setting_clock_step)) {

        gpudvfs.pre_step = cur_step;

        owl_set_clockspeed(gpudvfs.freq_table[setting_clock_step].clock * GPU_MHZ);

        gpudvfs.cur_step = setting_clock_step;
    }

    return MALI_TRUE;
}

static int match_clock_step(unsigned long freq, int relation)
{
    int i = 0;
    for (i = gpudvfs.num_steps - 1; i > 0; i--)
        if (freq >= (gpudvfs.freq_table[i].clock * GPU_MHZ))
            break;

    if (freq == (gpudvfs.freq_table[i].clock * GPU_MHZ))
        return i;
    else if ((relation == RELATION_H) && (i < (gpudvfs.num_steps - 1)))
        return i + 1;
    else
        return i;
}

int gpu_dvfs_get_current_step(void)
{
    return gpudvfs.cur_step;
}

int gpu_dvfs_get_maximum_step(void)
{
    return gpudvfs.num_steps - 1;
}

int gpu_dvfs_get_step(unsigned long freq)
{
    return match_clock_step(freq, RELATION_L);
}

mali_bool gpu_dvfs_set_step(int step)
{
    mali_bool ret = MALI_TRUE;

    down_write(&gpudvfs.dvfs_rwsem);

    if (step < gpudvfs.min_step_lock ||
            step > gpudvfs.max_step_lock) {
        ret = MALI_FALSE;
        goto err_up_rwsem;
    }

#if defined(CONFIG_THERMAL)
    if (step > gpudvfs.thermal_step_lock)
        step = gpudvfs.thermal_step_lock;
#endif

    /*change mali dvfs status*/
    /**
     * Pause the scheduling and power state changes of Mali device driver.
     * mali_dev_resume() must always be called as soon as possible after this function
     * in order to resume normal operation of the Mali driver.
     */
    mali_dev_pause();

    ret = mali_dvfs_set_step(step);

    /**
     * Resume scheduling and allow power changes in Mali device driver.
     * This must always be called after mali_dev_pause().
     */
    mali_dev_resume();

err_up_rwsem:
    up_write(&gpudvfs.dvfs_rwsem);

    return ret;
}

mali_bool gpu_dvfs_set_governor(enum gpufreq_governor governor)
{
    mali_bool ret = MALI_TRUE;

    MALI_DEBUG_PRINT(4, ("gpu_dvfs_set_governor %d\n", governor));
    if (governor < 0 || governor >= NUM_GPUFREQ_GOVERNOR) {
        MALI_PRINT(("invalid governor %d\n", governor));
        return MALI_FALSE;
    }

    down_write(&gpudvfs.dvfs_rwsem);

    gpudvfs.governor = &dvfs_governor[governor];

    up_write(&gpudvfs.dvfs_rwsem);

    /* Reset clock level range. */
//    ret = gpu_dvfs_set_step_range(0, gpudvfs.num_steps - 1);

    return ret;
}

static void mali_dvfs_work_handler(struct work_struct *w)
{
    MALI_DEBUG_PRINT(3, ("mali_dvfs_work_handler step = %d\n", mali_dvfs_step));
    if (!gpu_dvfs_set_step(mali_dvfs_step))
        MALI_DEBUG_PRINT(1,( "error on mali dvfs status in mali_dvfs_work_handler"));
}

mali_bool init_mali_dvfs_status(void)
{
    mali_bool ret = MALI_TRUE;
    int step = 0;
    int i = 0;

    //gpudvfs.num_steps = owl_get_freqtable(&gpudvfs.freq_table);
    gpudvfs.min_step_lock = 0;
    gpudvfs.max_step_lock = gpudvfs.num_steps - 1;
#if DEBUG
    for (i=0; i<gpudvfs.num_steps; i++)
    {
        MALI_PRINT(("[clock = %d, vol = %u]\n", gpudvfs.freq_table[i].clock, gpudvfs.freq_table[i].vol));
    }
#endif

#if defined(CONFIG_THERMAL)
    gpudvfs.thermal_step_lock = gpudvfs.num_steps - 1;
#endif

    init_rwsem(&gpudvfs.dvfs_rwsem);
    mutex_init(&gpudvfs.dvfs_enable_mutex);

    step = gpu_dvfs_get_step(mali_default_clk);
    MALI_DEBUG_PRINT(3, ("init step = %d, default_clk = %lu\n", step, mali_default_clk));

    gpudvfs.cur_step = step;
    gpudvfs.pre_step = step;
    gpudvfs.stat = GPUFREQ_THERMAL_NORMAL;

    /*set default governor*/
    ret = gpu_dvfs_set_governor(GPUFREQ_GOVERNOR_CONSERVATIVE);
    if (ret == MALI_FALSE) {
        MALI_PRINT(("set default governor failed\n"));
        return ret;
    }

#if defined(CONFIG_MALI_DVFS)
    /*default status
    add here with the right function to get initilization value.
    */
    if (!mali_dvfs_wq)
        mali_dvfs_wq = create_singlethread_workqueue("mali_dvfs");
#endif

	return ret;
}

void deinit_mali_dvfs_status(void)
{
    /*TODO*/
#if defined(CONFIG_MALI_DVFS)
    if (mali_dvfs_wq)
        destroy_workqueue(mali_dvfs_wq);
    mali_dvfs_wq = NULL;
#endif
}

mali_bool mali_dvfs_handler(int setting_clock_step)
{
    mali_dvfs_step = setting_clock_step;

    if (mali_dvfs_wq)
        queue_work_on(0, mali_dvfs_wq, &mali_dvfs_work);
    /*add error handle here*/
    return MALI_TRUE;
}

#if defined(CONFIG_THERMAL)
mali_bool mali_gpu_dvfs_set_thermal_stat(enum gpugreq_thermal_stat stat)
{
    int ret = MALI_FALSE, t_step = 0;
    /*TODO*/
    if (stat < 0 || stat >= NUM_GPUFREQ_THERMAL_STAT)
        return ret;

    switch (stat) {
    default:
    case GPUFREQ_THERMAL_NORMAL:
        t_step = gpudvfs.num_steps >> 1;
        gpudvfs.thermal_step_lock = gpudvfs.num_steps - 1;
        break;
    case GPUFREQ_THERMAL_HOT:
        if (gpudvfs.governor->governor == GPUFREQ_GOVERNOR_POWERSAVE    ||
            gpudvfs.governor->governor == GPUFREQ_GOVERNOR_PERFORMANCE ||
            gpudvfs.governor->governor == GPUFREQ_GOVERNOR_USER)
            t_step = gpudvfs.cur_step - 1;
        else
            t_step = gpudvfs.thermal_step_lock - 1;
        t_step = (t_step < 0) ? 0 : t_step;
        gpudvfs.thermal_step_lock = t_step;
        break;
    case GPUFREQ_THERMAL_CRITICAL:
        t_step = 0;
        gpudvfs.thermal_step_lock = t_step;
        break;
    }

    MALI_PRINT(("mali_gpu_dvfs_set_thermal_stat t_step = %d\n", t_step));

    gpudvfs.stat = stat;
    if(!mali_dvfs_handler(t_step))
        MALI_PRINT(( "error on mali dvfs set thermal in step\n"));

    ret = MALI_TRUE;

    return ret;
}

int thermal_notifier_callback(struct notifier_block *nb,
                                     unsigned long action, void *data)
{
    int ret = NOTIFY_OK;

    //switch (action) {
   // case CPUFREQ_COOLING_START:
    //    MALI_PRINT(("thermal_notifier_callback CPUFREQ_COOLING_START\n"));
    //    if (mali_gpu_dvfs_set_thermal_stat(GPUFREQ_THERMAL_CRITICAL) == MALI_FALSE)
    //        MALI_PRINT(("thermal_notifier_callback failed\n"));
    //    break;
   // case CPUFREQ_COOLING_STOP:
   //     MALI_PRINT(("thermal_notifier_callback CPUFREQ_COOLING_STOP\n"));
    //    mali_gpu_dvfs_set_thermal_stat(GPUFREQ_THERMAL_NORMAL);

        /* Recover the frequency for policy setting */
    //    MALI_DEBUG_PRINT(3, ("CPUFREQ_COOLING_STOP,should recover policy\n"));
        /*TODO*/
    //    break;
   // default:
  //      ret = NOTIFY_DONE;
   //     break;
   // }

    return ret;
}

#endif /* defined(CONFIG_OWL_THERMAL) */

/* Fuction that platform callback for freq setting, needed when CONFIG_MALI_DVFS enabled */
mali_bool mali_set_freq_step(int setting_clock_step)
{
    if(!mali_dvfs_handler(setting_clock_step))
        MALI_DEBUG_PRINT(1,( "error on mali dvfs status in step\n"));

	return MALI_TRUE;
}

/* Function that platfrom report it's clock info which driver can set, needed when CONFIG_MALI_DVFS enabled */
void mali_get_clock_info(struct mali_gpu_clock **data)
{
    /*TODO*/
    static struct mali_gpu_clock mali_clock_items = {};

   // mali_clock_items.num_of_steps = owl_get_freqtable(&mali_clock_items.item);
    *data = &mali_clock_items;
}

/* Function that get the current clock info, needed when CONFIG_MALI_DVFS enabled */
int mali_get_freq_step(void)
{
    int cur_step = 0;
    cur_step = gpu_dvfs_get_current_step();
    return cur_step;
}
