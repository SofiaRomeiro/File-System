#include "state.h"

/* Persistent FS state  (in reality, it should be maintained in secondary
 * memory; for simplicity, this project maintains it in primary memory) */

/* I-node table - It can be a struct */
//static inode_t inode_table[INODE_TABLE_SIZE];
//static allocation_state_t freeinode_ts[INODE_TABLE_SIZE];

typedef struct {
    inode_t inode_table[INODE_TABLE_SIZE];
    allocation_state_t freeinode_ts[INODE_TABLE_SIZE];
    pthread_mutex_t inode_table_mutex;
    pthread_rwlock_t inode_table_rwlock;
} inode_table_t;

static inode_table_t inode_table_s;

/* Data blocks - It can be a struct */
//static allocation_state_t fs_data[BLOCK_SIZE * DATA_BLOCKS];
//static allocation_state_t free_blocks[DATA_BLOCKS];

typedef struct {
    allocation_state_t fs_data[BLOCK_SIZE * DATA_BLOCKS];
    allocation_state_t free_blocks[DATA_BLOCKS];
    pthread_mutex_t data_blocks_mutex;
} data_blocks_t;

static data_blocks_t data_blocks_s;

/* Volatile FS state */

//static open_file_entry_t open_file_table[MAX_OPEN_FILES];
//static char free_open_file_entries[MAX_OPEN_FILES];

typedef struct {
    open_file_entry_t open_file_table[MAX_OPEN_FILES];
    char free_open_file_entries[MAX_OPEN_FILES]; 
    pthread_mutex_t fs_state_mutex;  
} fs_state_t;

static fs_state_t fs_state_s;

static inline bool valid_inumber(int inumber) {
    return inumber >= 0 && inumber < INODE_TABLE_SIZE;
}

static inline bool valid_block_number(int block_number) {
    return block_number >= 0 && block_number < DATA_BLOCKS;
}

static inline bool valid_file_handle(int file_handle) {
    return file_handle >= 0 && file_handle < MAX_OPEN_FILES;
}

/**
 * We need to defeat the optimizer for the insert_delay() function.
 * Under optimization, the empty loop would be completely optimized away.
 * This function tells the compiler that the assembly code being run (which is
 * none) might potentially change *all memory in the process*.
 *
 * This prevents the optimizer from optimizing this code away, because it does
 * not know what it does and it may have side effects.
 *
 * Reference with more information: https://youtu.be/nXaxk27zwlk?t=2775
 *
 * Exercise: try removing this function and look at the assembly generated to
 * compare.
 */
static void touch_all_memory() { __asm volatile("" : : : "memory"); }

/*
 * Auxiliary function to insert a delay.
 * Used in accesses to persistent FS state as a way of emulating access
 * latencies as if such data structures were really stored in secondary memory.
 */
static void insert_delay() {
    for (int i = 0; i < DELAY; i++) {
        touch_all_memory();
    }
}

/*
 * Initializes FS state
 */
// no need to be thread safe since it's called only once before threading 
void state_init() {

    for (size_t i = 0; i < INODE_TABLE_SIZE; i++) {
        inode_table_s.freeinode_ts[i] = FREE;
    }

    for (size_t i = 0; i < DATA_BLOCKS; i++) {
        data_blocks_s.free_blocks[i] = FREE;
    }

    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        fs_state_s.free_open_file_entries[i] = FREE;
    }
}

// mutexes and rwlocks created to static variables should only be destroyed here
void state_destroy() { /* nothing to do */ 
}

/*
 * Creates a new i-node in the i-node table.
 * Input:
 *  - n_type: the type of the node (file or directory)
 * Returns:
 *  new i-node's number if successfully created, -1 otherwise
 */

int inode_create(inode_type n_type) {
    for (int inumber = 0; inumber < INODE_TABLE_SIZE; inumber++) {
        if ((inumber * (int) sizeof(allocation_state_t) % BLOCK_SIZE) == 0) {
            insert_delay(); // simulate storage access delay (to freeinode_ts)
        }

// ----------------------------------- CRIT SPOT - MUTEX -----------------------------------------

        pthread_mutex_lock(&inode_table_s.inode_table_mutex);

        // Finds first free entry in i-node table 
        if (inode_table_s.freeinode_ts[inumber] == FREE) {
            // Found a free entry, so takes it for the new i-node
            inode_table_s.freeinode_ts[inumber] = TAKEN;

            insert_delay(); // simulate storage access delay (to i-node)

            inode_t *local_inode = &(inode_table_s.inode_table[inumber]);

            pthread_mutex_unlock(&inode_table_s.inode_table_mutex);


// --------------------------------- END CRIT SPOT ---------------------------------------

            local_inode->i_node_type = n_type;

            if (n_type == T_DIRECTORY) {
                // Initializes directory (filling its block with empty
                // entries, labeled with inumber==-1) 
                int b = data_block_alloc();
                if (b == -1 && ((dir_entry_t *)data_block_get(b)) == NULL) {

// ----------------------------------- CRIT SPOT - RWLOCK W -----------------------------------------

                    pthread_rwlock_wrlock(&inode_table_s.inode_table_rwlock);

                    inode_table_s.freeinode_ts[inumber] = FREE;

                    pthread_rwlock_unlock(&inode_table_s.inode_table_rwlock);

// --------------------------------- END CRIT SPOT ---------------------------------------

                    return -1;
                }

                local_inode->i_size = BLOCK_SIZE;
                local_inode->i_data_block = b;

                memset(local_inode->i_block, -1, sizeof(local_inode->i_block));

                dir_entry_t *dir_entry = (dir_entry_t *)data_block_get(b);

                /*
                if (dir_entry == NULL) {

// ----------------------------------- CRIT SPOT - RWLOCK W -----------------------------------------

                    pthread_rwlock_wrlock(&inode_table_s.inode_table_rwlock);

                    inode_table_s.freeinode_ts[inumber] = FREE;

                    pthread_rwlock_unlock(&inode_table_s.inode_table_rwlock);


// --------------------------------- END CRIT SPOT ---------------------------------------

                    return -1;
                }*/

                for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
                    dir_entry[i].d_inumber = -1;
                }
            } else {
                // In case of a new file, simply sets its size to 0 
                local_inode->i_size = 0;
                local_inode->i_data_block = -1;
            }

            return inumber;
        }

        // unlock lock's if
        pthread_mutex_unlock(&inode_table_s.inode_table_mutex);

    }
    return -1;
}


/*
 * Deletes the i-node.
 * Input:
 *  - inumber: i-node's number
 * Returns: 0 if successful, -1 if failed
 */
int inode_delete(int inumber) {
    // simulate storage access delay (to i-node and freeinode_ts)
    insert_delay();
    insert_delay();
// ----------------------------------- CRIT SPOT - MUTEX -----------------------------------------

    if (!valid_inumber(inumber) || inode_table_s.freeinode_ts[inumber] == FREE) {
        return -1;
    }

    inode_table_s.freeinode_ts[inumber] = FREE;

    inode_t local_inode = inode_table_s.inode_table[inumber];

// --------------------------------- END CRIT SPOT ---------------------------------------

    if (local_inode.i_size > 0) {
        if (data_block_free(local_inode.i_data_block) == -1) {
            return -1;
        }
    }


    return 0;
}

/*
 * Returns a pointer to an existing i-node.
 * Input:
 *  - inumber: identifier of the i-node
 * Returns: pointer if successful, NULL if failed
 */
inode_t *inode_get(int inumber) {
    if (!valid_inumber(inumber)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to i-node

// ----------------------------------- CRIT SPOT - RWLOCK R -----------------------------------------

    inode_t *address = &(inode_table_s.inode_table[inumber]);

// --------------------------------- END CRIT SPOT ---------------------------------------

    return address;
}

/*
 * Adds an entry to the i-node directory data.
 * Input:
 *  - inumber: identifier of the i-node
 *  - sub_inumber: identifier of the sub i-node entry
 *  - sub_name: name of the sub i-node entry
 * Returns: SUCCESS or FAIL
 */
int add_dir_entry(int inumber, int sub_inumber, char const *sub_name) {
    if (!valid_inumber(inumber) || !valid_inumber(sub_inumber)) {
        return -1;
    }

// ----------------------------------- CRIT SPOT - RWLOCK R -----------------------------------------

    inode_t local_inode = inode_table_s.inode_table[inumber];

// --------------------------------- END CRIT SPOT ---------------------------------------

    insert_delay(); // simulate storage access delay to i-node with inumber
    if (local_inode.i_node_type != T_DIRECTORY) {
        return -1;
    }

    if (strlen(sub_name) == 0) {
        return -1;
    }

    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(local_inode.i_data_block);

    if (dir_entry == NULL) {
        return -1;
    }

    /* Finds and fills the first empty entry */
    for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {

        if (dir_entry[i].d_inumber == -1) {

            dir_entry[i].d_inumber = sub_inumber;

            strncpy(dir_entry[i].d_name, sub_name, MAX_FILE_NAME - 1);

            dir_entry[i].d_name[MAX_FILE_NAME - 1] = 0;

            return 0;
        }
    }

    return -1;
}

/* Looks for a given name inside a directory
 * Input:
 * 	- parent directory's i-node number
 * 	- name to search
 * 	Returns i-number linked to the target name, -1 if not found
 */
int find_in_dir(int inumber, char const *sub_name) {
    insert_delay(); // simulate storage access delay to i-node with inumber

// ----------------------------------- CRIT SPOT - RWLOCK R -----------------------------------------

    if (!valid_inumber(inumber) ||
        inode_table_s.inode_table[inumber].i_node_type != T_DIRECTORY) {
        return -1;
    }

// --------------------------------- END CRIT SPOT ---------------------------------------


    /* Locates the block containing the DIRECTORY's entries */
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(inode_table_s.inode_table[inumber].i_data_block);

    if (dir_entry == NULL) {
        return -1;
    }

    /* Iterates over the directory entries looking for one that has the target
     * name */

    for (int i = 0; i < MAX_DIR_ENTRIES; i++)
        if ((dir_entry[i].d_inumber != -1) &&
            (strncmp(dir_entry[i].d_name, sub_name, MAX_FILE_NAME) == 0)) {
            return dir_entry[i].d_inumber;
        }

    return -1;
}

/*
 * Allocated a new data block
 * Returns: block index if successful, -1 otherwise
 */
int data_block_alloc() {

// ----------------------------------- CRIT SPOT - MUTEX -----------------------------------------

    for (int i = 0; i < DATA_BLOCKS; i++) {
        if (i * (int) sizeof(allocation_state_t) % BLOCK_SIZE == 0) {
            insert_delay(); // simulate storage access delay to free_blocks
        }

        if (data_blocks_s.free_blocks[i] == FREE) {
            data_blocks_s.free_blocks[i] = TAKEN;

// --------------------------------- END CRIT SPOT (if) ---------------------------------------

            return i;
        }
    }

// --------------------------------- END CRIT SPOT (!if) ---------------------------------------

    return -1;
}

/* Frees a data block
 * Input
 * 	- the block index
 * Returns: 0 if success, -1 otherwise
 */
int data_block_free(int block_number) {
    if (!valid_block_number(block_number)) {
        return -1;
    }

    insert_delay(); // simulate storage access delay to free_blocks

// ----------------------------------- CRIT SPOT - RWLOCK W -----------------------------------------

    data_blocks_s.free_blocks[block_number] = FREE;

// --------------------------------- END CRIT SPOT ---------------------------------------

    return 0;
}

/* Returns a pointer to the contents of a given block
 * Input:
 * 	- Block's index
 * Returns: pointer to the first byte of the block, NULL otherwise
 */
void *data_block_get(int block_number) {
    if (!valid_block_number(block_number)) {
        return NULL;
    }

    insert_delay(); // simulate storage access delay to block

// ----------------------------------- CRIT SPOT - RWLOCK R -----------------------------------------

    allocation_state_t *local_state = &(data_blocks_s.fs_data[block_number * BLOCK_SIZE]);

// --------------------------------- END CRIT SPOT ---------------------------------------

    return local_state;
}

/* Add new entry to the open file table
 * Inputs:
 * 	- I-node number of the file to open
 * 	- Initial offset
 * Returns: file handle if successful, -1 otherwise
 */
int add_to_open_file_table(int inumber, size_t offset) {

// ----------------------------------- CRIT SPOT - MUTEX -----------------------------------------

    for (int i = 0; i < MAX_OPEN_FILES; i++) {

        if (fs_state_s.free_open_file_entries[i] == FREE) {

            fs_state_s.free_open_file_entries[i] = TAKEN;

            fs_state_s.open_file_table[i].of_inumber = inumber;
            fs_state_s.open_file_table[i].of_offset = offset;

// --------------------------------- END CRIT SPOT ---------------------------------------

            return i;
        }
    }

// --------------------------------- END CRIT SPOT ---------------------------------------

    return -1;
}

/* Frees an entry from the open file table
 * Inputs:
 * 	- file handle to free/close
 * Returns 0 is success, -1 otherwise
 */
int remove_from_open_file_table(int fhandle) {

// ----------------------------------- CRIT SPOT - MUTEX -----------------------------------------

    if (!valid_file_handle(fhandle) ||
        fs_state_s.free_open_file_entries[fhandle] != TAKEN) {

// --------------------------------- END CRIT SPOT (if)---------------------------------------

        return -1;
    }

    fs_state_s.free_open_file_entries[fhandle] = FREE;

// --------------------------------- END CRIT SPOT (!if)---------------------------------------

    return 0;
}

/* Returns pointer to a given entry in the open file table
 * Inputs:
 * 	 - file handle
 * Returns: pointer to the entry if sucessful, NULL otherwise
 */
open_file_entry_t *get_open_file_entry(int fhandle) {
    if (!valid_file_handle(fhandle)) {
        return NULL;
    }

// ----------------------------------- CRIT SPOT - RWLOCK R -----------------------------------------

    open_file_entry_t *local_file = &(fs_state_s.open_file_table[fhandle]);

// --------------------------------- END CRIT SPOT ---------------------------------------

    return local_file;
}
