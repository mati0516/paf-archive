#include "paf_extractor.h"
#include <stdlib.h>
#include <string.h>

int paf_extractor_open(paf_extractor_t* ext, const char* path) {
    if (!ext || !path) return -1;
    
    ext->fp = fopen(path, "rb");
    if (!ext->fp) return -1;

    // 1. Read Header
    if (fread(&ext->header, sizeof(paf_header_t), 1, ext->fp) != 1) {
        fclose(ext->fp);
        return -1;
    }

    // 2. Validate Magic
    if (memcmp(ext->header.magic, PAF_MAGIC, 4) != 0) {
        fclose(ext->fp);
        return -1;
    }

    // 3. Load Index Entries
    ext->entries = (paf_index_entry_t*)malloc(sizeof(paf_index_entry_t) * ext->header.file_count);
    if (!ext->entries) {
        fclose(ext->fp);
        return -1;
    }

    fseek(ext->fp, ext->header.index_offset, SEEK_SET);
    if (fread(ext->entries, sizeof(paf_index_entry_t), ext->header.file_count, ext->fp) != ext->header.file_count) {
        free(ext->entries);
        fclose(ext->fp);
        return -1;
    }

    return 0;
}

int paf_extractor_get_file(paf_extractor_t* ext, uint32_t index, char* out_path, uint8_t** out_data, uint64_t* out_size) {
    if (!ext || index >= ext->header.file_count) return -1;

    paf_index_entry_t* entry = &ext->entries[index];

    // 1. Read Path
    fseek(ext->fp, ext->header.path_offset + entry->path_buffer_offset, SEEK_SET);
    if (fread(out_path, 1, entry->path_length, ext->fp) != entry->path_length) return -1;
    out_path[entry->path_length] = '\0';

    // 2. Read Data
    *out_data = (uint8_t*)malloc((size_t)entry->data_size);
    if (!*out_data) return -1;

    // Data Block starts right after Header (at offset 32)
    fseek(ext->fp, sizeof(paf_header_t) + entry->data_offset, SEEK_SET);
    if (fread(*out_data, 1, (size_t)entry->data_size, ext->fp) != (size_t)entry->data_size) {
        free(*out_data);
        return -1;
    }

    if (out_size) *out_size = entry->data_size;

    return 0;
}

void paf_extractor_close(paf_extractor_t* ext) {
    if (!ext) return;
    if (ext->entries) free(ext->entries);
    if (ext->fp) fclose(ext->fp);
    ext->entries = NULL;
    ext->fp = NULL;
}
