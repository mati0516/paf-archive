// libpaf_extract.c
#include "libpaf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

static int path_starts_with(const char* path, const char* prefix) {
    size_t plen = strlen(prefix);
    return strncmp(path, prefix, plen) == 0;
}

int paf_extract_file(const char* paf_path, const char* internal_path, const char* output_path) {
    if (!file_exists_in_archive(paf_path, internal_path)) return -4;

    FILE* fp = fopen(paf_path, "rb");
    if (!fp) return -1;

    paf_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1 || memcmp(header.magic, PAF_MAGIC, 4) != 0) {
        fclose(fp);
        return -2;
    }

    paf_index_entry_t* idx = (paf_index_entry_t*)malloc(sizeof(paf_index_entry_t) * header.file_count);
    if (!idx) { fclose(fp); return -1; }

    fseek(fp, (long)header.index_offset, SEEK_SET);
    if (fread(idx, sizeof(paf_index_entry_t), header.file_count, fp) != header.file_count) {
        free(idx); fclose(fp); return -3;
    }

    for (uint32_t i = 0; i < header.file_count; ++i) {
        char path[1024] = {0};
        uint32_t path_len = idx[i].path_length;
        if (path_len >= sizeof(path)) path_len = (uint32_t)sizeof(path) - 1;

        fseek(fp, (long)(header.path_offset + idx[i].path_buffer_offset), SEEK_SET);
        if (fread(path, 1, path_len, fp) != path_len) continue;
        path[path_len] = '\0';

        if (strcmp(path, internal_path) != 0) continue;

        fseek(fp, (long)(sizeof(paf_header_t) + idx[i].data_offset), SEEK_SET);
        FILE* out = fopen(output_path, "wb");
        if (!out) { free(idx); fclose(fp); return -4; }

        char* buf = (char*)malloc((size_t)idx[i].data_size);
        if (!buf) { fclose(out); free(idx); fclose(fp); return -5; }

        if (fread(buf, 1, (size_t)idx[i].data_size, fp) != (size_t)idx[i].data_size) {
            free(buf); fclose(out); free(idx); fclose(fp); return -7;
        }
        fwrite(buf, 1, (size_t)idx[i].data_size, out);
        free(buf); fclose(out); free(idx); fclose(fp);
        return 0;
    }

    free(idx);
    fclose(fp);
    return -6;
}

int paf_extract_folder(const char* paf_path, const char* internal_dir, const char* output_dir) {
    if (!folder_exists_in_archive(paf_path, internal_dir)) return -4;

    FILE* fp = fopen(paf_path, "rb");
    if (!fp) return -1;

    paf_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1 || memcmp(header.magic, PAF_MAGIC, 4) != 0) {
        fclose(fp);
        return -2;
    }

    paf_index_entry_t* idx = (paf_index_entry_t*)malloc(sizeof(paf_index_entry_t) * header.file_count);
    if (!idx) { fclose(fp); return -1; }

    fseek(fp, (long)header.index_offset, SEEK_SET);
    if (fread(idx, sizeof(paf_index_entry_t), header.file_count, fp) != header.file_count) {
        free(idx); fclose(fp); return -3;
    }

    size_t dir_len = strlen(internal_dir);

    for (uint32_t i = 0; i < header.file_count; ++i) {
        char path[1024] = {0};
        uint32_t path_len = idx[i].path_length;
        if (path_len >= sizeof(path)) path_len = (uint32_t)sizeof(path) - 1;

        fseek(fp, (long)(header.path_offset + idx[i].path_buffer_offset), SEEK_SET);
        if (fread(path, 1, path_len, fp) != path_len) continue;
        path[path_len] = '\0';

        if (!path_starts_with(path, internal_dir)) continue;
        if (strstr(path, "..") || path[0] == '/' || path[0] == '\\') continue;

        char fullpath[1024];
        if (snprintf(fullpath, sizeof(fullpath), "%s/%s", output_dir, path + dir_len) >= (int)sizeof(fullpath)) continue;

        for (char* p = fullpath + strlen(output_dir) + 1; *p; ++p) {
            if (*p == '/' || *p == '\\') { *p = '\0'; MKDIR(fullpath); *p = '/'; }
        }

        fseek(fp, (long)(sizeof(paf_header_t) + idx[i].data_offset), SEEK_SET);
        FILE* out = fopen(fullpath, "wb");
        if (!out) continue;

        char* buf = (char*)malloc((size_t)idx[i].data_size);
        if (buf) {
            (void)fread(buf, 1, (size_t)idx[i].data_size, fp);
            fwrite(buf, 1, (size_t)idx[i].data_size, out);
            free(buf);
        }
        fclose(out);
    }

    free(idx);
    fclose(fp);
    return 0;
}
