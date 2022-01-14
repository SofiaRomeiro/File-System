#include "../fs/operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#define N_THREADS 7 // from 7 to 20 the best begins to fail

/**
   This test used multiple threads to create multiple files.
   A maximum of N_THREADS = 20 can be used (if the value is exceeded, an error will occur because it exceeds
   the maximum number of open files the open_file table can have).
   N_THREADS threads are created and each creates a file. After the threads are finished, checks if all the files
   exist in the filesystem.
 */


/* Function used in the threads */
void* fn(void* arg){
    char * res = (char*) arg;
    /* Each thread calls `tfs_open` to create a new file */
    int fd = tfs_open(res, TFS_O_CREAT);
    assert(fd !=-1);
    /* Closes the file afterwards */
    assert(tfs_close(fd) != -1);
    return NULL;
}


int main() {

    pthread_t threads[N_THREADS];

    /* Initializes the TFS */
    assert(tfs_init() != -1);

    /* Array of different path names (like "/fP", "/fQ", "/fR", ...) */
    char arg[N_THREADS][4];
    for(int i = 0; i < N_THREADS; i++){
        arg[i][0] = '/';
        arg[i][1] = 'f';
        arg[i][2] = (char) (i + 1 + 79);
        arg[i][3] = '\0';

    }

    /* Creates all the files using threads */
    for (int i = 0; i < N_THREADS; i++) {
        assert(pthread_create(&threads[i], NULL, fn, (void *) arg[i]) == 0);
    }

    /* Waits for all the threads to finish */
    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Check if the files were created */
    for (int i = 0; i < N_THREADS; i++) {
        int fd = tfs_open(arg[i], 0);
        /* If fd isn't -1, then the file was successfully created */
        assert(fd != -1);
        assert(tfs_close(fd) != -1);
    }

    printf("Sucessful test\n");

    return 0;
}
