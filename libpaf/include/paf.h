#ifndef LIBPAF_H_V2
#define LIBPAF_H_V2

#include <stdint.h>

#define PAF_MAGIC "PAF1"
#define PAF_VERSION 1

#define PAF_EXTRACT_SMART_OVERWRITE 0x01
#define PAF_FLAG_INDEX_ONLY         0x02

// Per-entry flags (paf_index_entry_t.flags)
#define PAF_ENTRY_DELETED           0x04   // File deleted; no data block
#define PAF_ENTRY_BINARY_DELTA      0x08   // Data block is a binary delta (paf_bdelta format)

#ifdef _WIN32
  #ifdef LIBPAF_EXPORTS
    #define PAF_API __declspec(dllexport)
  #else
    #define PAF_API __declspec(dllimport)
  #endif
#else
  #define PAF_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

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
    uint32_t flags;              // Per-file flags (e.g. compression type)
    uint64_t data_offset;        // Offset within Data Block (relative to start of Data Block)
    uint64_t data_size;          // Size of file data
    uint8_t  hash[32];           // SHA-256 hash
} paf_index_entry_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // LIBPAF2_H
