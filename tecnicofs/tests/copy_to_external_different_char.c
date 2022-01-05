/*  *   *   *   *   *   *  *   *   *   *   *    *  *   *   *   *   *    *
 *                                                                      *
 *      MADE BY    :  Sofia Romeiro, ist198968, LETI                    *
 *                                                                      *
 *      BUG REPORT :  Karate Kid#9295 (Discord)                         *
 *                    sofiaromeiro23@tecnico.ulisboa.pt (webmail)       *
 *                                                                      *
 *  *   *   *   *   *   *  *   *   *   *   *    *  *   *   *   *   *    */

#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define SIZE_TO_TEST (1024*20)


int main() {

    char big_str[SIZE_TO_TEST+1];

    memset(big_str, 'b', sizeof(big_str) / 2);
    memset(big_str + (sizeof(big_str) / 2), 'u', sizeof(big_str) / 2);

    char buffer[SIZE_TO_TEST+1];

    char *path = "/f1";
    char *path2 = "./tests/output/test10.txt";  

    printf("Size to test = %ld\n", sizeof(buffer));

    memset(buffer, '\0', sizeof(buffer));

    memcpy(buffer, big_str, SIZE_TO_TEST);

    //printf("str: |%s|\n\n", big_str);
    printf("BUFFER: |%s|\nsize of buffer: %ld\n\n", buffer, sizeof(buffer));
    
    assert(tfs_init() != -1);

    printf("A\n");

    int tfs_file = tfs_open(path, TFS_O_CREAT);
    assert(tfs_file != -1);
    printf("B\n"); 
    assert(tfs_write(tfs_file, buffer, strlen(big_str)) == strlen(big_str));
    printf("C\n"); 
    assert(tfs_close(tfs_file) != -1);
    printf("D\n"); 
    assert(tfs_copy_to_external_fs(path, path2) != -1);
    printf("E\n"); 
    FILE *fp = fopen(path2, "r");
    printf("F\n"); 
    assert(fp != NULL);
    printf("g\n"); 
    assert(fread(buffer, sizeof(char), strlen(big_str), fp) == strlen(big_str)); //PROBLEM
    printf("H\n"); 
    assert(strncmp(big_str, buffer, strlen(big_str)) == 0);
    printf("I\n"); 
    assert(fclose(fp) != -1);
    printf("J\n"); 
    unlink(path2);
     
    printf("======> Successful test.\n\n");

    return 0;
}
