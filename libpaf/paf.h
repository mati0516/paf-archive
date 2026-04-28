#ifndef LIBPAF_H_V2
#define LIBPAF_H_V2

#include <stdint.h>

#define PAF_MAGIC "PAF1"
#define PAF_VERSION 1

#pragma pack(push, 1)

/**
 * PAF v2 Main Header (32 bytes)
 */
typedef struct {
    char magic[4];           // 'PAF1'
    uint32_t version;        // 1
    uint32_t flags;          // Future use
    uint32_t file_count;     // Total number of files
    uint64_t index_offset;   // Offset to the start of Index Block
    uint64_t path_offset;    // Offset to the start of Path Buffer
} paf_header_t;

/**
 * PAF v1 Index Entry (40 bytes)
 * Designed for GPU coalesced access (fixed size).
 */
typedef struct {
    uint64_t path_buffer_offset; // Offset within Path Buffer
    uint32_t path_length;        // Length of path string
    uint64_t data_offset;        // Offset within Data Block (relative to start of Data Block)
    uint64_t data_size;          // Size of file data
    uint32_t crc32;              // CRC32 checksum
    uint32_t flags;              // Per-file flags (e.g. compression type)
    uint32_t reserved;           // Alignment / Future use
} paf_index_entry_t;

#pragma pack(pop)

#endif // LIBPAF2_H
