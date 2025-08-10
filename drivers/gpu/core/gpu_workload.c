// SPDX-License-Identifier: GPL-2.0-only
/*
 * GPU Workload Detection and Optimization
 *
 * Copyright (c) 2025 Linux Foundation
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/gpu_power_mode.h>
#include <linux/binfmts.h>
#include <linux/hash.h>
#include <linux/crc32.h>

/* Maximum number of known applications */
#define MAX_KNOWN_APPS 1024

/* Application profile flags */
#define APP_PROFILE_GAME      (1 << 0)
#define APP_PROFILE_3D_RENDER (1 << 1)
#define APP_PROFILE_VIDEO_EDIT (1 << 2)
#define APP_PROFILE_ML        (1 << 3)

/* Application profile structure */
struct app_profile {
    char comm[TASK_COMM_LEN];
    u32 binary_hash;
    u32 flags;
    struct gpu_profile_config config;
    struct hlist_node node;
};

/* Profile database */
static DEFINE_HASHTABLE(app_profiles, 10); /* 1024 buckets */
static DEFINE_SPINLOCK(profile_lock);

/* Known gaming engines/frameworks signatures */
static const char *game_signatures[] = {
    "Unity",
    "UnrealEngine",
    "Godot",
    "SDL",
    "GLFW",
    "Vulkan",
    "DirectX",
    "OpenGL",
    NULL
};

/* Known 3D/Video apps signatures */
static const char *render_signatures[] = {
    "Blender",
    "Maya",
    "3dsMax",
    "Cinema4D",
    "DaVinci",
    "PremierePro",
    "AfterEffects",
    NULL
};

/* Known ML framework signatures */
static const char *ml_signatures[] = {
    "TensorFlow",
    "PyTorch",
    "CUDA",
    "OpenCL",
    "ROCm",
    NULL
};

/**
 * detect_app_type - Detect application type from binary
 * @mm: Memory descriptor of the process
 * @flags: Detected flags
 */
static void detect_app_type(struct mm_struct *mm, u32 *flags)
{
    char *binary;
    int i;
    
    /* Get binary path */
    binary = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!binary)
        return;
        
    if (mm->exe_file && getname_kernel(mm->exe_file->f_path.dentry, binary, PATH_MAX) > 0) {
        /* Check for game signatures */
        for (i = 0; game_signatures[i]; i++) {
            if (strstr(binary, game_signatures[i])) {
                *flags |= APP_PROFILE_GAME;
                break;
            }
        }
        
        /* Check for 3D/Video signatures */
        for (i = 0; render_signatures[i]; i++) {
            if (strstr(binary, render_signatures[i])) {
                *flags |= APP_PROFILE_3D_RENDER | APP_PROFILE_VIDEO_EDIT;
                break;
            }
        }
        
        /* Check for ML signatures */
        for (i = 0; ml_signatures[i]; i++) {
            if (strstr(binary, ml_signatures[i])) {
                *flags |= APP_PROFILE_ML;
                break;
            }
        }
    }
    
    kfree(binary);
}

/**
 * optimize_for_workload - Optimize GPU settings for detected workload
 * @dev: GPU device
 * @control: Power control structure
 * @flags: Application type flags
 */
static void optimize_for_workload(struct device *dev,
                                struct gpu_power_control *control,
                                u32 flags)
{
    struct gpu_profile_config *config = &control->profiles[control->current_profile];
    
    /* Start with high performance profile for all intensive workloads */
    if (flags & (APP_PROFILE_GAME | APP_PROFILE_3D_RENDER | APP_PROFILE_ML)) {
        gpu_power_set_profile(dev, GPU_PROFILE_HIGH_PERF);
        config->ai_boost_enabled = true;
    }
    
    /* Gaming optimizations */
    if (flags & APP_PROFILE_GAME) {
        /* Optimize for low latency and consistent frame times */
        config->min_core_freq = max(config->min_core_freq,
                                  control->hw_max_freq * 70 / 100);
        config->ai_boost_duration = 2000; /* 2ms boost duration */
        config->ai_sample_interval = 16; /* ~60Hz sampling */
    }
    
    /* 3D/Video rendering optimizations */
    if (flags & APP_PROFILE_3D_RENDER) {
        /* Optimize for maximum sustained performance */
        config->power_limit = control->hw_max_power * 90 / 100;
        config->temp_limit = 85000; /* 85°C */
        config->ai_sample_interval = 100; /* 100ms sampling */
    }
    
    /* ML workload optimizations */
    if (flags & APP_PROFILE_ML) {
        /* Optimize for compute performance */
        config->min_core_freq = control->hw_max_freq * 80 / 100;
        config->min_mem_freq = control->hw_max_freq * 90 / 100;
        config->ai_sample_interval = 500; /* 500ms sampling */
    }
    
    /* Apply optimizations */
    if (control->update_config)
        control->update_config(dev, config);
}

/**
 * add_app_profile - Add application profile to database
 * @comm: Command name
 * @binary_hash: Binary hash
 * @flags: Application type flags
 * @config: GPU configuration
 */
int add_app_profile(const char *comm, u32 binary_hash, u32 flags,
                   struct gpu_profile_config *config)
{
    struct app_profile *profile;
    
    profile = kmalloc(sizeof(*profile), GFP_KERNEL);
    if (!profile)
        return -ENOMEM;
        
    strncpy(profile->comm, comm, TASK_COMM_LEN);
    profile->binary_hash = binary_hash;
    profile->flags = flags;
    if (config)
        memcpy(&profile->config, config, sizeof(*config));
        
    spin_lock(&profile_lock);
    hash_add(app_profiles, &profile->node, binary_hash);
    spin_unlock(&profile_lock);
    
    return 0;
}
EXPORT_SYMBOL_GPL(add_app_profile);

/**
 * lookup_app_profile - Find application profile
 * @comm: Command name
 * @binary_hash: Binary hash
 */
struct app_profile *lookup_app_profile(const char *comm, u32 binary_hash)
{
    struct app_profile *profile;
    
    spin_lock(&profile_lock);
    hash_for_each_possible(app_profiles, profile, node, binary_hash) {
        if (profile->binary_hash == binary_hash &&
            !strncmp(profile->comm, comm, TASK_COMM_LEN)) {
            spin_unlock(&profile_lock);
            return profile;
        }
    }
    spin_unlock(&profile_lock);
    
    return NULL;
}
EXPORT_SYMBOL_GPL(lookup_app_profile);

/**
 * gpu_workload_notify - Handle new GPU workload
 * @dev: GPU device
 * @control: Power control structure
 * @tsk: Task starting GPU workload
 */
void gpu_workload_notify(struct device *dev,
                        struct gpu_power_control *control,
                        struct task_struct *tsk)
{
    struct mm_struct *mm;
    u32 binary_hash;
    u32 flags = 0;
    struct app_profile *profile;
    
    if (!tsk || !tsk->mm)
        return;
        
    mm = tsk->mm;
    
    /* Calculate binary hash */
    if (mm->exe_file) {
        binary_hash = crc32_le(0, mm->exe_file->f_path.dentry->d_name.name,
                             strlen(mm->exe_file->f_path.dentry->d_name.name));
    } else {
        binary_hash = 0;
    }
    
    /* Look for existing profile */
    profile = lookup_app_profile(tsk->comm, binary_hash);
    if (profile) {
        /* Use stored profile */
        flags = profile->flags;
        if (control->update_config)
            control->update_config(dev, &profile->config);
    } else {
        /* Detect application type */
        detect_app_type(mm, &flags);
        
        /* Create new profile if workload detected */
        if (flags) {
            add_app_profile(tsk->comm, binary_hash, flags,
                          &control->profiles[control->current_profile]);
        }
    }
    
    /* Optimize for detected workload */
    if (flags)
        optimize_for_workload(dev, control, flags);
}
EXPORT_SYMBOL_GPL(gpu_workload_notify);
