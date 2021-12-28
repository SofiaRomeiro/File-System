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

    printf("[ tfs write ] inode-> i_size = %ld\n", inode->i_size);

    return (ssize_t)to_write;
}

int tfs_write_direct_region(inode_t *inode, open_file_entry_t *file, void const *buffer, size_t write_size) {

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

    return 0;
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

int tfs_write_indirect_region(inode_t *inode, open_file_entry_t *file, void const *buffer, size_t write_size) {

    int i;

    int *blocks_arr = inode->i_block;

    for (i = 10; blocks_arr[i] != '\0' && i < MAX_DATA_BLOCKS_FOR_INODE; i++);

    if (i == MAX_DATA_BLOCKS_FOR_INODE) {
        printf("[ direct_block_insert ] Error : Reached indirect region\n");
        return -1;
    }    

    tfs_handle_indirect_block(file, inode, write_size, buffer, i);
    
    return 0;
}

ssize_t tfs_handle_indirect_block(open_file_entry_t *file, inode_t *inode, size_t write_size, void const *buffer, int i) {

    if (inode->indirect_block_index == BLOCK_SIZE) {                                                             
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
    return (ssize_t)write_size;
}

int indirect_block_insert(inode_t *inode, int i) {
    inode->i_data_block = data_block_alloc();
    inode->i_block[i] = inode->i_data_block;
    inode->indirect_block_index = 0;
    int target_data_block = data_block_alloc();
    void *block = data_block_get(target_data_block);
    memcpy(block + inode->indirect_block_index, block, sizeof(target_data_block));
    inode->indirect_block_index++;
    inode->i_data_block = target_data_block;
    return 0;
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
        to_read = (inode->i_size - 1024);
    }   

    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {

        file->of_offset = 0;

        void *block = data_block_get(inode->i_block[0]);
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
