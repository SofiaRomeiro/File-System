/*  *   *   *   *   *   *  *   *   *   *   *    *  *   *   *   *   *    *
 *                                                                      *
 *      MADE BY    :  Sofia Romeiro, ist198968, LETI                    *
 *                                                                      *
 *      BUG REPORT :  Karate Kid#9295 (Discord)                         *
 *                    sofiaromeiro23@tecnico.ulisboa.pt (webmail)       *
 *                                                                      *
 *  *   *   *   *   *   *  *   *   *   *   *    *  *   *   *   *   *    */

#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define SIZE_TO_TEST (1024*15)


int main() {
    char str[SIZE_TO_TEST];

    memset(str, 'i', BLOCK_SIZE * 6);
    memset(str + (sizeof(str) / 2), 'e', BLOCK_SIZE * 9);

    char buffer[SIZE_TO_TEST+1];

    char *path = "/f1";
    char *path2 = "./tests/output/test18.txt";  

    memset(buffer, '\0', sizeof(buffer));

    assert(tfs_init() != -1);

    int tfs_file = tfs_open(path, TFS_O_CREAT);
    assert(tfs_file != -1);
    
    assert(tfs_write(tfs_file, str, strlen(str)) == strlen(str));

    assert(tfs_close(tfs_file) != -1);

    assert(tfs_copy_to_external_fs(path, path2) != -1);

    FILE *fp = fopen(path2, "r");

    //como mudo o offset?
    //fp->of_offset = (7*BLOCK_SIZE + 512);

    assert(fp != NULL);

    size_t fr = fread(buffer, sizeof(char), strlen(str), fp);

    size_t str_len = strlen(str);

    assert(fr == str_len);

    assert(strncmp(str, buffer, strlen(str)) == 0);

    assert(fclose(fp) != -1);

    printf("======> Successful test.\n\n");
    return 0;
}
