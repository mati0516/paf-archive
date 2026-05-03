#include "libpaf.h"
#include "paf_extractor.h"
#include "paf_gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
static double now_sec(void) {
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    return (double)t.QuadPart / (double)f.QuadPart;
}
#else
static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s create   <src_dir>  <out.paf>\n"
        "  %s extract  <in.paf>   <out_dir>\n"
        "  %s extract_gpu <in.paf> <out_dir>\n"
        "  %s info     <in.paf>\n",
        prog, prog, prog, prog);
}

static int cmd_create(const char* src_dir, const char* out_paf) {
    const char* inputs[] = { src_dir };
    double start = now_sec();
    int res = paf_create_binary(out_paf, inputs, 1, NULL, 1);
    double end = now_sec();
    if (res != 0) {
        fprintf(stderr, "RESULT: FAIL (paf_create_binary returned %d)\n", res);
        return 1;
    }
    printf("RESULT: OK, Time: %.3f sec\n", end - start);
    return 0;
}

static int cmd_extract(const char* paf_path, const char* out_dir) {
    double start = now_sec();
    int res = paf_extract_binary(paf_path, out_dir, 1);
    double end = now_sec();
    if (res != 0) {
        fprintf(stderr, "RESULT: FAIL (paf_extract_binary returned %d)\n", res);
        return 1;
    }
    printf("RESULT: OK, Time: %.3f sec\n", end - start);
    return 0;
}

static int cmd_extract_gpu(const char* paf_path, const char* out_dir) {
    paf_extractor_t ext;
    if (paf_extractor_open(&ext, paf_path) != 0) {
        fprintf(stderr, "RESULT: FAIL (could not open %s)\n", paf_path);
        return 1;
    }

    double start = now_sec();
    int res = paf_extractor_gpu_run(&ext, paf_path, out_dir);
    double end = now_sec();

    paf_extractor_close(&ext);

    /* paf_extractor_gpu_run returns 0 on success, negative on error */
    if (res == 0) {
        printf("RESULT: OK, Time: %.3f sec\n", end - start);
        return 0;
    } else {
        fprintf(stderr, "RESULT: FAIL (errors %d)\n", -res);
        return 1;
    }
}

static int cmd_info(const char* paf_path) {
    paf_extractor_t ext;
    if (paf_extractor_open(&ext, paf_path) != 0) {
        fprintf(stderr, "Could not open %s\n", paf_path);
        return 1;
    }
    printf("file_count : %u\n", ext.header.file_count);
    printf("index_offset: %llu\n", (unsigned long long)ext.header.index_offset);
    printf("path_offset : %llu\n", (unsigned long long)ext.header.path_offset);

    paf_gpu_info_t gpu;
    paf_gpu_get_info(&gpu);
    printf("gpu_device : %s\n", gpu.device_name);
    printf("total_vram : %llu MB\n", (unsigned long long)(gpu.total_vram / 1024 / 1024));

    paf_extractor_close(&ext);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) { usage(argv[0]); return 1; }

    const char* cmd = argv[1];
    if (strcmp(cmd, "create") == 0 && argc == 4)
        return cmd_create(argv[2], argv[3]);
    if (strcmp(cmd, "extract") == 0 && argc == 4)
        return cmd_extract(argv[2], argv[3]);
    if (strcmp(cmd, "extract_gpu") == 0 && argc == 4)
        return cmd_extract_gpu(argv[2], argv[3]);
    if (strcmp(cmd, "info") == 0 && argc == 3)
        return cmd_info(argv[2]);

    usage(argv[0]);
    return 1;
}
