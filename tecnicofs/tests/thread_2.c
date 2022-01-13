#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define SIZE 1024
#define N_THREADS 4


typedef struct {
    size_t thread_offset;
    int fd;
    size_t to_read;
    void *buffer;
} myargs_t;

void *fn(void *arg) {
    myargs_t args = *((myargs_t *)arg);

    open_file_entry_t *file = get_open_file_entry(args.fd);

    if (file == NULL) {
        printf("NULL pointer : File not open\n");
        return NULL;
    }

    file->of_offset = args.thread_offset;

    ssize_t read = tfs_read(args.fd, args.buffer + args.thread_offset, args.to_read);

    //printf("Output threading |%s|\n", (char *)(args.buffer));

    return (void *)read;
}


int main() {
    pthread_t tids[N_THREADS];
    int fhs[N_THREADS];

    char *path = "/f1";

    char to_write[SIZE];

    char output[SIZE];

    memset(to_write, 'V', sizeof(to_write));

    assert(tfs_init() != -1);

    int fd = tfs_open(path,TFS_O_CREAT);
    assert(fd !=-1);

    ssize_t written = tfs_write(fd, to_write, SIZE * N_THREADS);
    assert(written == SIZE * N_THREADS);

    assert(tfs_close(fd) != -1);

    myargs_t *s[N_THREADS];

    fhs[0] = tfs_open(path, 0);
    fhs[1] = tfs_open(path, 0);
    fhs[2] = tfs_open(path, 0);
    fhs[3] = tfs_open(path, 0);

    for (int i = 0; i < N_THREADS; i++) {
        printf("%d ", fhs[i]);
    }
    printf("\n");

    
    for (size_t i = 0; i < N_THREADS; i++) {
        s[i] = (myargs_t *)malloc(sizeof(myargs_t));
        s[i]->thread_offset = i * BLOCK_SIZE;
        s[i]->fd = fhs[i];
        s[i]->to_read = BLOCK_SIZE;
        s[i]->buffer = output;
        assert(pthread_create(&tids[i], NULL, fn, (void *)s[i]) == 0);
    }
    



    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(tids[i], NULL);
        assert(tfs_close(fhs[i]) != -1);
        free(s[i]);
    }

    printf("Output |%s|\n", output);

    assert(memcmp(to_write, output, SIZE)==0);

    printf("====> Successfull test\n");

    return 0;
    
}