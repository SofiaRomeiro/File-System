#ifndef CONFIG_H
#define CONFIG_H

/* FS root inode number */
#define ROOT_DIR_INUM (0)

#define BLOCK_SIZE (1024)
#define DATA_BLOCKS (1024)
#define INODE_TABLE_SIZE (50)
#define MAX_OPEN_FILES (20)
#define MAX_FILE_NAME (40)

#define DELAY (5000)

#define INT_SIZE (4)
#define MAX_DATA_BLOCKS_FOR_INODE (10 + BLOCK_SIZE / INT_SIZE)
#define MAX_BYTES (272384)
#define MAX_DIRECT_BLOCKS (10)
#define MAX_BYTES_DIRECT_DATA (10240)

#define MAX_DIRECT_DATA_SIZE (10 * BLOCK_SIZE)

#endif // CONFIG_H
