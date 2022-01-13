#include "../fs/operations.h"
#include <assert.h>
#include <string.h>

#define BYTES 3072
#define THREADS 3

char output[BYTES];

typedef struct {

    int offset_thread;
    int fhandler;
    int to_read;

} info_t;


void *funcao_das_threads(info_t *info) {

    open_file_entry_t *file = get_open_file_entry(info->fhandler);

    file->of_offset = (size_t) info->offset_thread;

    ssize_t read_b = tfs_read(info->fhandler, output, (size_t)info->to_read);

    assert(read_b == info->to_read);

    free(info);

    return NULL;

}

int main() {

    pthread_t tids[THREADS];

    info_t *info[THREADS];

    char *path_to_give = "/f1";

    char input[BYTES]; 
    memset(input, 'R', BYTES);
    input[BYTES-1]='\0';

    memset(output, '\0', BYTES);

    assert(tfs_init() != -1);

    int fd = tfs_open(path_to_give, TFS_O_CREAT);
    assert(fd != -1);
    assert(tfs_write(fd, input, BYTES) == BYTES);
    assert(tfs_close(fd) != -1);


    for (int i = 0; i < THREADS; i++) {

        info[i] = (info_t *)malloc(sizeof(info_t));

        int fh = tfs_open(path_to_give, 0);

        assert(fh != -1);

        info[i]->offset_thread = i * BLOCK_SIZE;
        info[i]->to_read = BLOCK_SIZE;

        info[i]->fhandler = fh;

        if (pthread_create(&tids[i], NULL, (void*)(info_t *)funcao_das_threads, (void *)&info[i]) != 0) return -1;

    }

    for (int i = 0; i < THREADS; i++) {

        pthread_join(tids[i], NULL);
        free(info[i]);
    }

    printf("%s\n", output);
    
    printf("======> Sucessful test\n\n");

    return 0;
}