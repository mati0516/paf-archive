#define LIBPAF_EXPORTS
#include "paf_gpu_loader.h"
#include <stddef.h>
#include <stdint.h>

paf_cuda_hash_flat_fn g_paf_cuda_hash_flat = NULL;

#if defined(_WIN32) && !defined(__ANDROID__) && !defined(__linux__)
#include <windows.h>

static int g_cuda_avail     = -1;  /* -1 = not yet probed */
static int g_dstorage_avail = -1;

int paf_gpu_init(void) {
    g_cuda_avail     = 0;
    g_dstorage_avail = 0;

    /* --- CUDA --- */
    HMODULE hcuda = LoadLibraryA("paf_cuda.dll");
    if (hcuda) {
        typedef int (*init_fn)(void);
        init_fn              cuda_init  = (init_fn)             (uintptr_t)GetProcAddress(hcuda, "paf_cuda_init");
        paf_cuda_hash_flat_fn hash_flat = (paf_cuda_hash_flat_fn)(uintptr_t)GetProcAddress(hcuda, "paf_cuda_hash_flat");

        if (cuda_init && hash_flat && cuda_init() == 0) {
            g_paf_cuda_hash_flat = hash_flat;
            g_cuda_avail = 1;
        } else {
            FreeLibrary(hcuda);
        }
    }

    /* --- DirectStorage --- */
    HMODULE hds = LoadLibraryA("dstorage.dll");
    if (hds) {
        if (GetProcAddress(hds, "DStorageGetFactory") != NULL) {
            g_dstorage_avail = 1;
        }
        /* Keep loaded so PafDirectStorage can also LoadLibrary it without re-mapping.
           The OS reference-counts the module so two LoadLibrary calls are cheap.    */
        FreeLibrary(hds);
    }

    return (g_cuda_avail ? PAF_GPU_CUDA : 0) | (g_dstorage_avail ? PAF_GPU_DSTORAGE : 0);
}

int paf_cuda_is_available(void) {
    if (g_cuda_avail < 0) paf_gpu_init();
    return g_cuda_avail;
}

int paf_dstorage_is_available(void) {
    if (g_dstorage_avail < 0) paf_gpu_init();
    return g_dstorage_avail;
}

#else

/* Non-Windows platforms: no CUDA/DirectStorage support */
int paf_gpu_init(void)           { return 0; }
int paf_cuda_is_available(void)  { return 0; }
int paf_dstorage_is_available(void) { return 0; }

#endif
