/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_FSPROTECT_H
#define _LINUX_FSPROTECT_H

#include <linux/fs.h>
#include <uapi/linux/fsprotect.h>

<<<<<<< HEAD
/* Internal helper functions */
extern int getAttributeFromFile(struct inode *inode);
extern int getAttributeFromDirectory(struct inode *dir_inode);

/* External helper functions */
extern int setAttributeOnFile(struct inode *inode, int flag);
extern int clearAttributeOnFile(struct inode *inode);
extern int setAttributeOnDirectory(struct dentry *dir_dentry, int flag);
extern int clearAttributeOnDirectory(struct inode *inode);
extern int canRemove(struct inode *inode);
extern int canWrite(struct inode *inode);
=======
/* Filesystem types */
#define FS_TYPE_UNKNOWN     0
#define FS_TYPE_EXT2        1
#define FS_TYPE_EXT3        2
#define FS_TYPE_EXT4        3
#define FS_TYPE_XFS         4
#define FS_TYPE_BTRFS       5
#define FS_TYPE_F2FS        6
#define FS_TYPE_UFS         7
#define FS_TYPE_REISERFS    8
#define FS_TYPE_FAT         9
#define FS_TYPE_VFAT        10
#define FS_TYPE_EXFAT       11
#define FS_TYPE_NTFS        12
#define FS_TYPE_NFS         13
#define FS_TYPE_NFS4        14
#define FS_TYPE_CIFS        15
#define FS_TYPE_FUSE        16
#define FS_TYPE_TMPFS       17
#define FS_TYPE_PROC        18
#define FS_TYPE_SYSFS       19
#define FS_TYPE_SQUASHFS    20
#define FS_TYPE_CRAMFS      21
#define FS_TYPE_ISO9660     22
#define FS_TYPE_OVERLAY     23
#define FS_TYPE_OCFS2       24
#define FS_TYPE_MINIX       25

/* Filesystem capabilities */
#define FS_CAP_XATTR            0x00000001
#define FS_CAP_ACL              0x00000002
#define FS_CAP_QUOTA            0x00000004
#define FS_CAP_COMPRESSION      0x00000008
#define FS_CAP_ENCRYPTION       0x00000010
#define FS_CAP_SNAPSHOTS        0x00000020
#define FS_CAP_HARDLINKS        0x00000040
#define FS_CAP_SYMLINKS         0x00000080
#define FS_CAP_CASE_INSENSITIVE 0x00000100
#define FS_CAP_SPARSE_FILES     0x00000200
#define FS_CAP_LARGE_FILES      0x00000400
#define FS_CAP_ATOMIC_WRITE     0x00000800
#define FS_CAP_JOURNALING       0x00001000
#define FS_CAP_NETWORK_FS       0x00002000
#define FS_CAP_READ_ONLY        0x00004000
#define FS_CAP_VIRTUAL_FS       0x00008000

/* Filesystem info structure */
struct fs_info {
    int type;
    const char *name;
    __u32 magic;
    __u32 capabilities;
    int (*get_attr)(struct inode *inode, int *value);
    int (*set_attr)(struct inode *inode, int flag);
    int (*clear_attr)(struct inode *inode);
};

/* Protection flags */
#define FSPROTECT_NONE          0x00000000
#define FSPROTECT_READONLY      0x00000001
#define FSPROTECT_EDITONLY      0x00000002
#define FSPROTECT_WRITEONLY     0x00000004
#define FSPROTECT_APPENDONLY    0x00000008

/* UFS variant detection */
enum ufs_variant {
    UFS_VARIANT_NONE = 0,
    UFS_VARIANT_OLD,
    UFS_VARIANT_44BSD,
    UFS_VARIANT_SUB,
    UFS_VARIANT_HPUX,
    UFS_VARIANT_SUNX86,
    UFS_VARIANT_SUN = 3,
    UFS_VARIANT_NEXTSTEP,
    UFS_VARIANT_NEXTSTEP_CD,
    UFS_VARIANT_OPENSTEP,
    UFS_VARIANT_UFS2
};

/* External API functions */
extern int canRemove(struct inode *inode);
extern int canWrite(struct inode *inode);
extern int get_filesystem_info(struct super_block *sb, char *info_buf, size_t buf_size);
extern struct fs_info *detect_filesystem_type(struct super_block *sb);
extern __u32 get_filesystem_capabilities(struct super_block *sb);
extern bool filesystem_supports_feature(struct super_block *sb, __u32 feature);
extern int getAttributeFromFile(struct inode *inode);
extern int getAttributeFromDirectory(struct inode *inode);
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
>>>>>>> 3bcd6da06a3d (feat: all filesystems capablity added in kernel space fsprotect.c)

#endif /* _LINUX_FSPROTECT_H */
