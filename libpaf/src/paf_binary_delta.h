#ifndef PAF_BINARY_DELTA_H
#define PAF_BINARY_DELTA_H

#include <stdint.h>
#include <stddef.h>

// Binary delta format (PAFD):
//   [4B magic "PAFD"] [4B instr_count] [8B old_file_size]
//   N × { [1B type] [8B offset] [4B size] [size bytes if LITERAL] }
//
// type 0 = COPY   — reuse old[offset..offset+size]
// type 1 = LITERAL — new bytes follow

// Create a binary delta from old_path → new_path.
// Returns heap-allocated delta bytes (caller must free); sets *out_size.
// Returns NULL on failure.
uint8_t* paf_bdelta_create(const char* old_path, const char* new_path, size_t* out_size);

// Apply a binary delta: reads old_path, writes reconstructed file to out_path.
// delta / delta_size must point to a valid PAFD buffer.
// Returns 0 on success, -1 on failure.
int paf_bdelta_apply(const uint8_t* delta, size_t delta_size,
                     const char* old_path, const char* out_path);

#endif // PAF_BINARY_DELTA_H
