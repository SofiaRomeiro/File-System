#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int tfs_init() {
    state_init();

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    state_destroy();
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(ROOT_DIR_INUM, name);
}

int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);
    if (inum >= 0) {
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }

        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                if (data_block_free(inode->i_data_block) == -1) {
                    return -1;
                }
                inode->i_size = 0;
            }
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
    } else {
        return -1;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}

int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }

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

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {

    size_t to_read = 0;
    
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    if (file->of_offset < 1024) {
        /* Determine how many bytes to read */
        to_read = inode->i_size - file->of_offset;
    }

    else {
        file->of_offset = 0;
        to_read = (inode->i_size - 1024);
    }   

    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        if (block == NULL) {
            return -1;
        }

        /* Perform the actual read */
        memcpy(buffer, block + file->of_offset, to_read);
        /* The offset associated with the file handle is
         * incremented accordingly */
        file->of_offset += to_read;
    }

    else {

        file->of_offset = 0;
        void *block = data_block_get(inode->i_data_block);
        if (block == NULL) {
            return -1;
        }

        /* Perform the actual read */
        memcpy(buffer, block + file->of_offset, to_read);
        /* The offset associated with the file handle is
         * incremented accordingly */
        file->of_offset += to_read;      
    }

    return (ssize_t)to_read;
}

int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {

    void *buffer[100];
    int source_file;
    FILE *dest_file;
    ssize_t read_bytes = 0;
    size_t to_write_bytes = 0;
    size_t written_bytes = 0;

    memset(buffer, '\0', sizeof(buffer));

    if (tfs_lookup(source_path) == -1) {
        source_file = tfs_open(source_path, TFS_O_APPEND);
    }
    else {
        source_file = tfs_open(source_path, TFS_O_CREAT);
    }    

    if (source_file < 0) {
        printf("[-] Open error in src (%s): %s\n", source_path, strerror(errno));
		return -1;
    }

    dest_file = fopen(dest_path, "w");

    if (dest_file == NULL) {
        printf("[-] Open error in dest (%s) : %s\n", dest_path, strerror(errno));
		return -1;
    }  

    open_file_entry_t *file = get_open_file_entry(source_file);
    inode_t *inode = inode_get(file->of_inumber);
    size_t total_size_to_read = inode->i_size;

    do {

        read_bytes += tfs_read(source_file, buffer, sizeof(buffer));

        if (read_bytes < 0) {
            printf("[-] Read error: %s\n", strerror(errno));
		    return -1;
        }

        to_write_bytes = (size_t) read_bytes;   // since the check for negative values was made before, casting is safe

        written_bytes = fwrite(buffer, sizeof(void), to_write_bytes, dest_file);

        if (written_bytes < read_bytes) {
            printf("[-] Write error: %s\n", strerror(errno));
		    return -1;
        }

        memset(buffer, '\0', sizeof(buffer));  

    } while (total_size_to_read > read_bytes);

    int close_status_source =  tfs_close(source_file);
    int close_status_dest = fclose(dest_file);

    if (close_status_dest < 0 || close_status_source < 0) {
        printf("[-] Close error: %s\n", strerror(errno));
		return -1;
    }

    return 0;
}

/*
int main() {

}
*/
