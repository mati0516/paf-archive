#ifndef PAF_GENERATOR_H
#define PAF_GENERATOR_H

#include "paf.h"
#include <stdio.h>

typedef struct {
    FILE* data_tmp;
    FILE* index_tmp;
    FILE* path_tmp;
    uint32_t file_count;
    uint64_t current_data_offset;
    uint64_t current_path_offset;
} paf_generator_t;

/**
 * Initialize a PAF v2 generation session.
 * Creates temporary files.
 */
int paf_generator_init(paf_generator_t* gen);

/**
 * Add a file to the PAF v1 archive.
 * @param gen The generator session.
 * @param path The path of the file to add.
 * @param data The file content.
 * @param size The size of the content.
 */
int paf_generator_add_file(paf_generator_t* gen, const char* path, const uint8_t* data, uint64_t size);

/**
 * Finalize the archive and write to output file.
 * Merges parts and cleans up temporary files.
 */
int paf_generator_finalize(paf_generator_t* gen, const char* output_path);

/**
 * Cleanup generator resources.
 */
void paf_generator_cleanup(paf_generator_t* gen);

#endif // PAF_GENERATOR_H
