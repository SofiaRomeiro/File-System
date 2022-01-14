#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define WRITE 5000
#define N_THREADS 10

static int counter;
static pthread_mutex_t mutex;

static char buffer[WRITE];

void *fn(void *args) {

    int fh = *((int *)args);

    printf("fh is %d\n", fh);

    ssize_t total_written = tfs_write(fh, buffer, WRITE);

    printf("total written is %ld\n", total_written);

    assert(total_written != -1);

    pthread_mutex_lock(&mutex);

    counter += (int) total_written;
    
    pthread_mutex_unlock(&mutex);

    return (void *)NULL;

}



int main() {

    char *path = "/f5";

    pthread_mutex_init(&mutex, NULL);

    counter = 0;

    memset(buffer, 'V', sizeof(buffer));

    assert(tfs_init() != -1);

    int fh = tfs_open(path, TFS_O_CREAT);
    assert(fh != -1);

    pthread_t tids[N_THREADS];

    for (int i = 0; i < N_THREADS; i++) assert(pthread_create(&tids[i], NULL, fn, (void *)&fh) == 0);

    for (int i = 0; i < N_THREADS; i++) pthread_join(tids[i], NULL);


    printf("Counter is %d\n", counter);

    assert(counter == N_THREADS * WRITE);

    pthread_mutex_destroy(&mutex);

    assert(tfs_close(fh) != -1);

    return 0;

}