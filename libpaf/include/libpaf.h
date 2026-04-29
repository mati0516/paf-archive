#ifndef LIBPAF_H
#define LIBPAF_H

#include <stdint.h>
#include <stddef.h>

#include "paf.h"

#ifdef __cplusplus
extern "C" {
#endif

// Entry structure representing a file inside a .paf archive
typedef struct {
    char path[1024];       // UTF-8 file path (null-terminated)
    uint32_t size;         // File size in bytes
    uint32_t offset;       // Offset to the file data inside .paf
    uint32_t crc32;        // CRC32 checksum of the file
    uint8_t hash[32];      // SHA-256 hash
} PafEntry;

// List of entries in a .paf archive
typedef struct {
    PafEntry* entries;     // Array of file entries
    uint32_t count;        // Number of files
} PafList;

// Progress callback: called after each file during long operations
// done/total = file counts, path = current file path, user_data = caller context
typedef void (*paf_progress_fn)(uint32_t done, uint32_t total, const char* path, void* user_data);

// Create a .paf archive from specified files or folders
PAF_API int paf_create_binary(const char* out_paf_path, const char** input_paths, int path_count, const char* ignore_file_path, int recursive_ignore);

// Extract all contents of a .paf archive into a directory
PAF_API int paf_extract_binary(const char* paf_path, const char* output_dir, int overwrite);

// Retrieve the list of files in the archive
PAF_API int paf_list_binary(const char* paf_path, PafList* out_list);
PAF_API int paf_export_index(const char* paf_path, const char* out_index_path);
PAF_API void free_paf_list(PafList* list);

// Extract a single file from the archive
PAF_API int paf_extract_file(const char* paf_path, const char* internal_path, const char* output_path);

// Extract all files under a specified folder (prefix match)
PAF_API int paf_extract_folder(const char* paf_path, const char* internal_dir, const char* output_dir);

// Check if a file exists in the archive
PAF_API int file_exists_in_archive(const char* paf_path, const char* internal_path);

// Check if a folder (prefix) exists in the archive
PAF_API int folder_exists_in_archive(const char* paf_path, const char* internal_dir);

#ifdef __cplusplus
}
#endif

#endif // LIBPAF_H
