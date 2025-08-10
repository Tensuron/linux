// SPDX-License-Identifier: GPL-2.0-only
/*
 * AMD GPU Power Profile Management Integration
 *
 * Copyright (c) 2025 Linux Foundation
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/gpu_power_mode.h>
#include <drm/amd_drm.h>
#include <drm/amdgpu.h>

/* AMD-specific profile settings */
struct amdgpu_power_info {
    struct gpu_power_control base;
    struct amdgpu_device *adev;
    
    /* Original values for restore */
    u32 orig_power_limit;
    u32 orig_core_clock;
    u32 orig_memory_clock;
};

static void amdgpu_set_power_profile(struct device *dev, enum gpu_power_profile profile)
{
    struct amdgpu_power_info *info = dev_get_drvdata(dev);
    struct amdgpu_device *adev = info->adev;
    struct gpu_profile_config *config = &info->base.profiles[profile];
    
    /* Update power limit */
    amdgpu_set_power_limit(adev, config->power_limit);
    
    /* Update clock limits */
    amdgpu_set_clockgating_state(adev, AMD_CG_SUPPORT_GFX_MGCG, 
                                profile == GPU_PROFILE_POWER_SAVE);
    
    amdgpu_set_powergating_state(adev, AMD_PG_SUPPORT_GFX_PG,
                                profile == GPU_PROFILE_POWER_SAVE);
    
    /* Set core clock range */
    amdgpu_set_clock_limit(adev, PP_SCLK, 
                          config->min_core_freq / 100,
                          config->max_core_freq / 100);
                          
    /* Set memory clock range */
    amdgpu_set_clock_limit(adev, PP_MCLK,
                          config->min_mem_freq / 100,
                          config->max_mem_freq / 100);
    
    /* Update fan control */
    if (adev->pm.fan) {
        amdgpu_fan_set_min_pwm(adev, config->fan_min_speed);
        amdgpu_fan_set_target_temperature(adev, config->fan_target_temp / 1000);
    }
    
    /* Apply voltage offset if supported */
    if (adev->pm.ppm_table) {
        int vid_offset = DIV_ROUND_CLOSEST(config->voltage_offset, 6250);
        amdgpu_set_vddc_offset(adev, vid_offset);
    }
}

static void amdgpu_update_power_config(struct device *dev, 
                                     struct gpu_profile_config *config)
{
    struct amdgpu_power_info *info = dev_get_drvdata(dev);
    struct amdgpu_device *adev = info->adev;
    
    /* Update dynamic parameters */
    amdgpu_set_power_limit(adev, config->power_limit);
    
    amdgpu_set_clock_limit(adev, PP_SCLK,
                          config->min_core_freq / 100,
                          config->max_core_freq / 100);
}

static void amdgpu_get_metrics(struct amdgpu_device *adev,
                             unsigned int *fps,
                             unsigned int *power,
                             unsigned int *temp,
                             unsigned int *util)
{
    /* Get FPS from DRM stats if available */
    struct drm_device *ddev = adev_to_drm(adev);
    *fps = ddev->vblank_count[0] - ddev->vblank_count_last[0];
    ddev->vblank_count_last[0] = ddev->vblank_count[0];
    
    /* Get power consumption in milliwatts */
    *power = amdgpu_get_power_usage(adev);
    
    /* Get temperature in millicelsius */
    *temp = amdgpu_get_temperature(adev) * 1000;
    
    /* Get GPU utilization percentage */
    *util = amdgpu_get_gpu_usage(adev);
}

static void amdgpu_power_metrics_work(struct work_struct *work)
{
    struct amdgpu_power_info *info = container_of(work, struct amdgpu_power_info,
                                                 base.metrics_work);
    struct device *dev = &info->adev->pdev->dev;
    unsigned int fps, power, temp, util;
    
    /* Get current metrics */
    amdgpu_get_metrics(info->adev, &fps, &power, &temp, &util);
    
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

int amdgpu_gpu_power_init(struct amdgpu_device *adev)
{
    struct device *dev = &adev->pdev->dev;
    struct amdgpu_power_info *info;
    
    info = kzalloc(sizeof(*info), GFP_KERNEL);
    if (!info)
        return -ENOMEM;
        
    info->adev = adev;
    info->base.set_profile = amdgpu_set_power_profile;
    info->base.update_config = amdgpu_update_power_config;
    
    /* Store original values */
    info->orig_power_limit = amdgpu_get_power_limit(adev);
    info->orig_core_clock = amdgpu_get_engine_clock(adev);
    info->orig_memory_clock = amdgpu_get_memory_clock(adev);
    
    /* Set hardware limits */
    info->base.hw_max_freq = adev->pm.dpm.dyn_state.max_clock[PP_SCLK] * 100;
    info->base.hw_min_freq = adev->pm.dpm.dyn_state.min_clock[PP_SCLK] * 100;
    info->base.hw_max_power = adev->pm.dpm.dyn_state.max_power_limit;
    
    /* Initialize metrics work */
    INIT_DELAYED_WORK(&info->base.metrics_work, amdgpu_power_metrics_work);
    
    dev_set_drvdata(dev, info);
    
    return gpu_power_init_profiles(dev, &info->base);
}
EXPORT_SYMBOL(amdgpu_gpu_power_init);

void amdgpu_gpu_power_fini(struct amdgpu_device *adev)
{
    struct device *dev = &adev->pdev->dev;
    struct amdgpu_power_info *info = dev_get_drvdata(dev);
    
    if (!info)
        return;
        
    /* Cancel any pending work */
    cancel_delayed_work_sync(&info->base.metrics_work);
    
    /* Restore original values */
    amdgpu_set_power_limit(adev, info->orig_power_limit);
    amdgpu_set_clock_limit(adev, PP_SCLK,
                          info->orig_core_clock / 100,
                          info->orig_core_clock / 100);
    amdgpu_set_clock_limit(adev, PP_MCLK,
                          info->orig_memory_clock / 100,
                          info->orig_memory_clock / 100);
                          
    kfree(info);
    dev_set_drvdata(dev, NULL);
}
EXPORT_SYMBOL(amdgpu_gpu_power_fini);
