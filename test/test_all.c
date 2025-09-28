#include <stdio.h>
#include "libpaf.h"
#include <string.h>

int main() {
    const char* paf_path = "test_all.paf";
    const char* extract_dir = "out";
    const char* extract_single = "out_single";
    const char* input_paths[] = {"test_all_data"};
    const int path_count = 1;

    printf("============================================\n");
    printf("         PAF Total Function Test\n");
    printf("============================================\n");

    // [1] Create .paf archive
    printf("[1/6] Creating archive...\n");
    int result = paf_create_binary(paf_path, input_paths, path_count, NULL, 1);
    if (result != 0) {
        printf("❌ paf_create_binary failed (%d)\n", result);
        return 1;
    }
    printf("[ok] paf_create_binary success: %s\n", paf_path);

    // [2] Extract whole archive
    printf("\n[2/6] Extracting all files...\n");
    result = paf_extract_binary(paf_path, extract_dir, 1);
    if (result != 0) {
        printf("❌ paf_extract_binary failed (%d)\n", result);
        return 1;
    }
    printf("[ok] paf_extract_binary success → %s/\n", extract_dir);

    // [3] List contents
    printf("\n[3/6] Listing contents...\n");
    PafList list;
    result = paf_list_binary(paf_path, &list);
    if (result != 0) {
        printf("❌ paf_list_binary failed (%d)\n", result);
        return 1;
    }
    for (uint32_t i = 0; i < list.count; ++i) {
        printf(" - %s (%u bytes)\n", list.entries[i].path, list.entries[i].size);
    }
    printf("[ok] paf_list_binary success (%u files)\n", list.count);

    // [4] Extract first file
    printf("\n[4/6] Extracting first file → %s/...\n", extract_single);
    if (list.count > 0) {
        result = paf_extract_file(paf_path, list.entries[0].path, extract_single);
        if (result != 0) {
            printf("❌ paf_extract_file failed (%d)\n", result);
            return 1;
        }
        printf("[ok] paf_extract_file success");
    }

    // [5] Extract first folder
    printf("\n[5/6] Extracting first folder → %s/...\n", extract_dir);
    for (uint32_t i = 0; i < list.count; ++i) {
        const char* p = list.entries[i].path;
        const char* slash = strchr(p, '/');
        if (slash) {
            char folder[256] = {0};
            strncpy(folder, p, slash - p);
            result = paf_extract_folder(paf_path, folder, extract_dir);
            if (result != 0) {
                printf("❌ paf_extract_folder failed (%d)\n", result);
                return 1;
            }
            printf("[ok] paf_extract_folder success");
            break;
        }
    }

    // [7] Invalid file extraction test
    printf("[test] Trying to extract a non-existent file...");
    result = paf_extract_file(paf_path, "nonexistent.file", extract_single);
    if (result == -4) {
        printf("[ok] Correctly failed with error code -4 (file not found)");
    } else {
        printf("[warn] Unexpected result when extracting non-existent file: %d", result);
    }

    // [8] Invalid folder extraction test
    printf("[test] Trying to extract a non-existent folder...");
    result = paf_extract_folder(paf_path, "no/such/folder", extract_dir);
    if (result == -4) {
        printf("[ok] Correctly failed with error code -4 (folder not found)");
    } else {
        printf("[warn] Unexpected result when extracting non-existent folder: %d", result);
    }

    free_paf_list(&list);
    printf("\n✅ All tests passed!\n");
    return 0;
}
