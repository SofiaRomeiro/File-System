#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main() {

    char *str = "AAA! AAA";
    char *path = "/f1";
    char *path2 = "./tests/test16.txt";
    char to_read[40];

    assert(tfs_init() != -1);

    memset(to_read, '\0', sizeof(to_read));

    int file = tfs_open(path, TFS_O_CREAT);
    assert(file != -1);

    assert(tfs_write(file, str, strlen(str)) != -1);

    assert(tfs_close(file) != -1);

    assert(tfs_copy_to_external_fs(path, path2) != -1);

    FILE *fp = fopen(path2, "r");

    assert(fp != NULL);

    assert(fread(to_read, sizeof(char), strlen(str), fp) == strlen(str));
    
    assert(strcmp(str, to_read) == 0);

    assert(fclose(fp) != -1);

    //unlink(path2);

    printf("======> Successful test.\n\n");

    return 0;
}
