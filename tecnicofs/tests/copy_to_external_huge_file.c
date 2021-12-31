#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define SIZE_TO_TEST (5204)


int main() {

    char big_str[SIZE_TO_TEST];

    char buffer[SIZE_TO_TEST];

    char *tfs_path = "/f1";
    char *path = "./tests/test9.txt";  

    printf("Size to test = %ld\n", sizeof(buffer));

    memset(big_str, 'g', sizeof(big_str));

    memset(buffer, '\0', sizeof(buffer));

    memcpy(buffer, big_str, SIZE_TO_TEST);

    assert(tfs_init() != -1);

    int tfs_file = tfs_open(tfs_path, TFS_O_CREAT);
    assert(tfs_file != -1);

    assert(tfs_write(tfs_file, buffer, sizeof(buffer)) == sizeof(buffer));

    assert(tfs_close(tfs_file) != -1);

    assert(tfs_copy_to_external_fs(tfs_path, path) != -1);

    // read to copied file - to to_read - and compare it to to_write

    FILE *fp = fopen(path, "r");

    assert(fp != NULL);

    assert(fread(buffer, sizeof(char), SIZE_TO_TEST, fp) == sizeof(buffer));

    printf("Len of %ld vs len of %ld\n", strlen(big_str), strlen(buffer));

    assert(strncmp(big_str, buffer, SIZE_TO_TEST) == 0);
    
    assert(fclose(fp) != -1);

    unlink(path);

    printf("======> Successful test.\n\n");

    return 0;
}