#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {

    /* gets the reference to the position (pointer) where the file is stored in the 
    open file's table*/
    open_file_entry_t *file = get_open_file_entry(fhandle);
    //int block_to_write = 0;

    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }    

    if (to_write == 0) {
        printf("[ - ] Data error : Nothing to write\n");
        return -1;
    } 

    if (to_write < BLOCK_SIZE) {

        if (inode->i_size < MAX_DIRECT_DATA_SIZE) {

            if (inode->i_size == 0 || file->of_offset == BLOCK_SIZE) { 

                inode->i_data_block = data_block_alloc();
                file->of_offset = 0;                                                            // colocar o offset no inicio do bloco
                int insert_status = data_block_insert(inode->i_block, inode->i_data_block);     // inserir o numero do bloco na regiao de dados do inode (i_block)   

                if (insert_status == -1) {
                    printf("[ tfs_write ] Error inserting new block: %s\n", strerror(errno));
                    return -1;
                }
            }
        }

        else if (inode->i_size % BLOCK_SIZE == 0) { 

            inode->i_data_block = data_block_alloc();
            file->of_offset = 0;

            int *block = data_block_get(inode->i_data_block);

            int insert_status = index_block_insert(block, inode->i_data_block);               

            if (insert_status == -1) {
                printf("[ tfs_write ] Error inserting new block: %s\n", strerror(errno));
                return -1;
            }          

            // finally we pass the allocated block number and we attribute it to inode->i_data_block
                
        }

        // from here, the method is equal to the 3 scenarios because we just have to give to data_block_get() the block number where it is supposed to write 

        inode->i_size += to_write;      // "volume" atual ocupado pelo inode

        void *block = data_block_get(inode->i_data_block);
        if (block == NULL) {
            return -1;
        }

        /* Perform the actual write */
        memcpy(block + file->of_offset, buffer, to_write);

        /* The offset associated with the file handle is
         * incremented accordingly */
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }

    /* if we need more than 1 block to perform the writing */

    else {

        size_t written = 0;

        int number_of_requested_blocks = to_write % BLOCK_SIZE;

        if (number_of_requested_blocks != 0) {
            number_of_requested_blocks = (int) (to_write / BLOCK_SIZE) + 1;
        } 
        else {
            number_of_requested_blocks = (int) (to_write / BLOCK_SIZE);
        } 

        /* alloc enough blocks to write */
        int advance_block_size = 0;
        size_t write_size = 1024;

        for (int i = 1; i <= number_of_requested_blocks; i++) {
  
            inode->i_data_block = data_block_alloc();                                // apenas se deve manter o registo do 1º bloco alocado pois ainda nao houve escrita          
            file->of_offset = 0;                                                            // colocar o offset no inicio do bloco
            int insert_status = data_block_insert(inode->i_block, inode->i_data_block);         // inserir o numero do bloco na regiao de dados do inode (i_block)   

            if (insert_status == -1) {
                printf("[ tfs_write ] Error inserting new block: %s\n", strerror(errno));
                return -1;
            }     

            void *block = data_block_get(inode->i_data_block);
            if (block == NULL) {
                return -1;
            }

            if (i > 1) {
                advance_block_size = 1;
            }

            if (i == number_of_requested_blocks) {
                write_size = to_write;
            }

            memcpy(block + file->of_offset, buffer + (BLOCK_SIZE * advance_block_size), write_size);

            to_write -= write_size;
            written += write_size;

            file->of_offset += to_write;

            inode->i_size += write_size;
        }

        // after having all blocks allocated, perform the writing

        return (ssize_t)written;
    }
    return (ssize_t)to_write;
}