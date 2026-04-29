#define LIBPAF_EXPORTS
#include "libpaf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "fnmatch.h"

#if defined(_WIN32) && !defined(__ANDROID__) && !defined(__linux__)
#include "dirent_win.h"
#include <direct.h>
#include <io.h>
#define MKDIR(path) _mkdir(path)
#define F_OK 0
#define access _access
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

// Security: Check if a path is safe (no traversal, valid length)
static int paf_is_path_safe(const char* path) {
    if (!path || path[0] == '\0') return 0;
    if (strlen(path) >= 1024) return 0;
    
    // Block absolute paths and traversal
    if (path[0] == '/' || path[0] == '\\') return 0;
    if (strstr(path, "..") != NULL) return 0;
    if (strstr(path, ":") != NULL) return 0; // Block drive letters on Windows
    
    return 1;
}

uint32_t crc32(const unsigned char* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

void ensure_directory(const char* full_path) {
    char path[1024];
    strncpy(path, full_path, sizeof(path));
    path[sizeof(path) - 1] = '\0';

    for (char* p = path + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            MKDIR(path);
            *p = '/';
        }
    }
}

typedef struct {
    char* path;
    uint32_t size;
    uint32_t offset;
    uint32_t crc32;
} FileEntry;

typedef struct {
    char** patterns;
    int count;
} IgnoreRuleList;

static int load_ignore_file(const char* filepath, IgnoreRuleList* list) {
    FILE* fp = fopen(filepath, "r");
    if (!fp) return 0;
    char line[512];
    list->patterns = NULL;
    list->count = 0;
    while (fgets(line, sizeof(line), fp)) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;
        list->patterns = realloc(list->patterns, sizeof(char*) * (list->count + 1));
        list->patterns[list->count++] = strdup(line);
    }
    fclose(fp);
    return 1;
}

static int match_ignore(const char* filename, const IgnoreRuleList* rules) {
    for (int i = 0; i < rules->count; ++i) {
        if (fnmatch(rules->patterns[i], filename, 0) == 0) return 1;
    }
    return 0;
}

static void free_ignore_list(IgnoreRuleList* rules) {
    for (int i = 0; i < rules->count; ++i) {
        free(rules->patterns[i]);
    }
    free(rules->patterns);
    rules->patterns = NULL;
    rules->count = 0;
}

static int collect_files_binary(const char* base_dir, const char* rel_path,
                                 FileEntry** out_entries, int* out_count,
                                 const IgnoreRuleList* parent_rules,
                                 int recursive_ignore) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", base_dir, rel_path[0] ? rel_path : ".");
    DIR* dir = opendir(path);
    if (!dir) return -1;

    struct dirent* entry;
    IgnoreRuleList local_rules = {0};
    char ignore_path[2048];
    snprintf(ignore_path, sizeof(ignore_path), "%s/.pafignore", path);
    if (recursive_ignore && access(ignore_path, F_OK) == 0) {
        load_ignore_file(ignore_path, &local_rules);
    }

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (match_ignore(entry->d_name, &local_rules) ||
            (parent_rules && match_ignore(entry->d_name, parent_rules)))
            continue;

        char child_rel[1024];
        if (rel_path[0])
            snprintf(child_rel, sizeof(child_rel), "%s/%s", rel_path, entry->d_name);
        else
            snprintf(child_rel, sizeof(child_rel), "%s", entry->d_name);

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", base_dir, child_rel);
        struct stat st;
        if (stat(fullpath, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            collect_files_binary(base_dir, child_rel, out_entries, out_count,
                                 recursive_ignore ? &local_rules : parent_rules,
                                 recursive_ignore);
        } else if (S_ISREG(st.st_mode)) {
            FILE* fp = fopen(fullpath, "rb");
            if (!fp) continue;
            fseek(fp, 0, SEEK_END);
            uint32_t size = (uint32_t)ftell(fp);
            fseek(fp, 0, SEEK_SET);
            unsigned char* buf = malloc(size);
            (void)fread(buf, 1, size, fp);
            fclose(fp);
            uint32_t crc = crc32(buf, size);
            free(buf);

            FileEntry fe;
            fe.path = strdup(child_rel);
            fe.size = size;
            fe.offset = 0;
            fe.crc32 = crc;

            *out_entries = realloc(*out_entries, sizeof(FileEntry) * (*out_count + 1));
            (*out_entries)[(*out_count)++] = fe;
        }
    }

    closedir(dir);
    free_ignore_list(&local_rules);
    return 0;
}

int paf_create_binary(const char* out_paf_path, const char** input_paths, int path_count,
                      const char* ignore_file_path, int recursive_ignore) {
    FileEntry* entries = NULL;
    int count = 0;
    IgnoreRuleList root_rules = {0};

    if (ignore_file_path) {
        load_ignore_file(ignore_file_path, &root_rules);
    } else {
        char default_path[1024];
        snprintf(default_path, sizeof(default_path), "%s/.pafignore", input_paths[0]);
        load_ignore_file(default_path, &root_rules);
    }

    for (int i = 0; i < path_count; ++i) {
        collect_files_binary(input_paths[i], "", &entries, &count, &root_rules, recursive_ignore);
    }
    free_ignore_list(&root_rules);

    FILE* out = fopen(out_paf_path, "wb");
    if (!out) return -1;

    fwrite("PAF1", 1, 4, out);
    fwrite(&count, sizeof(uint32_t), 1, out);

    uint32_t offset = 0;
    for (int i = 0; i < count; ++i) {
        uint16_t len = (uint16_t)strlen(entries[i].path);
        fwrite(&len, sizeof(uint16_t), 1, out);
        fwrite(entries[i].path, 1, len, out);
        fwrite(&entries[i].size, sizeof(uint32_t), 1, out);
        fwrite(&offset, sizeof(uint32_t), 1, out);
        fwrite(&entries[i].crc32, sizeof(uint32_t), 1, out);
        entries[i].offset = offset;
        offset += entries[i].size;
    }

    for (int i = 0; i < count; ++i) {
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", input_paths[0], entries[i].path);
        FILE* fp = fopen(fullpath, "rb");
        if (!fp) continue;
        char* buf = malloc(entries[i].size);
        (void)fread(buf, 1, entries[i].size, fp);
        fclose(fp);
        fwrite(buf, 1, entries[i].size, out);
        free(buf);
    }

    for (int i = 0; i < count; ++i) free(entries[i].path);
    free(entries);
    fclose(out);
    return 0;
}

int paf_extract_binary(const char* paf_path, const char* output_dir, int overwrite) {
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

    long index_start = ftell(fp);
    for (uint32_t j = 0; j < file_count; ++j) {
        uint16_t skip_len;
        if (fread(&skip_len, sizeof(uint16_t), 1, fp) != 1) break;
        fseek(fp, skip_len + sizeof(uint32_t) * 3, SEEK_CUR);
    }
    long data_block_start = ftell(fp);

    for (uint32_t i = 0; i < file_count; ++i) {
        fseek(fp, index_start, SEEK_SET);
        for (uint32_t j = 0; j < i; ++j) {
            uint16_t skip_len;
            if (fread(&skip_len, sizeof(uint16_t), 1, fp) != 1) break;
            fseek(fp, skip_len + sizeof(uint32_t) * 3, SEEK_CUR);
        }

        uint16_t len;
        char path[1024];
        if (fread(&len, sizeof(uint16_t), 1, fp) != 1) break;
        if (len >= sizeof(path)) {
            continue;
        }
        if (fread(path, 1, len, fp) != len) break;
        path[len] = '\0';
        
        uint32_t size, offset, crc;
        if (fread(&size, sizeof(uint32_t), 1, fp) != 1) break;
        if (fread(&offset, sizeof(uint32_t), 1, fp) != 1) break;
        if (fread(&crc, sizeof(uint32_t), 1, fp) != 1) break;

        if (!paf_is_path_safe(path)) continue;

        char fullpath[1024];
        if (snprintf(fullpath, sizeof(fullpath), "%s/%s", output_dir, path) >= (int)sizeof(fullpath)) continue;
        ensure_directory(fullpath);

        if (!overwrite) {
            FILE* test = fopen(fullpath, "rb");
            if (test) { fclose(test); continue; }
        }

        fseek(fp, data_block_start + offset, SEEK_SET);

        FILE* out_fp = fopen(fullpath, "wb");
        if (!out_fp) continue;

        char* buffer = malloc(size);
        if (buffer) {
            if (fread(buffer, 1, size, fp) == size) {
                fwrite(buffer, 1, size, out_fp);
            }
            free(buffer);
        }
        fclose(out_fp);
    }

    fclose(fp);
    return 0;
}