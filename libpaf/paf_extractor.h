#ifndef PAF_EXTRACTOR_H
#define PAF_EXTRACTOR_H

#include "paf.h"
#include <stdio.h>

typedef struct {
    FILE* fp;
    paf_header_t header;
    paf_index_entry_t* entries;
} paf_extractor_t;

/**
 * Peek at the header of a PAF archive without allocating memory for index.
 */
int paf_extractor_peek_header(const char* path, paf_header_t* out_header);

/**
 * Open a PAF v1 archive for extraction.
 */
int paf_extractor_open(paf_extractor_t* ext, const char* path);

/**
 * Extract a file by index.
 * @param ext The extractor session.
 * @param index The index of the file (0 to file_count-1).
 * @param out_path Buffer to store the path.
 * @param path_max_len Size of the out_path buffer.
 * @param out_data Pointer to a pointer that will hold the allocated data.
 * @param out_size Pointer to store the size of the data.
 */
int paf_extractor_get_file(paf_extractor_t* ext, uint32_t index, char* out_path, size_t path_max_len, uint8_t** out_data, uint64_t* out_size);

/**
 * Close the extractor and free resources.
 */
void paf_extractor_close(paf_extractor_t* ext);

#endif // PAF_EXTRACTOR_H
