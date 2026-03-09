#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include "filesys/off_t.h"

#include <stdbool.h>

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0 /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1 /* Root directory file inode sector. */

/* Block device that contains the file system. */
extern struct block *fs_device;

void filesys_init(bool format);
void filesys_done(void);
bool filesys_create(char const *name, off_t initial_size);
struct file *filesys_open(char const *name);
bool filesys_remove(char const *name);

#endif /* filesys/filesys.h */
