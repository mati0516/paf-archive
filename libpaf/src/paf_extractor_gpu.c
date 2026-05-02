#define LIBPAF_EXPORTS
#include "paf_extractor.h"
#include "paf_gpu.h"
#include "paf_gpu_loader.h"
#include "paf_sha256_hw.h"
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

// DirectStorage batch load — declared only where it can be called (Windows non-CI).
// The implementation lives in win/paf_io_directstorage.cpp.
#if defined(_WIN32) && !defined(PAF_CI_BUILD)
int paf_io_directstorage_load_batch(const wchar_t* path,
                                    const uint64_t* paf_offsets,
                                    const uint64_t* sizes,
                                    uint8_t* flat,
                                    const uint64_t* dst_offsets,
                                    uint32_t count,
                                    uint8_t* io_failed);
#endif

// ── Helpers ───────────────────────────────────────────────────────────────────

static int is_safe_path(const char* path) {
    if (!path || path[0] == '/' || path[0] == '\\') return 0;
    for (const char* p = path; *p; p++) {
        if (p[0] == '.' && p[1] == '.') {
            if (p[2] == '/' || p[2] == '\\' || p[2] == '\0') return 0;
        }
    }
    return 1;
}

static void ensure_dir(const char* full_path) {
    char buf[2048];
    strncpy(buf, full_path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char* p = buf + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            *p = '\0'; MKDIR(buf); *p = '/';
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

// ── Phase 1: Batch I/O ────────────────────────────────────────────────────────
// Loads all n files in one pass.
// DS path: enqueues all requests and submits once per DS_BATCH_CAP entries.
// fread path: opens the PAF file once and fseeks per entry.

static void phase1_io(paf_extractor_t* ext,
                      const char* paf_path,
                      uint32_t processed, uint32_t n,
                      const uint64_t* offsets,   // flat dst offsets
                      const uint64_t* sizes,
                      uint8_t* flat,
                      uint8_t* io_failed)
{
#if defined(_WIN32) && !defined(PAF_CI_BUILD)
    if (paf_dstorage_is_available()) {
        // Build per-entry absolute PAF offsets for the batch DS call.
        uint64_t* paf_offs = (uint64_t*)malloc(n * sizeof(uint64_t));
        if (paf_offs) {
            for (uint32_t i = 0; i < n; i++)
                paf_offs[i] = sizeof(paf_header_t) +
                              ext->entries[processed + i].data_offset;

            wchar_t wpath[1024];
            mbstowcs(wpath, paf_path, sizeof(wpath)/sizeof(wpath[0]) - 1);
            wpath[sizeof(wpath)/sizeof(wpath[0]) - 1] = L'\0';

            if (paf_io_directstorage_load_batch(wpath, paf_offs, sizes,
                                                flat, offsets, n, io_failed) == 0) {
                free(paf_offs);
                return;
            }
            free(paf_offs);
        }
        // Fall through to fread on DS failure.
    }
#endif

    // fread fallback: open once, seek per entry.
    FILE* fp = fopen(paf_path, "rb");
    if (!fp) {
        memset(io_failed, 1, n);
        return;
    }
    for (uint32_t i = 0; i < n; i++) {
        if (sizes[i] == 0) continue;
        long pos = (long)(sizeof(paf_header_t) +
                         ext->entries[processed + i].data_offset);
        if (fseek(fp, pos, SEEK_SET) != 0 ||
            fread(flat + offsets[i], 1, (size_t)sizes[i], fp) != (size_t)sizes[i])
            io_failed[i] = 1;
    }
    fclose(fp);
}

// ── Phase 3: Parallel file write ─────────────────────────────────────────────
// nCPU×2 threads, each owning a static contiguous file range.
// Per-thread last_dir cache skips ensure_dir when consecutive files in the
// chunk share the same parent directory (common for sorted game asset trees).

typedef struct {
    uint32_t        start;
    uint32_t        end;
    const char    (*paths)[1024];
    const uint8_t*  io_failed;
    const uint8_t*  hash_ok;
    const uint64_t* offsets;
    const uint64_t* sizes;
    const uint8_t*  flat;
    const char*     out_dir;
    char            last_dir[2048];
    uint32_t        errors;  // owned exclusively by this thread
} chunk_ctx_t;

static void write_chunk(chunk_ctx_t* c) {
    c->last_dir[0] = '\0';
    for (uint32_t i = c->start; i < c->end; i++) {
        if (c->io_failed[i] || !c->hash_ok[i] || c->paths[i][0] == '\0') continue;
        char out_path[2048];
        if (snprintf(out_path, sizeof(out_path), "%s/%s",
                     c->out_dir, c->paths[i]) >= (int)sizeof(out_path)) continue;

        // Extract parent directory to compare against last_dir cache.
        char dir[2048];
        strncpy(dir, out_path, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
        char* sep = strrchr(dir, '/');
#if defined(_WIN32)
        { char* bk = strrchr(dir, '\\'); if (!sep || (bk && bk > sep)) sep = bk; }
#endif
        if (sep) *sep = '\0';
        if (strcmp(dir, c->last_dir) != 0) {
            ensure_dir(out_path);
            strncpy(c->last_dir, dir, sizeof(c->last_dir) - 1);
            c->last_dir[sizeof(c->last_dir) - 1] = '\0';
        }

        FILE* fp = fopen(out_path, "wb");
        if (!fp) { c->errors++; continue; }
        if (c->sizes[i] > 0 &&
            fwrite(c->flat + c->offsets[i], 1, (size_t)c->sizes[i], fp) !=
            (size_t)c->sizes[i])
            c->errors++;
        fclose(fp);
    }
}

static void fill_chunk(chunk_ctx_t* c, uint32_t start, uint32_t end,
                       const char (*paths)[1024],
                       const uint8_t* io_failed, const uint8_t* hash_ok,
                       const uint64_t* offsets, const uint64_t* sizes,
                       const uint8_t* flat, const char* out_dir) {
    c->start = start; c->end = end;
    c->paths = paths; c->io_failed = io_failed; c->hash_ok = hash_ok;
    c->offsets = offsets; c->sizes = sizes; c->flat = flat; c->out_dir = out_dir;
}

#if defined(_WIN32) && !defined(PAF_CI_BUILD)

static DWORD WINAPI write_chunk_fn(LPVOID arg) {
    write_chunk((chunk_ctx_t*)arg);
    return 0;
}

static uint32_t phase3_write_parallel(
        uint32_t n, const char (*paths)[1024],
        const uint8_t* io_failed, const uint8_t* hash_ok,
        const uint64_t* offsets, const uint64_t* sizes,
        const uint8_t* flat, const char* out_dir)
{
    if (n == 0) return 0;

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int nt = (int)si.dwNumberOfProcessors * 2;
    if (nt > MAXIMUM_WAIT_OBJECTS) nt = MAXIMUM_WAIT_OBJECTS;
    if (nt < 1) nt = 1;
    if ((uint32_t)nt > n) nt = (int)n;

    chunk_ctx_t* ctx = (chunk_ctx_t*)calloc(nt, sizeof(chunk_ctx_t));
    if (!ctx) {
        chunk_ctx_t c;
        memset(&c, 0, sizeof(c));
        fill_chunk(&c, 0, n, paths, io_failed, hash_ok, offsets, sizes, flat, out_dir);
        write_chunk(&c);
        return c.errors;
    }

    HANDLE handles[MAXIMUM_WAIT_OBJECTS];
    memset(handles, 0, sizeof(handles));
    for (int t = 0; t < nt; t++) {
        uint32_t start = (uint32_t)((uint64_t)t * n / nt);
        uint32_t end   = (t == nt - 1) ? n : (uint32_t)((uint64_t)(t + 1) * n / nt);
        fill_chunk(&ctx[t], start, end,
                   paths, io_failed, hash_ok, offsets, sizes, flat, out_dir);
        handles[t] = CreateThread(NULL, 0, write_chunk_fn, &ctx[t], 0, NULL);
        if (!handles[t]) write_chunk(&ctx[t]);  // run inline on thread creation failure
    }

    HANDLE valid[MAXIMUM_WAIT_OBJECTS];
    int n_valid = 0;
    for (int t = 0; t < nt; t++)
        if (handles[t]) valid[n_valid++] = handles[t];
    if (n_valid > 0)
        WaitForMultipleObjects(n_valid, valid, TRUE, INFINITE);
    for (int t = 0; t < nt; t++)
        if (handles[t]) CloseHandle(handles[t]);

    uint32_t total = 0;
    for (int t = 0; t < nt; t++) total += ctx[t].errors;
    free(ctx);
    return total;
}

#elif !defined(_WIN32)  // Linux / Android: pthreads

#include <pthread.h>
#define WRITE_THREADS_MAX 32

static void* write_chunk_fn(void* arg) {
    write_chunk((chunk_ctx_t*)arg);
    return NULL;
}

static uint32_t phase3_write_parallel(
        uint32_t n, const char (*paths)[1024],
        const uint8_t* io_failed, const uint8_t* hash_ok,
        const uint64_t* offsets, const uint64_t* sizes,
        const uint8_t* flat, const char* out_dir)
{
    if (n == 0) return 0;

    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    int nt = (ncpu > 0) ? (int)(ncpu * 2) : 2;
    if (nt > WRITE_THREADS_MAX) nt = WRITE_THREADS_MAX;
    if ((uint32_t)nt > n) nt = (int)n;

    chunk_ctx_t* ctx = (chunk_ctx_t*)calloc(nt, sizeof(chunk_ctx_t));
    if (!ctx) {
        chunk_ctx_t c;
        memset(&c, 0, sizeof(c));
        fill_chunk(&c, 0, n, paths, io_failed, hash_ok, offsets, sizes, flat, out_dir);
        write_chunk(&c);
        return c.errors;
    }

    pthread_t threads[WRITE_THREADS_MAX];
    int created[WRITE_THREADS_MAX];
    memset(created, 0, sizeof(created));
    for (int t = 0; t < nt; t++) {
        uint32_t start = (uint32_t)((uint64_t)t * n / nt);
        uint32_t end   = (t == nt - 1) ? n : (uint32_t)((uint64_t)(t + 1) * n / nt);
        fill_chunk(&ctx[t], start, end,
                   paths, io_failed, hash_ok, offsets, sizes, flat, out_dir);
        created[t] = (pthread_create(&threads[t], NULL, write_chunk_fn, &ctx[t]) == 0);
        if (!created[t]) write_chunk(&ctx[t]);
    }
    for (int t = 0; t < nt; t++)
        if (created[t]) pthread_join(threads[t], NULL);

    uint32_t total = 0;
    for (int t = 0; t < nt; t++) total += ctx[t].errors;
    free(ctx);
    return total;
}

#else  // Windows CI: sequential

static uint32_t phase3_write_parallel(
        uint32_t n, const char (*paths)[1024],
        const uint8_t* io_failed, const uint8_t* hash_ok,
        const uint64_t* offsets, const uint64_t* sizes,
        const uint8_t* flat, const char* out_dir)
{
    chunk_ctx_t c;
    memset(&c, 0, sizeof(c));
    fill_chunk(&c, 0, n, paths, io_failed, hash_ok, offsets, sizes, flat, out_dir);
    write_chunk(&c);
    return c.errors;
}

#endif

// ── Public API ────────────────────────────────────────────────────────────────

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
           paf_dstorage_is_available() ? "DirectStorage (batch)" : "fread");
    printf("Extracting %u file(s) to %s\n", ext->header.file_count, out_dir);

    paf_batch_config_t batch = paf_gpu_calculate_batch(
        gpu.total_vram, ext->header.file_count, 1024 * 1024);

    uint32_t io_errors   = 0;
    uint32_t hash_errors = 0;
    uint32_t processed   = 0;

    while (processed < ext->header.file_count) {
        uint32_t n = ext->header.file_count - processed;
        if (n > batch.files_per_batch) n = batch.files_per_batch;

        // Allocate per-batch metadata arrays.
        char     (*paths)[1024] = (char (*)[1024])calloc(n, sizeof(*paths));
        uint8_t*   io_failed    = (uint8_t*)calloc(n, 1);
        uint8_t*   hash_ok      = (uint8_t*)calloc(n, 1);
        uint64_t*  offsets      = (uint64_t*)malloc(n * sizeof(uint64_t));
        uint64_t*  sizes        = (uint64_t*)malloc(n * sizeof(uint64_t));
        uint8_t*   hashes       = (uint8_t*)malloc(n * 32);

        if (!paths || !io_failed || !hash_ok || !offsets || !sizes || !hashes) {
            free(paths); free(io_failed); free(hash_ok);
            free(offsets); free(sizes); free(hashes);
            return -1;
        }

        // Pre-pass: resolve paths and build flat-buffer layout.
        uint64_t total_size = 0;
        for (uint32_t i = 0; i < n; i++) {
            if (read_entry_path(ext, processed + i, paths[i], 1024) != 0 ||
                !is_safe_path(paths[i]))
                paths[i][0] = '\0';
            offsets[i]  = total_size;
            sizes[i]    = ext->entries[processed + i].data_size;
            total_size += sizes[i];
        }

        // Phase 1: I/O — batch DirectStorage or sequential fread.
        uint8_t* flat = total_size > 0 ? (uint8_t*)calloc(1, (size_t)total_size) : NULL;
        if (total_size > 0 && !flat) {
            free(paths); free(io_failed); free(hash_ok);
            free(offsets); free(sizes); free(hashes);
            return -1;
        }

        phase1_io(ext, paf_path, processed, n, offsets, sizes, flat, io_failed);

        for (uint32_t i = 0; i < n; i++) {
            if (io_failed[i]) io_errors++;
        }

        // Phase 2: SHA-256 — GPU (CUDA → Vulkan) → CPU fallback.
        int gpu_ok = total_size > 0 &&
                     ((paf_cuda_is_available()   && g_paf_cuda_hash_flat &&
                       g_paf_cuda_hash_flat(flat, offsets, sizes, n, hashes) == 0)
                   || (paf_vulkan_is_available() && g_paf_vulkan_hash_flat &&
                       g_paf_vulkan_hash_flat(flat, offsets, sizes, n, hashes) == 0));
        if (!gpu_ok) {
            for (uint32_t i = 0; i < n; i++) {
                if (sizes[i] == 0 || io_failed[i]) {
                    memset(hashes + i * 32, 0, 32); continue;
                }
                paf_sha256_compute(flat + offsets[i], (size_t)sizes[i], hashes + i * 32);
            }
        }

        // Verify hashes.
        for (uint32_t i = 0; i < n; i++) {
            if (io_failed[i]) continue;
            if (memcmp(hashes + i * 32, ext->entries[processed + i].hash, 32) != 0) {
                fprintf(stderr, "error: hash mismatch for entry %u (%s)\n",
                        processed + i, paths[i]);
                hash_errors++;
            } else {
                hash_ok[i] = 1;
            }
        }

        // Phase 3: Write verified files (parallel on Windows, sequential elsewhere).
        io_errors += phase3_write_parallel(n, paths, io_failed, hash_ok,
                                           offsets, sizes, flat, out_dir);

        free(flat);
        free(hashes);
        free(offsets);
        free(sizes);
        free(io_failed);
        free(hash_ok);
        free(paths);

        processed += n;
        printf("  %u / %u files done\n", processed, ext->header.file_count);
    }

    uint32_t total_errors = io_errors + hash_errors;
    if (total_errors > 0)
        fprintf(stderr, "warning: %u I/O error(s), %u hash mismatch(es)\n",
                io_errors, hash_errors);
    return total_errors > 0 ? -(int)total_errors : 0;
}
