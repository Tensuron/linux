// SPDX-License-Identifier: GPL-2.0-only
/*
 * GPU Power Profile Management Sysfs Interface
 *
 * Copyright (c) 2025 Linux Foundation
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/gpu_power_mode.h>

static const char *profile_names[] = {
    [GPU_PROFILE_POWER_SAVE] = "power_save",
    [GPU_PROFILE_BALANCED] = "balanced",
    [GPU_PROFILE_HIGH_PERF] = "high_performance",
};

static ssize_t power_profile_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
    struct gpu_power_control *control = dev_get_drvdata(dev);
    
    return sysfs_emit(buf, "%s\n", profile_names[control->current_profile]);
}

static ssize_t power_profile_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf,
                                 size_t count)
{
    int i;
    
    for (i = 0; i < ARRAY_SIZE(profile_names); i++) {
        if (sysfs_streq(buf, profile_names[i])) {
            gpu_power_set_profile(dev, i);
            return count;
        }
    }
    
    return -EINVAL;
}
DEVICE_ATTR_RW(power_profile);

static ssize_t available_profiles_show(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
    int i;
    ssize_t count = 0;
    
    for (i = 0; i < ARRAY_SIZE(profile_names); i++)
        count += sysfs_emit_at(buf, count, "%s ", profile_names[i]);
    count += sysfs_emit_at(buf, count, "\n");
    
    return count;
}
DEVICE_ATTR_RO(available_profiles);

static ssize_t ai_boost_show(struct device *dev,
                           struct device_attribute *attr,
                           char *buf)
{
    struct gpu_power_control *control = dev_get_drvdata(dev);
    bool enabled = control->profiles[GPU_PROFILE_HIGH_PERF].ai_boost_enabled;
    
    return sysfs_emit(buf, "%d\n", enabled);
}

static ssize_t ai_boost_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf,
                            size_t count)
{
    struct gpu_power_control *control = dev_get_drvdata(dev);
    bool enable;
    int ret;
    
    ret = kstrtobool(buf, &enable);
    if (ret)
        return ret;
        
    control->profiles[GPU_PROFILE_HIGH_PERF].ai_boost_enabled = enable;
    
    if (control->current_profile == GPU_PROFILE_HIGH_PERF &&
        control->update_config)
        control->update_config(dev, &control->profiles[GPU_PROFILE_HIGH_PERF]);
        
    return count;
}
DEVICE_ATTR_RW(ai_boost);

static struct attribute *gpu_power_attrs[] = {
    &dev_attr_power_profile.attr,
    &dev_attr_available_profiles.attr,
    &dev_attr_ai_boost.attr,
    NULL
};
ATTRIBUTE_GROUPS(gpu_power);

int gpu_power_sysfs_init(struct device *dev)
{
    return sysfs_create_groups(&dev->kobj, gpu_power_groups);
}
EXPORT_SYMBOL_GPL(gpu_power_sysfs_init);

void gpu_power_sysfs_remove(struct device *dev)
{
    sysfs_remove_groups(&dev->kobj, gpu_power_groups);
}
EXPORT_SYMBOL_GPL(gpu_power_sysfs_remove);
