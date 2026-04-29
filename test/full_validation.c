#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libpaf.h"

int main() {
    printf("--- Full Validation: Create & Extract ---\n");
    const char* out_paf = "validation.paf";
    const char* input_dirs[] = { "test_data" };
    
    printf("1. Creating PAF archive...\n");
    int res = paf_create_binary(out_paf, input_dirs, 1, NULL, 1);
    if (res != 0) {
        printf("[FAILED] paf_create_binary returned %d\n", res);
        return 1;
    }
    
    printf("2. Extracting PAF archive into 'extracted'...\n");
    system("mkdir -p extracted");
    res = paf_extract_binary(out_paf, "extracted", 1);
    if (res != 0) {
        printf("[FAILED] paf_extract_binary returned %d\n", res);
        return 1;
    }
    
    printf("3. Verifying files...\n");
    // Simple verification via diff
    int diff_res = system("diff -r test_data extracted");
    if (diff_res == 0) {
        printf("[SUCCESS] All files match!\n");
    } else {
        printf("[FAILED] Files do not match.\n");
        return 1;
    }
    
    return 0;
}
