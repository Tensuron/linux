// SPDX-License-Identifier: GPL-2.0-only
/*
 * Intel GPU Power Profile Management Integration
 *
 * Copyright (c) 2025 Linux Foundation
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/gpu_power_mode.h>
#include <drm/i915_drm.h>
#include "i915_drv.h"
#include "intel_pm.h"

struct intel_power_info {
    struct gpu_power_control base;
    struct drm_i915_private *i915;
    
    /* Original values for restore */
    u32 orig_min_freq;
    u32 orig_max_freq;
    u32 orig_boost_freq;
    u32 orig_power_limit;
};

static void intel_set_power_profile(struct device *dev, enum gpu_power_profile profile)
{
    struct intel_power_info *info = dev_get_drvdata(dev);
    struct drm_i915_private *i915 = info->i915;
    struct gpu_profile_config *config = &info->base.profiles[profile];
    struct intel_rps *rps = &i915->gt.rps;
    
    /* Update frequency limits */
    rps->min_freq = intel_gpu_freq(rps, config->min_core_freq / 1000);
    rps->max_freq = intel_gpu_freq(rps, config->max_core_freq / 1000);
    
    /* Update power limits */
    intel_set_power_limit(i915, config->power_limit);
    
    /* Configure power management features */
    switch (profile) {
    case GPU_PROFILE_POWER_SAVE:
        /* Enable aggressive power saving */
        intel_enable_rc6(i915);
        intel_enable_fbc(i915);
        intel_disable_boost_freq(rps);
        break;
        
    case GPU_PROFILE_BALANCED:
        /* Balanced power saving */
        intel_enable_rc6(i915);
        intel_enable_fbc(i915);
        intel_enable_boost_freq(rps);
        break;
        
    case GPU_PROFILE_HIGH_PERF:
        /* Maximum performance */
        intel_disable_rc6(i915);
        intel_disable_fbc(i915);
        intel_enable_boost_freq(rps);
        if (config->ai_boost_enabled)
            intel_set_boost_freq(rps, rps->max_freq);
        break;
    }
    
    /* Apply voltage offset if supported */
    if (IS_XEHPSDV(i915) || IS_PONTEVECCHIO(i915)) {
        int voltage_offset = DIV_ROUND_CLOSEST(config->voltage_offset, 1000);
        intel_set_voltage_offset(i915, voltage_offset);
    }
    
    /* Update memory controller frequency if supported */
    if (HAS_MEMORY_CLK_CONTROL(i915)) {
        intel_set_memory_freq(i915, 
                            config->min_mem_freq / 1000,
                            config->max_mem_freq / 1000);
    }
    
    /* Force GPU frequency update */
    intel_rps_mark_interactive(rps, true);
    intel_rps_update_frequencies(rps);
}

static void intel_update_power_config(struct device *dev,
                                    struct gpu_profile_config *config)
{
    struct intel_power_info *info = dev_get_drvdata(dev);
    struct drm_i915_private *i915 = info->i915;
    struct intel_rps *rps = &i915->gt.rps;
    
    /* Update dynamic parameters */
    rps->min_freq = intel_gpu_freq(rps, config->min_core_freq / 1000);
    rps->max_freq = intel_gpu_freq(rps, config->max_core_freq / 1000);
    
    intel_set_power_limit(i915, config->power_limit);
    
    if (info->base.current_profile == GPU_PROFILE_HIGH_PERF && 
        config->ai_boost_enabled)
        intel_set_boost_freq(rps, rps->max_freq);
        
    /* Force frequency update */
    intel_rps_update_frequencies(rps);
}

static void intel_get_metrics(struct drm_i915_private *i915,
                            unsigned int *fps,
                            unsigned int *power,
                            unsigned int *temp,
                            unsigned int *util)
{
    struct intel_rps *rps = &i915->gt.rps;
    
    /* Get FPS from vblank counter if available */
    struct drm_crtc *crtc;
    drm_for_each_crtc(crtc, &i915->drm) {
        if (crtc->enabled) {
            *fps = drm_vblank_count(&i915->drm, drm_crtc_index(crtc));
            break;
        }
    }
    
    /* Get power consumption */
    *power = intel_get_gpu_power(i915);
    
    /* Get GPU temperature */
    *temp = intel_read_gpu_temp(i915) * 1000;
    
    /* Get GPU utilization */
    *util = intel_get_gpu_utilization(rps);
}

static void intel_power_metrics_work(struct work_struct *work)
{
    struct intel_power_info *info = container_of(work, struct intel_power_info,
                                               base.metrics_work);
    struct device *dev = info->i915->drm.dev;
    unsigned int fps, power, temp, util;
    
    /* Get current metrics */
    intel_get_metrics(info->i915, &fps, &power, &temp, &util);
    
    /* Update AI system */
    gpu_ai_update_metrics(dev, fps, power, temp, util);
    
    /* Run optimization if in high performance mode */
    if (info->base.current_profile == GPU_PROFILE_HIGH_PERF)
        gpu_ai_optimize_perf(dev);
        
    /* Schedule next update if still in high perf mode */
    if (info->base.current_profile == GPU_PROFILE_HIGH_PERF) {
        unsigned int interval = info->base.profiles[GPU_PROFILE_HIGH_PERF].ai_sample_interval;
        schedule_delayed_work(&info->base.metrics_work, msecs_to_jiffies(interval));
    }
}

int intel_gpu_power_init(struct drm_i915_private *i915)
{
    struct device *dev = i915->drm.dev;
    struct intel_power_info *info;
    struct intel_rps *rps = &i915->gt.rps;
    
    info = kzalloc(sizeof(*info), GFP_KERNEL);
    if (!info)
        return -ENOMEM;
        
    info->i915 = i915;
    info->base.set_profile = intel_set_power_profile;
    info->base.update_config = intel_update_power_config;
    
    /* Store original values */
    info->orig_min_freq = rps->min_freq;
    info->orig_max_freq = rps->max_freq;
    info->orig_boost_freq = rps->boost_freq;
    info->orig_power_limit = intel_get_power_limit(i915);
    
    /* Set hardware limits */
    info->base.hw_max_freq = intel_gpu_freq_to_khz(rps, rps->max_freq_softlimit) * 1000;
    info->base.hw_min_freq = intel_gpu_freq_to_khz(rps, rps->min_freq_softlimit) * 1000;
    info->base.hw_max_power = i915->gt.platform_power_max;
    
    /* Initialize metrics work */
    INIT_DELAYED_WORK(&info->base.metrics_work, intel_power_metrics_work);
    
    dev_set_drvdata(dev, info);
    
    return gpu_power_init_profiles(dev, &info->base);
}
EXPORT_SYMBOL(intel_gpu_power_init);

void intel_gpu_power_fini(struct drm_i915_private *i915)
{
    struct device *dev = i915->drm.dev;
    struct intel_power_info *info = dev_get_drvdata(dev);
    struct intel_rps *rps = &i915->gt.rps;
    
    if (!info)
        return;
        
    /* Cancel any pending work */
    cancel_delayed_work_sync(&info->base.metrics_work);
    
    /* Restore original values */
    rps->min_freq = info->orig_min_freq;
    rps->max_freq = info->orig_max_freq;
    rps->boost_freq = info->orig_boost_freq;
    intel_set_power_limit(i915, info->orig_power_limit);
    
    /* Force final frequency update */
    intel_rps_update_frequencies(rps);
    
    kfree(info);
    dev_set_drvdata(dev, NULL);
}
EXPORT_SYMBOL(intel_gpu_power_fini);
