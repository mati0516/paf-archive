// libpaf_list.c
#include "libpaf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>  // for mkdir on Windows
#else
#include <sys/stat.h>  // for mkdir on POSIX
#endif

// Extracts a single file from a .paf archive
int paf_extract_file(const char* paf_path, const char* internal_path, const char* output_path) {
    if (!file_exists_in_archive(paf_path, internal_path)) {
        return -4; // Not found
    }

    FILE* fp = fopen(paf_path, "rb");
    if (!fp) return -1;

    char magic[4];
    if (fread(magic, 1, 4, fp) != 4 || strncmp(magic, "PAF1", 4) != 0) {
        fclose(fp);
        return -2;
    }

    uint32_t file_count;
    if (fread(&file_count, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        return -3;
    }

    for (uint32_t i = 0; i < file_count; ++i) {
        char path[1024] = {0};
        uint16_t len;
        if (fread(&len, sizeof(uint16_t), 1, fp) != 1) break;
        if (len >= sizeof(path)) break;
        if (fread(path, 1, len, fp) != len) break;
        path[len] = '\0';

        uint32_t size, offset, crc;
        fread(&size, sizeof(uint32_t), 1, fp);
        fread(&offset, sizeof(uint32_t), 1, fp);
        fread(&crc, sizeof(uint32_t), 1, fp);

        if (strcmp(path, internal_path) == 0) {
            fseek(fp, 32 + offset, SEEK_SET);
            FILE* out = fopen(output_path, "wb");
            if (!out) {
                fclose(fp);
                return -4;
            }

            char* buffer = (char*)malloc(size);
            if (!buffer) {
                fclose(out);
                fclose(fp);
                return -5;
            }

            fread(buffer, 1, size, fp);
            fwrite(buffer, 1, size, out);
            free(buffer);
            fclose(out);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return -6;  // file not found
}

// Checks if a path starts with a prefix (folder match)
static int path_starts_with(const char* path, const char* prefix) {
    size_t plen = strlen(prefix);
    return strncmp(path, prefix, plen) == 0;
}

// Extracts all files under a given folder prefix
int paf_extract_folder(const char* paf_path, const char* internal_dir, const char* output_dir) {
    if (!folder_exists_in_archive(paf_path, internal_dir)) {
        return -4; // Not found
    }
    FILE* fp = fopen(paf_path, "rb");
    if (!fp) return -1;

    char magic[4];
    if (fread(magic, 1, 4, fp) != 4 || strncmp(magic, "PAF1", 4) != 0) {
        fclose(fp);
        return -2;
    }

    uint32_t file_count;
    if (fread(&file_count, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        return -3;
    }

    for (uint32_t i = 0; i < file_count; ++i) {
        char path[1024] = {0};
        uint16_t len;
        if (fread(&len, sizeof(uint16_t), 1, fp) != 1) break;
        if (len >= sizeof(path)) break;
        if (fread(path, 1, len, fp) != len) break;
        path[len] = '\0';

        uint32_t size, offset, crc;
        fread(&size, sizeof(uint32_t), 1, fp);
        fread(&offset, sizeof(uint32_t), 1, fp);
        fread(&crc, sizeof(uint32_t), 1, fp);

        if (!path_starts_with(path, internal_dir)) continue;

        fseek(fp, 32 + offset, SEEK_SET);

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", output_dir, path + strlen(internal_dir));

        // Ensure parent directories exist
        for (char* p = fullpath + strlen(output_dir) + 1; *p; ++p) {
            if (*p == '/' || *p == '\\') {
                *p = '\0';
                mkdir(fullpath);
                *p = '/';
            }
        }

        FILE* out = fopen(fullpath, "wb");
        if (!out) continue;

        char* buffer = (char*)malloc(size);
        if (!buffer) {
            fclose(out);
            continue;
        }

        fread(buffer, 1, size, fp);
        fwrite(buffer, 1, size, out);
        free(buffer);
        fclose(out);
    }

    fclose(fp);
    return 0;
}