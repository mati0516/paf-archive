#include "paf_generator.h"
#include "paf_extractor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#define FILE_COUNT 100000

double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main() {
    printf("--- PAF v1.0.0 Performance Run (100,000 Files) ---\n");

    // 1. Archive Creation
    printf("Phase 2: Creating PAF archive from bench_data...\n");
    paf_generator_t gen;
    paf_generator_init(&gen);
    
    double start_paf = get_time();
    for(int i=0; i<FILE_COUNT; i++) {
        char path[256];
        sprintf(path, "bench_data/f%d.bin", i);
        // We read from disk to be realistic
        FILE* fp = fopen(path, "rb");
        if(fp) {
            uint8_t buf[1024];
            fread(buf, 1, 1024, fp);
            paf_generator_add_file(&gen, path, buf, 1024);
            fclose(fp);
        }
    }
    paf_generator_finalize(&gen, "bench.paf");
    double end_paf = get_time();
    printf("PAF Creation: %.2f sec (%.0f files/sec)\n", 
           end_paf - start_paf, FILE_COUNT / (end_paf - start_paf));

    // 2. Extraction
    printf("Phase 3: PAF Extraction (Verification)...\n");
    paf_extractor_t ext;
    paf_extractor_open(&ext, "bench.paf");
    
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
