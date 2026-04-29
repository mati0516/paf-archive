#ifndef PAF_GENERATOR_H
#define PAF_GENERATOR_H

#include "paf.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FILE* data_tmp;
    FILE* index_tmp;
    FILE* path_tmp;
    uint32_t file_count;
    uint64_t current_data_offset;
    uint64_t current_path_offset;
    
    // GPU Batch Buffer (Pre-allocated)
    uint8_t* batch_data_buffer;
    uint64_t* batch_sizes;
    uint64_t* batch_offsets;
    char** batch_paths;
    uint32_t batch_count;
    uint64_t batch_buffer_pos;
    int index_only; // 1 = skip data block output (for paf_create_index_only)
} paf_generator_t;

/**
 * Initialize a PAF v2 generation session.
 * Creates temporary files.
 */
PAF_API int paf_generator_init(paf_generator_t* gen);

/**
 * Add a file to the PAF v1 archive.
 * @param gen The generator session.
 * @param path The path of the file to add.
 * @param data The file content.
 * @param size The size of the content.
 */
PAF_API int paf_generator_add_file(paf_generator_t* gen, const char* path, const uint8_t* data, uint64_t size);

/**
 * Finalize the archive and write to output file.
 * Merges parts and cleans up temporary files.
 */
PAF_API int paf_generator_finalize(paf_generator_t* gen, const char* output_path);

/**
 * Cleanup generator resources.
 */
PAF_API void paf_generator_cleanup(paf_generator_t* gen);

#ifdef __cplusplus
}
#endif

#endif // PAF_GENERATOR_H
