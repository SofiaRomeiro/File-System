#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/* 
 *  USAR
 */

#define READ 100000
#define WRITE 100000

static int counter;
static pthread_mutex_t mutex;

void *fn(void *args) {

    char buffer[READ];

    int fh = *((int *)args);

    ssize_t total_read = tfs_read(fh, buffer, READ);

    printf("total read is %ld\n", total_read);

    assert(total_read != -1);

    pthread_mutex_lock(&mutex);

    counter += (int) total_read;
    
    pthread_mutex_unlock(&mutex);

    return (void *)NULL;

}



int main() {

    char *path = "/f5";

    pthread_mutex_init(&mutex, NULL);

    counter = 0;

    char buffer[WRITE];

    memset(buffer, 'V', sizeof(buffer));

    assert(tfs_init() != -1);

    int fh = tfs_open(path, TFS_O_CREAT);
    assert(fh != -1);

    assert(tfs_write(fh, buffer, WRITE));

    assert(tfs_close(fh) != -1);

    fh = tfs_open(path, 0);
    assert(fh != -1);

    pthread_t tid1;
    pthread_t tid2;

    pthread_create(&tid1, NULL, fn, (void *)&fh);
    pthread_create(&tid2, NULL, fn, (void *)&fh);
    
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    printf("Counter is %d\n", counter);

    assert(counter == READ);

    pthread_mutex_destroy(&mutex);

    assert(tfs_close(fh) != -1);

    return 0;

}