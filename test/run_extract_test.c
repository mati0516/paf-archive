#include <stdio.h>
#include <stdlib.h>
#include "libpaf.h"

int main() {
    printf("Testing extraction of malicious.paf into 'output' folder...\n");
    system("mkdir -p output");
    int result = paf_extract_binary("malicious.paf", "output", 1);
    printf("Extraction result: %d\n", result);
    
    // Check if secret.txt exists in the parent directory
    FILE* f = fopen("secret.txt", "r");
    if (f) {
        printf("[FAILED] Path Traversal detected! secret.txt was created.\n");
        fclose(f);
        return 1;
    } else {
        printf("[SUCCESS] Path Traversal blocked. secret.txt does not exist.\n");
    }
    
    return 0;
}
