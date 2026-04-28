#include "paf_generator.h"
#include <stdlib.h>
#include <string.h>

// Forward declaration of crc32 from libpaf_core.c
uint32_t crc32(const unsigned char* data, size_t length);

int paf_generator_init(paf_generator_t* gen) {
    if (!gen) return -1;
    gen->data_tmp = tmpfile();
    gen->index_tmp = tmpfile();
    gen->path_tmp = tmpfile();
    gen->file_count = 0;
    gen->current_data_offset = 0;
    gen->current_path_offset = 0;

    if (!gen->data_tmp || !gen->index_tmp || !gen->path_tmp) {
        paf_generator_cleanup(gen);
        return -1;
    }
    return 0;
}

int paf_generator_add_file(paf_generator_t* gen, const char* path, const uint8_t* data, uint64_t size) {
    if (!gen || !path || !data) return -1;

    uint32_t path_len = (uint32_t)strlen(path);
    
    // 1. Write to Path Buffer
    if (fwrite(path, 1, path_len, gen->path_tmp) != path_len) return -1;

    // 2. Write to Data Block
    if (fwrite(data, 1, (size_t)size, gen->data_tmp) != (size_t)size) return -1;

    // 3. Prepare Index Entry
    paf_index_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.path_buffer_offset = gen->current_path_offset;
    entry.path_length = path_len;
    entry.data_offset = gen->current_data_offset;
    entry.data_size = size;
    entry.crc32 = crc32(data, (size_t)size);
    entry.flags = 0;

    if (fwrite(&entry, sizeof(entry), 1, gen->index_tmp) != 1) return -1;

    // Update offsets
    gen->current_data_offset += size;
    gen->current_path_offset += path_len;
    gen->file_count++;

    return 0;
}

static int copy_file_stream(FILE* src, FILE* dst) {
    char buffer[65536];
    size_t n;
    fseek(src, 0, SEEK_SET);
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, n, dst) != n) return -1;
    }
    return 0;
}

int paf_generator_finalize(paf_generator_t* gen, const char* output_path) {
    if (!gen || !output_path) return -1;

    FILE* out = fopen(output_path, "wb");
    if (!out) return -1;

    paf_header_t header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, PAF_MAGIC, 4);
    header.version = PAF_VERSION;
    header.file_count = gen->file_count;
    
    // Calculate offsets
    uint64_t header_size = sizeof(paf_header_t);
    uint64_t data_size = gen->current_data_offset;
    uint64_t index_size = (uint64_t)gen->file_count * sizeof(paf_index_entry_t);
    
    header.index_offset = header_size + data_size;
    header.path_offset = header.index_offset + index_size;

    // 1. Write Header
    if (fwrite(&header, sizeof(header), 1, out) != 1) {
        fclose(out);
        return -1;
    }

    // 2. Write Data Block
    if (copy_file_stream(gen->data_tmp, out) != 0) {
        fclose(out);
        return -1;
    }

    // 3. Write Index Block
    if (copy_file_stream(gen->index_tmp, out) != 0) {
        fclose(out);
        return -1;
    }

    // 4. Write Path Buffer
    if (copy_file_stream(gen->path_tmp, out) != 0) {
        fclose(out);
        return -1;
    }

    fclose(out);
    return 0;
}

void paf_generator_cleanup(paf_generator_t* gen) {
    if (!gen) return;
    if (gen->data_tmp) fclose(gen->data_tmp);
    if (gen->index_tmp) fclose(gen->index_tmp);
    if (gen->path_tmp) fclose(gen->path_tmp);
    gen->data_tmp = NULL;
    gen->index_tmp = NULL;
    gen->path_tmp = NULL;
}
