#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include "devices/block.h"
#include "filesys/off_t.h"

#include <stdbool.h>

struct bitmap;

void inode_init(void);
bool inode_create(block_sector_t, off_t);
struct inode *inode_open(block_sector_t);
struct inode *inode_reopen(struct inode *);
block_sector_t inode_get_inumber(const struct inode *);
void inode_close(struct inode *);
void inode_remove(struct inode *);
off_t inode_read_at(struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at(struct inode *, void const *, off_t size, off_t offset);
off_t inode_length(const struct inode *);

void dr_lock_aquire(struct inode *inode);
void dr_lock_release(struct inode *inode);

#endif /* filesys/inode.h */
