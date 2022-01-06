#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <assert.h>

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
        inode_t *inode = inode_get(inum);       // PROBLEMA AQUI

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

    if (inode->i_size + to_write <= MAX_DIRECT_DATA_SIZE) {
        ssize_t insert_status = tfs_write_direct_region(inode, file, buffer, to_write);
        if (insert_status == -1) {
            return -1;
        }

        to_write = (size_t) insert_status;
    }

    else if (inode->i_size >= MAX_DIRECT_DATA_SIZE) {

        if (inode->i_block[MAX_DIRECT_BLOCKS] == 0) {
            tfs_handle_indirect_block(inode);
        }

        
        ssize_t insert_status = tfs_write_indirect_region(inode, file, buffer, to_write);
        if (insert_status == -1) {
            return -1;
        }

        to_write = (size_t) insert_status;

    }

    else {

        size_t direct_size = MAX_DIRECT_DATA_SIZE - inode->i_size;
        size_t indirect_size = to_write - direct_size;

        ssize_t written_direct = tfs_write_direct_region(inode, file, buffer, direct_size);  //escrever parte na regiao direta
        
        if (inode->i_block[MAX_DIRECT_BLOCKS] == 0) {
            tfs_handle_indirect_block(inode);
        }

        ssize_t written_indirect = tfs_write_indirect_region(inode, file, buffer + direct_size, indirect_size); // escrever o resto na indireta
    
        if (written_direct == -1 || written_indirect == -1) {
            printf("[ tfs_write ] Error writing\n");
        }

        to_write = (size_t) (written_direct + written_indirect);
    }

    return (ssize_t)to_write;
}

ssize_t tfs_write_direct_region(inode_t *inode, open_file_entry_t *file, void const *buffer, size_t write_size) {

    size_t bytes_written = 0;
    size_t block_written_bytes = 0;


    for (int i = 0; write_size > 0 && i < 11; i++) {

        if (inode->i_size % BLOCK_SIZE == 0) {                                                             
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
        
        if (write_size >= BLOCK_SIZE || BLOCK_SIZE - (file->of_offset % BLOCK_SIZE) < write_size) {

            block_written_bytes = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);

            memcpy(block + (file->of_offset % BLOCK_SIZE), buffer + bytes_written, block_written_bytes);

            write_size -= block_written_bytes;
            file->of_offset += block_written_bytes;
            inode->i_size += block_written_bytes;
            bytes_written += block_written_bytes;

        } else  {

            memcpy(block + (file->of_offset % BLOCK_SIZE), buffer + bytes_written, write_size);
           
            file->of_offset += write_size;
            inode->i_size += write_size;
            bytes_written += write_size;
            write_size = 0;
        }

    }

    return (ssize_t)bytes_written;
}

int direct_block_insert(inode_t *inode) {

    inode->i_data_block = data_block_alloc();

    if (inode->i_data_block == -1) {
        printf("[ direct_block_insert ] Error : alloc block failed\n");
        return -1;
    }

    memset(data_block_get(inode->i_data_block),'\0', sizeof(data_block_get(inode->i_data_block)));

    inode->i_block[inode->i_data_block - 1] = inode->i_data_block;
    return 0;
}

ssize_t tfs_write_indirect_region(inode_t *inode, open_file_entry_t *file, void const *buffer, size_t write_size) {

    size_t bytes_written = 0;
    size_t block_written_bytes = 0;

    for (int i = 0; write_size > 0; i++) {

        if (inode->i_size + write_size > MAX_BYTES) {
            write_size = MAX_BYTES - inode->i_size;
        }

        if (inode->i_size % BLOCK_SIZE == 0) { 

            int insert_status = indirect_block_insert(inode);  

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
        
        if (write_size >= BLOCK_SIZE || BLOCK_SIZE - (file->of_offset % BLOCK_SIZE) < write_size) {

            block_written_bytes = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);

            memcpy(block + (file->of_offset % BLOCK_SIZE), buffer + bytes_written, block_written_bytes);

            write_size -= block_written_bytes;
            file->of_offset += block_written_bytes;
            inode->i_size += block_written_bytes;
            bytes_written += block_written_bytes;

        }

        else  {

            memcpy(block + (file->of_offset % BLOCK_SIZE), buffer + bytes_written, write_size);
           
            file->of_offset += write_size;
            inode->i_size += write_size;
            bytes_written += write_size;
            write_size = 0;
        }
 
    }
    
    return (ssize_t)bytes_written;
}

int indirect_block_insert(inode_t *inode) {

    int *last_i_block = (int *) data_block_get(inode->i_block[MAX_DIRECT_BLOCKS]);

    int block_number = data_block_alloc();

    //printf("[ indirect_block_insert] block_number = %d\n", block_number);

    if (block_number == -1) {
        printf(" Error : Invalid block insertion\n");
        return -1;
    }

    inode->i_data_block = block_number;

    memset(data_block_get(block_number), '\0', sizeof(data_block_get(block_number)));

    last_i_block[block_number - 12] = block_number;    
   
    return 0;

}

int tfs_handle_indirect_block(inode_t *inode) {

    int block_number = data_block_alloc();

    if (block_number == -1) {
        return -1;
    }

    inode->i_block[MAX_DIRECT_BLOCKS] = block_number;
    inode->i_data_block = block_number;

    memset(data_block_get(inode->i_data_block), '\0', sizeof(data_block_get(inode->i_data_block)));

    return 0;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {

    open_file_entry_t *file = get_open_file_entry(fhandle);

    if (file == NULL) {
        return -1;
    }

    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }   

    if (len == 0) {
        printf("[ - ] Data error : Nothing to read\n");
        return -1;
    } 

    size_t to_read = inode->i_size - file->of_offset;

    if (to_read > len) {
        to_read = len;
    }

    size_t read_bytes_per_cycle = 0;
    size_t total_read = 0;

    size_t current_block = (file->of_offset / BLOCK_SIZE) + 1;     // os blocos começam no numero 1
    size_t block_offset = file->of_offset % BLOCK_SIZE;
    
    if (file->of_offset + to_read <= MAX_BYTES_DIRECT_DATA) {

        // ---------------------------------------- CASO DIRETO--------------------------------------------

        while (to_read != 0 && current_block <= MAX_DIRECT_BLOCKS) {        

            current_block = (file->of_offset / BLOCK_SIZE) + 1;     // os blocos de escrita de dados começam no numero 1
            block_offset = file->of_offset % BLOCK_SIZE;

            void *block = data_block_get((int) current_block);

            if (block == NULL) {
                return -1;
            }

            if (to_read + block_offset > BLOCK_SIZE ) { 
                read_bytes_per_cycle = BLOCK_SIZE - block_offset;

                memcpy(buffer + total_read, block + block_offset, read_bytes_per_cycle);

                to_read -= read_bytes_per_cycle;
                file->of_offset += read_bytes_per_cycle;
                total_read += read_bytes_per_cycle;

                //char *b = (char *)buffer;
               // b[total_read] = '\0';


            } else {

                memcpy(buffer + total_read, block + block_offset, to_read);
           
                file->of_offset += to_read;
                total_read += to_read;
                to_read = 0;

                //char *b = (char *)buffer;
               // b[total_read] = '\0';

            }
        }
    }

    // ---------------------------------------- CASO INDIRETO -------------------------------------------

    else if (file->of_offset >= 10240) {
        while (to_read != 0) {

            current_block = (file->of_offset / BLOCK_SIZE) + 2;     // os blocos indiretos começam no numero 12
            block_offset = file->of_offset % BLOCK_SIZE;

            void *block = data_block_get((int) current_block);
            if (block == NULL) {
                return -1;
            }

            if (to_read + block_offset > BLOCK_SIZE ) { 

                read_bytes_per_cycle = BLOCK_SIZE - block_offset;

                memcpy(buffer + total_read, block + block_offset, read_bytes_per_cycle);

                to_read -= read_bytes_per_cycle;
                file->of_offset += read_bytes_per_cycle;
                total_read += read_bytes_per_cycle;

                //char *b = (char *)buffer;
               // b[total_read] = '\0';


            } else {

                memcpy(buffer + total_read, block + block_offset, to_read);
           
                file->of_offset += to_read;
                total_read += to_read;
                to_read = 0;

                //char *b = (char *)buffer;
               // b[total_read] = '\0';

            }
        }
    }

    // ---------------------------------------- CASO MISTO --------------------------------------------

    else {

        size_t bytes_to_read_in_direct_region = 0;

        if (to_read + file->of_offset > MAX_DIRECT_DATA_SIZE) {
            bytes_to_read_in_direct_region = MAX_DIRECT_DATA_SIZE - file->of_offset;
        }

        to_read -= bytes_to_read_in_direct_region;

        while (bytes_to_read_in_direct_region != 0 && current_block < MAX_DIRECT_BLOCKS + 1) {

            current_block = (file->of_offset / BLOCK_SIZE) + 1;     // os blocos de escrita de dados começam no numero 1
            block_offset = file->of_offset % BLOCK_SIZE;

            void *block = data_block_get((int) current_block);

            if (block == NULL) {
                return -1;
            }


           if (bytes_to_read_in_direct_region + block_offset > BLOCK_SIZE) {          // vai ultrapassar o bloco

                read_bytes_per_cycle = BLOCK_SIZE - block_offset;

                memcpy(buffer + total_read, block + block_offset, read_bytes_per_cycle); 

                bytes_to_read_in_direct_region -= read_bytes_per_cycle;
                file->of_offset += read_bytes_per_cycle;
                total_read += read_bytes_per_cycle;

                //char *b = (char *)buffer;
               // b[total_read] = '\0';


            } else {

                memcpy(buffer + total_read, block + block_offset, bytes_to_read_in_direct_region);
           
                file->of_offset += bytes_to_read_in_direct_region;
                total_read += bytes_to_read_in_direct_region;
                bytes_to_read_in_direct_region = 0;

                //char *b = (char *)buffer;
               // b[total_read] = '\0';
            }
        }

        while (to_read != 0) {

            current_block = (file->of_offset / BLOCK_SIZE) + 2;     // os blocos indiretos começam no numero 12
            block_offset = file->of_offset % BLOCK_SIZE;

            read_bytes_per_cycle = 0;

            void *block = data_block_get((int) current_block);
            if (block == NULL) {
                return -1;
            }

            if (to_read + block_offset > BLOCK_SIZE ) {          // vai ultrapassar o bloco

                read_bytes_per_cycle = BLOCK_SIZE - block_offset;

                memcpy(buffer + total_read, block + block_offset, read_bytes_per_cycle);

                to_read -= read_bytes_per_cycle;
                file->of_offset += read_bytes_per_cycle;
                total_read += read_bytes_per_cycle;

               
                //char *b = (char *)buffer;
               // b[total_read] = '\0';
             

            } else {

                memcpy(buffer + total_read, block + block_offset, to_read);
           
                file->of_offset += to_read;
                total_read += to_read;
                to_read = 0;

                //char *b = (char *)buffer;
               // b[total_read] = '\0';

            }
        }

    }
    

    //printf("[ tfs_read ] Total read = %ld\n", total_read);
    
    return (ssize_t)total_read;
}




int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {

    char buffer[100];
    int source_file;
    FILE *dest_file;
    ssize_t read_bytes = 0;
    ssize_t read_bytes_per_reading = 0;
    size_t to_write_bytes = 0;
    size_t written_bytes = 0;

    memset(buffer, '\0', sizeof(buffer));

    if (tfs_lookup(source_path) == -1) {
        printf("[ operations.c ] Error : Source file doesn't exist!\n");
        return -1;
    }
    else {
        source_file = tfs_open(source_path, TFS_O_APPEND);
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

    file->of_offset = 0;

    ssize_t total_size_to_read = (ssize_t) inode->i_size;

    do {

        // se o que falta ler for maior do que o buffer, so leio o buffer
        if (total_size_to_read - read_bytes >= sizeof(buffer)) {
            read_bytes_per_reading = tfs_read(source_file, buffer, sizeof(buffer));
        }

        // se o tamanho total menos o que já foi lido for menor do que o buffer, ou seja, o que falta ler for menor, leio isso
        else {
            size_t r = (size_t) (total_size_to_read - read_bytes);
            read_bytes_per_reading = tfs_read(source_file, buffer, r);
        }

        if (read_bytes_per_reading < 0) {
            printf("[-] Read error: %s\n", strerror(errno));
		    return -1;
        }

        read_bytes += read_bytes_per_reading;

        to_write_bytes = (size_t) read_bytes_per_reading;   // since the check for negative values was made before, casting is safe

        written_bytes = fwrite(buffer, sizeof(char), to_write_bytes, dest_file);

        if (written_bytes != read_bytes_per_reading) {
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
