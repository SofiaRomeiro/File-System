#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

#define SIZE 26
#define N_THREADS 16

/**
   This test uses multiple threads to write on the same file (and same fh) and checks whether the result
   was the correct one.
   A maximum of N_THREADS = 10476 can be used (if the value is exceeded, an error will occur because
   it exceeds the file size.
   N_THREADS threads are created and each thread writes the abecedary to the file, by calling the
   function `tfs_write`.
 */


/* Struct used as argument in the threads */
typedef struct {
    int fh;
    void const *buffer;
    size_t to_write;
} Mystruct;

/* Function used in the threads */
void* fn(void* arg) {
    Mystruct s = *((Mystruct *)arg);
    /* Each thread calls `tfs_write` */
    ssize_t res = tfs_write(s.fh,s.buffer,s.to_write);
    return (void*) res;
}


int main() {

    pthread_t threads[N_THREADS];

    /* Only one file path is used in this test */
    char *path = "/f1";

    /* Variable used to write */
    char write[SIZE+1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    /* The expected output variable */
    char output[SIZE * N_THREADS + 1];

    /* Creates the expected output */
    /* It's the abecedary multiple times */
    /* For instance, "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ..."*/
    int of = 0;
    for (int i = 0; i < N_THREADS; i++){
        memcpy(output + of,write,SIZE);
        of += SIZE;
    }
    output[of] = '\0';

    /* My output. Used to compare to the expected output */
    char *myoutput = (char*) malloc(sizeof(char) * (SIZE*N_THREADS+1));

    // ----------------------

    /* Initializes the TFS */
    assert(tfs_init() != -1);

    /* Creates the file with the given path */
    int fd = tfs_open(path,TFS_O_CREAT);
    assert(fd !=-1);

    /* Fills the argument of pthread */
    Mystruct* s;
    s = (Mystruct*)malloc(sizeof (Mystruct));
    s->to_write = SIZE;
    s->buffer = write;
    s->fh = fd;

    /* Creates all the threads, all writing the abecedary to the same file */
    /* The expected behaviour is to have N_THREADS abecedaries, because we are
     * asking the program to write that many abecedaries */
    for (int i = 0; i < N_THREADS; i++) {
        assert(pthread_create(&threads[i], NULL, fn, (void *) s) == 0);
    }

    /* Waits for all the threads to finish */
    for (int i = 0; i < N_THREADS; i++){
        pthread_join(threads[i],NULL);
    }

    /* Closes the file */
    assert(tfs_close(fd) != -1);

    /* Open it again but in read mode (without flags) */
    fd = tfs_open(path,0);
    assert(fd !=-1);

    /* Reads the content of the file to `myoutput`.
     * The content must be of size SIZE*N_THREADS (where SIZE is the length of
     * the abecedary. */
    ssize_t res = tfs_read(fd, myoutput,SIZE*N_THREADS);
    assert(res == SIZE*N_THREADS);

    /* Compares if the output and the expected output are the same */
    assert(memcmp(output,myoutput, SIZE*N_THREADS)==0);

    /* Frees the buffer */
    free(myoutput);

    printf("Successful test\n");

    return 0;
}