// libpaf_list.c
#include "libpaf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Reads only the list of files from a .paf archive
int paf_list_binary(const char* paf_path, PafList* out_list) {
    FILE* fp = fopen(paf_path, "rb");
    if (!fp) return -1;

    paf_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1 || memcmp(header.magic, PAF_MAGIC, 4) != 0) {
        fclose(fp);
        return -2;
    }

    out_list->entries = (PafEntry*)malloc(sizeof(PafEntry) * header.file_count);
    out_list->count = header.file_count;

    // 1. Read Index Table
    paf_index_entry_t* idx_table = (paf_index_entry_t*)malloc(sizeof(paf_index_entry_t) * header.file_count);
    fseek(fp, header.index_offset, SEEK_SET);
    fread(idx_table, sizeof(paf_index_entry_t), header.file_count, fp);

    // 2. Read Path Buffer and fill PafEntry
    for (uint32_t i = 0; i < header.file_count; ++i) {
        fseek(fp, header.path_offset + idx_table[i].path_buffer_offset, SEEK_SET);
        uint32_t path_len = idx_table[i].path_length;
        if (path_len >= 1024) path_len = 1023;
        
        fread(out_list->entries[i].path, 1, path_len, fp);
        out_list->entries[i].path[path_len] = '\0';
        
        out_list->entries[i].size = (uint32_t)idx_table[i].data_size;
        out_list->entries[i].offset = (uint32_t)idx_table[i].data_offset;
        out_list->entries[i].crc32 = idx_table[i].flags;
        memcpy(out_list->entries[i].hash, idx_table[i].hash, 32);
    }

    free(idx_table);
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
