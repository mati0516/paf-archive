#include "paf_extractor.h"
#include "sha256.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int paf_extractor_peek_header(const char* path, paf_header_t* out_header) {
    if (!path || !out_header) return -1;
    
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;

    if (fread(out_header, sizeof(paf_header_t), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    if (memcmp(out_header->magic, PAF_MAGIC, 4) != 0) {
        fclose(fp);
        return -2;
    }

    fclose(fp);
    return 0;
}

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
    // Security check: Limit max files to 1 billion (approx 64GB index) for GenAI datasets.
    // Ensure the system has enough RAM to hold the index.
    if (ext->header.file_count > 1000000000) {
        fclose(ext->fp);
        return -4;
    }

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

static int is_safe_path(const char* path) {
    if (!path || path[0] == '/' || path[0] == '\\') return 0; // Absolute path blocked
    
    // Check for ".." segments
    const char* p = path;
    while (*p) {
        if (p[0] == '.' && p[1] == '.') {
            if (p[2] == '/' || p[2] == '\\' || p[2] == '\0') return 0;
        }
        p++;
    }
    return 1;
}

int paf_extractor_get_file(paf_extractor_t* ext, uint32_t index, char* out_path, size_t path_max_len, uint8_t** out_data, uint64_t* out_size) {
    if (!ext || index >= ext->header.file_count) return -1;

    paf_index_entry_t* entry = &ext->entries[index];

    // 1. Read Path
    if (entry->path_length >= path_max_len) return -2; // Buffer too small

    fseek(ext->fp, ext->header.path_offset + entry->path_buffer_offset, SEEK_SET);
    if (fread(out_path, 1, entry->path_length, ext->fp) != entry->path_length) return -1;
    out_path[entry->path_length] = '\0';

    // 1.1 Validate Path Safety
    if (!is_safe_path(out_path)) {
        return -3; // Malicious path detected
    }

    // [New] Smart Overwrite check
    // If the file exists and flags indicate smart overwrite, compare hash
    struct stat st;
    if (stat(out_path, &st) == 0) {
        if (st.st_size == (off_t)entry->data_size) {
            // Read existing file and check SHA-256
            FILE* existing_fp = fopen(out_path, "rb");
            if (existing_fp) {
                uint8_t* temp_buf = (uint8_t*)malloc((size_t)entry->data_size);
                if (temp_buf) {
                    fread(temp_buf, 1, (size_t)entry->data_size, existing_fp);
                    uint8_t current_hash[32];
                    sha256_context_t sha_ctx;
                    sha256_init(&sha_ctx);
                    sha256_update(&sha_ctx, temp_buf, (size_t)entry->data_size);
                    sha256_final(&sha_ctx, current_hash);
                    free(temp_buf);
                    
                    if (memcmp(current_hash, entry->hash, 32) == 0) {
                        fclose(existing_fp);
                        printf("[SmartExtract] Skipping %s (Identical hash)\n", out_path);
                        if (out_size) *out_size = entry->data_size;
                        *out_data = NULL; // Indicate skipped
                        return 0;
                    }
                }
                fclose(existing_fp);
            }
        }
    }

    // 2. Read Data
    // Security check: Limit max allocation to 2GB to prevent DoS (can be adjusted)
    if (entry->data_size > 0x7FFFFFFF) return -4;

    *out_data = (uint8_t*)malloc((size_t)entry->data_size);
    if (!*out_data) return -5;

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
