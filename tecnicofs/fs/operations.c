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

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /* Determine how many bytes to write */
    if (to_write + file->of_offset > BLOCK_SIZE) {
        to_write = BLOCK_SIZE - file->of_offset;
    }

    if (to_write > 0) {
        if (inode->i_size == 0) {
            /* If empty file, allocate new block */
            inode->i_data_block = data_block_alloc();
        }

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

    return (ssize_t)to_write;
}


ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        return -1;
    }

    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (file->of_offset + to_read >= BLOCK_SIZE) {
        return -1;
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

    return (ssize_t)to_read;
}

void strcopy(char *dest, char const *src, int to_copy) {

    int i=0;

    for (i = 0; i < to_copy; i++) {
        dest[i] = src[i];
    }

    dest[i] = '\0';
}



int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {

    char *buffer_to_write;

    // CREATE BUFFER
        // MAX SIZE => (BLOCK_SIZE * EACH ENTRY (total of 10))+ (BLOCK_SIZE * ref_indir * sizeof(char))

    char buffer[(BLOCK_SIZE * 10) + (BLOCK_SIZE * INODE_TABLE_SIZE * sizeof(char))];

    // OPEN FILE TO READ
    // TODO check for errors 

    int source_file = tfs_open(source_path, TFS_O_CREAT);

    if (source_file < 0) {
        printf("[ op.h ] source_file = %d\n", source_file);
        printf("[-] Open error in src: %s\n", strerror(errno));
		return -1;
    }
    
    memset(buffer, 0, sizeof(buffer));

    // READ FILE
    // TODO check for errors

    ssize_t read_size = tfs_read(source_file, buffer, sizeof(buffer));  

    if (read_size == -1) {
        printf("[-] Read error: %s\n", strerror(errno));
		return -1;
    }           

    buffer_to_write = (void *) malloc(sizeof(buffer[0]) * read_size);

    strcopy(buffer_to_write, buffer, read_size);

    memset(buffer, '\0', sizeof(buffer));    

    if (sizeof(buffer_to_write) < 0) {
        printf("[-] Buffer error : Not writing : %s\n", strerror(errno));
        return -1;
    }

    if (strlen(buffer_to_write) != read_size) {
        printf("[-] Buffer error : Incomplete writing : %s\n", strerror(errno));
        return -1;
    }

    // OPEN FILE TO WRITE
    // TODO check for errors

    FILE *wfp = fopen(dest_path, "w");

    if (wfp == NULL) {
        printf("[-] Open error: %s\n", strerror(errno));
		return -1;
    }

    // WRITE IN EXTERNAL FILE
    // TODO check for errors    

    ssize_t write_size = fwrite(buffer_to_write, sizeof(char), strlen(buffer_to_write), wfp);

    if (write_size < 0) {
        printf("[-] Write error: %s\n", strerror(errno));
		return -1;
    } 

    // CLOSE BOTH FILES
    // TODO check for errors

    int close_status_source =  tfs_close(source_file);
    int close_status_dest = fclose(wfp);

    if (close_status_dest < 0 || close_status_source < 0) {
        printf("[-] Close error: %s\n", strerror(errno));
		return -1;
    }

    return 0;
}

int main() {

    tfs_init();

    char *src = "/f1";
    char *dest = "/home/sofia/Documentos/File-System/tecnicofs/fs/txtout.txt";
    char *str = "Os Lusiadas e uma obra de poesia épica do escritor português Luís Vaz de Camões, a primeira epopeia portuguesa publicada em versão impressa.\n";

    int f = 0;

    f = tfs_open(src, TFS_O_CREAT);

    tfs_write(f, str, strlen(str));

    tfs_close(f);

    tfs_copy_to_external_fs(src, dest); 

    return 0;
}
