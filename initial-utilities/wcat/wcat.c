#include <stdio.h>
// No, you do not need to import anything extra to use stdout for writing
// in C. Including <stdio.h> is sufficient, since it declares 'stdout'
// as well as the fputs() function.


int main(int argc, char *argv[]) {
    // Check if at least one filename is provided
    if (argc < 2) {
        return 0;
    }
    // We'll process each file argument, one by one
    for (int i = 1; i < argc; i++) {
        char *file = argv[i];
        FILE *fp = fopen(file, "r");
        if (fp == NULL) {
            printf("wcat: cannot open file\n");
            return 1;
        }
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            printf("%s", buffer);
        }
        fclose(fp);
    }
    return 0;
}