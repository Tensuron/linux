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
void setAttributeOnFile(struct inode *inode, int flag)
{
    if (!inode)
        return;

    struct dentry *dentry;
    spin_lock(&inode->i_lock);
    dentry = d_find_alias(inode);
    spin_unlock(&inode->i_lock);

    if (!dentry)
        return;

    if (!d_really_is_negative(dentry))
        vfs_setxattr(&nop_mnt_idmap, dentry, "user.fsprotect", &flag, sizeof(flag), 0);

    dput(dentry);
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
    spin_unlock(&inode->i_lock);

    if (!dentry)
        return -ENOENT;

    if (!d_really_is_negative(dentry)) {
        ret = vfs_getxattr(&nop_mnt_idmap, dentry, "user.fsprotect", &value, sizeof(value));
        if (ret == sizeof(value))
            ret = (int)value;
        else if (ret >= 0)
            ret = -EINVAL;
    }
    
    dput(dentry);
    return ret;
}

/**
 * fsprotect_inode_write - Check if write is allowed on protected inode
 * @inode: The inode to check
 *
 * Returns: 0 if write is allowed, -EPERM if denied
 */
int fsprotect_inode_write(struct inode *inode)
{
    int attr = getAttributeFromFile(inode);
    if (attr == FSPROTECT_FLAG_READONLY)
        return -EPERM;
    return 0;
}
EXPORT_SYMBOL(fsprotect_inode_write);

/**
 * fsprotect_inode_unlink - Check if unlink is allowed on protected inode
 * @dir: Parent directory inode
 * @dentry: Dentry of file to unlink
 *
 * Returns: 0 if unlink is allowed, -EPERM if denied
 */
int fsprotect_inode_unlink(struct inode *dir, struct dentry *dentry)
{
    int attr = getAttributeFromFile(d_inode(dentry));
    if (attr == FSPROTECT_FLAG_READONLY)
        return -EPERM;
    return 0;
}
EXPORT_SYMBOL(fsprotect_inode_unlink);

/**
 * fsprotect_inode_rename - Check if rename is allowed on protected inode
 * @old_dir: Source directory inode
 * @old_dentry: Source file dentry
 * @new_dir: Destination directory inode
 * @new_dentry: Destination file dentry
 *
 * Returns: 0 if rename is allowed, -EPERM if denied
 */
int fsprotect_inode_rename(struct inode *old_dir, struct dentry *old_dentry,
                          struct inode *new_dir, struct dentry *new_dentry)
{
    int attr = getAttributeFromFile(d_inode(old_dentry));
    if (attr == FSPROTECT_FLAG_READONLY)
        return -EPERM;
    return 0;
}
EXPORT_SYMBOL(fsprotect_inode_rename);