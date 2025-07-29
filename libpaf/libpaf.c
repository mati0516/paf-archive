�ｻｿ#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "libpaf.h"
#include <dirent.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#define PAF_MAGIC "PAF\x01"
#define PAF_HEADER_SIZE 32

typedef struct {
    char** full_paths;
    char** archive_paths;
    size_t count;
} FileList;

// --- CRC32 險育ｮ鈴未謨ｰ ---
static uint32_t crc32(const unsigned char* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

// 蜀榊ｸｰ逧�縺ｫ繝�繧｣繝ｬ繧ｯ繝医Μ菴懈��
static void ensure_directory(const char* full_path) {
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

int paf_verify_file(const char* paf_path, const char* file_name) {
    PafList list;
    if (paf_list(paf_path, &list) != 0) return -1;

    FILE* fp = fopen(paf_path, "rb");
    if (!fp) return -2;

    for (uint32_t i = 0; i < list.count; ++i) {
        if (strcmp(list.entries[i].path, file_name) != 0) continue;

        fseek(fp, 32 + list.entries[i].offset, SEEK_SET);
        char* buf = malloc(list.entries[i].size);
        if (!buf) {
            fclose(fp);
            free_paf_list(&list);
            return -5;
        }
        fread(buf, 1, list.entries[i].size, fp);
        uint32_t calc_crc = crc32((unsigned char*)buf, list.entries[i].size);
        free(buf);
        fclose(fp);
        free_paf_list(&list);
        return (calc_crc == list.entries[i].crc32) ? 0 : -3;
    }

    fclose(fp);
    free_paf_list(&list);
    return -4; // not found
}

static int collect_files_rec(const char* base_dir, const char* current_path, FileList* list) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", base_dir, current_path);

    DIR* dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char rel_path[1024];
        snprintf(rel_path, sizeof(rel_path), "%s/%s", current_path, entry->d_name);
        snprintf(path, sizeof(path), "%s/%s", base_dir, rel_path);

        int len = snprintf(path, sizeof(path), "%s/%s", base_dir, rel_path);
        if (len < 0 || (size_t)len >= sizeof(path)) {
            fprintf(stderr, "笶� path too long: %s/%s\n", base_dir, rel_path);
            closedir(dir);
            return -1;
        }


        struct stat st;
        if (stat(path, &st) != 0) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            collect_files_rec(base_dir, rel_path, list);
        } else {
            list->full_paths[list->count] = strdup(path);
            list->archive_paths[list->count] = strdup(rel_path);
            list->count++;
        }
    }

    closedir(dir);
    return 0;
}


static void free_file_list(FileList* list) {
    for (size_t i = 0; i < list->count; ++i) {
        free(list->full_paths[i]);
        free(list->archive_paths[i]);
    }
    free(list->full_paths);
    free(list->archive_paths);
    list->count = 0;
}

int paf_create(const char* out_paf_path, const char** paths, int path_count) {
    FileList list = {0};

    // 繝代せ螻暮幕�ｼ医ヵ繧｡繧､繝ｫ or 繝輔か繝ｫ繝��ｼ�
    for (int i = 0; i < path_count; ++i) {
        struct stat st;
        if (stat(paths[i], &st) != 0) continue;

        if (S_ISREG(st.st_mode)) {
            list.full_paths = realloc(list.full_paths, sizeof(char*) * (list.count + 1));
            list.archive_paths = realloc(list.archive_paths, sizeof(char*) * (list.count + 1));
            list.full_paths[list.count] = strdup(paths[i]);

            const char* name = strrchr(paths[i], '/');
            if (!name) name = strrchr(paths[i], '\\');
            name = name ? name + 1 : paths[i];
            list.archive_paths[list.count] = strdup(name);

            list.count++;
        } else if (S_ISDIR(st.st_mode)) {
            collect_files_rec(paths[i], "", &list);
        }
    }

    FILE* out = fopen(out_paf_path, "wb");
    if (!out) {
        free_file_list(&list);
        return -1;
    }

    char header[32] = {0};
    memcpy(header, PAF_MAGIC, 4);
    fwrite(header, 1, 32, out);

    uint32_t offset = 0;
    char* index_block = NULL;
    size_t index_size = 0;

    for (size_t i = 0; i < list.count; ++i) {
        FILE* f = fopen(list.full_paths[i], "rb");
        if (!f) continue;

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size < 0) {
            fclose(f);
            continue;
        }

        char* buffer = (char*)malloc(size);
        if (!buffer) {
            fclose(f);
            continue;
        }

        fread(buffer, 1, size, f);
        uint32_t crc = crc32((const uint8_t*)buffer, size);
        fwrite(buffer, 1, size, out);
        fclose(f);
        free(buffer);

        const char* archive_name = list.archive_paths[i];
        uint16_t name_len = (uint16_t)strlen(archive_name);
        size_t entry_size = 2 + name_len + 4 + 4 + 4;

        index_block = realloc(index_block, index_size + entry_size);
        if (!index_block) continue;

        char* entry = index_block + index_size;
        memcpy(entry, &name_len, 2);
        memcpy(entry + 2, archive_name, name_len);
        memcpy(entry + 2 + name_len, &size, 4);
        memcpy(entry + 2 + name_len + 4, &offset, 4);
        memcpy(entry + 2 + name_len + 8, &crc, 4);

        offset += size;
        index_size += entry_size;
    }

    uint32_t file_count = (uint32_t)list.count;
    uint32_t index_offset = 32 + offset;

    fwrite(index_block, 1, index_size, out);

    fseek(out, 0, SEEK_SET);
    memcpy(header + 4, &file_count, 4);
    memcpy(header + 8, &index_offset, 4);
    fwrite(header, 1, 32, out);

    fclose(out);
    free(index_block);
    free_file_list(&list);
    return 0;
}


int paf_extract_all(const char* paf_path, const char* output_dir) {
    PafList list;
    if (paf_list(paf_path, &list) != 0) return -1;

    FILE* fp = fopen(paf_path, "rb");
    if (!fp) {
        free_paf_list(&list);
        return -2;
    }

    for (uint32_t i = 0; i < list.count; ++i) {
        const char* name = list.entries[i].path;
        uint32_t size = list.entries[i].size;
        uint32_t offset = list.entries[i].offset;

        // 蜃ｺ蜉帛�医ヱ繧ｹ菴懈��
        char out_path[1024];
        snprintf(out_path, sizeof(out_path), "%s/%s", output_dir, name);

        // 繝輔か繝ｫ繝�菴懈��
        ensure_directory(out_path);

        // 謚ｽ蜃ｺ蜃ｦ逅�
        if (fseek(fp, 32 + offset, SEEK_SET) != 0) continue;

        FILE* out_fp = fopen(out_path, "wb");
        if (!out_fp) continue;

        char* buffer = (char*)malloc(size);
        if (!buffer) {
            fclose(out_fp);
            continue;
        }

        fread(buffer, 1, size, fp);
        fwrite(buffer, 1, size, out_fp);

        free(buffer);
        fclose(out_fp);
    }

    fclose(fp);
    free_paf_list(&list);
    return 0;
}

int paf_extract_file(const char* paf_path, const char* file_name, const char* out_path) {
    FILE* fp = fopen(paf_path, "rb");
    if (!fp) return -1;

    char header[32];
    if (fread(header, 1, 32, fp) != 32) {
        fclose(fp);
        return -2;
    }

    if (memcmp(header, "PAF\x01", 4) != 0) {
        fclose(fp);
        return -3;
    }

    uint32_t file_count = *(uint32_t*)(header + 4);
    uint32_t index_offset = *(uint32_t*)(header + 8);

    if (fseek(fp, index_offset, SEEK_SET) != 0) {
        fclose(fp);
        return -4;
    }

    for (uint32_t i = 0; i < file_count; ++i) {
        uint16_t name_len;
        if (fread(&name_len, sizeof(uint16_t), 1, fp) != 1) break;

        char* name = (char*)malloc(name_len + 1);
        if (!name) break;

        if (fread(name, 1, name_len, fp) != name_len) {
            free(name);
            break;
        }
        name[name_len] = '\0';

        uint32_t size, offset;
        if (fread(&size, sizeof(uint32_t), 1, fp) != 1 ||
            fread(&offset, sizeof(uint32_t), 1, fp) != 1) {
            free(name);
            break;
        }

        if (strcmp(name, file_name) == 0) {
            free(name);
            if (fseek(fp, 32 + offset, SEEK_SET) != 0) {
                fclose(fp);
                return -5;
            }

            FILE* out_fp = fopen(out_path, "wb");
            if (!out_fp) {
                fclose(fp);
                return -6;
            }

            char* buffer = (char*)malloc(size);
            if (!buffer) {
                fclose(fp);
                fclose(out_fp);
                return -7;
            }

            fread(buffer, 1, size, fp);
            fwrite(buffer, 1, size, out_fp);

            fclose(out_fp);
            fclose(fp);
            free(buffer);
            return 0;
        }

        free(name);
    }

    fclose(fp);
    return -8; // 謖�螳壹ヵ繧｡繧､繝ｫ縺瑚ｦ九▽縺九ｉ縺ｪ縺九▲縺�
}

int paf_list(const char* paf_path, PafList* out_list) {
    FILE* fp = fopen(paf_path, "rb");
    if (!fp) return -1;

    char header[PAF_HEADER_SIZE];
    if (fread(header, 1, PAF_HEADER_SIZE, fp) != PAF_HEADER_SIZE) {
        fclose(fp);
        return -2;
    }

    if (memcmp(header, PAF_MAGIC, 4) != 0) {
        fclose(fp);
        return -3;
    }

    uint32_t file_count = *(uint32_t*)(header + 4);
    uint32_t index_offset = *(uint32_t*)(header + 8);

    if (fseek(fp, index_offset, SEEK_SET) != 0) {
        fclose(fp);
        return -4;
    }

    PafEntry* entries = (PafEntry*)malloc(sizeof(PafEntry) * file_count);
    if (!entries) {
        fclose(fp);
        return -5;
    }

    uint32_t i = 0;
    for (uint32_t i = 0; i < file_count; ++i) {
        uint16_t name_len;
        if (fread(&name_len, sizeof(uint16_t), 1, fp) != 1) goto error;

        char* name = (char*)malloc(name_len + 1);
        if (!name) goto error;

        if (fread(name, 1, name_len, fp) != name_len) {
            free(name);
            goto error;
        }
        name[name_len] = '\0';

        uint32_t size, offset;
        if (fread(&size, sizeof(uint32_t), 1, fp) != 1) {
            free(name);
            goto error;
        }
        if (fread(&offset, sizeof(uint32_t), 1, fp) != 1) {
            free(name);
            goto error;
        }

        entries[i].path = name;
        entries[i].size = size;
        entries[i].offset = offset;
    }

    out_list->entries = entries;
    out_list->count = file_count;

    fclose(fp);
    return 0;

error:
    for (uint32_t j = 0; j < i; ++j) {
        free(entries[j].path);
    }
    free(entries);
    fclose(fp);
    return -6;
}

void free_paf_list(PafList* list) {
    if (!list || !list->entries) return;
    for (uint32_t i = 0; i < list->count; ++i) {
        free(list->entries[i].path);
    }
    free(list->entries);
    list->entries = NULL;
    list->count = 0;
}


// PAF → ディレクトリ展開（paf_extract_allのエイリアス）
int paf_extract_to_dir(const char* paf_path, const char* output_dir) {
    return paf_extract_all(paf_path, output_dir);
}
