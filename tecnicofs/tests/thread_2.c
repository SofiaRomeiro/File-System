#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define SIZE 4096
#define N_THREADS 4

// MEDIUM EXECUTION TIME = 30-36 ms


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

    assert(read == args.to_read);

    return (void *)read;
}


int main() {


    pthread_t tids[N_THREADS];
    memset(tids, 0, sizeof(tids));

    int fhs[N_THREADS];
    memset(fhs, -1, sizeof(fhs));

    char *path = "/f1";

    char to_write[SIZE * N_THREADS];
    memset(to_write, 'V', SIZE * N_THREADS);

    char output[SIZE * N_THREADS];
    memset(output, '\0', SIZE * N_THREADS);   

    myargs_t *s[N_THREADS]; 

    float startTime = (float)clock()/CLOCKS_PER_SEC;

    assert(tfs_init() != -1);

    int fd = tfs_open(path,TFS_O_CREAT);
    assert(fd !=-1);

    ssize_t written = tfs_write(fd, to_write, SIZE * N_THREADS);

    assert(written == SIZE * N_THREADS);

    assert(tfs_close(fd) != -1);
    

    for (int i = 0; i < N_THREADS; i++) {
        fhs[i] = tfs_open(path, 0);
    }
    
    for (size_t i = 0; i < N_THREADS; i++) {
        s[i] = (myargs_t *)malloc(sizeof(myargs_t));
        s[i]->thread_offset = i * SIZE;
        s[i]->fd = fhs[i];
        s[i]->to_read = SIZE;
        s[i]->buffer = output;
        assert(pthread_create(&tids[i], NULL, fn, (void *)s[i]) == 0);
    }  

    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(tids[i], NULL);
    }

    for (int i = 0; i < N_THREADS; i++) {
        assert(tfs_close(fhs[i]) != -1);
        free(s[i]);
    }

    assert(memcmp(to_write, output, SIZE * N_THREADS)==0);


    float endTime = (float)clock()/CLOCKS_PER_SEC;

    float timeElapsed = endTime - startTime;

    printf("Time elapsed : %f\n", timeElapsed);
    printf("======> Sucessful test\n\n");

    return 0;
    
}