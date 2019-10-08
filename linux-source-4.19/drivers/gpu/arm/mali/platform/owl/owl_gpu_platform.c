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
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */
#include <linux/version.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "owl_gpu_dvfs.h"
#include "owl_gpu_platform.h"
#include "mali_pm.h"


#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif /* CONFIG_PM_RUNTIME */

#include <linux/clk.h>
#include <linux/err.h>

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/delay.h>
#include <linux/stat.h>
#include <linux/reset.h>

#if defined(CONFIG_MALI400_PROFILING)
#include "mali_osk_profiling.h"
#endif

#include <linux/io.h>

#define FPGA_VERIFY 0
#define CONFIG_DTS_FREQ_TABLE

static struct owl_clk gpu_clk = { .name = "gpu_clk" };

struct owl_clk gpu_dev_clk = { .name = "dev_pll" };
struct owl_clk gpu_disply_clk = { .name = "display_pll" };

static struct owl_clk gpu_parent_clk[] = {
    { .name = "display_pll" },
    { .name = "dev_clk" },
};

static struct mali_gpu_clk_item *gpu_freqtable = NULL;
static int num_of_freqtable = 0;

/*
 * ---------------------------------------------------------------------------------------
 *                          GPU Available Frequency Table (MHz)                          |
 * ---------------------------------------------------------------------------------------
 * gpu_clk div(1/?):     |   1   |  1.5  |   2   |  2.5  |   3   |   4   |   6   |   8   |
 * ---------------------------------------------------------------------------------------
 * dev_clk(600MHz):      |  600  |  400  |  300  |  240  |  200  |  150  |  100  |  75   |
 * ---------------------------------------------------------------------------------------
 * display_pll(480MHz):  |  480  |  320  |  240  | 192   |  160  |  120  |  80   |  60   |
 * ---------------------------------------------------------------------------------------
 */
static struct mali_gpu_clk_item mali_clk_table[] = {
/*step 0*/{ 240,  1050000 },
/*step 1*/{ 320,  1075000 },
/*step 2*/{ 400,  1100000 },
/*step 3*/{ 480,  1125000 },
};

unsigned long mali_driver_default_clk = 400000000;
unsigned long mali_default_clk;

#if defined(CONFIG_REGULATOR)
struct owl_regulator {
    struct regulator *regulator;
    char *name;
};

struct owl_regulator g3d_regulator = {
        .name = "gpuvdd",
};
#endif

#define GPU_ATTR(_name, _mode, _show, _store) \
    struct gpu_attribute gpu_attr_##_name = \
    __ATTR(_name, _mode, _show, _store)

GPU_ATTR(devices, S_IRUGO|S_IWUSR, gpu_clock_show, gpu_clock_store);
GPU_ATTR(runtime, S_IRUGO|S_IWUSR, clock_fun_show, clock_fun_store);
GPU_ATTR(lock, S_IRUGO|S_IWUSR, lock_show, lock_store);

static struct attribute *gpu_attrs[] = { &gpu_attr_devices.attr,
        &gpu_attr_runtime.attr, &gpu_attr_lock.attr, NULL, };

static int clk_modifier;

static ssize_t gpu_attr_show(struct kobject *kobj, struct attribute *attr,
        char *buf) {
    struct owl_gpu_t *gpu;
    struct gpu_attribute *gpu_attr;

    gpu = container_of(kobj, struct owl_gpu_t, kobj);
    gpu_attr = container_of(attr, struct gpu_attribute, attr);

    if (!gpu_attr->show)
        return -ENOENT;

    return gpu_attr->show(gpu, buf);
}

static ssize_t gpu_attr_store(struct kobject *kobj, struct attribute *attr,
        const char *buf, size_t size) {
    struct owl_gpu_t *gpu;
    struct gpu_attribute *gpu_attr;

    gpu = container_of(kobj, struct owl_gpu_t, kobj);
    gpu_attr = container_of(attr, struct gpu_attribute, attr);

    if (!gpu_attr->store)
        return -ENOENT;

    return gpu_attr->store(gpu, buf, size);
}

struct sysfs_ops gpu_sysops =
        { .show = gpu_attr_show, .store = gpu_attr_store, };

struct kobj_type gpu_ktype = { .sysfs_ops = &gpu_sysops, .default_attrs =
        gpu_attrs, };

struct owl_gpu_t owl_gpu;

static _mali_osk_mutex_rw_t *mali_dvfs_lock = NULL;

#if defined(CONFIG_REGULATOR)
void mali_regulator_disable(void)
{
    int err = 0;
    if (IS_ERR_OR_NULL(g3d_regulator.regulator)) {
        MALI_PRINT(("error on mali_regulator_disable : g3d_regulator.regulator is null\n"));
        return;
    }
    if (regulator_is_enabled(g3d_regulator.regulator)) {
        err = regulator_disable(g3d_regulator.regulator);
        if (err)
            pr_err("Failed to enable gpu power supply!\n");
    }
}

void mali_regulator_enable(void)
{
    int err = 0;
    if (IS_ERR_OR_NULL(g3d_regulator.regulator)) {
        MALI_PRINT(("error on mali_regulator_enable : g3d_regulator.regulator is null\n"));
        return;
    }
    if (!regulator_is_enabled(g3d_regulator.regulator)) {
        err = regulator_enable(g3d_regulator.regulator);
        if (err)
            pr_err("Failed to enable gpu power supply!\n");
    }
}

void mali_regulator_set_voltage(int min_uV, int max_uV)
{
    int voltage;

    if (IS_ERR_OR_NULL(g3d_regulator.regulator)) {
        MALI_PRINT(("error on mali_regulator_set_voltage : g3d_regulator.regulator is null\n"));
        return;
    }

    MALI_DEBUG_PRINT(3, ("= regulator_set_voltage: %d\n", min_uV));
    _mali_osk_mutex_rw_wait(mali_dvfs_lock, _MALI_OSK_LOCKMODE_RW);

    regulator_set_voltage(g3d_regulator.regulator, min_uV, INT_MAX);
    voltage = regulator_get_voltage(g3d_regulator.regulator);

    _mali_osk_mutex_rw_signal(mali_dvfs_lock, _MALI_OSK_LOCKMODE_RW);
    MALI_DEBUG_PRINT(3, ("= regulator_get_voltage: %d \n", voltage));
}

int mali_regulator_get_voltage(void)
{
    int oldvol = 0;

    _mali_osk_mutex_rw_wait(mali_dvfs_lock, _MALI_OSK_LOCKMODE_RW);
    /* if regulator is NULL, bypass regulator volatage get/set. */
    oldvol = regulator_get_voltage(g3d_regulator.regulator);

    _mali_osk_mutex_rw_signal(mali_dvfs_lock, _MALI_OSK_LOCKMODE_RW);
    return oldvol;
}

#endif

mali_bool mali_clk_get(void)
{
    int i,j;

    if (gpu_clk.clk == NULL) {
        gpu_clk.clk = devm_clk_get(&mali_platform_device->dev, gpu_clk.name);
        if (IS_ERR_OR_NULL(gpu_clk.clk)) {
            MALI_PRINT(("MALI Error : failed to get source fout_vpll_clock\n"));
            return MALI_FALSE;
        }
    }

    for (i = 0; i < ARRAY_SIZE(gpu_parent_clk); i++) {
        gpu_parent_clk[i].clk = clk_get(NULL, gpu_parent_clk[i].name);
        if (IS_ERR_OR_NULL(gpu_parent_clk[i].clk)) {
            MALI_PRINT(("Failed to get %s", gpu_parent_clk[i].name));

            for (j = 0; j < i; j++)
                clk_put(gpu_parent_clk[i].clk);

            return MALI_FALSE;
        }
    }

    return MALI_TRUE;
}

mali_bool mali_clk_set_rate(unsigned long rate)
{
    int i, err = 0;
    int best_parent = 0;
    unsigned long best_rate = 0;
    unsigned long cur_rate = 0;
    mali_bool ret = MALI_FALSE;

    /*pr_info("mali_clk_set_rate : %d \n", rate);*/
    _mali_osk_mutex_rw_wait(mali_dvfs_lock, _MALI_OSK_LOCKMODE_RW);

    for (i = 0; i < ARRAY_SIZE(gpu_parent_clk); i++) {
        long round_rate;
        err = clk_set_parent(gpu_clk.clk, gpu_parent_clk[i].clk);
        if (err)
            goto err_signal_mutex;

        /* round_rate <= rate*/
        round_rate = clk_round_rate(gpu_clk.clk, rate);
        if (round_rate < 0)
            goto err_signal_mutex;

        if (round_rate > best_rate) {
            best_rate = round_rate;
            best_parent = i;

            if (round_rate == rate)
                break;
        }
    }

    clk_set_parent(gpu_clk.clk, gpu_parent_clk[best_parent].clk);

    err = clk_set_rate(gpu_clk.clk, rate);
    if (err) {
        pr_err("Failed to set gpu clock rate %lu\n", rate);
        goto err_signal_mutex;
    }

    clk_prepare_enable(gpu_clk.clk);

    cur_rate = clk_get_rate(gpu_clk.clk);

    ret = MALI_TRUE;

err_signal_mutex:
    /*pr_info("mali_clk_set_rate clk_get_rate: %lu \n", cur_rate);*/
    _mali_osk_mutex_rw_signal(mali_dvfs_lock, _MALI_OSK_LOCKMODE_RW);

    return ret;
}

mali_bool mali_clk_enable(mali_bool enable)
{
    mali_bool ret = MALI_TRUE;
    MALI_DEBUG_PRINT(3, ("mali_clk_enable : %d, owl_gpu.cur_step %d.\n", enable, owl_gpu.cur_step));
    if (enable) {
        if (owl_set_clockspeed(gpu_freqtable[owl_gpu.cur_step].clock * GPU_MHZ) == MALI_FALSE) {
            ret = MALI_FALSE;
        }
    } else {
        clk_disable_unprepare(gpu_clk.clk);
    }

    return ret;
}

mali_bool mali_power_enable(mali_bool enable)
{
    mali_bool ret = MALI_FALSE;
    int err, res = 0;

    if (enable) {
#if defined(CONFIG_PM_RUNTIME)
        MALI_DEBUG_ASSERT_POINTER(mali_platform_device);
        res = pm_runtime_get_sync(&(mali_platform_device->dev));
        if (res < 0) {
            MALI_PRINT(("MALI WARN %s: pm_runtime_get_sync failed (%d)\n", __func__, -res));
            err = -ENODEV;
            goto err_out;
        }
#endif
    } else {
#if defined(CONFIG_PM_RUNTIME)
        MALI_DEBUG_ASSERT_POINTER(mali_platform_device);
        pm_runtime_put_sync(&(mali_platform_device->dev));
#endif
    }

    ret = MALI_TRUE;
err_out:
    return ret;
}

int owl_set_clockspeed(unsigned long rate)
{
    //struct opp *opp;
    int oldvol, newvol = 0;
    int err = 0;

    rcu_read_lock();
    MALI_DEBUG_PRINT(3, ("owl_set_clockspeed : %lu\n", rate));

   // opp = opp_find_freq_ceil(&mali_platform_device->dev, &rate);
    //if (IS_ERR(opp)) {
   //     opp = opp_find_freq_floor(&mali_platform_device->dev, &rate);
   //     if (IS_ERR(opp))
   //         goto rcu_unlock;
   // }

   // newvol = (int)opp_get_voltage(opp);

rcu_unlock:
    rcu_read_unlock();
#if defined(CONFIG_REGULATOR)
    oldvol = IS_ERR_OR_NULL(g3d_regulator.regulator) ? newvol:mali_regulator_get_voltage();
#endif

    /*TODO*/
    MALI_DEBUG_PRINT(2, ("owl_set_clockspeed : %d, %d, %lu\n", newvol, oldvol, rate));

    if (newvol == oldvol) {
        mali_clk_set_rate(rate);
    } else if (newvol < oldvol) {
        mali_clk_set_rate(rate);
#if defined(CONFIG_REGULATOR)
        mali_regulator_set_voltage(newvol, INT_MAX);
#endif
    } else {
#if defined(CONFIG_REGULATOR)
        mali_regulator_set_voltage(newvol, INT_MAX);
#endif
        mali_clk_set_rate(rate);
    }

    return err;
}

static int owl_init_gpufreq_table(void)
{
    unsigned long freq = 0;
    int err = 0, cnt = 0;
    int nent_of_freqtable = 0;

    MALI_DEBUG_PRINT(3, ("owl_init_gpufreq_table\n"));
#if defined(CONFIG_DTS_FREQ_TABLE)
    //err = of_init_opp_table(&mali_platform_device->dev);
#else
   // for (i = 0;i <= sizeof(mali_clk_table) / sizeof(mali_clk_table[0]); i++) {
   //    err  |= opp_add(&mali_platform_device->dev, mali_clk_table[i].clock * GPU_MHZ, mali_clk_table[i].vol);
   // }
#endif

    if (err)
        return err;

    rcu_read_lock();

   // nent_of_freqtable = opp_get_opp_count(&mali_platform_device->dev);
    MALI_DEBUG_PRINT(3, ("owl_init_gpufreq_table: nent_of_freqtable %d\n", nent_of_freqtable));
    gpu_freqtable = kmalloc(
            sizeof(struct mali_gpu_clk_item) * nent_of_freqtable, GFP_KERNEL);
    if (!gpu_freqtable) {
        MALI_PRINT(("%s: kmaloc failed", __func__));
        goto rcu_unlock;
    }

    for (cnt=0; cnt<nent_of_freqtable; cnt++, freq++)
    {
       // struct opp *opp = opp_find_freq_ceil(&mali_platform_device->dev, &freq);
       // if (IS_ERR(opp))
       //     break;
        /* frequency order: from low to high. */
      //  gpu_freqtable[cnt].clock = opp_get_freq(opp) / GPU_MHZ;
      //  gpu_freqtable[cnt].vol = (unsigned int)opp_get_voltage(opp);
        MALI_DEBUG_PRINT(3, ("%d, {clock = %d,vol = %u}\n", cnt, gpu_freqtable[cnt].clock, gpu_freqtable[cnt].vol));
    }

    num_of_freqtable = cnt;

rcu_unlock:
    rcu_read_unlock();

    return err;
}

static void owl_free_gpufreq_table(void)
{
    kfree(gpu_freqtable);
    num_of_freqtable = 0;
}

int owl_get_freqtable(struct mali_gpu_clk_item **freqtable)
{
    *freqtable = gpu_freqtable;
    return num_of_freqtable;
}

static mali_bool init_mali_clock(void)
{
    mali_bool ret = MALI_TRUE;

    if (gpu_clk.clk != 0)
        return ret;

    mali_dvfs_lock = _mali_osk_mutex_rw_init(_MALI_OSK_LOCKFLAG_UNORDERED,
            _MALI_OSK_LOCK_ORDER_FIRST);
    if (mali_dvfs_lock == NULL)
        return _MALI_OSK_ERR_FAULT;

    /*TODO: */
    if (mali_clk_get() == MALI_FALSE)
        return MALI_FALSE;

    MALI_DEBUG_PRINT(3, ("init_mali_clock mali_clock %p \n", gpu_clk.clk));

    owl_init_gpufreq_table();

    MALI_DEBUG_PRINT(3, ("mali clock init ok\n"));

    return MALI_TRUE;
}

static mali_bool deinit_mali_clock(void)
{
    int i;

    if (gpu_clk.clk != 0) {
        mali_clk_enable(MALI_FALSE);
    }

    for (i = 0; i < ARRAY_SIZE(gpu_parent_clk); i++)
        clk_put(gpu_parent_clk[i].clk);

    owl_free_gpufreq_table();
    return MALI_TRUE;
}

_mali_osk_errcode_t owl_mali_runtime_resume(void)
{

    MALI_DEBUG_PRINT(3, ("Mali PM: owl_mali_runtime_resume\n"));
    mali_clk_enable(MALI_TRUE);

    MALI_SUCCESS;
}

_mali_osk_errcode_t owl_mali_runtime_suspend(void)
{
    MALI_DEBUG_PRINT(3, ("Mali PM: owl_mali_runtime_suspend\n"));
    mali_clk_enable(MALI_FALSE);

    MALI_SUCCESS;
}

_mali_osk_errcode_t owl_mali_os_resume(void)
{
    pr_info("Mali PM: owl_mali_os_resume\n");

#if FPGA_VERIFY
    owl_mali_ioremap_init();

    owl_mali_release_reset(MALI_FALSE);
    owl_mali_enable_clk(MALI_FALSE);
    owl_mali_enable_isolation(MALI_TRUE);

    udelay(500);

    owl_mali_enable_isolation(MALI_FALSE);
    owl_mali_enable_clk(MALI_TRUE);
    owl_mali_release_reset(MALI_TRUE);

    owl_mali_ioremap_release();
#else
    mali_power_enable(MALI_TRUE);
    mali_clk_enable(MALI_TRUE);
#endif

    MALI_SUCCESS;
}

_mali_osk_errcode_t owl_mali_os_suspend(void)
{
    pr_info("Mali PM: owl_mali_os_suspend\n");

    mali_clk_enable(MALI_FALSE);
    mali_power_enable(MALI_FALSE);

    MALI_SUCCESS;
}

unsigned long mali_get_default_clk(void)
{
    struct device_node *node = mali_platform_device->dev.of_node;
    u32 default_normal_clk;

    MALI_DEBUG_ASSERT(NULL != node);

    if (0 == of_property_read_u32(node, "default_freq", &default_normal_clk)) {
        return default_normal_clk * GPU_MHZ;
    } else {
        MALI_PRINT(("Couldn't find default_normal_clk in device tree configuration.\n"));
    }

    return 0;
}

mali_bool init_mali_gpu_fun_active(int policy)
{
    int step = 0;
    step = gpu_dvfs_get_step(mali_default_clk);

    owl_gpu.cur_step = step;
    owl_gpu.policy = policy;

    return MALI_TRUE;
}

_mali_osk_errcode_t mali_platform_init(void)
{
#if defined(CONFIG_REGULATOR)
    g3d_regulator.regulator = devm_regulator_get(&mali_platform_device->dev, g3d_regulator.name);

    if (IS_ERR_OR_NULL(g3d_regulator.regulator)) {
        MALI_PRINT(("MALI Error : failed to get regulator\n"));
    } else {
        mali_regulator_enable();
        /* Check whether regulator is normal or not. */
        if (regulator_get_voltage(g3d_regulator.regulator) < 0) {
            devm_regulator_put(g3d_regulator.regulator);
            g3d_regulator.regulator = NULL;
            MALI_PRINT(("MALI Error : failed to get voltage\n"));
        }
    }

#endif

    mali_default_clk = mali_get_default_clk();

    if(mali_default_clk == 0) {
        mali_default_clk = mali_driver_default_clk;
    }

    MALI_CHECK(init_mali_clock(), _MALI_OSK_ERR_FAULT);

    if (!init_mali_dvfs_status())
        MALI_PRINT(("mali_platform_init failed\n"));

    if (!init_mali_gpu_fun_active(GPUFREQ_POLICY_NORMAL))
        MALI_PRINT(("init_mali_gpu_fun_active failed"));

    owl_mali_os_resume();

    MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(void)
{
    deinit_mali_clock();
    deinit_mali_dvfs_status();

#if defined(CONFIG_REGULATOR)
    /*TODO*/
    //mali_regulator_disable();
    if (!IS_ERR_OR_NULL(g3d_regulator.regulator)) {
        devm_regulator_put(g3d_regulator.regulator);
        g3d_regulator.regulator = NULL;
    }
#endif
    owl_mali_os_suspend();
    MALI_SUCCESS;
}

int owl_gpu_fun_add_attr(struct kobject *dev_kobj)
{
    int err = 0;

    err = kobject_init_and_add(&owl_gpu.kobj, &gpu_ktype, dev_kobj, "mali");
    if (err) {
        MALI_PRINT(("failed to create sysfs file\n"));
    }
    return err;
}

ssize_t gpu_clock_show(struct owl_gpu_t *gpu, char *buf)
{
    int len = 0;
    char *policy_str[GPUFREQ_NUMBER_OF_POLICY] = {
        "powersave", "normal", "performance", "userspace",
    };

    len = sprintf(buf, "%s\n", policy_str[gpu->policy]);
    return len;
}

ssize_t gpu_clock_store(struct owl_gpu_t *gpu, const char *buf, size_t count)
{
    unsigned long freq_wanted = 0;
    int policy = GPUFREQ_POLICY_NORMAL;

    if (owl_gpu.lock == 1) {
        MALI_DEBUG_PRINT(3, ("owl device lock\n"));
        return count;
    }

    if (sysfs_streq("3007", buf) || sysfs_streq("normal", buf))
        policy = GPUFREQ_POLICY_NORMAL;
    else if (sysfs_streq("2006", buf) || sysfs_streq("powersave", buf))
        policy = GPUFREQ_POLICY_POWERSAVE;
    else if (sysfs_streq("4008", buf) || sysfs_streq("performance", buf))
        policy = GPUFREQ_POLICY_PERFORMANCE;
    else if (buf[0] == 'f' && buf[1] == '=') {
        policy = GPUFREQ_POLICY_USERSPACE;
        freq_wanted = simple_strtoul(buf + 2, NULL, 10) * GPU_MHZ;
        if (freq_wanted < 192000000) {
            freq_wanted = 192000000;
        }
        if (owl_gpu.user_freq != freq_wanted) {
            owl_gpu.user_freq = freq_wanted;
            owl_gpu.user_freq_changed = MALI_TRUE;
        }
        owl_gpu.user_freq = freq_wanted;
    } else {
        MALI_PRINT(("invalid gpu policy %s", buf));
    }

    /*TODO:save policy*/
    owl_gpu_fun_active(policy);

    return count;
}

ssize_t clock_fun_show(struct owl_gpu_t *gpu, char *buf)
{
    int len;
    unsigned long gpu_freq;
    struct clk *clk;
    clk = devm_clk_get(&mali_platform_device->dev, gpu_clk.name);
    gpu_freq = clk_get_rate(clk) / GPU_MHZ + clk_modifier;
    len = sprintf(buf, "%ld\n", gpu_freq);
    return len;
}

ssize_t clock_fun_store(struct owl_gpu_t *gpu, const char *buf, size_t count)
{
    if (buf[0] == 'm' && buf[1] == '=') {
        clk_modifier = simple_strtoul(buf + 2, NULL, 10);
        MALI_DEBUG_PRINT(3, ("Set clk modifier %d.\n", clk_modifier));
    }
    return count;
}

ssize_t lock_show(struct owl_gpu_t *gpu, char *buf)
{
    int len;
    len = sprintf(buf, "%d\n", gpu->lock);
    return len;
}

ssize_t lock_store(struct owl_gpu_t *gpu, const char *buf, size_t count)
{
    int lock;
    lock = simple_strtoul(buf, NULL, 10);
    if (lock == 1010)
        gpu->lock = 0;
    else if (lock == 1111)
        gpu->lock = 1;
    return count;
}

mali_bool owl_gpu_fun_active(int policy)
{
    mali_bool ret = MALI_TRUE;
    enum gpufreq_governor governor = GPUFREQ_GOVERNOR_UNKNOWN;
    int step  = -1;

    switch (policy) {
    case GPUFREQ_POLICY_POWERSAVE:
        MALI_DEBUG_PRINT(4, ("owl_gpu_fun_active in GPUFREQ_POLICY_POWERSAVE \n"));
        governor = GPUFREQ_GOVERNOR_POWERSAVE;
        step = 0;
        break;
    case GPUFREQ_POLICY_NORMAL:
        MALI_DEBUG_PRINT(4, ("owl_gpu_fun_active in GPUFREQ_POLICY_NORMAL \n"));
        governor = GPUFREQ_GOVERNOR_CONSERVATIVE;
        step = gpu_dvfs_get_step(mali_default_clk);
        break;
    case GPUFREQ_POLICY_PERFORMANCE:
        MALI_DEBUG_PRINT(4, ("owl_gpu_fun_active in GPUFREQ_POLICY_PERFORMANCE \n"));
        governor = GPUFREQ_GOVERNOR_PERFORMANCE;
        step = gpu_dvfs_get_maximum_step();
        break;
    case GPUFREQ_POLICY_USERSPACE:
        governor = GPUFREQ_GOVERNOR_USER;
        MALI_DEBUG_PRINT(4, ("owl_gpu_fun_active in GPUFREQ_POLICY_USERSPACE \n"));
        step = gpu_dvfs_get_step(owl_gpu.user_freq);
        break;
    default:
        MALI_DEBUG_PRINT(4, ("%s: unknown gpu policy %d \n", __func__, policy));
        ret = MALI_FALSE;
        goto err_out;
        break;
    }

    if (step == owl_gpu.cur_step) {
        MALI_DEBUG_PRINT(3, ("owl_gpu_fun_active policy not change\n"));
        return ret;
    }

    /*TODO*/
    ret = gpu_dvfs_set_governor(governor);
    if (ret == MALI_FALSE)
        goto err_out;

    MALI_DEBUG_PRINT(3, ("%s: policy %d, governor %d, step = %d.\n", __func__, policy, governor, step));

    if (step >= 0) {
        ret = gpu_dvfs_set_step(step);
    }

    owl_gpu.cur_step = step;
    owl_gpu.policy = policy;

err_out:
    return ret;
}
