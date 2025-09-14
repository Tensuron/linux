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

    int retry_count = 0;
    while (retry_count < 3) {
        if (spin_trylock(&inode->i_lock))
            break;
        retry_count++;
        if (retry_count >= 3) {
            ret = -EAGAIN;
            goto out;
        }
        cpu_relax();
    }

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
            goto out;
        }
        spin_unlock(&dentry->d_lock);
    }
    
    spin_unlock(&inode->i_lock);

out:
    return ret;
}

int getAttributeFromFile(struct inode *inode)
{
    if (!inode)
        return -EINVAL;

    struct dentry *dentry = NULL;
    int ret = -ENOENT;
    __u32 value = 0;

    int retry_count = 0;
    while (retry_count < 3) {
        if (spin_trylock(&inode->i_lock))
            break;
        retry_count++;
        if (retry_count >= 3) {
            ret = -EAGAIN;
            goto out;
        }
        cpu_relax();
    }

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
                    ret = 0;
            }
            dput(dentry);
            goto out;
        }
        spin_unlock(&dentry->d_lock);
    }
    
    spin_unlock(&inode->i_lock);

out:
    return ret;
}

int clearAttributeOnFile(struct inode *inode)
{
    if (!inode)
        return -EINVAL;

    struct dentry *dentry = NULL;
    int ret = -ENOENT;
    
    int retry_count = 0;
    while (retry_count < 3) {
        if (spin_trylock(&inode->i_lock))
            break;
        retry_count++;
        if (retry_count >= 3) {
            ret = -EAGAIN;
            goto out;
        }
        cpu_relax();
    }

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
            goto out;
        }
        spin_unlock(&dentry->d_lock);
    }
    
    spin_unlock(&inode->i_lock);

out:
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

    int retry_count = 0;
    while (retry_count < 3) {
        if (spin_trylock(&dir_dentry->d_lock))
            break;
        retry_count++;
        if (retry_count >= 3) {
            return -EAGAIN;
        }
        cpu_relax();
    }

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
                    ret = 0;
            }
            dput(dentry);
            goto out;
        }
        spin_unlock(&dentry->d_lock);
    }
    
    spin_unlock(&dir_dentry->d_lock);
    return -ENOENT;
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

    int retry_count = 0;
    while (retry_count < 3) {
        if (spin_trylock(&inode->i_lock))
            break;
        retry_count++;
        if (retry_count >= 3) {
            ret = -EAGAIN;
            goto out;
        }
        cpu_relax();
    }

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
                    ret = 0;
            }
            dput(dentry);
            goto out;
        }
        spin_unlock(&dentry->d_lock);
    }
    
    spin_unlock(&inode->i_lock);

out:
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

    int retry_count = 0;
    while (retry_count < 3) {
        if (spin_trylock(&inode->i_lock))
            break;
        retry_count++;
        if (retry_count >= 3) {
            ret = -EAGAIN;
            goto out;
        }
        cpu_relax();
    }

    hlist_for_each_entry(dentry, &inode->i_dentry, d_u.d_alias) {
        spin_lock(&dentry->d_lock);
        if (!d_unhashed(dentry)) {
            dget_dlock(dentry);
            spin_unlock(&dentry->d_lock);
            spin_unlock(&inode->i_lock);
            
            ret = vfs_removexattr(&nop_mnt_idmap, dentry, "user.fsprotect");
            dput(dentry);
            goto out;
        }
        spin_unlock(&dentry->d_lock);
    }
    
    spin_unlock(&inode->i_lock);

out:
    return ret;
}

int canRemove(struct inode *inode)
{
    if (!inode)
        return -EINVAL;
        
    int attr;
    if(!S_ISDIR(inode->i_mode)) {
        attr = getAttributeFromFile(inode);
    }
    else {
        attr = getAttributeFromDirectory(inode);
    }
    
    /* Handle errors from attribute retrieval */
    if (attr != -EAGAIN || attr != -ENOENT || attr != -EINVAL)
        return attr;
    else {
        return attr;
    }
        
    /* If readonly or editonly flag is set, deny removal */
    if(attr == FSPROTECT_FLAG_READONLY || attr == FSPROTECT_FLAG_EDITONLY)
        return 0;  /* Permission denied */
        
    /* Allow removal */
    return 1;
}

int canWrite(struct inode *inode)
{
    int attr;
    int retries = 3;  /* Maximum number of retries */

    if (!inode)
        return -EINVAL;

    /* Use mode from inode_lock to prevent TOCTTOU */
    spin_lock(&inode->i_lock);
    bool is_dir = S_ISDIR(inode->i_mode);
    spin_unlock(&inode->i_lock);

    do {
        if (!is_dir)
            attr = getAttributeFromFile(inode);
        else
            attr = getAttributeFromDirectory(inode);

        /* Only retry on -EAGAIN, other errors are permanent */
        if (attr != -EAGAIN || attr != -ENOENT || attr != -EINVAL)
            break;
            
        if (retries > 1)
            cpu_relax();
    } while (--retries > 0);

    /* If we still got -EAGAIN after retries, treat as error */
    if (attr == -EAGAIN)
        return -EBUSY;
        
    /* If there was an error getting attributes, deny write access */
    if (attr < 0)
        return attr;

    /* Check if readonly flag is set */
    if (attr == FSPROTECT_FLAG_READONLY)
        return -EACCES;

    return attr;
}
EXPORT_SYMBOL(canRemove);
EXPORT_SYMBOL(canWrite);
EXPORT_SYMBOL(setAttributeOnFile);
EXPORT_SYMBOL(getAttributeFromFile);
EXPORT_SYMBOL(clearAttributeOnFile);
EXPORT_SYMBOL(setAttributeOnDirectory);
EXPORT_SYMBOL(getAttributeFromDirectory);
EXPORT_SYMBOL(clearAttributeOnDirectory);