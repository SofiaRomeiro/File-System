#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

#define SIZE (272384)

int main() {

   char *path = "/f1";
   int fd = 0;

   char input[SIZE]; 
   memset(input, 'R', SIZE);

   char output[SIZE];
   memset(output, '\0', SIZE);

   assert(tfs_init() != -1);

   fd = tfs_open(path, TFS_O_CREAT);

   assert(fd != -1);

   assert(tfs_write(fd, input, SIZE) == SIZE);



   /*open_file_entry_t *file = get_open_file_entry(fd);

   inode_t *inode = inode_get(file->of_inumber);

   printf("size %ld\n", inode->i_size);

   int *blocks = (int *)data_block_get(inode->i_block[10]);

   for (int i = 0; i < MAX_DATA_BLOCKS_FOR_INODE - 10; i++) {
      printf("%d ", blocks[i]);
   }*/



   assert(tfs_close(fd) != -1);

   fd = tfs_open(path,  0);                                            

   assert(fd != -1);

   assert(tfs_read(fd, output, SIZE) == SIZE);

   for (int i = 0; i < SIZE; i++) {
      if (input[i] != output[i]) {
         printf("input is %c and output is %c\n", input[i], output[i]);
         break;
      }
   }

   assert(memcmp(input, output, SIZE) == 0);

   assert(tfs_close(fd) != -1);

   printf("======> Sucessful test\n\n");

   return 0;
}