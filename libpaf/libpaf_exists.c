#include <stdio.h>
#include <string.h>
#include "libpaf.h"

int file_exists_in_archive(const char* paf_path, const char* internal_path) {
    PafList list;
    if (paf_list_binary(paf_path, &list) != 0) return 0;

    for (uint32_t i = 0; i < list.count; ++i) {
        if (strcmp(list.entries[i].path, internal_path) == 0) {
            free_paf_list(&list);
            return 1;
        }
    }

    free_paf_list(&list);
    return 0;
}

int folder_exists_in_archive(const char* paf_path, const char* internal_dir) {
    size_t dir_len = strlen(internal_dir);
    if (dir_len == 0) return 0;

    PafList list;
    if (paf_list_binary(paf_path, &list) != 0) return 0;

    for (uint32_t i = 0; i < list.count; ++i) {
        if (strncmp(list.entries[i].path, internal_dir, dir_len) == 0 &&
            list.entries[i].path[dir_len] == '/') {
            free_paf_list(&list);
            return 1;
        }
    }

    free_paf_list(&list);
    return 0;
}
