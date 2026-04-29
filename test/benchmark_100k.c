#include "paf_generator.h"
#include "paf_extractor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <inttypes.h>

#define FILE_COUNT 100000
#define FILE_SIZE 1024 // 1KB each

double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main() {
    printf("--- PAF v1.0.0 Benchmark (100,000 Files) ---\n");

    // 1. Generate 100k dummy files
    printf("Phase 1: Generating %d files...\n", FILE_COUNT);
    system("rm -rf bench_data && mkdir -p bench_data");
    double start_gen = get_time();
    for(int i=0; i<FILE_COUNT; i++) {
        char path[256];
        sprintf(path, "bench_data/f%d.bin", i);
        FILE* fp = fopen(path, "wb");
        if(fp) {
            uint8_t dummy[FILE_SIZE] = {0};
            fwrite(dummy, 1, FILE_SIZE, fp);
            fclose(fp);
        }
    }
    double end_gen = get_time();
    printf("Generation took: %.2f seconds\n", end_gen - start_gen);

    // 2. Archive Creation
    printf("Phase 2: Creating PAF archive...\n");
    paf_generator_t gen;
    paf_generator_init(&gen);
    
    double start_paf = get_time();
    for(int i=0; i<FILE_COUNT; i++) {
        char path[256];
        sprintf(path, "bench_data/f%d.bin", i);
        // Optimization: In real world, we stream, but for bench we just add
        uint8_t dummy[FILE_SIZE] = {0};
        paf_generator_add_file(&gen, path, dummy, FILE_SIZE);
    }
    paf_generator_finalize(&gen, "bench.paf");
    double end_paf = get_time();
    printf("PAF Creation took: %.2f seconds (%.0f files/sec)\n", 
           end_paf - start_paf, FILE_COUNT / (end_paf - start_paf));

    // 3. Extraction (Smart Overwrite disabled for first run)
    printf("Phase 3: PAF Extraction...\n");
    paf_extractor_t ext;
    paf_extractor_open(&ext, "bench.paf");
    
    double start_ext = get_time();
    for(uint32_t i=0; i<ext.header.file_count; i++) {
        char path[256];
        uint8_t* data;
        uint64_t size;
        // Mock extract (just metadata and path read to measure logic overhead)
        paf_extractor_get_file(&ext, i, path, sizeof(path), &data, &size);
        if(data) free(data);
    }
    double end_ext = get_time();
    printf("PAF Extraction took: %.2f seconds (%.0f files/sec)\n", 
           end_ext - start_ext, FILE_COUNT / (end_ext - start_ext));

    paf_extractor_close(&ext);
    printf("--- Benchmark Finished ---\n");
    return 0;
}
