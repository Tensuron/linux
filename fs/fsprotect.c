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

    struct dentry *dentry = NULL;
    int ret = -ENOENT;

    /* Get a reference to a dentry for this inode */
    spin_lock(&inode->i_lock);
    hlist_for_each_entry(dentry, &inode->i_dentry, d_u.d_alias) {
        spin_lock(&dentry->d_lock);
        if (!d_unhashed(dentry)) {
            dget_dlock(dentry);
            spin_unlock(&dentry->d_lock);
            spin_unlock(&inode->i_lock);
            
            if (!d_really_is_negative(dentry)) {
                ret = vfs_setxattr(&nop_mnt_idmap, dentry, "user.fsprotect", 
                                  &flag, sizeof(flag), 0);
            }
            dput(dentry);
            return ret;
        }
        spin_unlock(&dentry->d_lock);
    }
    spin_unlock(&inode->i_lock);

    return ret;
}

int getAttributeFromFile(struct inode *inode)
{
    if (!inode)
        return -EINVAL;

    struct dentry *dentry = NULL;
    int ret = -ENOENT;
    __u32 value = 0;

    /* Get a reference to a dentry for this inode */
    spin_lock(&inode->i_lock);
    hlist_for_each_entry(dentry, &inode->i_dentry, d_u.d_alias) {
        spin_lock(&dentry->d_lock);
        if (!d_unhashed(dentry)) {
            dget_dlock(dentry);
            spin_unlock(&dentry->d_lock);
            spin_unlock(&inode->i_lock);
            
            if (!d_really_is_negative(dentry)) {
                ret = vfs_getxattr(&nop_mnt_idmap, dentry, "user.fsprotect", 
                                  &value, sizeof(value));
                if (ret == sizeof(value))
                    ret = (int)value;
                else if (ret >= 0)
                    ret = 0; /* No attribute found or invalid size */
                else if (ret == -ENODATA)
                    ret = 0; /* No attribute set, return default */
            }
            dput(dentry);
            return ret;
        }
        spin_unlock(&dentry->d_lock);
    }
    spin_unlock(&inode->i_lock);

    return ret;
}

int clearAttributeOnFile(struct inode *inode)
{
    if (!inode)
        return -EINVAL;

    struct dentry *dentry = NULL;
    int ret = -ENOENT;
    
    /* Get a reference to a dentry for this inode */
    spin_lock(&inode->i_lock);
    hlist_for_each_entry(dentry, &inode->i_dentry, d_u.d_alias) {
        spin_lock(&dentry->d_lock);
        if (!d_unhashed(dentry)) {
            dget_dlock(dentry);
            spin_unlock(&dentry->d_lock);
            spin_unlock(&inode->i_lock);
            
            if (!d_really_is_negative(dentry)) {
                ret = vfs_removexattr(&nop_mnt_idmap, dentry, "user.fsprotect");
            }
            dput(dentry);
            return ret;
        }
        spin_unlock(&dentry->d_lock);
    }
    spin_unlock(&inode->i_lock);

    return ret;
}

int setAttributeOnDirectory(struct dentry *dir_dentry, int flag)
{
    if (!dir_dentry)
        return -EINVAL;

    struct inode *inode = d_inode(dir_dentry);
    int ret = -EINVAL;

    if (!inode)
        return -EINVAL;

    if (!S_ISDIR(inode->i_mode))
        return -ENOTDIR;

    /* Use the dentry directly since we already have it */
    if (!d_really_is_negative(dir_dentry)) {
        ret = vfs_setxattr(&nop_mnt_idmap, dir_dentry, "user.fsprotect", 
                          &flag, sizeof(flag), 0);
    }

    return ret;
}

int getAttributeFromDirectory(struct inode *inode)
{
    if (!inode)
        return -EINVAL;

    if (!S_ISDIR(inode->i_mode))
        return -ENOTDIR;

    struct dentry *dentry = NULL;
    int ret = -ENOENT;
    __u32 value = 0;

    /* Get a reference to a dentry for this inode */
    spin_lock(&inode->i_lock);
    hlist_for_each_entry(dentry, &inode->i_dentry, d_u.d_alias) {
        spin_lock(&dentry->d_lock);
        if (!d_unhashed(dentry)) {
            dget_dlock(dentry);
            spin_unlock(&dentry->d_lock);
            spin_unlock(&inode->i_lock);
            
            if (!d_really_is_negative(dentry)) {
                ret = vfs_getxattr(&nop_mnt_idmap, dentry, "user.fsprotect", 
                                  &value, sizeof(value));
                if (ret == sizeof(value))
                    ret = (int)value;
                else if (ret >= 0)
                    ret = 0; /* No attribute found or invalid size */
                else if (ret == -ENODATA)
                    ret = 0; /* No attribute set, return default */
            }
            dput(dentry);
            return ret;
        }
        spin_unlock(&dentry->d_lock);
    }
    spin_unlock(&inode->i_lock);

    return ret;
}

int clearAttributeOnDirectory(struct inode *inode)
{
    if (!inode)
        return -EINVAL;

    if (!S_ISDIR(inode->i_mode))
        return -ENOTDIR;

    struct dentry *dentry = NULL;
    int ret = -ENOENT;

    /* Get a reference to a dentry for this inode */
    spin_lock(&inode->i_lock);
    hlist_for_each_entry(dentry, &inode->i_dentry, d_u.d_alias) {
        spin_lock(&dentry->d_lock);
        if (!d_unhashed(dentry)) {
            dget_dlock(dentry);
            spin_unlock(&dentry->d_lock);
            spin_unlock(&inode->i_lock);
            
            if (!d_really_is_negative(dentry)) {
                ret = vfs_removexattr(&nop_mnt_idmap, dentry, "user.fsprotect");
            }
            dput(dentry);
            return ret;
        }
        spin_unlock(&dentry->d_lock);
    }
    spin_unlock(&inode->i_lock);

    return ret;
}

int canRemove(struct inode *inode)
{
    if (!inode)
        return -EINVAL;
        
    int attr;
    if (!S_ISDIR(inode->i_mode)) {
        attr = getAttributeFromFile(inode);
    } else {
        attr = getAttributeFromDirectory(inode);
    }
    
    /* Handle errors from attribute retrieval */
    if (attr < 0) {
        /* If no attribute is set (-ENODATA or -ENOENT), allow removal */
        if (attr == -ENODATA || attr == -ENOENT)
            return 1; /* Allow removal */
        
        return attr;
    }
        
    /* If readonly or editonly flag is set, deny removal */
    if (attr == FSPROTECT_FLAG_READONLY || attr == FSPROTECT_FLAG_EDITONLY)
        return 0;  /* Permission denied */
        
    /* Allow removal */
    return 1;
}

int canWrite(struct inode *inode)
{
    int attr;

    if (!inode)
        return -EINVAL;

    /* Use mode from inode safely */
    bool is_dir = S_ISDIR(inode->i_mode);

    if (!is_dir)
        attr = getAttributeFromFile(inode);
    else
        attr = getAttributeFromDirectory(inode);

    /* Handle errors from attribute retrieval */
    if (attr < 0) {
        /* If no attribute is set (-ENODATA or -ENOENT), allow write */
        if (attr == -ENODATA || attr == -ENOENT)
            return 1; /* Allow write */
        
        return attr;
    }

    /* Check if readonly flag is set */
    if (attr == FSPROTECT_FLAG_READONLY)
        return -EACCES;

    /* Allow write for other flags or no flags */
    return 1;
}

EXPORT_SYMBOL(canRemove);
EXPORT_SYMBOL(canWrite);
EXPORT_SYMBOL(setAttributeOnFile);
EXPORT_SYMBOL(getAttributeFromFile);
EXPORT_SYMBOL(clearAttributeOnFile);
EXPORT_SYMBOL(setAttributeOnDirectory);
EXPORT_SYMBOL(getAttributeFromDirectory);
EXPORT_SYMBOL(clearAttributeOnDirectory);