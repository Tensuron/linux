#ifndef _UAPI_LINUX_FSPROTECT_H
#define _UAPI_LINUX_FSPROTECT_H

#include <linux/types.h>

/* Protection flags */
#define FSPROTECT_READONLY  0x1FAA1DEA
#define FSPROTECT_EDITONLY  0x2FEA1ACA
#define FSPROTECT_NORMAL    0x00000000

enum fsprotect_flags {
    FSPROTECT_FLAG_READONLY = FSPROTECT_READONLY,
    FSPROTECT_FLAG_EDITONLY = FSPROTECT_EDITONLY
};

<<<<<<< HEAD
=======
struct dir_queue {
    struct list_head list;
    struct dentry *dentry;
};

extern inline void setAttributeOnFile(struct inode *inode, enum fsprotect_flags flag);
extern inline int getAttributeFromFile(struct inode *inode);
extern inline int getDirectoryAttribute(struct inode *dir_inode);
extern inline void setDirectoryAttribute(struct dentry *dir_dentry, enum fsprotect_flags flag);

>>>>>>> 3bcd6da06a3d (feat: all filesystems capablity added in kernel space fsprotect.c)
#endif