#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/fsnotify.h>
#include <linux/fsprotect.h>

static int fsprotect_check_access(struct inode *inode, bool write_access);
static int fsprotect_check_dir_access(struct inode *dir);

/**
 * fsprotect_check_access - Check if access is allowed based on fsprotect flags
 * @inode: The inode to check
 * @write_access: Whether the operation requires write access
 *
 * Returns: 0 if access is allowed, -EPERM if not
 */
static int fsprotect_check_access(struct inode *inode, bool write_access)
{
    int flag;
    
    if (!inode)
        return -EINVAL;

    flag = getAttributeFromFile(inode);

    // If no attribute is set, allow access
    if (flag < 0)
        return 0;

    // For write operations, check READONLY and EDITONLY flags
    if (write_access) {
        if (flag == FSPROTECT_READONLY)
            return -EPERM;
        if (flag != FSPROTECT_EDITONLY && flag != FSPROTECT_NORMAL)
            return -EPERM;
    }

    return 0;
}

/**
 * fsprotect_check_dir_access - Check if directory access is allowed
 * @dir: The directory inode to check
 *
 * Returns: 0 if access is allowed, -EPERM if not
 */
static int fsprotect_check_dir_access(struct inode *dir)
{
    int flag;

    if (!dir)
        return -EINVAL;

    flag = getDirectoryAttribute(dir);

    // If no attribute is set, allow access
    if (flag < 0)
        return 0;

    // For directories, only allow modifications if not READONLY
    if (flag == FSPROTECT_READONLY)
        return -EPERM;

    return 0;
}

/**
 * fsprotect_inode_unlink - Check if file/directory can be unlinked
 * @dir: Parent directory inode
 * @dentry: The dentry being unlinked
 *
 * Returns: 0 if unlink is allowed, -EPERM if not
 */
int fsprotect_inode_unlink(struct inode *dir, struct dentry *dentry)
{
    int ret;

    // Check if parent directory allows modifications
    ret = fsprotect_check_dir_access(dir);
    if (ret)
        return ret;

    // Check if target allows modifications 
    return fsprotect_check_access(d_inode(dentry), true);
}

/**
 * fsprotect_inode_rename - Check if file/directory can be renamed
 * @old_dir: Source directory inode
 * @old_dentry: Source dentry
 * @new_dir: Target directory inode
 * @new_dentry: Target dentry
 *
 * Returns: 0 if rename is allowed, -EPERM if not
 */
int fsprotect_inode_rename(struct inode *old_dir, struct dentry *old_dentry,
                          struct inode *new_dir, struct dentry *new_dentry)
{
    int ret;

    // Check source directory
    ret = fsprotect_check_dir_access(old_dir);
    if (ret)
        return ret;

    // Check target directory if different from source
    if (new_dir != old_dir) {
        ret = fsprotect_check_dir_access(new_dir);
        if (ret)
            return ret;
    }

    // Check source file/dir
    ret = fsprotect_check_access(d_inode(old_dentry), true);
    if (ret)
        return ret;

    // Check target file/dir if it exists
    if (d_really_is_positive(new_dentry)) {
        ret = fsprotect_check_access(d_inode(new_dentry), true);
        if (ret)
            return ret;
    }

    return 0;
}

/**
 * fsprotect_inode_write - Check if file can be written to
 * @inode: The inode being written to
 *
 * Returns: 0 if write is allowed, -EPERM if not
 */
int fsprotect_inode_write(struct inode *inode)
{
    return fsprotect_check_access(inode, true);
}

EXPORT_SYMBOL(fsprotect_inode_unlink);
EXPORT_SYMBOL(fsprotect_inode_rename);
EXPORT_SYMBOL(fsprotect_inode_write);
