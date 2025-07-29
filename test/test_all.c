#include "../libpaf/libpaf.h"
#include <stdio.h>
#include <string.h>

int main() {
    const char* src_files[] = {"../libpaf/libpaf.h"};
    const char* paf_path = "test_all.paf";
    const char* extract_single = "extracted_libpaf.h";
    const char* extract_dir = "out";

    printf("📦 paf_create...\n");
    int result = paf_create(paf_path, src_files, 1);
    if (result != 0) {
        printf("❌ paf_create failed (%d)\n", result);
        return 1;
    }
    printf("✅ paf_create success\n\n");

    printf("📋 paf_list...\n");
    PafList list;
    result = paf_list(paf_path, &list);
    if (result != 0) {
        printf("❌ paf_list failed (%d)\n", result);
        return 1;
    }
    printf("✅ paf_list success: %u files\n", list.count);
    for (uint32_t i = 0; i < list.count; ++i) {
        printf(" - %s (%u bytes)\n", list.entries[i].path, list.entries[i].size);
    }
    printf("\n");

    printf("📂 paf_extract_file...\n");
    if (list.count > 0) {
        result = paf_extract_file(paf_path, list.entries[0].path, extract_single);
        if (result != 0) {
            printf("❌ paf_extract_file failed (%d)\n", result);
            return 1;
        }
        printf("✅ paf_extract_file success → %s\n\n", extract_single);
    }

    printf("📦 paf_extract_all...\n");
    result = paf_extract_all(paf_path, extract_dir);
    if (result != 0) {
        printf("❌ paf_extract_all failed (%d)\n", result);
        return 1;
    }
    printf("✅ paf_extract_all success → %s/\n", extract_dir);

    free_paf_list(&list);
    return 0;
}
