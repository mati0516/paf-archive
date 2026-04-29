#include "paf_generator.h"
#include "paf_extractor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <direct.h>

#define FILE_COUNT 100000
#define FILE_SIZE 1024

double get_time() {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / freq.QuadPart;
}

int main() {
    printf("--- PAF v1.0.0 Windows Native Benchmark (100,000 Files) ---\n");

    // 1. Generate Files
    printf("Phase 1: Generating files...\n");
    _mkdir("bench_win_data");
    double start_gen = get_time();
    for(int i=0; i<FILE_COUNT; i++) {
        char path[256];
        sprintf(path, "bench_win_data\\f%d.bin", i);
        FILE* fp = fopen(path, "wb");
        if(fp) {
            uint8_t dummy[FILE_SIZE] = {0};
            fwrite(dummy, 1, FILE_SIZE, fp);
            fclose(fp);
        }
    }
    double end_gen = get_time();
    printf("Generation: %.2f sec\n", end_gen - start_gen);

    // 2. Creation
    printf("Phase 2: Creating Archive...\n");
    paf_generator_t gen;
    paf_generator_init(&gen);
    double start_paf = get_time();
    for(int i=0; i<FILE_COUNT; i++) {
        char path[256];
        sprintf(path, "bench_win_data\\f%d.bin", i);
        uint8_t dummy[FILE_SIZE] = {0};
        paf_generator_add_file(&gen, path, dummy, FILE_SIZE);
    }
    paf_generator_finalize(&gen, "bench_win.paf");
    double end_paf = get_time();
    printf("PAF Creation: %.2f sec (%.0f files/sec)\n", 
           end_paf - start_paf, FILE_COUNT / (end_paf - start_paf));

    // 3. Extraction
    printf("Phase 3: Extraction (Smart Overwrite Enabled)...\n");
    paf_extractor_t ext;
    paf_extractor_open(&ext, "bench_win.paf");
    double start_ext = get_time();
    for(uint32_t i=0; i<ext.header.file_count; i++) {
        char path[256];
        uint8_t* data;
        uint64_t size;
        paf_extractor_get_file(&ext, i, path, sizeof(path), &data, &size);
        if(data) free(data);
    }
    double end_ext = get_time();
    printf("PAF Extraction: %.2f sec (%.0f files/sec)\n", 
           end_ext - start_ext, FILE_COUNT / (end_ext - start_ext));

    paf_extractor_close(&ext);
    return 0;
}
