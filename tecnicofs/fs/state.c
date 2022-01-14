#include "state.h"

#define FIRST_INDIRECT_BLOCK (12)
#define REFERENCE_BLOCK_INDEX (11)

/* Persistent FS state  (in reality, it should be maintained in secondary
 * memory; for simplicity, this project maintains it in primary memory) */

pthread_mutex_t global_mutex;

/* I-node table */
typedef struct {
    inode_t inode_table[INODE_TABLE_SIZE];
    allocation_state_t freeinode_ts[INODE_TABLE_SIZE];
    // mutexes array to match freeinode_ts
    pthread_mutex_t inode_table_mutex;
    pthread_rwlock_t inode_table_rwlock;
} inode_table_t;

static inode_table_t inode_table_s;

typedef struct {
    allocation_state_t fs_data[BLOCK_SIZE * DATA_BLOCKS];
    allocation_state_t free_blocks[DATA_BLOCKS];
    pthread_mutex_t data_blocks_mutex;
} data_blocks_t;

static data_blocks_t data_blocks_s;

/* Volatile FS state */

typedef struct {
    open_file_entry_t open_file_table[MAX_OPEN_FILES];
    char free_open_file_entries[MAX_OPEN_FILES]; 
    pthread_mutex_t fs_state_mutex; 
    pthread_rwlock_t fs_state_rwlock; 
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

    pthread_mutex_init(&global_mutex, NULL);

    for (size_t i = 0; i < INODE_TABLE_SIZE; i++) {
        inode_table_s.freeinode_ts[i] = FREE;
        pthread_mutex_init(&(inode_table_s.inode_table[i].inode_mutex), NULL);
        pthread_rwlock_init(&(inode_table_s.inode_table[i].inode_rwlock), NULL);
    }

    for (size_t i = 0; i < DATA_BLOCKS; i++) {
        data_blocks_s.free_blocks[i] = FREE;
    }

    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        fs_state_s.free_open_file_entries[i] = FREE;
        pthread_mutex_init(&(fs_state_s.open_file_table[i].open_file_mutex), NULL);
        pthread_rwlock_init(&(fs_state_s.open_file_table[i].open_file_rwlock), NULL);
    }
}

// mutexes and rwlocks created to static variables should only be destroyed here
void state_destroy() { /* nothing to do */ 

    pthread_mutex_destroy(&global_mutex);

    for (size_t i = 0; i < INODE_TABLE_SIZE; i++) {
        pthread_mutex_destroy(&(inode_table_s.inode_table[i].inode_mutex));
        pthread_rwlock_destroy(&(inode_table_s.inode_table[i].inode_rwlock));
    }

    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        pthread_mutex_destroy(&(fs_state_s.open_file_table[i].open_file_mutex));
        pthread_rwlock_destroy(&(fs_state_s.open_file_table[i].open_file_rwlock));

    }
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

            local_inode->i_node_type = n_type;

            /*
                NOTA 
                Este if apenas uma vez por cada programa cliente, ou seja, no tfs_init()
                Logo, pensamos que nao vale a pena ter mutexes nesta zona, visto que não há possibilidade de acesso concorrente
            */

           pthread_mutex_unlock(&inode_table_s.inode_table_mutex);

            if (n_type == T_DIRECTORY) {
                // Initializes directory (filling its block with empty
                // entries, labeled with inumber==-1) 
                int b = data_block_alloc();
                if (b == -1 && ((dir_entry_t *)data_block_get(b)) == NULL) {
                    inode_table_s.freeinode_ts[inumber] = FREE;
                    return -1;
                }

                // NOTA : pensamos que aqui também não é necessário proteger o inode pela mesma razão que não é necessário proteger esta secção de código (if)

                local_inode->i_size = BLOCK_SIZE;
                local_inode->i_data_block = b;
                memset(local_inode->i_block, -1, sizeof(local_inode->i_block));

                dir_entry_t *dir_entry = (dir_entry_t *)data_block_get(b);

                for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {
                    dir_entry[i].d_inumber = -1;
                }
            } else {
                // In case of a new file, simply sets its size to 0 
                pthread_rwlock_wrlock(&(local_inode->inode_rwlock));

                local_inode->i_size = 0;
                local_inode->i_data_block = -1;
                memset(local_inode->i_block, -1, sizeof(local_inode->i_block));

                pthread_rwlock_unlock(&(local_inode->inode_rwlock));
            }


            return inumber;
        }

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

    /*
        NOTA 
        Esta seccao precisa de proteção pois pode aceder a inodes que nao representem a root
    */

    pthread_mutex_lock(&(inode_table_s.inode_table_mutex));

    if (!valid_inumber(inumber) || inode_table_s.freeinode_ts[inumber] == FREE) {
        pthread_mutex_unlock(&(inode_table_s.inode_table_mutex));
        return -1;
    }

    inode_table_s.freeinode_ts[inumber] = FREE;

    inode_t *local_inode = &inode_table_s.inode_table[inumber];

    if (local_inode->i_size > 0 && (data_block_free(local_inode->i_data_block) == -1)) {
        pthread_mutex_unlock(&(inode_table_s.inode_table_mutex));
        return -1;
    }

    pthread_mutex_unlock(&(inode_table_s.inode_table_mutex));

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

    /*
        NOTA
        Como proteger adequadamente aqui? Visto que entre a "leitura" e o "return" pode haver uma mudança
        Não pensamos que esta seja a melhor solução mas nao estamos a ver outra que não conduza a bloqueios
    */ 

    insert_delay(); // simulate storage access delay to i-node

    pthread_rwlock_rdlock(&(inode_table_s.inode_table_rwlock));

    inode_t *address = &(inode_table_s.inode_table[inumber]);

    pthread_rwlock_unlock(&(inode_table_s.inode_table_rwlock));

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

    /*
        NOTA
        Como estamos apenas a focar-nos em operações com um determinado inode, o da posicao inumber, podemos trancar a posicao 
        para leitura e logo de seguida usar o trinco do inode para libertar por completo a tabela de inodes para outras threads acederem?
    */

    pthread_rwlock_rdlock(&(inode_table_s.inode_table_rwlock));

    inode_t *local_inode = &(inode_table_s.inode_table[inumber]);

    insert_delay(); // simulate storage access delay to i-node with inumber
    if (local_inode->i_node_type != T_DIRECTORY) {
        return -1;
    }

    if (strlen(sub_name) == 0) {
        return -1;
    }

    /* Locates the block containing the directory's entries */
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(local_inode->i_data_block);

    pthread_rwlock_unlock(&(inode_table_s.inode_table_rwlock));

    /*
        NOTA
        Dado o tamanho da secção critica, o custo de um lock e um unlock justifica trancar e destrancar duas vezes ou ter esta secção critica "grande"?
        (linhas 275 a 292)
    */

    if (dir_entry == NULL) {
        return -1;
    }

    /* Finds and fills the first empty entry */
    for (size_t i = 0; i < MAX_DIR_ENTRIES; i++) {

        pthread_mutex_lock(&(fs_state_s.fs_state_mutex));

        if (dir_entry[i].d_inumber == -1) {

            dir_entry[i].d_inumber = sub_inumber;

            strncpy(dir_entry[i].d_name, sub_name, MAX_FILE_NAME - 1);

            dir_entry[i].d_name[MAX_FILE_NAME - 1] = 0;

            pthread_mutex_unlock(&(fs_state_s.fs_state_mutex));

            return 0;
        }

        pthread_mutex_unlock(&(fs_state_s.fs_state_mutex));
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

    pthread_rwlock_rdlock(&(inode_table_s.inode_table_rwlock));

    if (!valid_inumber(inumber) ||
        inode_table_s.inode_table[inumber].i_node_type != T_DIRECTORY) {
        return -1;
    }

    /* Locates the block containing the DIRECTORY's entries */
    dir_entry_t *dir_entry =
        (dir_entry_t *)data_block_get(inode_table_s.inode_table[inumber].i_data_block);

        pthread_rwlock_unlock(&(inode_table_s.inode_table_rwlock));

    if (dir_entry == NULL) {
        return -1;
    }

    /* Iterates over the directory entries looking for one that has the target
     * name */

    for (int i = 0; i < MAX_DIR_ENTRIES; i++) {

        pthread_mutex_lock(&(fs_state_s.fs_state_mutex));

        if ((dir_entry[i].d_inumber != -1) &&
            (strncmp(dir_entry[i].d_name, sub_name, MAX_FILE_NAME) == 0)) {

            pthread_mutex_unlock(&(fs_state_s.fs_state_mutex));
            return dir_entry[i].d_inumber;
        }

        pthread_mutex_unlock(&(fs_state_s.fs_state_mutex));
    }

    return -1;
}

/*
 * Allocated a new data block
 * Returns: block index if successful, -1 otherwise
 */
// ignorar os locks?
int data_block_alloc() {

    
    for (int i = 0; i < DATA_BLOCKS; i++) {
        if (i * (int) sizeof(allocation_state_t) % BLOCK_SIZE == 0) {
            insert_delay(); // simulate storage access delay to free_blocks
        }

        pthread_mutex_lock(&(data_blocks_s.data_blocks_mutex));        

        if (data_blocks_s.free_blocks[i] == FREE) {
            data_blocks_s.free_blocks[i] = TAKEN;
        
            pthread_mutex_unlock(&(data_blocks_s.data_blocks_mutex));
            return i;
        }

        pthread_mutex_unlock(&(data_blocks_s.data_blocks_mutex));
    }
    return -1;
}

/* Frees a data block
 * Input
 * 	- the block index
 * Returns: 0 if success, -1 otherwise
 */
int data_block_free(int block_number) {
    pthread_mutex_lock(&(data_blocks_s.data_blocks_mutex));
    if (!valid_block_number(block_number)) {
        pthread_mutex_unlock(&(data_blocks_s.data_blocks_mutex));
        return -1;
    }

    insert_delay(); // simulate storage access delay to free_blocks

    pthread_rwlock_wrlock(&(fs_state_s.fs_state_rwlock)); // ?

    data_blocks_s.free_blocks[block_number] = FREE;

    pthread_rwlock_unlock(&(fs_state_s.fs_state_rwlock)); // ?

    pthread_mutex_unlock(&(data_blocks_s.data_blocks_mutex));

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

    /*
        NOTA
        Como proteger adequadamente aqui? Visto que entre a "leitura" e o "return" pode haver uma mudança
        Não pensamos que esta seja a melhor solução mas nao estamos a ver outra que não conduza a bloqueios
    */ 

    //pthread_rwlock_rdlock(&(fs_state_s.fs_state_rwlock));

    allocation_state_t *local_state = &(data_blocks_s.fs_data[block_number * BLOCK_SIZE]);

    //pthread_rwlock_unlock(&(fs_state_s.fs_state_rwlock));

    return local_state;
}

/* Add new entry to the open file table
 * Inputs:
 * 	- I-node number of the file to open
 * 	- Initial offset
 * Returns: file handle if successful, -1 otherwise
 */
int add_to_open_file_table(int inumber, size_t offset) {

    pthread_mutex_lock(&(fs_state_s.fs_state_mutex));

    for (int i = 0; i < MAX_OPEN_FILES; i++) {

        if (fs_state_s.free_open_file_entries[i] == FREE) {
            fs_state_s.free_open_file_entries[i] = TAKEN;
            fs_state_s.open_file_table[i].of_inumber = inumber;
            fs_state_s.open_file_table[i].of_offset = offset;

            pthread_mutex_unlock(&(fs_state_s.fs_state_mutex));

            return i;
        }       

    }

    pthread_mutex_unlock(&(fs_state_s.fs_state_mutex));

    return -1;
}

/* Frees an entry from the open file table
 * Inputs:
 * 	- file handle to free/close
 * Returns 0 is success, -1 otherwise
 */
int remove_from_open_file_table(int fhandle) {

    pthread_mutex_lock(&(fs_state_s.fs_state_mutex));

    if (!valid_file_handle(fhandle) ||
        fs_state_s.free_open_file_entries[fhandle] != TAKEN) {

        pthread_mutex_unlock(&(fs_state_s.fs_state_mutex));

        return -1;
    }

    fs_state_s.free_open_file_entries[fhandle] = FREE;

    pthread_mutex_unlock(&(fs_state_s.fs_state_mutex));

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

    /*
        NOTA
        Como proteger adequadamente aqui? Visto que entre a "leitura" e o "return" pode haver uma mudança
        Não pensamos que esta seja a melhor solução mas nao estamos a ver outra que não conduza a bloqueios
    */ 

    pthread_mutex_lock(&(fs_state_s.fs_state_mutex));

    open_file_entry_t *local_file = &(fs_state_s.open_file_table[fhandle]);

    pthread_mutex_unlock(&(fs_state_s.fs_state_mutex));

    return local_file;
}



// ------------------------------- AUX FUNCTIONS ---------------------------------------------

ssize_t tfs_write_direct_region(inode_t *inode, open_file_entry_t *file, void const *buffer, size_t write_size) {

    size_t bytes_written = 0;
    size_t to_write_block = 0;

// ------------------------------------------------ CRIT SPOT - RWLOCK R ----------------------------------
    /*pthread_rwlock_rdlock(&inode->inode_rwlock);
    size_t local_offset = file->of_offset;
    pthread_rwlock_unlock(&inode->inode_rwlock);

    pthread_rwlock_rdlock(&file->open_file_rwlock);
    size_t local_isize = inode->i_size;
    pthread_rwlock_unlock(&file->open_file_rwlock);
    */
// ------------------------------------------------- END CRIT SPOT ----------------------------------------
    pthread_mutex_lock(&inode->inode_mutex);
    pthread_mutex_lock(&file->open_file_mutex);

    for (int i = 0; write_size > 0 && i < REFERENCE_BLOCK_INDEX; i++) {

        if (inode->i_size % BLOCK_SIZE == 0) {                                                           
            int insert_status = direct_block_insert(inode);     
            if (insert_status == -1) {
                printf("[ tfs_write_direct_region ] Error writing in direct region: %s\n", strerror(errno));
                return -1;
            }
        }

        //pthread_rwlock_rdlock(&inode->inode_rwlock);

        void *block = data_block_get(inode->i_data_block);

        //pthread_rwlock_unlock(&inode->inode_rwlock);

        if (block == NULL) {
            return -1;
        }
        
        if (write_size >= BLOCK_SIZE || BLOCK_SIZE - (file->of_offset % BLOCK_SIZE) < write_size) {
            to_write_block = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);
            write_size -= to_write_block;

        } else  {   
            to_write_block = write_size;
            write_size = 0;
        }

        memcpy(block + (file->of_offset % BLOCK_SIZE), buffer + bytes_written, to_write_block);

        file->of_offset += to_write_block;
        inode->i_size += to_write_block;
        bytes_written += to_write_block;



    }

    pthread_mutex_unlock(&inode->inode_mutex);
    pthread_mutex_unlock(&file->open_file_mutex);

// ------------------------------------------------ CRIT SPOT - RWLOCK W ----------------------------------
    /*
    pthread_rwlock_wrlock(&inode->inode_rwlock);
    inode->i_size = local_isize;
    pthread_rwlock_unlock(&inode->inode_rwlock);
   

    pthread_rwlock_wrlock(&file->open_file_rwlock);
    file->of_offset = local_offset;
    pthread_rwlock_unlock(&file->open_file_rwlock);
    */

// ------------------------------------------------ END CRIT SPOT ----------------------------------

    return (ssize_t)bytes_written;
}

int direct_block_insert(inode_t *inode) {

// ------------------------------------------------ CRIT SPOT - MUTEX ----------------------------------  

    inode->i_data_block = data_block_alloc();

    void *block = data_block_get(inode->i_data_block);

    //pthread_mutex_lock(&inode->inode_mutex);

    if (inode->i_data_block == -1) {
        printf("[ direct_block_insert ] Error : alloc block failed\n");
        return -1;
    }

    memset(block, -1, BLOCK_SIZE);

    inode->i_block[inode->i_data_block - 1] = inode->i_data_block;

    //pthread_mutex_unlock(&inode->inode_mutex);

// ------------------------------------------------ END CRIT SPOT ----------------------------------

    return 0;
}

ssize_t tfs_write_indirect_region(inode_t *inode, open_file_entry_t *file, void const *buffer, size_t write_size) {

    size_t bytes_written = 0;
    size_t to_write_block = 0;
    int insert_status = 0;

// ------------------------------------------------ CRIT SPOT - RWLOCK R ----------------------------------

/*
    pthread_rwlock_rdlock(&inode->inode_rwlock);
    size_t local_offset = file->of_offset;
    pthread_rwlock_unlock(&inode->inode_rwlock);

    pthread_rwlock_rdlock(&file->open_file_rwlock);
    size_t local_isize = inode->i_size;
    pthread_rwlock_unlock(&file->open_file_rwlock);

*/

// ------------------------------------------------- END CRIT SPOT ----------------------------------------
    pthread_mutex_lock(&inode->inode_mutex);
    pthread_mutex_lock(&file->open_file_mutex);

    for (int i = 0; write_size > 0; i++) {

        if (inode->i_size + write_size > MAX_BYTES) {
            write_size = MAX_BYTES - inode->i_size;
        }

        if (inode->i_size % BLOCK_SIZE == 0) { 

            insert_status = indirect_block_insert(inode);  

            if (insert_status == -1) {
                printf("[ tfs_write_indirect_region ] Error writing in indirect region: %s\n", strerror(errno));
                return -1;
            }
        }

        //pthread_rwlock_rdlock(&inode->inode_rwlock);

        void *block = data_block_get(inode->i_data_block);

        //pthread_rwlock_unlock(&inode->inode_rwlock);

        if (block == NULL) {
            printf("[ tfs_write_indirect_region ] Error : NULL block\n");
            return -1;
        }
        
        if (write_size >= BLOCK_SIZE || BLOCK_SIZE - (file->of_offset % BLOCK_SIZE) < write_size) {
            to_write_block = BLOCK_SIZE - (file->of_offset % BLOCK_SIZE);           
            write_size -= to_write_block;
        }

        else  {
            to_write_block = write_size;
            write_size = 0;
        }

        //pthread_rwlock_rdlock(&inode->inode_rwlock);

        memcpy(block + (file->of_offset % BLOCK_SIZE), buffer + bytes_written, to_write_block);

        //pthread_rwlock_unlock(&inode->inode_rwlock);

        file->of_offset += to_write_block;
        inode->i_size += to_write_block;
        bytes_written += to_write_block;

    
    }

    pthread_mutex_unlock(&file->open_file_mutex);
    pthread_mutex_unlock(&inode->inode_mutex);

// ------------------------------------------------ CRIT SPOT - RWLOCK W ----------------------------------

    /*
    pthread_rwlock_wrlock(&inode->inode_rwlock);
    inode->i_size = local_isize;
    pthread_rwlock_unlock(&inode->inode_rwlock);
   

    pthread_rwlock_wrlock(&file->open_file_rwlock);
    file->of_offset = local_offset;
    pthread_rwlock_unlock(&file->open_file_rwlock);
    */

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

    //pthread_mutex_lock(&inode->inode_mutex);

    inode->i_data_block = block_number;

    memset(data_block_get(block_number), -1, BLOCK_SIZE / sizeof(int));

    last_i_block[block_number - FIRST_INDIRECT_BLOCK] = block_number;  

    //pthread_mutex_unlock(&inode->inode_mutex);
  

// ------------------------------------------------ END CRIT SPOT ----------------------------------

    return 0;

}

int tfs_handle_indirect_block(inode_t *inode) {

// ------------------------------------------------ CRIT SPOT - MUTEX ----------------------------------
    
    int block_number = data_block_alloc();

    if (block_number == -1) {
        return -1;
    }

    pthread_mutex_lock(&inode->inode_mutex);

    inode->i_block[MAX_DIRECT_BLOCKS] = block_number;
    inode->i_data_block = block_number;

    memset(data_block_get(inode->i_data_block), -1, sizeof(data_block_get(inode->i_data_block)));

    pthread_mutex_unlock(&inode->inode_mutex);

// ------------------------------------------------ END CRIT SPOT ----------------------------------

    return 0;
}

ssize_t tfs_read_direct_region(open_file_entry_t *file, size_t to_read, void *buffer) {

// ------------------------------------------------ CRIT SPOT - RWLOCK R ----------------------------------

    pthread_rwlock_rdlock(&file->open_file_rwlock);

    size_t local_offset = file->of_offset;

    pthread_rwlock_unlock(&file->open_file_rwlock);

// ------------------------------------------------ END CRIT SPOT ----------------------------------


    size_t current_block = (local_offset / BLOCK_SIZE) + 1;
    size_t block_offset = local_offset % BLOCK_SIZE;
    size_t to_read_block = 0;
    size_t total_read = 0;
    
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

            //pthread_mutex_lock(&global_mutex);

            memcpy(buffer + total_read, block + block_offset, to_read_block);

            //pthread_mutex_unlock(&global_mutex);

            local_offset += to_read_block;
            total_read += to_read_block;

            current_block = (local_offset / BLOCK_SIZE) + 1;
            block_offset = local_offset % BLOCK_SIZE;

        }
    }

// ------------------------------------------------ CRIT SPOT - RWLOCK W ----------------------------------

    pthread_rwlock_wrlock(&file->open_file_rwlock);

    file->of_offset = local_offset;

    pthread_rwlock_unlock(&file->open_file_rwlock);

// ------------------------------------------------ END CRIT SPOT ----------------------------------
    return (ssize_t) total_read;
}

ssize_t tfs_read_indirect_region(open_file_entry_t *file, size_t to_read, void *buffer) {

// ----------------------------------- CRIT SPOT - RWLOCK R -----------------------------------------
    pthread_rwlock_rdlock(&file->open_file_rwlock);

    size_t local_offset = file->of_offset;

    pthread_rwlock_unlock(&file->open_file_rwlock);

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

        //pthread_mutex_lock(&data_blocks_s.data_blocks_mutex);

        memcpy(buffer + total_read, block + block_offset, to_read_block);

        //pthread_mutex_unlock(&data_blocks_s.data_blocks_mutex);

        local_offset += to_read_block;
        total_read += to_read_block;

        current_block = (local_offset / BLOCK_SIZE) + 2;
        block_offset = local_offset % BLOCK_SIZE;
    }

// ------------------------------------------------ CRIT SPOT - RWLOCK W ----------------------------------
    pthread_rwlock_wrlock(&file->open_file_rwlock);

    file->of_offset = local_offset;

    pthread_rwlock_unlock(&file->open_file_rwlock);

// ------------------------------------------------ END CRIT SPOT ----------------------------------

    return (ssize_t)total_read;
}

/*
void inode_lock(inode_table_t inode_table_s) {
    pthread_mutex_lock(&(inode_table_s.inode_table_mutex));
}

void inode_unlock(inode_table_t inode_table_s) {
    pthread_mutex_unlock(&(inode_table_s.inode_table_mutex));
}
*/