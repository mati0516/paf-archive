// libpaf_list.c
#include "libpaf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Reads only the list of files from a .paf archive
int paf_list_binary(const char* paf_path, PafList* out_list) {
    FILE* fp = fopen(paf_path, "rb");
    if (!fp) return -1;

    char magic[4];
    if (fread(magic, 1, 4, fp) != 4 || strncmp(magic, "PAF1", 4) != 0) {
        fclose(fp);
        return -2;
    }

    uint32_t file_count;
    if (fread(&file_count, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        return -3;
    }

    out_list->entries = (PafEntry*)malloc(sizeof(PafEntry) * file_count);
    out_list->count = file_count;

    for (uint32_t i = 0; i < file_count; ++i) {
        uint16_t len;
        if (fread(&len, sizeof(uint16_t), 1, fp) != 1) break;
        if (fread(out_list->entries[i].path, 1, len, fp) != len) break;
        out_list->entries[i].path[len] = '\0';

        fread(&out_list->entries[i].size, sizeof(uint32_t), 1, fp);
        fread(&out_list->entries[i].offset, sizeof(uint32_t), 1, fp);
        fread(&out_list->entries[i].crc32, sizeof(uint32_t), 1, fp);
    }

    fclose(fp);
    return 0;
}

// Frees the memory allocated by paf_list_binary
void free_paf_list(PafList* list) {
    if (list->entries) {
        free(list->entries);
        list->entries = NULL;
    }
    list->count = 0;
}
