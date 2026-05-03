#pragma once
// Internal header: parallel SHA-256 using a platform thread pool.
// Include from .c files that already pull in paf_sha256_hw.h.

#include "paf_sha256_hw.h"
#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32) && !defined(__ANDROID__) && !defined(__linux__)
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

typedef struct {
    const uint8_t*  buf;
    const uint64_t* offsets;
    const uint64_t* sizes;
    uint8_t*        hashes;
    uint32_t        start;
    uint32_t        end;
} paf_sha256_worker_t;

static void paf_sha256_worker_run(paf_sha256_worker_t* w) {
    for (uint32_t i = w->start; i < w->end; i++)
        paf_sha256_compute(w->buf + w->offsets[i], (size_t)w->sizes[i],
                           w->hashes + i * 32);
}

#if defined(_WIN32) && !defined(__ANDROID__) && !defined(__linux__)

static DWORD WINAPI paf_sha256_worker_fn(LPVOID arg) {
    paf_sha256_worker_run((paf_sha256_worker_t*)arg);
    return 0;
}

static void parallel_sha256_cpu(const uint8_t* buf, const uint64_t* offsets,
                                 const uint64_t* sizes, uint32_t n, uint8_t* hashes)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int nt = (int)si.dwNumberOfProcessors;
    if (nt > MAXIMUM_WAIT_OBJECTS) nt = MAXIMUM_WAIT_OBJECTS;
    if (nt < 1) nt = 1;
    if ((uint32_t)nt > n) nt = (int)n;

    paf_sha256_worker_t* ctx = (paf_sha256_worker_t*)malloc(nt * sizeof(paf_sha256_worker_t));
    if (!ctx) {
        for (uint32_t i = 0; i < n; i++)
            paf_sha256_compute(buf + offsets[i], (size_t)sizes[i], hashes + i * 32);
        return;
    }

    HANDLE handles[MAXIMUM_WAIT_OBJECTS];
    for (int t = 0; t < nt; t++) {
        ctx[t].buf = buf; ctx[t].offsets = offsets; ctx[t].sizes = sizes;
        ctx[t].hashes = hashes;
        ctx[t].start = (uint32_t)((uint64_t)t * n / nt);
        ctx[t].end   = (t == nt - 1) ? n : (uint32_t)((uint64_t)(t + 1) * n / nt);
        handles[t] = CreateThread(NULL, 0, paf_sha256_worker_fn, &ctx[t], 0, NULL);
        if (!handles[t]) paf_sha256_worker_run(&ctx[t]);
    }
    HANDLE valid[MAXIMUM_WAIT_OBJECTS];
    int nv = 0;
    for (int t = 0; t < nt; t++) if (handles[t]) valid[nv++] = handles[t];
    if (nv > 0) WaitForMultipleObjects(nv, valid, TRUE, INFINITE);
    for (int t = 0; t < nt; t++) if (handles[t]) CloseHandle(handles[t]);
    free(ctx);
}

#else  // Linux / Android: pthreads

#define PAF_SHA256_THREADS_MAX 32

static void* paf_sha256_worker_fn(void* arg) {
    paf_sha256_worker_run((paf_sha256_worker_t*)arg);
    return NULL;
}

static void parallel_sha256_cpu(const uint8_t* buf, const uint64_t* offsets,
                                 const uint64_t* sizes, uint32_t n, uint8_t* hashes)
{
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    int nt = (ncpu > 0) ? (int)ncpu : 2;
    if (nt > PAF_SHA256_THREADS_MAX) nt = PAF_SHA256_THREADS_MAX;
    if ((uint32_t)nt > n) nt = (int)n;

    paf_sha256_worker_t* ctx = (paf_sha256_worker_t*)malloc(nt * sizeof(paf_sha256_worker_t));
    if (!ctx) {
        for (uint32_t i = 0; i < n; i++)
            paf_sha256_compute(buf + offsets[i], (size_t)sizes[i], hashes + i * 32);
        return;
    }

    pthread_t threads[PAF_SHA256_THREADS_MAX];
    int created[PAF_SHA256_THREADS_MAX];
    for (int t = 0; t < nt; t++) {
        ctx[t].buf = buf; ctx[t].offsets = offsets; ctx[t].sizes = sizes;
        ctx[t].hashes = hashes;
        ctx[t].start = (uint32_t)((uint64_t)t * n / nt);
        ctx[t].end   = (t == nt - 1) ? n : (uint32_t)((uint64_t)(t + 1) * n / nt);
        created[t] = (pthread_create(&threads[t], NULL, paf_sha256_worker_fn, &ctx[t]) == 0);
        if (!created[t]) paf_sha256_worker_run(&ctx[t]);
    }
    for (int t = 0; t < nt; t++)
        if (created[t]) pthread_join(threads[t], NULL);
    free(ctx);
}

#endif
