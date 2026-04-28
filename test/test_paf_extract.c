#include "paf_extractor.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    paf_extractor_t ext;
    if (paf_extractor_open(&ext, "output.paf") != 0) {
        fprintf(stderr, "Failed to open output.paf\n");
        return 1;
    }

    printf("Archive opened. File count: %u\n", ext.header.file_count);

    for (uint32_t i = 0; i < ext.header.file_count; i++) {
        char path[1024];
        uint8_t* data = NULL;
        uint64_t size = 0;

        if (paf_extractor_get_file(&ext, i, path, sizeof(path), &data, &size) == 0) {
            printf("[%u] Path: %s, Size: %lu bytes\n", i, path, size);
            printf("    SHA-256: ");
            for (int j = 0; j < 32; j++) printf("%02x", ext.entries[i].hash[j]);
            printf("\n");
            if (i == 0) {
                printf("    Content: %.*s\n", (int)size, (char*)data);
            } else if (i == 1) {
                printf("    Binary content: ");
                for (uint64_t j = 0; j < size; j++) printf("%02X ", data[j]);
                printf("\n");
            }
            free(data);
        } else {
            fprintf(stderr, "Failed to extract file %u\n", i);
        }
    }

    paf_extractor_close(&ext);
    return 0;
}
