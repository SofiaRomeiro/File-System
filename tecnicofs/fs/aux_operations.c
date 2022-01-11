#include "aux_operations.h"

#define FIRST_INDIRECT_BLOCK (12)
#define REFERENCE_BLOCK_INDEX (11)

ssize_t tfs_write_direct_region(inode_t *inode, open_file_entry_t *file, void const *buffer, size_t write_size) {

    size_t bytes_written = 0;
    size_t to_write_block = 0;

// ------------------------------------------------ CRIT SPOT - RWLOCK R ----------------------------------

    size_t local_offset = file->of_offset;
    size_t local_isize = inode->i_size;

// ------------------------------------------------- END CRIT SPOT ----------------------------------------
    
    for (int i = 0; write_size > 0 && i < REFERENCE_BLOCK_INDEX; i++) {

        if (local_isize % BLOCK_SIZE == 0) {                                                             
            int insert_status = direct_block_insert(inode);     
            if (insert_status == -1) {
                printf("[ tfs_write_direct_region ] Error writing in direct region: %s\n", strerror(errno));
                return -1;
            }
        }

        void *block = data_block_get(inode->i_data_block);
        if (block == NULL) {
            return -1;
        }
        
        if (write_size >= BLOCK_SIZE || BLOCK_SIZE - (local_offset % BLOCK_SIZE) < write_size) {
            to_write_block = BLOCK_SIZE - (local_offset % BLOCK_SIZE);
            write_size -= to_write_block;

        } else  {   
            to_write_block = write_size;
            write_size = 0;
        }

        memcpy(block + (local_offset % BLOCK_SIZE), buffer + bytes_written, to_write_block);

        local_offset += to_write_block;
        local_isize += to_write_block;
        bytes_written += to_write_block;

    }

// ------------------------------------------------ CRIT SPOT - RWLOCK W ----------------------------------

    file->of_offset = local_offset;
    inode->i_size = local_isize;

// ------------------------------------------------ END CRIT SPOT ----------------------------------

    return (ssize_t)bytes_written;
}

int direct_block_insert(inode_t *inode) {

// ------------------------------------------------ CRIT SPOT - MUTEX ----------------------------------

    inode->i_data_block = data_block_alloc();

    if (inode->i_data_block == -1) {
        printf("[ direct_block_insert ] Error : alloc block failed\n");
        return -1;
    }

    memset(data_block_get(inode->i_data_block),-1, sizeof(data_block_get(inode->i_data_block)));

    inode->i_block[inode->i_data_block - 1] = inode->i_data_block;

// ------------------------------------------------ END CRIT SPOT ----------------------------------

    return 0;
}

ssize_t tfs_write_indirect_region(inode_t *inode, open_file_entry_t *file, void const *buffer, size_t write_size) {

    size_t bytes_written = 0;
    size_t to_write_block = 0;
    int insert_status = 0;

// ------------------------------------------------ CRIT SPOT - RWLOCK R ----------------------------------

    size_t local_offset = file->of_offset;
    size_t local_isize = inode->i_size;

// ------------------------------------------------- END CRIT SPOT ----------------------------------------

    for (int i = 0; write_size > 0; i++) {

        if (local_isize + write_size > MAX_BYTES) {
            write_size = MAX_BYTES - local_isize;
        }

        if (local_isize % BLOCK_SIZE == 0) { 

            insert_status = indirect_block_insert(inode);  

            if (insert_status == -1) {
                printf("[ tfs_write_indirect_region ] Error writing in indirect region: %s\n", strerror(errno));
                return -1;
            }
        }

        void *block = data_block_get(inode->i_data_block);
        if (block == NULL) {
            printf("[ tfs_write_indirect_region ] Error : NULL block\n");
            return -1;
        }
        
        if (write_size >= BLOCK_SIZE || BLOCK_SIZE - (local_offset % BLOCK_SIZE) < write_size) {
            to_write_block = BLOCK_SIZE - (local_offset % BLOCK_SIZE);           
            write_size -= to_write_block;
        }

        else  {
            to_write_block = write_size;
            write_size = 0;
        }

        memcpy(block + (local_offset % BLOCK_SIZE), buffer + bytes_written, to_write_block);

        local_offset += to_write_block;
        local_isize += to_write_block;
        bytes_written += to_write_block;
    }

// ------------------------------------------------ CRIT SPOT - RWLOCK W ----------------------------------

    file->of_offset = local_offset;
    inode->i_size = local_isize;

// ------------------------------------------------ END CRIT SPOT ----------------------------------
    
    return (ssize_t)bytes_written;
}

int indirect_block_insert(inode_t *inode) {

// ------------------------------------------------ CRIT SPOT - MUTEX ----------------------------------

    int *last_i_block = (int *)data_block_get(inode->i_block[MAX_DIRECT_BLOCKS]);

    int block_number = data_block_alloc();

    if (block_number == -1) {
        printf(" Error : Invalid block insertion\n");
        return -1;
    }

    inode->i_data_block = block_number;

    memset(data_block_get(block_number), -1, BLOCK_SIZE / sizeof(int));

    last_i_block[block_number - FIRST_INDIRECT_BLOCK] = block_number;    

// ------------------------------------------------ END CRIT SPOT ----------------------------------

    return 0;

}

int tfs_handle_indirect_block(inode_t *inode) {

// ------------------------------------------------ CRIT SPOT - MUTEX ----------------------------------

    int block_number = data_block_alloc();

    if (block_number == -1) {
        return -1;
    }

    inode->i_block[MAX_DIRECT_BLOCKS] = block_number;
    inode->i_data_block = block_number;

    memset(data_block_get(inode->i_data_block), -1, sizeof(data_block_get(inode->i_data_block)));

// ------------------------------------------------ END CRIT SPOT ----------------------------------

    return 0;
}

ssize_t tfs_read_direct_region(open_file_entry_t *file, size_t to_read, void *buffer) {

// ------------------------------------------------ CRIT SPOT - RWLOCK R ----------------------------------

    size_t local_offset = file->of_offset;

// ------------------------------------------------ END CRIT SPOT ----------------------------------


    size_t current_block = (local_offset / BLOCK_SIZE) + 1;
    size_t block_offset = local_offset % BLOCK_SIZE;
    size_t to_read_block = 0;
    size_t total_read = 0;

    printf("reading...\nlocal offset is %ld and to read is %ld\n", local_offset, to_read);
    printf("current block is %ld\n", current_block);
    
    if (local_offset + to_read <= MAX_BYTES_DIRECT_DATA) {

        while (to_read > 0 && current_block <= MAX_DIRECT_BLOCKS) {        

            void *block = data_block_get((int) current_block);

            if (block == NULL) {
                return -1;
            }

            if (to_read + block_offset > BLOCK_SIZE ) { 
                to_read_block = BLOCK_SIZE - block_offset;               
                to_read -= to_read_block;

            } else {
                to_read_block = to_read;
                to_read = 0;
            }

            printf("current block %ld\n", current_block);
            //printf("(%ld) %s\n", strlen((char *)block), (char *)block);
            printf("buffer : %ld\n", strlen((char *)buffer));
            //printf("hi\n");

            memcpy(buffer + total_read, block + block_offset, to_read_block);

            printf("after buffer : %ld\n", strlen((char *)buffer));

            local_offset += to_read_block;
            total_read += to_read_block;

            current_block = (local_offset / BLOCK_SIZE) + 1;
            block_offset = local_offset % BLOCK_SIZE;

            printf("to read = %ld\n", to_read);

            printf("total_read = %ld\n", total_read);

        }
    }

// ------------------------------------------------ CRIT SPOT - RWLOCK W ----------------------------------

    file->of_offset = local_offset;

// ------------------------------------------------ END CRIT SPOT ----------------------------------

    printf("total_read at return = %ld\n", total_read);

    return (ssize_t) total_read;
}

ssize_t tfs_read_indirect_region(open_file_entry_t *file, size_t to_read, void *buffer) {

// ----------------------------------- CRIT SPOT - RWLOCK R -----------------------------------------

    size_t local_offset = file->of_offset;

// --------------------------------- END CRIT SPOT ---------------------------------------

    size_t to_read_block = 0;
    size_t total_read = 0;
    size_t current_block = (local_offset / BLOCK_SIZE) + 2;
    size_t block_offset = local_offset % BLOCK_SIZE;

    while (to_read > 0) {      

        void *block = data_block_get((int) current_block);
        if (block == NULL) {
            return -1;
        }

        if (to_read + block_offset > BLOCK_SIZE ) {
            to_read_block = BLOCK_SIZE - block_offset;              
            to_read -= to_read_block;

        } else {
            to_read_block = to_read;
            to_read = 0;
        }

        memcpy(buffer + total_read, block + block_offset, to_read_block);

        local_offset += to_read_block;
        total_read += to_read_block;

        current_block = (local_offset / BLOCK_SIZE) + 2;
        block_offset = local_offset % BLOCK_SIZE;
    }

// ------------------------------------------------ CRIT SPOT - RWLOCK W ----------------------------------

    file->of_offset = local_offset;

// ------------------------------------------------ END CRIT SPOT ----------------------------------

    return (ssize_t)total_read;
}