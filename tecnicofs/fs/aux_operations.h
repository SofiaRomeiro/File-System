#ifndef AUX_OPERATIONS_H
#define AUX_OPERATIONS_H

#include "config.h"
#include "operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

ssize_t tfs_write_direct_region(inode_t *inode, open_file_entry_t *file, void const *buffer, size_t write_size);

int direct_block_insert(inode_t *inode);

ssize_t tfs_write_indirect_region(inode_t *inode, open_file_entry_t *file, void const *buffer, size_t write_size);

int indirect_block_insert(inode_t *inode);

int tfs_handle_indirect_block(inode_t *inode);

ssize_t tfs_read_direct_region(open_file_entry_t *file, size_t to_read, void *buffer);

ssize_t tfs_read_indirect_region(open_file_entry_t *file, size_t to_read, void *buffer);

#endif // AUX_OPERATIONS_H