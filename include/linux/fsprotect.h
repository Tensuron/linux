/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_FSPROTECT_H
#define _LINUX_FSPROTECT_H

#include <linux/fs.h>
#include <uapi/linux/fsprotect.h>

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

#endif /* _LINUX_FSPROTECT_H */
