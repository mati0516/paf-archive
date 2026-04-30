#ifndef LIBPAF_H_V2
#define LIBPAF_H_V2

#include <stdint.h>

#define PAF_MAGIC   "PAF1"
#define PAF_VERSION 1

// ── Header-level flags (paf_header_t.flags) ──────────────────────────────
#define PAF_EXTRACT_SMART_OVERWRITE 0x01  // Skip identical files during extract
#define PAF_FLAG_INDEX_ONLY         0x02  // Data Block absent; index + paths only

// ── Per-entry flags (paf_index_entry_t.flags) ────────────────────────────
#define PAF_ENTRY_DELETED           0x04  // File deleted; data_size = 0
#define PAF_ENTRY_BINARY_DELTA      0x08  // data block is a PAFD binary delta

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
 * PAF Header — 32 bytes, always at offset 0.
 *
 *  Offset  Size  Field
 *  0       4     magic       "PAF1"
 *  4       4     version     1
 *  8       4     flags       PAF_FLAG_* bitmask
 *  12      4     file_count  number of index entries
 *  16      8     index_offset  byte offset of Index Block
 *  24      8     path_offset   byte offset of Path Buffer
 */
typedef struct {
    char     magic[4];        // "PAF1"
    uint32_t version;         // 1
    uint32_t flags;           // PAF_FLAG_* bitmask
    uint32_t file_count;
    uint64_t index_offset;    // absolute offset to Index Block
    uint64_t path_offset;     // absolute offset to Path Buffer
} paf_header_t;

/**
 * Index Entry — 64 bytes per file, fixed-size for GPU coalesced access.
 *
 *  Offset  Size  Field
 *  0       8     path_buffer_offset  byte offset within Path Buffer
 *  8       4     path_length         byte length of UTF-8 path (no NUL)
 *  12      4     flags               PAF_ENTRY_* bitmask
 *  16      8     data_offset         byte offset within Data Block
 *  24      8     data_size           size of data block entry in bytes
 *  32      32    hash                SHA-256 of the reconstructed file
 *                                    (for PAF_ENTRY_BINARY_DELTA this is the
 *                                     hash of the file *after* applying the delta)
 */
typedef struct {
    uint64_t path_buffer_offset;
    uint32_t path_length;
    uint32_t flags;
    uint64_t data_offset;
    uint64_t data_size;
    uint8_t  hash[32];        // SHA-256 — sole integrity checksum
} paf_index_entry_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // LIBPAF_H_V2
