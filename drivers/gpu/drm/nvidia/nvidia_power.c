// SPDX-License-Identifier: GPL-2.0-only
/*
 * NVIDIA GPU Power Profile Management Integration
 *
 * Copyright (c) 2025 Linux Foundation
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/gpu_power_mode.h>
#include "nv-linux.h"
#include "nv-reg.h"

struct nvidia_power_info {
    struct gpu_power_control base;
    struct nv_device *nv;
    
    /* Original values for restore */
    NvU32 orig_power_limit;
    NvU32 orig_core_clock;
    NvU32 orig_memory_clock;
    NvU32 orig_voltage;
};

static void nvidia_set_power_profile(struct device *dev, enum gpu_power_profile profile)
{
    struct nvidia_power_info *info = dev_get_drvdata(dev);
    struct nv_device *nv = info->nv;
    struct gpu_profile_config *config = &info->base.profiles[profile];
    
    /* Update power limit */
    nv_set_power_limit(nv, config->power_limit);
    
    /* Set GPU clock limits */
    nv_set_gpu_clocks(nv, config->min_core_freq / 1000,
                      config->max_core_freq / 1000);
                      
    /* Set memory clock limits */
    nv_set_memory_clocks(nv, config->min_mem_freq / 1000,
                         config->max_mem_freq / 1000);
    
    /* Update fan control */
    nv_set_fan_control(nv, config->fan_min_speed,
                       config->fan_target_temp / 1000);
    
    /* Apply voltage offset */
    if (config->voltage_offset)
        nv_set_voltage_offset(nv, config->voltage_offset / 1000);
    
    /* Configure power management mode */
    switch (profile) {
    case GPU_PROFILE_POWER_SAVE:
        nv_set_power_state(nv, NV_POWER_STATE_ADAPTIVE);
        break;
    case GPU_PROFILE_BALANCED:
        nv_set_power_state(nv, NV_POWER_STATE_BALANCED);
        break;
    case GPU_PROFILE_HIGH_PERF:
        nv_set_power_state(nv, NV_POWER_STATE_MAXIMUM_PERFORMANCE);
        break;
    }
}

static void nvidia_update_power_config(struct device *dev,
                                     struct gpu_profile_config *config)
{
    struct nvidia_power_info *info = dev_get_drvdata(dev);
    struct nv_device *nv = info->nv;
    
    /* Update dynamic parameters */
    nv_set_power_limit(nv, config->power_limit);
    
    nv_set_gpu_clocks(nv, config->min_core_freq / 1000,
                      config->max_core_freq / 1000);
}

static void nvidia_get_metrics(struct nv_device *nv,
                             unsigned int *fps,
                             unsigned int *power,
                             unsigned int *temp,
                             unsigned int *util)
{
    NV_STATUS status;
    NvU32 value;
    
    /* Get FPS from NVAPI if available */
    status = nv_get_framerate(nv, &value);
    *fps = (status == NV_OK) ? value : 0;
    
    /* Get power consumption (in milliwatts) */
    status = nv_get_power_usage(nv, &value);
    *power = (status == NV_OK) ? value : 0;
    
    /* Get temperature (in millicelsius) */
    status = nv_get_temperature(nv, &value);
    *temp = (status == NV_OK) ? value * 1000 : 0;
    
    /* Get GPU utilization percentage */
    status = nv_get_utilization(nv, &value);
    *util = (status == NV_OK) ? value : 0;
}

static void nvidia_power_metrics_work(struct work_struct *work)
{
    struct nvidia_power_info *info = container_of(work, struct nvidia_power_info,
                                                base.metrics_work);
    struct device *dev = info->nv->dev;
    unsigned int fps, power, temp, util;
    
    /* Get current metrics */
    nvidia_get_metrics(info->nv, &fps, &power, &temp, &util);
    
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

int nvidia_gpu_power_init(struct nv_device *nv)
{
    struct device *dev = nv->dev;
    struct nvidia_power_info *info;
    NV_STATUS status;
    
    info = kzalloc(sizeof(*info), GFP_KERNEL);
    if (!info)
        return -ENOMEM;
        
    info->nv = nv;
    info->base.set_profile = nvidia_set_power_profile;
    info->base.update_config = nvidia_update_power_config;
    
    /* Store original values */
    status = nv_get_power_limit(nv, &info->orig_power_limit);
    if (status != NV_OK) {
        kfree(info);
        return -EIO;
    }
    
    nv_get_gpu_clocks(nv, &info->orig_core_clock, NULL);
    nv_get_memory_clocks(nv, &info->orig_memory_clock, NULL);
    
    /* Set hardware limits */
    info->base.hw_max_freq = nv->max_gpu_clock * 1000;
    info->base.hw_min_freq = nv->min_gpu_clock * 1000;
    info->base.hw_max_power = nv->max_power_limit;
    
    /* Initialize metrics work */
    INIT_DELAYED_WORK(&info->base.metrics_work, nvidia_power_metrics_work);
    
    dev_set_drvdata(dev, info);
    
    return gpu_power_init_profiles(dev, &info->base);
}
EXPORT_SYMBOL(nvidia_gpu_power_init);

void nvidia_gpu_power_fini(struct nv_device *nv)
{
    struct device *dev = nv->dev;
    struct nvidia_power_info *info = dev_get_drvdata(dev);
    
    if (!info)
        return;
        
    /* Cancel any pending work */
    cancel_delayed_work_sync(&info->base.metrics_work);
    
    /* Restore original values */
    nv_set_power_limit(nv, info->orig_power_limit);
    nv_set_gpu_clocks(nv, info->orig_core_clock, info->orig_core_clock);
    nv_set_memory_clocks(nv, info->orig_memory_clock, info->orig_memory_clock);
    
    kfree(info);
    dev_set_drvdata(dev, NULL);
}
EXPORT_SYMBOL(nvidia_gpu_power_fini);
