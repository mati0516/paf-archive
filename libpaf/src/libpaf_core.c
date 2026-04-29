#define LIBPAF_EXPORTS
#include "libpaf.h"
#include "paf_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "fnmatch.h"
#include "sha256.h"

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
            FileEntry fe;
            fe.path = strdup(child_rel);
            fe.size = (uint32_t)st.st_size;
            fe.offset = 0;
            fe.crc32 = 0;

            if (*out_count % 64 == 0) {
                FileEntry* tmp = realloc(*out_entries, sizeof(FileEntry) * (*out_count + 64));
                if (!tmp) { free(fe.path); continue; }
                *out_entries = tmp;
            }
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

    paf_generator_t gen;
    if (paf_generator_init(&gen) != 0) {
        for (int i = 0; i < count; ++i) free(entries[i].path);
        free(entries);
        return -1;
    }

    for (int i = 0; i < count; ++i) {
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", input_paths[0], entries[i].path);
        FILE* fp = fopen(fullpath, "rb");
        if (!fp) { free(entries[i].path); continue; }
        uint8_t* buf = (uint8_t*)malloc(entries[i].size);
        if (buf && fread(buf, 1, entries[i].size, fp) == entries[i].size) {
            paf_generator_add_file(&gen, entries[i].path, buf, entries[i].size);
        }
        free(buf);
        fclose(fp);
        free(entries[i].path);
    }
    free(entries);

    int result = paf_generator_finalize(&gen, out_paf_path);
    paf_generator_cleanup(&gen);
    return result;
}

int paf_create_index_only(const char* out_paf, const char** input_paths, int path_count,
                           const char* filter) {
    (void)filter;

    FileEntry* entries = NULL;
    int count = 0;
    IgnoreRuleList root_rules = {0};

    if (path_count > 0) {
        char default_path[1024];
        snprintf(default_path, sizeof(default_path), "%s/.pafignore", input_paths[0]);
        load_ignore_file(default_path, &root_rules);
    }
    for (int i = 0; i < path_count; i++) {
        collect_files_binary(input_paths[i], "", &entries, &count, &root_rules, 1);
    }
    free_ignore_list(&root_rules);

    FILE* fp = fopen(out_paf, "wb");
    if (!fp) {
        for (int i = 0; i < count; i++) free(entries[i].path);
        free(entries);
        return -1;
    }

    paf_header_t header;
    memcpy(header.magic, PAF_MAGIC, 4);
    header.version = PAF_VERSION;
    header.flags   = PAF_FLAG_INDEX_ONLY;
    header.file_count   = (uint32_t)count;
    header.index_offset = sizeof(paf_header_t);
    header.path_offset  = sizeof(paf_header_t) + (uint64_t)count * sizeof(paf_index_entry_t);
    fwrite(&header, sizeof(header), 1, fp);

    uint64_t path_offset_accum = 0;
    for (int i = 0; i < count; i++) {
        paf_index_entry_t idx;
        memset(&idx, 0, sizeof(idx));
        idx.path_buffer_offset = path_offset_accum;
        idx.path_length        = (uint32_t)strlen(entries[i].path);
        idx.data_size          = entries[i].size;

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", input_paths[0], entries[i].path);
        FILE* fin = fopen(fullpath, "rb");
        if (fin) {
            sha256_context_t ctx;
            sha256_init(&ctx);
            uint8_t chunk[65536];
            size_t n;
            while ((n = fread(chunk, 1, sizeof(chunk), fin)) > 0)
                sha256_update(&ctx, chunk, n);
            sha256_final(&ctx, idx.hash);
            fclose(fin);
        }

        fwrite(&idx, sizeof(idx), 1, fp);
        path_offset_accum += (uint64_t)strlen(entries[i].path);
    }

    for (int i = 0; i < count; i++) {
        fwrite(entries[i].path, 1, strlen(entries[i].path), fp);
        free(entries[i].path);
    }
    free(entries);
    fclose(fp);
    return 0;
}

int paf_extract_binary(const char* paf_path, const char* output_dir, int overwrite) {
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

        if (!paf_is_path_safe(path)) continue;

        char fullpath[1024];
        if (snprintf(fullpath, sizeof(fullpath), "%s/%s", output_dir, path) >= (int)sizeof(fullpath)) continue;
        ensure_directory(fullpath);

        if (!overwrite) {
            FILE* test = fopen(fullpath, "rb");
            if (test) { fclose(test); continue; }
        }

        fseek(fp, (long)(sizeof(paf_header_t) + idx[i].data_offset), SEEK_SET);
        FILE* out_fp = fopen(fullpath, "wb");
        if (!out_fp) continue;

        char* buffer = (char*)malloc((size_t)idx[i].data_size);
        if (buffer) {
            if (fread(buffer, 1, (size_t)idx[i].data_size, fp) == (size_t)idx[i].data_size) {
                fwrite(buffer, 1, (size_t)idx[i].data_size, out_fp);
            }
            free(buffer);
        }
        fclose(out_fp);
    }

    free(idx);
    fclose(fp);
    return 0;
}