#include "libpaf.h"
#include "paf_delta.h"
#include "paf_extractor.h"
#include "paf_gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

static int g_run = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    g_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [line %d]: %s\n", __LINE__, msg); \
        g_fail++; \
    } else { \
        printf("  ok: %s\n", msg); \
    } \
} while (0)

static int write_file(const char* path, const char* content) {
    FILE* fp = fopen(path, "w");
    if (!fp) return -1;
    fputs(content, fp);
    fclose(fp);
    return 0;
}

static char* read_file(const char* path) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) { fclose(fp); return NULL; }
    if (fread(buf, 1, (size_t)len, fp) != (size_t)len) {
        free(buf); fclose(fp); return NULL;
    }
    buf[len] = '\0';
    fclose(fp);
    return buf;
}

// ── Tests ────────────────────────────────────────────────────────────────────

static void test_create_extract(void) {
    printf("\n[test_create_extract]\n");

    MKDIR("/tmp/paf_src");
    CHECK(write_file("/tmp/paf_src/a.txt", "hello") == 0, "write a.txt");
    CHECK(write_file("/tmp/paf_src/b.txt", "world") == 0, "write b.txt");

    const char* inputs[] = { "/tmp/paf_src" };
    CHECK(paf_create_binary("/tmp/paf_test.paf", inputs, 1, NULL, 1) == 0,
          "paf_create_binary");
    CHECK(paf_extract_binary("/tmp/paf_test.paf", "/tmp/paf_out", 1) == 0,
          "paf_extract_binary");

    char* a = read_file("/tmp/paf_out/a.txt");
    CHECK(a && strcmp(a, "hello") == 0, "a.txt content matches");
    free(a);

    char* b = read_file("/tmp/paf_out/b.txt");
    CHECK(b && strcmp(b, "world") == 0, "b.txt content matches");
    free(b);

    remove("/tmp/paf_test.paf");
}

static void test_list(void) {
    printf("\n[test_list]\n");

    const char* inputs[] = { "/tmp/paf_src" };
    CHECK(paf_create_binary("/tmp/paf_list.paf", inputs, 1, NULL, 1) == 0,
          "paf_create_binary for list");

    PafList list = {0};
    CHECK(paf_list_binary("/tmp/paf_list.paf", &list) == 0, "paf_list_binary");
    CHECK(list.count == 2, "list.count == 2");
    free_paf_list(&list);
    remove("/tmp/paf_list.paf");
}

static void test_index_only_delta_no_change(void) {
    printf("\n[test_index_only_delta_no_change]\n");

    const char* inp[] = { "/tmp/paf_src" };
    CHECK(paf_create_index_only("/tmp/paf_idx1.paf", inp, 1, NULL) == 0, "index 1");
    CHECK(paf_create_index_only("/tmp/paf_idx2.paf", inp, 1, NULL) == 0, "index 2");

    paf_delta_t delta = {0};
    CHECK(paf_delta_calculate("/tmp/paf_idx1.paf", "/tmp/paf_idx2.paf", &delta) == 0,
          "delta_calculate same");
    CHECK(delta.count == 0, "no changes when identical");

    paf_delta_free(&delta);
    remove("/tmp/paf_idx1.paf");
    remove("/tmp/paf_idx2.paf");
}

static void test_index_only_delta_one_change(void) {
    printf("\n[test_index_only_delta_one_change]\n");

    MKDIR("/tmp/paf_v2");
    CHECK(write_file("/tmp/paf_v2/a.txt", "v2")   == 0, "write v2/a.txt");
    CHECK(write_file("/tmp/paf_v2/b.txt", "world") == 0, "write v2/b.txt");

    const char* inp_v1[] = { "/tmp/paf_src" };
    const char* inp_v2[] = { "/tmp/paf_v2"  };
    CHECK(paf_create_index_only("/tmp/paf_v1.idx", inp_v1, 1, NULL) == 0, "index v1");
    CHECK(paf_create_index_only("/tmp/paf_v2.idx", inp_v2, 1, NULL) == 0, "index v2");

    paf_delta_t delta = {0};
    CHECK(paf_delta_calculate("/tmp/paf_v1.idx", "/tmp/paf_v2.idx", &delta) == 0,
          "delta_calculate");
    CHECK(delta.count == 1, "exactly one changed file");
    if (delta.count == 1) {
        CHECK(delta.entries[0].status == PAF_DELTA_UPDATED, "status == UPDATED");
        CHECK(strstr(delta.entries[0].path, "a.txt") != NULL, "changed file is a.txt");
    }

    paf_delta_free(&delta);
    remove("/tmp/paf_v1.idx");
    remove("/tmp/paf_v2.idx");
}

static void test_patch_apply_from_dir(void) {
    printf("\n[test_patch_apply_from_dir]\n");

    // dst starts at v1 content
    MKDIR("/tmp/paf_dst");
    CHECK(write_file("/tmp/paf_dst/a.txt", "hello") == 0, "setup dst/a.txt");
    CHECK(write_file("/tmp/paf_dst/b.txt", "world") == 0, "setup dst/b.txt");

    const char* inp_dst[] = { "/tmp/paf_dst" };
    const char* inp_v2[]  = { "/tmp/paf_v2"  };
    CHECK(paf_create_index_only("/tmp/paf_dst.idx", inp_dst, 1, NULL) == 0, "index dst");
    CHECK(paf_create_index_only("/tmp/paf_src.idx", inp_v2,  1, NULL) == 0, "index src");

    paf_delta_t delta = {0};
    CHECK(paf_delta_calculate("/tmp/paf_dst.idx", "/tmp/paf_src.idx", &delta) == 0,
          "delta_calculate");
    CHECK(delta.count == 1, "one file to update");

    CHECK(paf_patch_apply_from_dir("/tmp/paf_v2", &delta, "/tmp/paf_dst",
                                    NULL, NULL) == 0,
          "patch_apply_from_dir");

    char* a = read_file("/tmp/paf_dst/a.txt");
    CHECK(a && strcmp(a, "v2") == 0, "a.txt updated to v2");
    free(a);

    char* b = read_file("/tmp/paf_dst/b.txt");
    CHECK(b && strcmp(b, "world") == 0, "b.txt unchanged");
    free(b);

    paf_delta_free(&delta);
    remove("/tmp/paf_dst.idx");
    remove("/tmp/paf_src.idx");
}

static void test_gpu_search(void) {
    printf("\n[test_gpu_search]\n");

    const char* inputs[] = { "/tmp/paf_src" };
    CHECK(paf_create_binary("/tmp/paf_search.paf", inputs, 1, NULL, 1) == 0,
          "create for search");

    paf_extractor_t ext = {0};
    CHECK(paf_extractor_open(&ext, "/tmp/paf_search.paf") == 0, "extractor_open");

    uint32_t indices[16];
    int n = paf_gpu_search_files(&ext, "*.txt", indices, 16);
    CHECK(n == 2, "search *.txt finds 2 files");

    n = paf_gpu_search_files(&ext, "a.txt", indices, 16);
    CHECK(n == 1, "search a.txt finds 1 file");

    n = paf_gpu_search_files(&ext, "*.png", indices, 16);
    CHECK(n == 0, "search *.png finds 0 files");

    paf_extractor_close(&ext);
    remove("/tmp/paf_search.paf");
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main(void) {
    printf("=== PAF test suite ===\n");

    test_create_extract();
    test_list();
    test_index_only_delta_no_change();
    test_index_only_delta_one_change();
    test_patch_apply_from_dir();
    test_gpu_search();

    printf("\n=== %d / %d passed ===\n", g_run - g_fail, g_run);
    return g_fail > 0 ? 1 : 0;
}
