// SPDX-License-Identifier: GPL-2.0-only
/*
 * GPU Power Profile Persistence
 *
 * Copyright (c) 2025 Linux Foundation
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/nvram.h>
#include <linux/gpu_power_mode.h>

/* NVRAM offsets for storing profiles */
#define GPU_NVRAM_SIGNATURE    0xGPU1
#define GPU_NVRAM_OFFSET      0x100
#define GPU_NVRAM_SIZE        0x1000

struct gpu_nvram_data {
    u32 signature;
    u32 version;
    struct {
        enum gpu_power_profile profile;
        bool ai_boost_enabled;
        struct gpu_profile_config config;
    } per_gpu[8];
};

/**
 * gpu_profile_save - Save profile configuration to NVRAM
 * @dev: GPU device
 * @control: Power control structure
 */
int gpu_profile_save(struct device *dev, struct gpu_power_control *control)
{
    struct gpu_nvram_data data;
    int gpu_index;
    
    /* Find GPU index from PCI address */
    gpu_index = pci_dev_id(to_pci_dev(dev));
    if (gpu_index >= 8)
        return -EINVAL;
        
    /* Initialize data */
    memset(&data, 0, sizeof(data));
    data.signature = GPU_NVRAM_SIGNATURE;
    data.version = 1;
    
    /* Store current profile data */
    data.per_gpu[gpu_index].profile = control->current_profile;
    data.per_gpu[gpu_index].ai_boost_enabled = 
        control->profiles[GPU_PROFILE_HIGH_PERF].ai_boost_enabled;
    memcpy(&data.per_gpu[gpu_index].config,
           &control->profiles[control->current_profile],
           sizeof(struct gpu_profile_config));
           
    /* Write to NVRAM */
    return nvram_write(GPU_NVRAM_OFFSET, &data, sizeof(data));
}
EXPORT_SYMBOL_GPL(gpu_profile_save);

/**
 * gpu_profile_load - Load profile configuration from NVRAM
 * @dev: GPU device
 * @control: Power control structure
 */
int gpu_profile_load(struct device *dev, struct gpu_power_control *control)
{
    struct gpu_nvram_data data;
    int gpu_index, ret;
    
    /* Read from NVRAM */
    ret = nvram_read(GPU_NVRAM_OFFSET, &data, sizeof(data));
    if (ret < 0)
        return ret;
        
    /* Verify signature */
    if (data.signature != GPU_NVRAM_SIGNATURE)
        return -EINVAL;
        
    /* Find GPU index */
    gpu_index = pci_dev_id(to_pci_dev(dev));
    if (gpu_index >= 8)
        return -EINVAL;
        
    /* Restore profile if valid */
    if (data.per_gpu[gpu_index].profile <= GPU_PROFILE_HIGH_PERF) {
        /* Restore profile and config */
        control->current_profile = data.per_gpu[gpu_index].profile;
        control->profiles[GPU_PROFILE_HIGH_PERF].ai_boost_enabled =
            data.per_gpu[gpu_index].ai_boost_enabled;
        memcpy(&control->profiles[control->current_profile],
               &data.per_gpu[gpu_index].config,
               sizeof(struct gpu_profile_config));
               
        /* Apply restored profile */
        if (control->set_profile)
            control->set_profile(dev, control->current_profile);
    }
    
    return 0;
}
EXPORT_SYMBOL_GPL(gpu_profile_load);
