/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_GPU_POWER_MODE_H
#define _LINUX_GPU_POWER_MODE_H

#include <linux/device.h>
#include <linux/pm.h>

/* Performance profiles */
enum gpu_power_profile {
    GPU_PROFILE_POWER_SAVE,    /* Battery optimization - reduced performance */
    GPU_PROFILE_BALANCED,      /* Normal balanced performance */
    GPU_PROFILE_HIGH_PERF,     /* Maximum performance with AI optimization */
};

/* Profile configurations */
struct gpu_profile_config {
    /* Core frequency ranges */
    unsigned int min_core_freq;
    unsigned int max_core_freq;
    
    /* Memory frequency ranges */
    unsigned int min_mem_freq;
    unsigned int max_mem_freq;
    
    /* Power limits in milliwatts */
    unsigned int power_limit;       /* Max power consumption */
    
    /* Thermal limits in millicelsius */
    unsigned int temp_limit;        /* Max temperature target */
    
    /* Voltage offsets in microvolts */
    int voltage_offset;            /* + or - voltage adjustment */
    
    /* Fan control */
    unsigned int fan_min_speed;     /* Minimum fan speed % */
    unsigned int fan_target_temp;   /* Target temperature for fan control */
    
    /* AI Performance parameters */
    bool ai_boost_enabled;          /* Enable AI performance boost */
    unsigned int ai_boost_duration; /* Max duration of AI boost in ms */
    unsigned int ai_sample_interval;/* Sampling interval for AI optimization */
};

/* Performance metrics collection */
struct gpu_metrics {
    unsigned int fps;
    unsigned int frame_time;
    unsigned int gpu_load;
    unsigned int vram_load;
    unsigned int power_draw;
    unsigned int temperature;
    ktime_t timestamp;
};

/* Workload statistics */
struct gpu_workload_stats {
    atomic_t active_contexts;
    u64 total_render_time_ns;
    u64 total_compute_time_ns;
    u32 avg_batch_size;
    u32 last_gpu_load;
};

/* Main profile control structure */
struct gpu_power_control {
    enum gpu_power_profile current_profile;
    struct gpu_profile_config profiles[3]; /* One per profile */
    
    /* Hardware limits */
    unsigned int hw_max_freq;
    unsigned int hw_min_freq;
    unsigned int hw_max_power;
    
    /* Profile switching */
    void (*set_profile)(struct device *dev, enum gpu_power_profile profile);
    void (*update_config)(struct device *dev, struct gpu_profile_config *config);
    
    /* Metrics and statistics */
    struct gpu_metrics metrics;
    struct gpu_workload_stats stats;
    
    /* Profile persistence */
    bool profile_dirty;
    struct delayed_work persist_work;
    
    /* Workload detection */
    spinlock_t workload_lock;
    struct list_head active_workloads;
};

/* Profile management functions */
int gpu_power_init_profiles(struct device *dev, struct gpu_power_control *control);
int gpu_power_set_profile(struct device *dev, enum gpu_power_profile profile);
int gpu_power_get_current_profile(struct device *dev);

/* AI optimization functions */
int gpu_ai_optimize_perf(struct device *dev);
void gpu_ai_update_metrics(struct device *dev, 
                         unsigned int fps,
                         unsigned int power,
                         unsigned int temp,
                         unsigned int utilization);

/* Profile persistence */
int gpu_profile_save(struct device *dev, struct gpu_power_control *control);
int gpu_profile_load(struct device *dev, struct gpu_power_control *control);

/* Workload detection and optimization */
void gpu_workload_notify(struct device *dev,
                        struct gpu_power_control *control,
                        struct task_struct *tsk);
                        
/* Extended metrics collection */
void gpu_update_extended_metrics(struct device *dev,
                               unsigned int frame_time,
                               unsigned int vram_usage,
                               unsigned int batch_size);

/* Profile optimization flags */
#define GPU_OPT_ADAPTIVE_SYNC  (1 << 0)  /* Enable adaptive sync */
#define GPU_OPT_LOW_LATENCY    (1 << 1)  /* Optimize for low latency */
#define GPU_OPT_POWER_SAVE     (1 << 2)  /* Aggressive power saving */
#define GPU_OPT_ML_COMPUTE     (1 << 3)  /* ML/compute workload */
#define GPU_OPT_RAY_TRACING    (1 << 4)  /* Ray tracing workload */

#endif /* _LINUX_GPU_POWER_MODE_H */
