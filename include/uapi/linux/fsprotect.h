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

struct dir_queue {
    struct list_head list;
    struct dentry *dentry;
};

extern inline void setAttributeOnFile(struct inode *inode, enum fsprotect_flags flag);
extern inline int getAttributeFromFile(struct inode *inode);
extern inline int getDirectoryAttribute(struct inode *dir_inode);
extern inline void setDirectoryAttribute(struct dentry *dir_dentry, enum fsprotect_flags flag);

#endif