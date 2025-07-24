#ifndef FSPROTECT_H
#define FSPROTECT_H

#include <linux/xattr.h>
#include <linux/fs.h>
#include <linux/lsm_hooks.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/dcache.h>
#include <linux/user_namespace.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mnt_idmapping.h>

#define READONLY_FL        0x1FAA1DEA
#define EDITONLY_FL        0x2FEA1ACA
#define NORMAL_FL          0x00000000

enum fsprotect_flags {
    FSPROTECT_READONLY = READONLY_FL,
    FSPROTECT_EDITONLY = EDITONLY_FL,
    FSPROTECT_NORMAL = NORMAL_FL,
};

// Set attribute on a single file
static inline void setAttributeOnFile(struct inode *inode, enum fsprotect_flags flag) {
    if (!inode)
        return;

    struct dentry *dentry;
    spin_lock(&inode->i_lock);
    dentry = d_find_alias(inode);
    spin_unlock(&inode->i_lock);

    if (!dentry)
        return;

    if (!d_really_is_negative(dentry)) {
        vfs_setxattr(&nop_mnt_idmap, dentry, "user.fsprotect", &flag, sizeof(flag), 0);
    }
    dput(dentry);
}

// Get attribute from a single file
static inline int getAttributeFromFile(struct inode *inode) {
    if (!inode)
        return -EINVAL;

    struct dentry *dentry;
    int ret = -ENOENT;
    __u32 value = 0;

    spin_lock(&inode->i_lock);
    dentry = d_find_alias(inode);
    spin_unlock(&inode->i_lock);

    if (!dentry)
        return -ENOENT;

    if (!d_really_is_negative(dentry)) {
        ret = vfs_getxattr(&nop_mnt_idmap, dentry, "user.fsprotect", &value, sizeof(value));
        if (ret == sizeof(value)) {
            ret = (int)value;
        }
        else if (ret >= 0) {
            ret = -EINVAL;
        }
    }
    
    dput(dentry);
    return ret;
}

// Get attribute specifically for directory inode
static inline int getDirectoryAttribute(struct inode *dir_inode) {
    if (!dir_inode)
        return -EINVAL;

    struct dentry *dentry;
    int ret = -ENOENT;
    __u32 value = 0;

    // Directory-specific validation
    if (!S_ISDIR(dir_inode->i_mode))
        return -ENOTDIR;

    spin_lock(&dir_inode->i_lock);
    dentry = d_find_alias(dir_inode);
    spin_unlock(&dir_inode->i_lock);

    if (!dentry)
        return -ENOENT;

    if (!d_really_is_negative(dentry)) {
        ret = vfs_getxattr(&nop_mnt_idmap, dentry, "user.fsprotect", &value, sizeof(value));
        if (ret == sizeof(value)) {
            ret = (int)value;
        }
        else if (ret >= 0) {
            ret = -EINVAL;
        }
    }
    
    dput(dentry);
    return ret;
}

// Queue structure for directory traversal
struct dir_queue {
    struct list_head list;
    struct dentry *dentry;
};

// Set attribute on directory and all contents recursively
static inline void setDirectoryAttribute(struct dentry *dir_dentry, enum fsprotect_flags flag) {
    if (!dir_dentry || d_really_is_negative(dir_dentry))
        return;

    LIST_HEAD(queue);
    struct dir_queue *entry = kmalloc(sizeof(*entry), GFP_KERNEL);
    if (!entry)
        return;
    
    INIT_LIST_HEAD(&entry->list);
    entry->dentry = dget(dir_dentry);
    list_add_tail(&entry->list, &queue);

    while (!list_empty(&queue)) {
        entry = list_first_entry(&queue, struct dir_queue, list);
        list_del(&entry->list);
        struct dentry *dentry = entry->dentry;
        struct inode *inode = d_inode(dentry);

        // Set attribute on current item
        setAttributeOnFile(inode, flag);

        // Process directories recursively
        if (S_ISDIR(inode->i_mode)) {
            struct dentry *child;
            spin_lock(&dentry->d_lock);
            // Use hlist_for_each_entry for d_children (which is now an hlist)
            hlist_for_each_entry(child, &dentry->d_children, d_sib) {
                // Skip invalid entries
                if (d_unhashed(child) || !child->d_inode || child == dentry)
                    continue;
                
                // Skip "." and ".." entries
                if (child->d_name.len == 1 && child->d_name.name[0] == '.')
                    continue;
                if (child->d_name.len == 2 && child->d_name.name[0] == '.' && child->d_name.name[1] == '.')
                    continue;

                // Add child to processing queue
                struct dir_queue *new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
                if (new_entry) {
                    INIT_LIST_HEAD(&new_entry->list);
                    new_entry->dentry = dget(child);
                    list_add_tail(&new_entry->list, &queue);
                }
            }
            spin_unlock(&dentry->d_lock);
        }
        
        // Clean up processed entry
        dput(dentry);
        kfree(entry);
    }
}

#endif