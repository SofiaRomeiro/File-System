#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    size_t size_read = 0;
    
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    inode_t *inode = inode_get(file->of_inumber);

    printf("[ tfs_read ] inode i size = %ld\n", inode->i_size);
    printf("[ tfs_read ] len = %ld\n", len);
    
    if (inode == NULL) {
        return -1;
    }

    file->of_offset = 0;

    for (int i = 0; size_read <= len && (i < 10); i++) {
        void* block = data_block_get(inode->i_block[i]);

        if (block == NULL) {
            printf("[-] Error: NULL block\n");
            return -1;
        }

        if (len - size_read > 1024) {
            memcpy(buffer + size_read, block, 1024);
            size_read += 1024;
        }

        else {
            memcpy(buffer + size_read, block, len - size_read);
            size_read += (len - size_read);
        }
        printf("size_read: %ld\n", size_read);
    }

    if ((len - size_read) > 0) {
        int *block_from_i_block = (int *) data_block_get(inode->i_block[10]);

        for (int i = 0; size_read <= len && (i < 256); i++) {
            printf("i_block: block number %d\n", block_from_i_block[i]);
            void* block = data_block_get(block_from_i_block[i]);
            char* x_block = (char*) block;
            printf("block: %ld\n", sizeof(x_block));

            if (block == NULL) {
                printf("[-] Error: NULL block_from_iblock\n");
                return -1;
            }

            if (len - size_read > 1024) {
                memcpy(buffer + size_read, block, 1024);
                printf("FINALLLLLLLLLL ize_read: %ld\n", strlen(buffer));
                size_read += 1024;
            }

            else {
                memcpy(buffer + size_read, block, len - size_read);
                printf("FINALLLLLLLLLL ize_read: %ld\n", strlen(buffer));
                size_read += (len - size_read);
            }
            printf("size_read: %ld\n", size_read);
        }
    }
    printf("FINALLLLLLLLLL ize_read: %ld\n", strlen(buffer));
    return (ssize_t) size_read;
}