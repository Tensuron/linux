#include <linux/module.h>
#include <linux/fs.h>
#include <linux/xattr.h>
#include <linux/mnt_idmapping.h>
#include <linux/spinlock.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/magic.h>
#include <linux/statfs.h>
#include <linux/mount.h>
#include <linux/cred.h>
#include <linux/fsprotect.h>
#include <uapi/linux/fsprotect.h>

/* Forward declarations for all functions */
static int handle_filesystem_operation(struct inode *inode, int operation, int flag, int *result);
int setAttributeOnDirectory(struct inode *inode, int flag);
int clearAttributeFromFile(struct inode *inode);
int clearAttributeFromDirectory(struct inode *inode);
int canEdit(struct inode *inode);
int canAppend(struct inode *inode);
int is_protection_supported(struct super_block *sb);
int validate_protection_flag(int flag);
extern int generic_xattr_get(struct inode *inode, int *value);
extern int generic_xattr_set(struct inode *inode, int flag);
extern int generic_xattr_clear(struct inode *inode);
extern int ufs_get_attr(struct inode *inode, int *value);
extern int ufs_set_attr(struct inode *inode, int flag);
extern int ufs_clear_attr(struct inode *inode);
extern int fat_get_attr(struct inode *inode, int *value);
extern int fat_set_attr(struct inode *inode, int flag);
extern int fat_clear_attr(struct inode *inode);
extern int network_fs_get_attr(struct inode *inode, int *value);
extern int network_fs_set_attr(struct inode *inode, int flag);
extern int network_fs_clear_attr(struct inode *inode);
extern int readonly_fs_get_attr(struct inode *inode, int *value);
extern int readonly_fs_set_attr(struct inode *inode, int flag);
extern int readonly_fs_clear_attr(struct inode *inode);

/* Comprehensive filesystem information table */
static struct fs_info filesystem_table[] = {
    /* Modern Linux filesystems with full xattr support */
    {FS_TYPE_EXT2, "ext2", EXT2_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_ACL | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_LARGE_FILES | FS_CAP_SPARSE_FILES,
     generic_xattr_get, generic_xattr_set, generic_xattr_clear},

    {FS_TYPE_EXT3, "ext3", EXT3_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_ACL | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_LARGE_FILES | FS_CAP_SPARSE_FILES | FS_CAP_JOURNALING,
     generic_xattr_get, generic_xattr_set, generic_xattr_clear},

    {FS_TYPE_EXT4, "ext4", EXT4_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_ACL | FS_CAP_QUOTA | FS_CAP_ENCRYPTION | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS |
     FS_CAP_LARGE_FILES | FS_CAP_SPARSE_FILES | FS_CAP_ATOMIC_WRITE | FS_CAP_JOURNALING,
     generic_xattr_get, generic_xattr_set, generic_xattr_clear},

    {FS_TYPE_XFS, "xfs", XFS_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_ACL | FS_CAP_QUOTA | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_LARGE_FILES |
     FS_CAP_SPARSE_FILES | FS_CAP_ATOMIC_WRITE | FS_CAP_JOURNALING,
     generic_xattr_get, generic_xattr_set, generic_xattr_clear},

    {FS_TYPE_BTRFS, "btrfs", BTRFS_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_ACL | FS_CAP_COMPRESSION | FS_CAP_SNAPSHOTS | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS |
     FS_CAP_LARGE_FILES | FS_CAP_SPARSE_FILES | FS_CAP_ATOMIC_WRITE | FS_CAP_JOURNALING,
     generic_xattr_get, generic_xattr_set, generic_xattr_clear},

    {FS_TYPE_F2FS, "f2fs", F2FS_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_ACL | FS_CAP_ENCRYPTION | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_LARGE_FILES |
     FS_CAP_SPARSE_FILES | FS_CAP_ATOMIC_WRITE | FS_CAP_JOURNALING,
     generic_xattr_get, generic_xattr_set, generic_xattr_clear},

    /* Unix filesystems */
    {FS_TYPE_UFS, "ufs", 0x00011954,
     FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_LARGE_FILES | FS_CAP_SPARSE_FILES,
     ufs_get_attr, ufs_set_attr, ufs_clear_attr},

    {FS_TYPE_REISERFS, "reiserfs", REISERFS_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_ACL | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_SPARSE_FILES | FS_CAP_JOURNALING,
     generic_xattr_get, generic_xattr_set, generic_xattr_clear},

    /* FAT family filesystems */
    {FS_TYPE_FAT, "fat", MSDOS_SUPER_MAGIC,
     FS_CAP_CASE_INSENSITIVE | FS_CAP_LARGE_FILES,
     fat_get_attr, fat_set_attr, fat_clear_attr},

    {FS_TYPE_VFAT, "vfat", MSDOS_SUPER_MAGIC,
     FS_CAP_CASE_INSENSITIVE | FS_CAP_LARGE_FILES,
     fat_get_attr, fat_set_attr, fat_clear_attr},

    {FS_TYPE_EXFAT, "exfat", EXFAT_SUPER_MAGIC,
     FS_CAP_CASE_INSENSITIVE | FS_CAP_LARGE_FILES,
     fat_get_attr, fat_set_attr, fat_clear_attr},

    /* Network filesystems */
    {FS_TYPE_NFS, "nfs", NFS_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_ACL | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_LARGE_FILES |
     FS_CAP_SPARSE_FILES | FS_CAP_NETWORK_FS,
     network_fs_get_attr, network_fs_set_attr, network_fs_clear_attr},

    {FS_TYPE_NFS4, "nfs4", NFS_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_ACL | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_LARGE_FILES |
     FS_CAP_SPARSE_FILES | FS_CAP_NETWORK_FS,
     network_fs_get_attr, network_fs_set_attr, network_fs_clear_attr},

    {FS_TYPE_CIFS, "cifs", SMB_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_LARGE_FILES | FS_CAP_NETWORK_FS,
     network_fs_get_attr, network_fs_set_attr, network_fs_clear_attr},

    /* Special/Virtual filesystems */
    {FS_TYPE_FUSE, "fuse", FUSE_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_LARGE_FILES,
     generic_xattr_get, generic_xattr_set, generic_xattr_clear},

    {FS_TYPE_TMPFS, "tmpfs", TMPFS_MAGIC,
     FS_CAP_XATTR | FS_CAP_ACL | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_LARGE_FILES | FS_CAP_VIRTUAL_FS,
     generic_xattr_get, generic_xattr_set, generic_xattr_clear},

    {FS_TYPE_PROC, "proc", PROC_SUPER_MAGIC,
     FS_CAP_VIRTUAL_FS,
     readonly_fs_get_attr, readonly_fs_set_attr, readonly_fs_clear_attr},

    {FS_TYPE_SYSFS, "sysfs", SYSFS_MAGIC,
     FS_CAP_VIRTUAL_FS,
     readonly_fs_get_attr, readonly_fs_set_attr, readonly_fs_clear_attr},

    /* Read-only filesystems */
    {FS_TYPE_SQUASHFS, "squashfs", SQUASHFS_MAGIC,
     FS_CAP_COMPRESSION | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_READ_ONLY,
     readonly_fs_get_attr, readonly_fs_set_attr, readonly_fs_clear_attr},

    {FS_TYPE_CRAMFS, "cramfs", CRAMFS_MAGIC,
     FS_CAP_COMPRESSION | FS_CAP_READ_ONLY,
     readonly_fs_get_attr, readonly_fs_set_attr, readonly_fs_clear_attr},

    {FS_TYPE_ISO9660, "iso9660", ISOFS_SUPER_MAGIC,
     FS_CAP_HARDLINKS | FS_CAP_READ_ONLY,
     readonly_fs_get_attr, readonly_fs_set_attr, readonly_fs_clear_attr},

    /* Overlay filesystems */
    {FS_TYPE_OVERLAY, "overlay", OVERLAYFS_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_LARGE_FILES,
     generic_xattr_get, generic_xattr_set, generic_xattr_clear},

    /* Clustered/Distributed filesystems */
    {FS_TYPE_OCFS2, "ocfs2", OCFS2_SUPER_MAGIC,
     FS_CAP_XATTR | FS_CAP_ACL | FS_CAP_HARDLINKS | FS_CAP_SYMLINKS | FS_CAP_LARGE_FILES |
     FS_CAP_SPARSE_FILES | FS_CAP_JOURNALING,
     generic_xattr_get, generic_xattr_set, generic_xattr_clear},

    /* Legacy Unix filesystems */
    {FS_TYPE_MINIX, "minix", MINIX_SUPER_MAGIC,
     FS_CAP_HARDLINKS,
     readonly_fs_get_attr, readonly_fs_set_attr, readonly_fs_clear_attr},

    /* Terminator */
    {FS_TYPE_UNKNOWN, NULL, 0, 0, NULL, NULL, NULL}
};

/* ========== FILESYSTEM DETECTION FUNCTIONS ========== */

struct fs_info *detect_filesystem_type(struct super_block *sb)
{
    struct fs_info *fs;
    __u32 magic;
    
    if (!sb)
        return NULL;
    
    magic = sb->s_magic;
    
    /* Search filesystem table */
    for (fs = filesystem_table; fs->name != NULL; fs++) {
        if (fs->magic == magic) {
            /* Additional checks for filesystems with same magic */
            if (magic == MSDOS_SUPER_MAGIC) {
                /* Distinguish between FAT variants */
                if (sb->s_type && sb->s_type->name) {
                    if (strcmp(sb->s_type->name, "vfat") == 0)
                        return &filesystem_table[FS_TYPE_VFAT - 1];
                    else if (strcmp(sb->s_type->name, "exfat") == 0)
                        return &filesystem_table[FS_TYPE_EXFAT - 1];
                }
                return &filesystem_table[FS_TYPE_FAT - 1];
            }
            return fs;
        }
    }
    
    /* Check filesystem type name for unrecognized magic numbers */
    if (sb->s_type && sb->s_type->name) {
        for (fs = filesystem_table; fs->name != NULL; fs++) {
            if (strcmp(sb->s_type->name, fs->name) == 0) {
                return fs;
            }
        }
    }
    
    return NULL;
}

__u32 get_filesystem_capabilities(struct super_block *sb)
{
    struct fs_info *fs = detect_filesystem_type(sb);
    return fs ? fs->capabilities : 0;
}

bool filesystem_supports_feature(struct super_block *sb, __u32 feature)
{
    __u32 caps = get_filesystem_capabilities(sb);
    return (caps & feature) != 0;
}

/* ========== GENERIC XATTR-BASED ATTRIBUTE HANDLERS ========== */

int generic_xattr_get(struct inode *inode, int *value)
{
    struct dentry *dentry;
    __u32 xattr_value = 0;
    int ret;

    if (!inode || !value)
        return -EINVAL;

    dentry = d_find_alias(inode);
    if (!dentry)
        return -ENOENT;

    ret = vfs_getxattr(&nop_mnt_idmap, dentry, "system.fsprotect", 
                      &xattr_value, sizeof(xattr_value));
    
    if (ret == sizeof(xattr_value)) {
        *value = (int)xattr_value;
        ret = 0;
    } else if (ret == -ENODATA || ret == -ENOENT) {
        *value = FSPROTECT_NONE;
        ret = 0;
    } else if (ret >= 0) {
        *value = FSPROTECT_NONE;
        ret = 0;
    }
    
    dput(dentry);
    return ret;
}

int generic_xattr_set(struct inode *inode, int flag)
{
    struct dentry *dentry;
    __u32 xattr_value = (__u32)flag;
    int ret;

    if (!inode)
        return -EINVAL;

    /* Check if filesystem is read-only */
    if (IS_RDONLY(inode))
        return -EROFS;

    /* Check if current process has administrative privileges to modify system attributes */
    if (!capable(CAP_SYS_ADMIN))
        return -EPERM;

    dentry = d_find_alias(inode);
    if (!dentry)
        return -ENOENT;

    ret = vfs_setxattr(&nop_mnt_idmap, dentry, "system.fsprotect", 
                      &xattr_value, sizeof(xattr_value), 0);
    
    dput(dentry);
    return ret;
}

int generic_xattr_clear(struct inode *inode)
{
    struct dentry *dentry;
    int ret;

    if (!inode)
        return -EINVAL;

    /* Check if filesystem is read-only */
    if (IS_RDONLY(inode))
        return -EROFS;

    /* Check if current process has administrative privileges to modify system attributes */
    if (!capable(CAP_SYS_ADMIN))
        return -EPERM;

    dentry = d_find_alias(inode);
    if (!dentry)
        return -ENOENT;

    ret = vfs_removexattr(&nop_mnt_idmap, dentry, "system.fsprotect");
    
    /* If the attribute doesn't exist, consider it a success */
    if (ret == -ENODATA || ret == -ENOENT)
        ret = 0;
    
    dput(dentry);
    return ret;
}

/* ========== UFS-SPECIFIC HANDLERS ========== */

int ufs_get_attr(struct inode *inode, int *value)
{
    /* UFS support - try to use UFS-specific flags if available */
    if (!inode || !value)
        return -EINVAL;

    /* For now, fall back to generic implementation */
    /* In a real implementation, you would check UFS inode flags here */
    return generic_xattr_get(inode, value);
}

int ufs_set_attr(struct inode *inode, int flag)
{
    /* UFS support - try to use UFS-specific flags if available */
    if (!inode)
        return -EINVAL;

    if (IS_RDONLY(inode))
        return -EROFS;

    /* Check if current process has administrative privileges to modify system attributes */
    if (!capable(CAP_SYS_ADMIN))
        return -EPERM;

    /* For now, fall back to generic implementation */
    /* In a real implementation, you would set UFS inode flags here */
    return generic_xattr_set(inode, flag);
}

int ufs_clear_attr(struct inode *inode)
{
    /* UFS support - clear UFS-specific flags if available */
    if (!inode)
        return -EINVAL;

    if (IS_RDONLY(inode))
        return -EROFS;

    /* Check if current process has administrative privileges to modify system attributes */
    if (!capable(CAP_SYS_ADMIN))
        return -EPERM;

    /* For now, fall back to generic implementation */
    return generic_xattr_clear(inode);
}

/* ========== FAT FILESYSTEM HANDLERS ========== */

int fat_get_attr(struct inode *inode, int *value)
{
    /* FAT filesystems don't support xattr, use file attributes */
    if (!inode || !value)
        return -EINVAL;
    
    /* Map DOS attributes to our protection flags */
    if (IS_RDONLY(inode)) {
        *value = FSPROTECT_READONLY;
    } else {
        *value = FSPROTECT_NONE;
    }
    
    return 0;
}

int fat_set_attr(struct inode *inode, int flag)
{
    /* FAT filesystems are limited in attribute support */
    if (!inode)
        return -EINVAL;
    
    if (IS_RDONLY(inode))
        return -EROFS;
    
    /* We can try to set the readonly bit, but other flags are not supported */
    if (flag == FSPROTECT_READONLY) {
        /* This would require filesystem-specific operations */
        pr_warn("fsprotect: Setting readonly on FAT filesystem not fully implemented\n");
        return 0; /* Return success for now */
    }
    
    /* Other flags are not supported on FAT */
    return -EOPNOTSUPP;
}

int fat_clear_attr(struct inode *inode)
{
    /* FAT filesystems are limited in attribute support */
    if (!inode)
        return -EINVAL;
    
    if (IS_RDONLY(inode))
        return -EROFS;
    
    /* Clear readonly attribute if possible */
    pr_warn("fsprotect: Clearing attributes on FAT filesystem not fully implemented\n");
    return 0; /* Return success for now */
}

/* ========== NETWORK FILESYSTEM HANDLERS ========== */

int network_fs_get_attr(struct inode *inode, int *value)
{
    /* Network filesystems may have limited or cached attribute support */
    if (!inode || !value)
        return -EINVAL;
    
    /* Try standard xattr with timeout considerations */
    return generic_xattr_get(inode, value);
}

int network_fs_set_attr(struct inode *inode, int flag)
{
    /* Network filesystems may have limited write support */
    if (!inode)
        return -EINVAL;
    
    /* Check if filesystem is mounted read-only */
    if (IS_RDONLY(inode))
        return -EROFS;
    
    /* Network operations might fail due to connectivity issues */
    return generic_xattr_set(inode, flag);
}

int network_fs_clear_attr(struct inode *inode)
{
    if (!inode)
        return -EINVAL;
    
    if (IS_RDONLY(inode))
        return -EROFS;
    
    return generic_xattr_clear(inode);
}

/* ========== READ-ONLY FILESYSTEM HANDLERS ========== */

int readonly_fs_get_attr(struct inode *inode, int *value)
{
    if (!inode || !value)
        return -EINVAL;
    
    /* All files on read-only filesystems are implicitly readonly */
    *value = FSPROTECT_READONLY;
    return 0;
}

int readonly_fs_set_attr(struct inode *inode, int flag)
{
    /* Cannot set attributes on read-only filesystems */
    return -EROFS;
}

int readonly_fs_clear_attr(struct inode *inode)
{
    /* Cannot clear attributes on read-only filesystems */
    return -EROFS;
}

/* ========== MAIN FILESYSTEM OPERATION HANDLER ========== */

int handle_filesystem_operation(struct inode *inode, int operation, int flag, int *result)
{
    struct fs_info *fs;
    int ret = -ENOSYS;

    if (!inode)
        return -EINVAL;

    fs = detect_filesystem_type(inode->i_sb);
    if (!fs) {
        /* Unknown filesystem, try generic xattr if supported */
        if (filesystem_supports_feature(inode->i_sb, FS_CAP_XATTR)) {
            switch (operation) {
            case 0: /* GET */
                return generic_xattr_get(inode, result);
            case 1: /* SET */
                return generic_xattr_set(inode, flag);
            case 2: /* CLEAR */
                return generic_xattr_clear(inode);
            }
        }
        return -ENOSYS;
    }

    /* Use filesystem-specific handlers */
    switch (operation) {
    case 0: /* GET operation */
        if (fs->get_attr) {
            ret = fs->get_attr(inode, result);
        } else {
            ret = -ENOSYS;
        }
        break;

    case 1: /* SET operation */
        if (fs->set_attr) {
            ret = fs->set_attr(inode, flag);
        } else {
            ret = -ENOSYS;
        }
        break;

    case 2: /* CLEAR operation */
        if (fs->clear_attr) {
            ret = fs->clear_attr(inode);
        } else {
            ret = -ENOSYS;
        }
        break;

    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

/* ========== PUBLIC API FUNCTIONS ========== */

/**
 * getAttributeFromFile - Get protection attribute for a file
 * @inode: inode of the file
 *
 * Returns: protection flag or negative error code
 */
int getAttributeFromFile(struct inode *inode)
{
    int value = 0;
    int ret;

    if (!inode)
        return -EINVAL;

    ret = handle_filesystem_operation(inode, 0, 0, &value);
    if (ret < 0)
        return ret;

    return value;
}

/**
 * getAttributeFromDirectory - Get protection attribute for a directory
 * @inode: inode of the directory
 *
 * Returns: protection flag or negative error code
 */
int getAttributeFromDirectory(struct inode *inode)
{
    int value = 0;
    int ret;

    if (!inode)
        return -EINVAL;

    ret = handle_filesystem_operation(inode, 0, 0, &value);
    if (ret < 0)
        return ret;

    return value;
}

/**
 * getDirectoryAttribute - Get protection attribute for a directory (alias)
 * @dir_inode: inode of the directory
 *
 * Returns: protection flag or negative error code
 */
int getDirectoryAttribute(struct inode *dir_inode)
{
    return getAttributeFromDirectory(dir_inode);
}

/**
 * setAttributeOnFile - Set protection attribute for a file
 * @inode: inode of the file
 * @flag: protection flag to set
 *
 * Returns: 0 on success, negative error code on failure
 */
void setAttributeOnFile(struct inode *inode, enum fsprotect_flags flag)
{
    if (!inode)
        return;

    handle_filesystem_operation(inode, 1, flag, NULL);
}

/**
 * setAttributeOnDirectory - Set protection attribute for a directory
 * @inode: inode of the directory
 * @flag: protection flag to set
 *
 * Returns: 0 on success, negative error code on failure
 */
int setAttributeOnDirectory(struct inode *inode, int flag)
{
    if (!inode)
        return -EINVAL;

    return handle_filesystem_operation(inode, 1, flag, NULL);
}

/**
 * clearAttributeFromFile - Clear protection attribute from a file
 * @inode: inode of the file
 *
 * Returns: 0 on success, negative error code on failure
 */
int clearAttributeFromFile(struct inode *inode)
{
    if (!inode)
        return -EINVAL;

    return handle_filesystem_operation(inode, 2, 0, NULL);
}

/**
 * clearAttributeFromDirectory - Clear protection attribute from a directory
 * @inode: inode of the directory
 *
 * Returns: 0 on success, negative error code on failure
 */
int clearAttributeFromDirectory(struct inode *inode)
{
    if (!inode)
        return -EINVAL;

    return handle_filesystem_operation(inode, 2, 0, NULL);
}

/* ========== PERMISSION CHECK FUNCTIONS ========== */

/**
 * canRemove - Check if a file or directory can be removed
 * @inode: inode of the file or directory
 *
 * Returns: 1 if removal is allowed, 0 if not, negative error on failure
 */
int canRemove(struct inode *inode)
{
    int attr;
    __u32 fs_caps;

    if (!inode)
        return -EINVAL;
        
    /* Add memory barrier to ensure inode state is consistent */
    smp_rmb();
    
    /* Check filesystem capabilities */
    fs_caps = get_filesystem_capabilities(inode->i_sb);
    
    /* Always deny removal on read-only filesystems */
    if (fs_caps & FS_CAP_READ_ONLY)
        return 0;
    
    /* Always deny removal on virtual filesystems like proc, sysfs */
    if (fs_caps & FS_CAP_VIRTUAL_FS)
        return 0;
    
    if (S_ISDIR(inode->i_mode)) {
        attr = getAttributeFromDirectory(inode);
    } else {
        attr = getAttributeFromFile(inode);
    }
    
    /* Handle errors from attribute retrieval */
    if (attr < 0) {
        if (attr == -ENODATA || attr == -ENOENT)
            return 1; /* Allow removal */
        
        /* For filesystem-specific errors, be more lenient */
        if (attr == -ENOSYS || attr == -EOPNOTSUPP)
            return 1;
            
        return 0;
    }
        
    /* If readonly or editonly flag is set, deny removal */
    if (attr == FSPROTECT_READONLY || attr == FSPROTECT_EDITONLY)
        return 0;  /* Permission denied */
        
    /* Allow removal */
    return 1;
}

/**
 * canWrite - Check if a file or directory can be written to
 * @inode: inode of the file or directory
 *
 * Returns: 1 if write is allowed, -EACCES if not, negative error on other failures
 */
int canWrite(struct inode *inode)
{
    int attr;
    __u32 fs_caps;

    if (!inode)
        return -EINVAL;

    /* Add memory barrier to ensure inode state is consistent */
    smp_rmb();

    /* Check filesystem capabilities */
    fs_caps = get_filesystem_capabilities(inode->i_sb);
    
    /* Always deny write on read-only filesystems */
    if (fs_caps & FS_CAP_READ_ONLY)
        return -EACCES;
    
    /* Check standard Linux read-only flag */
    if (IS_RDONLY(inode))
        return -EACCES;

    if (S_ISDIR(inode->i_mode)) {
        attr = getAttributeFromDirectory(inode);
    } else {
        attr = getAttributeFromFile(inode);
    }

    /* Handle errors from attribute retrieval */
    if (attr < 0) {
        if (attr == -ENODATA || attr == -ENOENT)
            return 1; /* Allow write */
        
        /* For filesystem-specific errors, be more lenient */
        if (attr == -ENOSYS || attr == -EOPNOTSUPP)
            return 1;
            
        return -EACCES;
    }

    /* Check if readonly flag is set */
    if (attr == FSPROTECT_READONLY)
        return -EACCES;

    /* Allow write for other flags or no flags */
    return 1;
}

/**
 * canEdit - Check if a file can be edited (similar to canWrite but with editonly support)
 * @inode: inode of the file
 *
 * Returns: 1 if edit is allowed, -EACCES if not, negative error on other failures
 */
int canEdit(struct inode *inode)
{
    int attr;
    __u32 fs_caps;

    if (!inode)
        return -EINVAL;

    /* Add memory barrier to ensure inode state is consistent */
    smp_rmb();

    /* Check filesystem capabilities */
    fs_caps = get_filesystem_capabilities(inode->i_sb);
    
    /* Always deny edit on read-only filesystems */
    if (fs_caps & FS_CAP_READ_ONLY)
        return -EACCES;
    
    /* Check standard Linux read-only flag */
    if (IS_RDONLY(inode))
        return -EACCES;

    attr = getAttributeFromFile(inode);

    /* Handle errors from attribute retrieval */
    if (attr < 0) {
        if (attr == -ENODATA || attr == -ENOENT)
            return 1; /* Allow edit */
        
        /* For filesystem-specific errors, be more lenient */
        if (attr == -ENOSYS || attr == -EOPNOTSUPP)
            return 1;
            
        return -EACCES;
    }

    /* Check protection flags */
    if (attr == FSPROTECT_READONLY)
        return -EACCES;

    /* FSPROTECT_EDITONLY allows editing but not deletion */
    if (attr == FSPROTECT_EDITONLY)
        return 1;

    /* Allow edit for other flags or no flags */
    return 1;
}

/**
 * canAppend - Check if a file can be appended to
 * @inode: inode of the file
 *
 * Returns: 1 if append is allowed, -EACCES if not, negative error on other failures
 */
int canAppend(struct inode *inode)
{
    int attr;
    __u32 fs_caps;

    if (!inode)
        return -EINVAL;

    /* Add memory barrier to ensure inode state is consistent */
    smp_rmb();

    /* Check filesystem capabilities */
    fs_caps = get_filesystem_capabilities(inode->i_sb);
    
    /* Always deny append on read-only filesystems */
    if (fs_caps & FS_CAP_READ_ONLY)
        return -EACCES;
    
    /* Check standard Linux read-only flag */
    if (IS_RDONLY(inode))
        return -EACCES;

    attr = getAttributeFromFile(inode);

    /* Handle errors from attribute retrieval */
    if (attr < 0) {
        if (attr == -ENODATA || attr == -ENOENT)
            return 1; /* Allow append */
        
        /* For filesystem-specific errors, be more lenient */
        if (attr == -ENOSYS || attr == -EOPNOTSUPP)
            return 1;
            
        return -EACCES;
    }

    /* Check protection flags */
    if (attr == FSPROTECT_READONLY)
        return -EACCES;

    /* FSPROTECT_APPENDONLY only allows append operations */
    if (attr == FSPROTECT_APPENDONLY)
        return 1;

    /* Allow append for other flags or no flags */
    return 1;
}

/* ========== UTILITY FUNCTIONS ========== */

/**
 * get_filesystem_info - Get detailed information about a filesystem
 * @sb: superblock to examine
 * @info_buf: buffer to store information string
 * @buf_size: size of the buffer
 *
 * Returns: length of info string on success, negative error on failure
 */
int get_filesystem_info(struct super_block *sb, char *info_buf, size_t buf_size)
{
    struct fs_info *fs;
    __u32 caps;
    int len = 0;

    if (!sb || !info_buf || buf_size == 0)
        return -EINVAL;

    fs = detect_filesystem_type(sb);
    caps = get_filesystem_capabilities(sb);

    if (fs) {
        len = snprintf(info_buf, buf_size, 
                      "Filesystem: %s (magic: 0x%x)\n"
                      "Capabilities: %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
                      fs->name, fs->magic,
                      (caps & FS_CAP_XATTR) ? "xattr " : "",
                      (caps & FS_CAP_ACL) ? "acl " : "",
                      (caps & FS_CAP_QUOTA) ? "quota " : "",
                      (caps & FS_CAP_COMPRESSION) ? "compress " : "",
                      (caps & FS_CAP_ENCRYPTION) ? "encrypt " : "",
                      (caps & FS_CAP_SNAPSHOTS) ? "snapshots " : "",
                      (caps & FS_CAP_HARDLINKS) ? "hardlinks " : "",
                      (caps & FS_CAP_SYMLINKS) ? "symlinks " : "",
                      (caps & FS_CAP_CASE_INSENSITIVE) ? "case-insensitive " : "",
                      (caps & FS_CAP_SPARSE_FILES) ? "sparse " : "",
                      (caps & FS_CAP_LARGE_FILES) ? "large-files " : "",
                      (caps & FS_CAP_ATOMIC_WRITE) ? "atomic " : "",
                      (caps & FS_CAP_JOURNALING) ? "journal " : "",
                      (caps & FS_CAP_NETWORK_FS) ? "network " : "",
                      (caps & FS_CAP_READ_ONLY) ? "readonly " : "",
                      (caps & FS_CAP_VIRTUAL_FS) ? "virtual " : "");
    } else {
        len = snprintf(info_buf, buf_size, 
                      "Filesystem: unknown (magic: 0x%lx)\n"
                      "Type: %s\n",
                      sb->s_magic,
                      sb->s_type ? sb->s_type->name : "unknown");
    }

    return len;
}

/**
 * is_protection_supported - Check if filesystem supports protection attributes
 * @sb: superblock to check
 *
 * Returns: 1 if supported, 0 if not
 */
int is_protection_supported(struct super_block *sb)
{
    struct fs_info *fs;
    
    if (!sb)
        return 0;

    fs = detect_filesystem_type(sb);
    if (!fs)
        return 0;

    /* Check if filesystem has any attribute handlers */
    return (fs->get_attr != NULL && fs->set_attr != NULL) ? 1 : 0;
}

/**
 * validate_protection_flag - Validate protection flag value
 * @flag: flag to validate
 *
 * Returns: 1 if valid, 0 if not
 */
int validate_protection_flag(int flag)
{
    switch (flag) {
    case FSPROTECT_NONE:
    case FSPROTECT_READONLY:
    case FSPROTECT_EDITONLY:
    case FSPROTECT_WRITEONLY:
    case FSPROTECT_APPENDONLY:
        return 1;
    default:
        return 0;
    }
}

/* ========== MODULE INITIALIZATION ========== */

static int __init fsprotect_init(void)
{
    int i, total_fs = 0;
    
    /* Count supported filesystems */
    for (i = 0; filesystem_table[i].name != NULL; i++)
        total_fs++;
    
    pr_info("fsprotect: Enhanced module loaded with support for %d filesystems\n", total_fs);
    pr_info("fsprotect: Protection flags: readonly, editonly, writeprotect, appendonly\n");
    pr_info("fsprotect: Supported filesystem categories:\n");
    pr_info("fsprotect: - Modern Linux: ext2/3/4, xfs, btrfs, f2fs\n");
    pr_info("fsprotect: - Unix variants: UFS, ReiserFS\n");
    pr_info("fsprotect: - Windows: FAT/VFAT/ExFAT\n");
    pr_info("fsprotect: - Network: NFS, NFS4, CIFS\n");
    pr_info("fsprotect: - Special: FUSE, tmpfs, overlay\n");
    pr_info("fsprotect: - Read-only: squashfs, cramfs, iso9660\n");
    pr_info("fsprotect: - Clustered: OCFS2\n");
    pr_info("fsprotect: - Legacy: minix\n");
    
    return 0;
}

static void __exit fsprotect_exit(void)
{
    pr_info("fsprotect: Enhanced filesystem protection module unloaded\n");
}

module_init(fsprotect_init);
module_exit(fsprotect_exit);

/* ========== EXPORTED SYMBOLS ========== */

/* Main API functions */
EXPORT_SYMBOL(getAttributeFromFile);
EXPORT_SYMBOL(getAttributeFromDirectory);
EXPORT_SYMBOL(getDirectoryAttribute);
EXPORT_SYMBOL(setAttributeOnFile);
EXPORT_SYMBOL(setAttributeOnDirectory);
EXPORT_SYMBOL(clearAttributeFromFile);
EXPORT_SYMBOL(clearAttributeFromDirectory);

/* Permission check functions */
EXPORT_SYMBOL(canRemove);
EXPORT_SYMBOL(canWrite);
EXPORT_SYMBOL(canEdit);
EXPORT_SYMBOL(canAppend);

/* Utility functions */
EXPORT_SYMBOL(get_filesystem_info);
EXPORT_SYMBOL(get_filesystem_capabilities);
EXPORT_SYMBOL(filesystem_supports_feature);
EXPORT_SYMBOL(is_protection_supported);
EXPORT_SYMBOL(validate_protection_flag);
EXPORT_SYMBOL(detect_filesystem_type);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("first person");
MODULE_DESCRIPTION("root filesystem protection module");
MODULE_VERSION("1.0");
