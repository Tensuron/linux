// SPDX-License-Identifier: GPL-2.0-only
/*
 * GPU Power Profile Management and AI Optimization
 *
 * Copyright (c) 2025 Linux Foundation
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/gpu_power_mode.h>

/* Default profile configurations */
static const struct gpu_profile_config default_profiles[3] = {
    [GPU_PROFILE_POWER_SAVE] = {
        .min_core_freq = 300000,  /* 300MHz base */
        .max_core_freq = 800000,  /* 800MHz max */
        .min_mem_freq = 400000,   /* 400MHz min memory */
        .max_mem_freq = 1200000,  /* 1.2GHz max memory */
        .power_limit = 35000,     /* 35W power limit */
        .temp_limit = 75000,      /* 75°C temp limit */
        .voltage_offset = -50000, /* -50mV undervolt */
        .fan_min_speed = 20,      /* 20% minimum fan */
        .fan_target_temp = 65000, /* 65°C target */
        .ai_boost_enabled = false,
        .ai_boost_duration = 0,
        .ai_sample_interval = 0,
    },
    [GPU_PROFILE_BALANCED] = {
        .min_core_freq = 500000,  /* 500MHz base */
        .max_core_freq = 1500000, /* 1.5GHz max */
        .min_mem_freq = 800000,   /* 800MHz min memory */
        .max_mem_freq = 1600000,  /* 1.6GHz max memory */
        .power_limit = 80000,     /* 80W power limit */
        .temp_limit = 85000,      /* 85°C temp limit */
        .voltage_offset = 0,      /* Stock voltage */
        .fan_min_speed = 30,      /* 30% minimum fan */
        .fan_target_temp = 75000, /* 75°C target */
        .ai_boost_enabled = false,
        .ai_boost_duration = 0,
        .ai_sample_interval = 0,
    },
    [GPU_PROFILE_HIGH_PERF] = {
        .min_core_freq = 800000,  /* 800MHz base */
        .max_core_freq = 2500000, /* 2.5GHz max */
        .min_mem_freq = 1200000,  /* 1.2GHz min memory */
        .max_mem_freq = 2000000,  /* 2GHz max memory */
        .power_limit = 150000,    /* 150W power limit */
        .temp_limit = 95000,      /* 95°C temp limit */
        .voltage_offset = 25000,  /* +25mV overvolt */
        .fan_min_speed = 40,      /* 40% minimum fan */
        .fan_target_temp = 85000, /* 85°C target */
        .ai_boost_enabled = true,
        .ai_boost_duration = 5000,   /* 5sec boost */
        .ai_sample_interval = 100,   /* 100ms sampling */
    },
};

/* AI optimization metrics tracking */
struct ai_metrics {
    unsigned int fps_history[100];
    unsigned int power_history[100];
    unsigned int temp_history[100];
    unsigned int util_history[100];
    int history_idx;
    
    unsigned int fps_target;
    unsigned int temp_target;
    unsigned int power_target;
};

static DEFINE_MUTEX(profile_lock);

/**
 * gpu_power_init_profiles - Initialize power profiles for a GPU device
 * @dev: GPU device to initialize
 * @control: Power control structure to initialize
 */
int gpu_power_init_profiles(struct device *dev, struct gpu_power_control *control)
{
    if (!dev || !control)
        return -EINVAL;

    mutex_lock(&profile_lock);
    
    /* Set up default profile configs */
    memcpy(control->profiles, default_profiles, sizeof(default_profiles));
    
    /* Start in balanced mode */
    control->current_profile = GPU_PROFILE_BALANCED;
    
    /* Apply initial profile */
    if (control->set_profile)
        control->set_profile(dev, GPU_PROFILE_BALANCED);
        
    mutex_unlock(&profile_lock);
    
    return 0;
}
EXPORT_SYMBOL_GPL(gpu_power_init_profiles);

/**
 * gpu_power_set_profile - Switch to a different power profile
 * @dev: GPU device to update
 * @profile: New profile to apply
 */
int gpu_power_set_profile(struct device *dev, enum gpu_power_profile profile)
{
    struct gpu_power_control *control;
    
    if (!dev || profile > GPU_PROFILE_HIGH_PERF)
        return -EINVAL;
        
    control = dev_get_drvdata(dev);
    if (!control)
        return -ENODEV;

    mutex_lock(&profile_lock);
    
    /* Update current profile */
    control->current_profile = profile;
    
    /* Apply new profile settings */
    if (control->set_profile)
        control->set_profile(dev, profile);
    
    if (control->update_config)
        control->update_config(dev, &control->profiles[profile]);
        
    mutex_unlock(&profile_lock);
    
    return 0;
}
EXPORT_SYMBOL_GPL(gpu_power_set_profile);

/**
 * gpu_ai_optimize_perf - Run AI performance optimization
 * @dev: GPU device to optimize
 */
int gpu_ai_optimize_perf(struct device *dev)
{
    struct gpu_power_control *control;
    struct ai_metrics *metrics;
    struct gpu_profile_config *config;
    int i, avg_fps = 0, avg_temp = 0, avg_power = 0;

    control = dev_get_drvdata(dev);
    if (!control)
        return -ENODEV;

    /* Only run in high performance mode with AI enabled */
    if (control->current_profile != GPU_PROFILE_HIGH_PERF ||
        !control->profiles[GPU_PROFILE_HIGH_PERF].ai_boost_enabled)
        return 0;

    metrics = dev_get_drvdata(dev);
    if (!metrics)
        return -ENODATA;

    config = &control->profiles[GPU_PROFILE_HIGH_PERF];

    mutex_lock(&profile_lock);

    /* Calculate averages from history */
    for (i = 0; i < 100; i++) {
        avg_fps += metrics->fps_history[i];
        avg_temp += metrics->temp_history[i];
        avg_power += metrics->power_history[i];
    }
    avg_fps /= 100;
    avg_temp /= 100;
    avg_power /= 100;

    /* Adjust frequencies based on metrics */
    if (avg_fps < metrics->fps_target && avg_temp < config->temp_limit) {
        /* Boost performance if we have thermal headroom */
        config->min_core_freq = min(control->hw_max_freq,
                                  config->min_core_freq + 50000);
        config->max_core_freq = min(control->hw_max_freq,
                                  config->max_core_freq + 100000);
    } else if (avg_temp > config->temp_limit || avg_power > config->power_limit) {
        /* Reduce frequencies if hitting limits */
        config->min_core_freq = max(control->hw_min_freq,
                                  config->min_core_freq - 50000);
        config->max_core_freq = max(control->hw_min_freq,
                                  config->max_core_freq - 100000);
    }

    /* Apply updated configuration */
    if (control->update_config)
        control->update_config(dev, config);

    mutex_unlock(&profile_lock);

    return 0;
}
EXPORT_SYMBOL_GPL(gpu_ai_optimize_perf);

/**
 * gpu_ai_update_metrics - Update AI optimization metrics
 */
void gpu_ai_update_metrics(struct device *dev,
                          unsigned int fps,
                          unsigned int power,
                          unsigned int temp,
                          unsigned int utilization)
{
    struct ai_metrics *metrics = dev_get_drvdata(dev);
    
    if (!metrics)
        return;

    mutex_lock(&profile_lock);
    
    /* Update circular buffer of metrics */
    metrics->fps_history[metrics->history_idx] = fps;
    metrics->power_history[metrics->history_idx] = power;
    metrics->temp_history[metrics->history_idx] = temp;
    metrics->util_history[metrics->history_idx] = utilization;
    
    metrics->history_idx = (metrics->history_idx + 1) % 100;
    
    mutex_unlock(&profile_lock);
}
EXPORT_SYMBOL_GPL(gpu_ai_update_metrics);

static int __init gpu_power_init(void)
{
    pr_info("GPU Power Profile Management Initialized\n");
    return 0;
}

static void __exit gpu_power_exit(void)
{
    pr_info("GPU Power Profile Management Exiting\n");
}

module_init(gpu_power_init);
module_exit(gpu_power_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPU Power Profile Management");
MODULE_AUTHOR("Linux Foundation");
