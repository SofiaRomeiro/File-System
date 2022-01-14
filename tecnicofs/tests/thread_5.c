#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define WRITE 20480
#define N_THREADS 20

static int counter;
static pthread_mutex_t mutex;

static char buffer[WRITE];

typedef struct {
    int fh;
    int offset;
} myargs_t;

static char read_info[BLOCK_SIZE];

void *fn(void *arg) {
    
    myargs_t args = *((myargs_t *)arg);

    ssize_t total_written = tfs_write(args.fh, buffer + args.offset, BLOCK_SIZE);

    assert(total_written != -1);

    return (void *)NULL;
}

int check() {
    char control = read_info[0];

    for (int i = 1; i < BLOCK_SIZE; i++) {
        if (read_info[i] != control) {
            return -1;
        }
    }
    return 0;
}



int main() {

    char *path = "/f5";

    pthread_mutex_init(&mutex, NULL);

    counter = 0;

    int fhs[N_THREADS];

    pthread_t tids[N_THREADS];

    myargs_t *s[N_THREADS]; 

    memset(buffer, '\0', sizeof(buffer));

    memset(buffer, 'O', sizeof(buffer) / 3);    
    memset(buffer + (BLOCK_SIZE), 'L', sizeof(buffer) / 3);
    memset(buffer + (2 * BLOCK_SIZE), 'A', sizeof(buffer) / 3);

    memset(read_info, '\0', sizeof(read_info));

    printf("Read info : %ld\n", sizeof(read_info));

    assert(tfs_init() != -1);

    for (int i = 0; i < N_THREADS; i++) {
       fhs[i] = tfs_open(path, TFS_O_CREAT); 
    } 

    for (int i = 0; i < N_THREADS; i++)  {
        s[i] = (myargs_t *)malloc(sizeof(myargs_t));
        s[i]->fh = fhs[i];
        s[i]->offset = BLOCK_SIZE * i;
        assert(pthread_create(&tids[i], NULL, fn, (void *)s[i]) == 0);

    }

    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(tids[i], NULL);
    }

    for (int i = 0; i < N_THREADS; i++) {
        assert(tfs_close(fhs[i]) != -1);
        free(s[i]);
    }

    memset(buffer, '\0', sizeof(buffer));


    pthread_mutex_destroy(&mutex);

    int fh = tfs_open(path, 0);

    ssize_t read = tfs_read(fh, read_info, BLOCK_SIZE);

    assert(check() == 0);
    assert(read = BLOCK_SIZE);

    printf("====> Successfull test\n");

    return 0;

}