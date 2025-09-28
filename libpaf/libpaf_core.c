#include "libpaf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "fnmatch.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

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
    if ((void)fread(magic, 1, 4, fp) != 4 || strncmp(magic, "PAF1", 4) != 0) {
        fclose(fp);
        return -2;
    }

    uint32_t file_count;
    if ((void)fread(&file_count, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp);
        return -3;
    }

    for (uint32_t i = 0; i < file_count; ++i) {
        uint16_t len;
        char path[1024];
        (void)fread(&len, sizeof(uint16_t), 1, fp);
        (void)fread(path, 1, len, fp);
        path[len] = '\0';
        uint32_t size, offset, crc;
        (void)fread(&size, sizeof(uint32_t), 1, fp);
        (void)fread(&offset, sizeof(uint32_t), 1, fp);
        (void)fread(&crc, sizeof(uint32_t), 1, fp);

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", output_dir, path);
        ensure_directory(fullpath);

        if (!overwrite) {
            FILE* test = fopen(fullpath, "rb");
            if (test) { fclose(test); continue; }
        }

        long data_offset = ftell(fp);
        fseek(fp, 4 + sizeof(uint32_t), SEEK_SET);
        for (uint32_t j = 0; j < file_count; ++j) {
            uint16_t skip_len;
            (void)fread(&skip_len, sizeof(uint16_t), 1, fp);
            fseek(fp, skip_len + sizeof(uint32_t)*3 + sizeof(uint32_t), SEEK_CUR);
        }

        fseek(fp, offset, SEEK_CUR);

        FILE* out_fp = fopen(fullpath, "wb");
        if (!out_fp) continue;

        char* buffer = malloc(size);
        (void)fread(buffer, 1, size, fp);
        fwrite(buffer, 1, size, out_fp);
        free(buffer);
        fclose(out_fp);

        fseek(fp, data_offset, SEEK_SET);
    }

    fclose(fp);
    return 0;
}