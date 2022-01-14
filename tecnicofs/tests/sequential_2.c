#include "../fs/operations.h"
#include <assert.h>
#include <string.h>
#include <time.h>

#define COUNT 4
#define SIZE 68096

int main() {

    char *path = "/f1";

    char input[SIZE]; 
    memset(input, 'A', SIZE);

    char output [SIZE];

    float startTime = (float)clock()/CLOCKS_PER_SEC;

    assert(tfs_init() != -1);

    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    for (int i = 0; i < COUNT; i++) {
        assert(tfs_write(fd, input, SIZE) == SIZE);
    }
    assert(tfs_close(fd) != -1);

    fd = tfs_open(path, 0);
    assert(fd != -1 );

    for (int i = 0; i < COUNT; i++) {
        assert(tfs_read(fd, output, SIZE) == SIZE);
        assert(memcmp(input, output, SIZE) == 0);
    }

    assert(tfs_close(fd) != -1);

    float endTime = (float)clock()/CLOCKS_PER_SEC;

    float timeElapsed = endTime - startTime;

    printf("Time elapsed : %f\n", timeElapsed);
    printf("======> Sucessful test\n\n");

    return 0;
}