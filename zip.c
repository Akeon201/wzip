#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Author: Kenyon Leblanc

void compress(int size, char **argv);
 
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("wzip: file1 [file2 ...]\n");
        return 1;
    }

    compress(argc, argv);

    return 0;
}

void compress(int size, char **argv) {
    int count = 0;
    char c; 
    char temp;

    for (int i = 1; i < size; i++) {
        FILE *input_file = fopen(argv[i], "r");
        if (input_file == NULL) {
            printf("Error: Unable to open input file.\n");
            exit(1);
        }

        if (i == 1) {
            c = fgetc(input_file);
            count = 1;
        }

        while ((temp = fgetc(input_file)) != EOF) {
            if (c == temp) {
                count++;
            } else {
                fwrite(&count, sizeof(int), 1, stdout);
                fwrite(&c, sizeof(char), 1, stdout);
                c = temp;
                count = 1;
            }
        }
        fclose(input_file);
    }

    fwrite(&count, sizeof(int), 1, stdout);
    fwrite(&c, sizeof(char), 1, stdout);
}
