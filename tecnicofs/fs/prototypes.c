#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {

    open_file_entry_t *file = get_open_file_entry(fhandle);

    if (file == NULL) {
        return -1;
    }

    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }    

    if (to_write == 0) {
        printf("[ - ] Data error : Nothing to write\n");
        return -1;
    } 

    if (inode->i_size + to_write < MAX_DIRECT_DATA_SIZE) {

        int insert_status = tfs_write_direct_region(inode, file, buffer, to_write);
        if (insert_status == -1) {
            return -1;
        }

    }

    else if (inode->i_size >= MAX_DIRECT_DATA_SIZE) {

        int insert_status = tfs_write_indirect_region(inode, file, buffer, to_write);
        if (insert_status == -1) {
            return -1;
        }

    }

    else {

        size_t direct_size = MAX_DIRECT_DATA_SIZE - inode->i_size;
        size_t indirect_size = to_write - direct_size;

        tfs_write_direct_region(inode, file, buffer, direct_size);  //escrever parte na regiao direta
        tfs_write_indirect_region(inode, file, buffer + direct_size, indirect_size); // escrever o resto na indireta
    }

}

ssize_t tfs_write_direct_region(inode_t *inode, open_file_entry_t *file, void const *buffer, size_t write_size) {

    if (inode->i_size == 0 || file->of_offset == BLOCK_SIZE) { 
                                                        
        int insert_status = direct_block_insert(inode, file);     

        if (insert_status == -1) {
            printf("[ tfs_write_direct_region ] Error writing in direct region: %s\n", strerror(errno));
            return -1;
        }
    }

    inode->i_size += write_size;

    void *block = data_block_get(inode->i_data_block);
    if (block == NULL) {
        return -1;
    }

    /* Perform the actual write */
    memcpy(block + file->of_offset, buffer, write_size);

    /* The offset associated with the file handle is
        * incremented accordingly */
    file->of_offset += write_size;
    if (file->of_offset > inode->i_size) {
        inode->i_size = file->of_offset;
    }

}

int direct_block_insert(inode_t *inode, open_file_entry_t *file) {

    int i;

    int *blocks_arr = inode->i_block;

    for (i = 0; blocks_arr[i] != '\0' && i < 10; i++);

    if (i == 10) {
        printf("[ direct_block_insert ] Error : Reached indirect region\n");
        return -1;
    }

    inode->i_data_block = data_block_alloc();
    inode->i_block[i] = inode->i_data_block;
    file->of_offset = 0;

    return 0;

}

ssize_t tfs_write_indirect_region(inode_t *inode, open_file_entry_t *file, void const *buffer, size_t write_size) {

    int i, j;

    int *blocks_arr = inode->i_block;

    for (i = 10; blocks_arr[i] != '\0' && i < MAX_DATA_BLOCKS_FOR_INODE; i++);

    if (i == MAX_DATA_BLOCKS_FOR_INODE) {
        printf("[ direct_block_insert ] Error : Reached indirect region\n");
        return -1;
    }    

    tfs_handle_indirect_block(file, inode, write_size, buffer, i);
    
    return 0;
}

size_t tfs_handle_indirect_block(open_file_entry_t *file, inode_t *inode, size_t write_size, void const *buffer, int i) {

    if (file->of_offset == BLOCK_SIZE || inode->indirect_block_index == BLOCK_SIZE) {                                                             
        indirect_block_insert(inode, i);
    }
        
    inode->i_size += write_size;

    void *block = data_block_get(inode->i_data_block);
    if (block == NULL) {
        return -1;
    }

    /* Perform the actual write */
    memcpy(block + file->of_offset, buffer, write_size);

    /* The offset associated with the file handle is
        * incremented accordingly */
    file->of_offset += write_size;
    if (file->of_offset > inode->i_size) {
        inode->i_size = file->of_offset;
    }
}

int indirect_block_insert(inode_t *inode, int i) {
    inode->i_data_block = data_block_alloc();
    inode->i_block[i] = inode->i_data_block;
    inode->indirect_block_index = 0;
    int target_data_block = data_block_alloc();
    void *block = data_block_get(target_data_block);
    memcpy(block + inode->indirect_block_index, target_data_block, sizeof(target_data_block));
    inode->indirect_block_index++;
    inode->i_data_block = target_data_block;
}


