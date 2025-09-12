/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_FSPROTECT_H
#define _LINUX_FSPROTECT_H

#include <linux/fs.h>
#include <uapi/linux/fsprotect.h>

/* Internal helper functions */
void setAttributeOnFile(struct inode *inode, int flag);
int getAttributeFromFile(struct inode *inode);
void setDirectoryAttribute(struct dentry *dir_dentry, int flag);
int getDirectoryAttribute(struct inode *dir_inode);

/* External kernel API */
extern int fsprotect_inode_write(struct inode *inode);
extern int fsprotect_inode_unlink(struct inode *dir, struct dentry *dentry);
extern int fsprotect_inode_rename(struct inode *old_dir, struct dentry *old_dentry,
                                struct inode *new_dir, struct dentry *new_dentry);

#endif /* _LINUX_FSPROTECT_H */
