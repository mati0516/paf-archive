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

PAF_API int paf_export_index(const char* paf_path, const char* out_index_path) {
    FILE* fin = fopen(paf_path, "rb");
    if (!fin) return -1;

    paf_header_t header;
    if (fread(&header, sizeof(header), 1, fin) != 1) {
        fclose(fin);
        return -1;
    }

    if (memcmp(header.magic, PAF_MAGIC, 4) != 0) {
        fclose(fin);
        return -2;
    }

    // Calculate metadata end point (max of index or path buffer end)
    uint64_t metadata_end = sizeof(paf_header_t);
    
    if (header.file_count > 0) {
        uint64_t index_end = header.index_offset + (uint64_t)header.file_count * sizeof(paf_index_entry_t);
        
        // We need to find the end of the path buffer
        fseek(fin, header.index_offset + (header.file_count - 1) * sizeof(paf_index_entry_t), SEEK_SET);
        paf_index_entry_t last_idx;
        if (fread(&last_idx, sizeof(last_idx), 1, fin) == 1) {
            uint64_t path_buffer_end = header.path_offset + last_idx.path_buffer_offset + last_idx.path_length;
            metadata_end = (index_end > path_buffer_end) ? index_end : path_buffer_end;
        }
    }

    // Prepare header for the new index-only file
    header.flags |= PAF_FLAG_INDEX_ONLY;

    FILE* fout = fopen(out_index_path, "wb");
    if (!fout) {
        fclose(fin);
        return -1;
    }

    fwrite(&header, sizeof(header), 1, fout);

    // Copy metadata block
    uint64_t metadata_size = metadata_end - sizeof(paf_header_t);
    if (metadata_size > 0) {
        void* buffer = malloc((size_t)metadata_size);
        if (buffer) {
            fseek(fin, sizeof(paf_header_t), SEEK_SET);
            fread(buffer, 1, (size_t)metadata_size, fin);
            fwrite(buffer, 1, (size_t)metadata_size, fout);
            free(buffer);
        }
    }

    fclose(fin);
    fclose(fout);
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
