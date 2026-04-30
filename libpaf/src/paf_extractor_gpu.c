#define LIBPAF_EXPORTS
#include "paf_extractor.h"
#include "paf_gpu.h"
#include "paf_gpu_loader.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32) && !defined(__ANDROID__) && !defined(__linux__)
#include <windows.h>
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

// DirectStorage I/O backend. Non-Windows / CI builds use a stub.
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

// ── Internal helpers ─────────────────────────────────────────────────────────

static int is_safe_path(const char* path) {
    if (!path || path[0] == '/' || path[0] == '\\') return 0;
    const char* p = path;
    while (*p) {
        if (p[0] == '.' && p[1] == '.') {
            if (p[2] == '/' || p[2] == '\\' || p[2] == '\0') return 0;
        }
        p++;
    }
    return 1;
}

static void ensure_dir(const char* full_path) {
    char buf[2048];
    strncpy(buf, full_path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char* p = buf + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            MKDIR(buf);
            *p = '/';
        }
    }
}

static int read_entry_path(paf_extractor_t* ext, uint32_t idx,
                            char* buf, size_t buflen) {
    const paf_index_entry_t* e = &ext->entries[idx];
    if (e->path_length == 0 || e->path_length >= buflen) return -1;
    long pos = (long)(ext->header.path_offset + e->path_buffer_offset);
    if (fseek(ext->fp, pos, SEEK_SET) != 0) return -1;
    if (fread(buf, 1, e->path_length, ext->fp) != e->path_length) return -1;
    buf[e->path_length] = '\0';
    return 0;
}

// Load entry data using DirectStorage when available, fread otherwise.
static int load_entry_data(const char* paf_path,
                            const paf_index_entry_t* entry,
                            uint8_t* dst) {
    if (entry->data_size == 0) return 0;
    uint64_t offset = sizeof(paf_header_t) + entry->data_offset;

#if defined(_WIN32) && !defined(PAF_CI_BUILD)
    if (paf_dstorage_is_available()) {
        wchar_t wpath[1024];
        mbstowcs(wpath, paf_path, sizeof(wpath) / sizeof(wpath[0]) - 1);
        wpath[sizeof(wpath) / sizeof(wpath[0]) - 1] = L'\0';
        if (paf_io_directstorage_load(wpath, offset, entry->data_size, dst) == 0)
            return 0;
        // fall through to fread on DirectStorage failure
    }
#else
    (void)paf_path;
#endif

    FILE* fp = fopen(paf_path, "rb");
    if (!fp) return -1;
    int ok = fseek(fp, (long)offset, SEEK_SET) == 0 &&
             fread(dst, 1, (size_t)entry->data_size, fp) == (size_t)entry->data_size;
    fclose(fp);
    return ok ? 0 : -1;
}

// ── Public API ───────────────────────────────────────────────────────────────

// GPU-accelerated batch extraction.
// Phase 1 : I/O    — load each batch into a flat contiguous buffer
//                    (DirectStorage → fread fallback)
// Phase 2 : Compute — SHA-256 batch hash (GPU → CPU fallback)
// Phase 3 : Write  — verify hash then write to out_dir
int paf_extractor_gpu_run(paf_extractor_t* ext,
                           const char* paf_path,
                           const char* out_dir) {
    if (!ext || !paf_path || !out_dir) return -1;

    paf_gpu_info_t gpu;
    paf_gpu_get_info(&gpu);
    printf("Detected hardware : %s\n", gpu.device_name);
    printf("  VRAM            : %llu MB\n",
           (unsigned long long)(gpu.total_vram / 1024 / 1024));
    printf("  GPU compute     : %s\n",
           paf_cuda_is_available()   ? "CUDA"
         : paf_vulkan_is_available() ? "Vulkan"
         : "CPU fallback");
    printf("  Fast I/O        : %s\n",
           paf_dstorage_is_available() ? "DirectStorage" : "fread");
    printf("Extracting %u file(s) to %s\n", ext->header.file_count, out_dir);

    paf_batch_config_t batch = paf_gpu_calculate_batch(
        gpu.total_vram, ext->header.file_count, 1024 * 1024);

    uint32_t io_errors   = 0;
    uint32_t hash_errors = 0;
    uint32_t processed   = 0;

    while (processed < ext->header.file_count) {
        uint32_t n = ext->header.file_count - processed;
        if (n > batch.files_per_batch) n = batch.files_per_batch;

        // Pre-pass: resolve file paths for this batch
        char (*paths)[1024] = (char (*)[1024])calloc(n, sizeof(*paths));
        uint8_t* io_failed  = (uint8_t*)calloc(n, 1);
        if (!paths || !io_failed) {
            free(paths); free(io_failed);
            return -1;
        }
        for (uint32_t i = 0; i < n; i++) {
            if (read_entry_path(ext, processed + i, paths[i], 1024) != 0 ||
                !is_safe_path(paths[i]))
                paths[i][0] = '\0';
        }

        // Build flat buffer layout
        uint64_t* offsets = (uint64_t*)malloc(n * sizeof(uint64_t));
        uint64_t* sizes   = (uint64_t*)malloc(n * sizeof(uint64_t));
        if (!offsets || !sizes) {
            free(paths); free(io_failed); free(offsets); free(sizes);
            return -1;
        }
        uint64_t total_size = 0;
        for (uint32_t i = 0; i < n; i++) {
            offsets[i]  = total_size;
            sizes[i]    = ext->entries[processed + i].data_size;
            total_size += sizes[i];
        }

        // Phase 1: I/O — calloc so I/O-failed slots are zero-filled
        uint8_t* flat = total_size > 0 ? (uint8_t*)calloc(1, (size_t)total_size) : NULL;
        if (total_size > 0 && !flat) {
            free(paths); free(io_failed); free(offsets); free(sizes);
            return -1;
        }
        for (uint32_t i = 0; i < n; i++) {
            if (sizes[i] == 0) continue;
            if (load_entry_data(paf_path, &ext->entries[processed + i],
                                flat + offsets[i]) != 0) {
                fprintf(stderr, "error: I/O failed for entry %u (%s)\n",
                        processed + i, paths[i]);
                io_failed[i] = 1;
                io_errors++;
            }
        }

        // Phase 2: Compute — batch SHA-256 (GPU → CPU fallback)
        uint8_t* hashes = (uint8_t*)malloc(n * 32);
        if (!hashes) {
            free(flat); free(paths); free(io_failed); free(offsets); free(sizes);
            return -1;
        }
        // Phase 2: CUDA → Vulkan → CPU fallback
        int gpu_ok = total_size > 0 &&
                     ((paf_cuda_is_available() && g_paf_cuda_hash_flat != NULL &&
                       g_paf_cuda_hash_flat(flat, offsets, sizes, n, hashes) == 0)
                    || (paf_vulkan_is_available() && g_paf_vulkan_hash_flat != NULL &&
                        g_paf_vulkan_hash_flat(flat, offsets, sizes, n, hashes) == 0));
        if (!gpu_ok) {
            for (uint32_t i = 0; i < n; i++) {
                if (sizes[i] == 0 || io_failed[i]) {
                    memset(hashes + i * 32, 0, 32);
                    continue;
                }
                sha256_context_t ctx;
                sha256_init(&ctx);
                sha256_update(&ctx, flat + offsets[i], (size_t)sizes[i]);
                sha256_final(&ctx, hashes + i * 32);
            }
        }

        // Phase 3: Verify hash then write to out_dir
        for (uint32_t i = 0; i < n; i++) {
            if (io_failed[i] || paths[i][0] == '\0') continue;

            const paf_index_entry_t* entry = &ext->entries[processed + i];

            if (memcmp(hashes + i * 32, entry->hash, 32) != 0) {
                fprintf(stderr, "error: hash mismatch for entry %u (%s)\n",
                        processed + i, paths[i]);
                hash_errors++;
                continue;
            }

            char out_path[2048];
            if (snprintf(out_path, sizeof(out_path), "%s/%s", out_dir, paths[i]) >=
                (int)sizeof(out_path))
                continue;
            ensure_dir(out_path);

            FILE* fp = fopen(out_path, "wb");
            if (!fp) { io_errors++; continue; }
            if (sizes[i] > 0 &&
                fwrite(flat + offsets[i], 1, (size_t)sizes[i], fp) != (size_t)sizes[i])
                io_errors++;
            fclose(fp);
        }

        free(flat);
        free(hashes);
        free(offsets);
        free(sizes);
        free(paths);
        free(io_failed);

        processed += n;
        printf("  %u / %u files done\n", processed, ext->header.file_count);
    }

    uint32_t total_errors = io_errors + hash_errors;
    if (total_errors > 0)
        fprintf(stderr, "warning: %u I/O error(s), %u hash mismatch(es)\n",
                io_errors, hash_errors);
    return total_errors > 0 ? -(int)total_errors : 0;
}
