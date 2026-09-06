#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]){
    if (argc < 2){
        printf("wgrep: searchterm [file ...]\n");
        exit(1);
    }
    //search term only
    if (argc == 2){
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, stdin)) != -1) {
            if (strstr(line, argv[1]) != NULL) {
                printf("%s", line);
            }
        }
        free(line);
   
    }

    char *file = argv[2]; //get name of file
    FILE *fp = fopen(file, "r"); //get the file pointer.
    if (fp == NULL) {
        printf("wcat: cannot open file\n");
        exit(1);
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, fp)) != -1) {
        if (strstr(line, argv[1]) != NULL) {
            printf("%s", line);
        }
    }
    free(line);

    fclose(fp);
    exit(0);
}