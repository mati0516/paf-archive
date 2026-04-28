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
 * Open a PAF v2 archive for extraction.
 */
int paf_extractor_open(paf_extractor_t* ext, const char* path);

/**
 * Extract a file by index.
 * @param ext The extractor session.
 * @param index The index of the file (0 to file_count-1).
 * @param out_path Buffer to store the path (must be large enough).
 * @param out_data Pointer to a pointer that will hold the allocated data.
 * @param out_size Pointer to store the size of the data.
 */
int paf_extractor_get_file(paf_extractor_t* ext, uint32_t index, char* out_path, uint8_t** out_data, uint64_t* out_size);

/**
 * Close the extractor and free resources.
 */
void paf_extractor_close(paf_extractor_t* ext);

#endif // PAF_EXTRACTOR_H
