#include "operations.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/* 
 *  USAR
 */

#define N_THREADS 20

#define PATH ("/f1")


int fhandlers[N_THREADS];

static pthread_mutex_t mutex;

static int count;

int check_array() {
    
    int i = 0;
    int j = 1;

    while(i < N_THREADS) {
        while (j < N_THREADS) {
            if (fhandlers[i] == fhandlers[j]) return -1;
            j++;
        }
        i++;
        j = i + 1;
    }
    return 0;
}

void *fn() {

    int fhandler = tfs_open(PATH, 0);

    pthread_mutex_lock(&mutex);

    fhandlers[count] = fhandler;
    count++;
    
    pthread_mutex_unlock(&mutex);

    return (void *)NULL;
}


int main() {

    pthread_mutex_init(&mutex, NULL);

    count = 0;

    pthread_t tids[N_THREADS];
    memset(tids, 0, sizeof(tids));

    assert(tfs_init() != -1);   

    int fx = tfs_open(PATH, TFS_O_CREAT);
    assert(fx != -1);
    assert(tfs_close(fx) != -1); 

    for (size_t i = 0; i < N_THREADS; i++) {
        
        assert(pthread_create(&tids[i], NULL, fn, NULL) == 0);
    }  

    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(tids[i], NULL);
    }  

    int check = check_array();
    pthread_mutex_destroy(&mutex);

    assert(check == 0);

    printf("======> Sucessful test\n\n");

    return 0;
    
}