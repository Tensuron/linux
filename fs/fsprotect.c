#include <linux/module.h>
#include <linux/fs.h>
#include <linux/xattr.h>
#include <linux/mnt_idmapping.h>
#include <linux/fsprotect.h>
#include <linux/spinlock.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/slab.h>
#include <uapi/linux/fsprotect.h>

/* Implementation of internal helper functions */
int setAttributeOnFile(struct inode *inode, int flag)
{
    if (!inode)
        return -EINVAL;

    struct dentry *dentry;
    spin_lock(&inode->i_lock);
    dentry = d_find_alias(inode);

    if (!dentry) {
        spin_unlock(&inode->i_lock);
        return -ENOENT;
    }

    if (!d_really_is_negative(dentry))
        vfs_setxattr(&nop_mnt_idmap, dentry, "user.fsprotect", &flag, sizeof(flag), 0);

    spin_unlock(&inode->i_lock);
    dput(dentry);
    return 0;
}

int getAttributeFromFile(struct inode *inode)
{
    if (!inode)
        return -EINVAL;

    struct dentry *dentry;
    int ret = -ENOENT;
    __u32 value = 0;

    spin_lock(&inode->i_lock);
    dentry = d_find_alias(inode);

    if (!dentry) {
        spin_unlock(&inode->i_lock);
        return -ENOENT;
    }

    if (!d_really_is_negative(dentry)) {
        ret = vfs_getxattr(&nop_mnt_idmap, dentry, "user.fsprotect", &value, sizeof(value));
        if (ret == sizeof(value))
            ret = (int)value;
        else if (ret >= 0)
            return 0;
    }

    spin_unlock(&inode->i_lock);
    dput(dentry);
    return ret;
}

int clearAttributeOnFile(struct inode *inode)
{
    if (!inode)
        return -EINVAL;

    struct dentry *dentry;
    spin_lock(&inode->i_lock);
    dentry = d_find_alias(inode);

    if (!dentry) {
        spin_unlock(&inode->i_lock);
        return -ENOENT;
    }

    if (!d_really_is_negative(dentry))
        vfs_removexattr(&nop_mnt_idmap, dentry, "user.fsprotect");

    spin_unlock(&inode->i_lock);
    dput(dentry);
    return 0;
}

int setAttributeOnDirectory(struct dentry *dir_dentry, int flag)
{
    if (!dir_dentry)
        return -EINVAL;

    spin_lock(&dir_dentry->d_lock);
    struct inode *inode = d_inode(dir_dentry);
    spin_unlock(&dir_dentry->d_lock);

    if (!inode)
        return -EINVAL;

    if (!S_ISDIR(inode->i_mode))
        return -ENOTDIR;

    int ret = vfs_setxattr(&nop_mnt_idmap, dir_dentry, "user.fsprotect", 
                          &flag, sizeof(flag), 0);
    return ret;
}

int getAttributeFromDirectory(struct inode *inode)
{
    if (!inode)
        return -EINVAL;

    struct dentry *dentry;
    int ret = -ENOENT;
    __u32 value = 0;

    if (!S_ISDIR(inode->i_mode))
        return -ENOTDIR;

    spin_lock(&inode->i_lock);
    dentry = d_find_alias(inode);

    if (!dentry) {
        spin_unlock(&inode->i_lock);
        return -ENOENT;
    }

    if (!d_really_is_negative(dentry)) {
        ret = vfs_getxattr(&nop_mnt_idmap, dentry, "user.fsprotect", &value, sizeof(value));
        if (ret == sizeof(value))
            ret = (int)value;
        else if (ret >= 0)
            ret = 0;
    }

    spin_unlock(&inode->i_lock);
    dput(dentry);
    return ret;
}

int clearAttributeOnDirectory(struct inode *inode)
{
    if (!inode)
        return -EINVAL;

    if (!S_ISDIR(inode->i_mode))
        return -ENOTDIR;

    struct dentry *dentry;
    int ret;

    spin_lock(&inode->i_lock);
    dentry = d_find_alias(inode);
    if (!dentry) {
        spin_unlock(&inode->i_lock);
        return -ENOENT;
    }

    ret = vfs_removexattr(&nop_mnt_idmap, dentry, "user.fsprotect");
    spin_unlock(&inode->i_lock);
    dput(dentry);
    
    return ret;
}

int canRemove(struct inode *inode)
{
    if(!S_ISDIR(inode->i_mode)) {
        int attr = getAttributeFromFile(inode);
        if(attr == FSPROTECT_FLAG_READONLY || attr == FSPROTECT_FLAG_EDITONLY)
            return 0;
        else
            return attr;
    }
    else {
        int attr = getAttributeFromDirectory(inode);
        if(attr == FSPROTECT_FLAG_READONLY || attr == FSPROTECT_FLAG_EDITONLY)
            return 0;
        else
            return attr;
    }
}

int canWrite(struct inode *inode)
{
    if(!S_ISDIR(inode->i_mode)) {
        int attr = getAttributeFromFile(inode);
        if(attr == FSPROTECT_FLAG_READONLY)
            return 0;
        else
            return attr;
    }
    else {
        int attr = getAttributeFromDirectory(inode);
        if(attr == FSPROTECT_FLAG_READONLY)
            return 0;
        else
            return attr;
    }
}
EXPORT_SYMBOL(canRemove);
EXPORT_SYMBOL(canWrite);
EXPORT_SYMBOL(setAttributeOnFile);
EXPORT_SYMBOL(clearAttributeOnFile);
EXPORT_SYMBOL(setAttributeOnDirectory);
EXPORT_SYMBOL(clearAttributeOnDirectory);