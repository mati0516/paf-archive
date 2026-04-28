#include "paf_generator.h"
#include <stdio.h>
#include <string.h>

int main() {
    paf_generator_t gen;
    if (paf_generator_init(&gen) != 0) {
        fprintf(stderr, "Failed to initialize generator\n");
        return 1;
    }

    const char* file1_name = "hello.txt";
    const char* file1_data = "Hello, PAF v2 World!";
    
    const char* file2_name = "nested/folder/test.bin";
    uint8_t file2_data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};

    printf("Adding %s...\n", file1_name);
    if (paf_generator_add_file(&gen, file1_name, (const uint8_t*)file1_data, strlen(file1_data)) != 0) {
        fprintf(stderr, "Failed to add file 1\n");
        return 1;
    }

    printf("Adding %s...\n", file2_name);
    if (paf_generator_add_file(&gen, file2_name, file2_data, sizeof(file2_data)) != 0) {
        fprintf(stderr, "Failed to add file 2\n");
        return 1;
    }

    printf("Finalizing to output.paf...\n");
    if (paf_generator_finalize(&gen, "output.paf") != 0) {
        fprintf(stderr, "Failed to finalize archive\n");
        return 1;
    }

    paf_generator_cleanup(&gen);
    printf("Success!\n");

    return 0;
}
