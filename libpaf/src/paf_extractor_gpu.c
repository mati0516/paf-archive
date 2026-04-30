#include "paf_extractor.h"
#include "paf_gpu.h"
#include "paf_gpu_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// DirectStorage I/O backend declaration.
// On non-Windows or CI builds a stub that always returns -1 is used so the
// compute path can still fall back gracefully.
#if defined(_WIN32) && !defined(PAF_CI_BUILD)
int paf_io_directstorage_load(const wchar_t* path, uint64_t offset,
                               uint64_t size, void* destination);
#else
static int paf_io_directstorage_load(const void* path, uint64_t offset,
                                      uint64_t size, void* destination) {
    (void)path; (void)offset; (void)size; (void)destination;
    return -1;
}
#endif

// ── Phase helpers ────────────────────────────────────────────────────────────

// Load one index entry's data into *buf_out (caller must free).
// Returns 0 on success, non-zero on any I/O error.
static int load_entry(const char* paf_path, const paf_index_entry_t* entry,
                       void** buf_out) {
    *buf_out = NULL;
    if (entry->data_size == 0) return 0;

    void* buf = malloc((size_t)entry->data_size);
    if (!buf) return -1;

    uint64_t offset = sizeof(paf_header_t) + entry->data_offset;
    int err = -1;

#if defined(_WIN32) && !defined(PAF_CI_BUILD)
    if (paf_dstorage_is_available()) {
        wchar_t wpath[1024];
        mbstowcs(wpath, paf_path, sizeof(wpath) / sizeof(wpath[0]) - 1);
        wpath[sizeof(wpath) / sizeof(wpath[0]) - 1] = L'\0';
        err = paf_io_directstorage_load(wpath, offset, entry->data_size, buf);
    }
#endif

    if (err != 0) {
        // Fallback: regular fread
        FILE* fp = fopen(paf_path, "rb");
        if (!fp) { free(buf); return -1; }
        if (fseek(fp, (long)offset, SEEK_SET) != 0 ||
            fread(buf, 1, (size_t)entry->data_size, fp) != (size_t)entry->data_size) {
            fclose(fp);
            free(buf);
            return -1;
        }
        fclose(fp);
    }

    *buf_out = buf;
    return 0;
}

// ── Public API ───────────────────────────────────────────────────────────────

int paf_extractor_gpu_run(paf_extractor_t* ext, const char* path) {
    if (!ext || !path) return -1;

    paf_gpu_info_t gpu;
    paf_gpu_get_info(&gpu);
    printf("Detected hardware: %s\n", gpu.device_name);
    printf("  VRAM         : %llu MB\n",
           (unsigned long long)(gpu.total_vram / 1024 / 1024));
    printf("  GPU compute  : %s (%s)\n",
           gpu.supports_gpu_compute ? "yes" : "no", gpu.compute_backend);
    printf("  Fast I/O     : %s\n",
           gpu.supports_direct_io ? "yes (DirectStorage/GDS)" : "no");

    paf_batch_config_t batch = paf_gpu_calculate_batch(
        gpu.total_vram, ext->header.file_count, 1024 * 1024);

    uint32_t io_errors      = 0;
    uint32_t compute_errors = 0;
    uint32_t processed      = 0;

    while (processed < ext->header.file_count) {
        uint32_t batch_count = ext->header.file_count - processed;
        if (batch_count > batch.files_per_batch)
            batch_count = batch.files_per_batch;

        // ── Phase 1: I/O ──────────────────────────────────────────────────
        // Allocate a slot per entry; entries that fail load are left NULL.
        void** bufs = (void**)calloc(batch_count, sizeof(void*));
        if (!bufs) return -1;

        for (uint32_t i = 0; i < batch_count; i++) {
            const paf_index_entry_t* entry = &ext->entries[processed + i];
            if (load_entry(path, entry, &bufs[i]) != 0) {
                fprintf(stderr, "error: I/O failed for entry %u "
                        "(offset=%llu size=%llu)\n",
                        processed + i,
                        (unsigned long long)(sizeof(paf_header_t) + entry->data_offset),
                        (unsigned long long)entry->data_size);
                io_errors++;
            }
        }

        // ── Phase 2: Compute (SHA-256 verification) ───────────────────────
        // GPU path via paf_cuda.dll when available; CPU fallback otherwise.
        // (Full implementation populates verified[] flags; omitted here.)
        for (uint32_t i = 0; i < batch_count; i++) {
            if (!bufs[i]) continue;  // I/O failed — already counted above
            (void)compute_errors;    // placeholder until compute pass is wired
            free(bufs[i]);
        }
        free(bufs);

        processed += batch_count;
    }

    if (io_errors > 0)
        fprintf(stderr, "warning: %u I/O error(s) during GPU extraction\n", io_errors);

    return (int)(io_errors + compute_errors) > 0 ? -(int)(io_errors + compute_errors) : 0;
}
