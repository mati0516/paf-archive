#define LIBPAF_EXPORTS
#include "paf_gpu_loader.h"
#include "paf_vulkan.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Returns 1 if the env var is set to a non-empty, non-"0" value ("1" being the typical use). */
static int env_is_disabled(const char* name) {
    const char* v = getenv(name);
    return v && v[0] != '\0' && v[0] != '0';
}

paf_cuda_hash_flat_fn   g_paf_cuda_hash_flat   = NULL;
// paf_vulkan_hash_flat is a real function in paf_vulkan.c; point to it unconditionally.
// paf_vulkan_hash_flat() returns -1 when Vulkan is unavailable, so the caller
// always guards with paf_vulkan_is_available() before trusting the result.
paf_vulkan_hash_flat_fn g_paf_vulkan_hash_flat = paf_vulkan_hash_flat;

#if defined(_WIN32) && !defined(__ANDROID__) && !defined(__linux__)
#include <windows.h>

static int g_cuda_avail     = -1;  /* -1 = not yet probed */
static int g_dstorage_avail = -1;

int paf_gpu_init(void) {
    g_cuda_avail     = 0;
    g_dstorage_avail = 0;

    /* --- CUDA --- */
    if (!env_is_disabled("PAF_DISABLE_CUDA")) {
#ifdef PAF_USE_CUDA
        /* CUDA kernels are statically linked (paf_cuda_kernels.obj -> libpaf.dll);
           no external paf_cuda.dll exists, so call the symbols directly. */
        {
            extern int paf_cuda_init(void);
            extern int paf_cuda_hash_flat(const uint8_t*, const uint64_t*, const uint64_t*,
                                          uint32_t, uint8_t*);
            if (paf_cuda_init() == 0) {
                g_paf_cuda_hash_flat = paf_cuda_hash_flat;
                g_cuda_avail = 1;
            }
        }
#else
        {
            HMODULE hcuda = LoadLibraryA("paf_cuda.dll");
            if (hcuda) {
                typedef int (*init_fn)(void);
                init_fn               cuda_init = (init_fn)              (uintptr_t)GetProcAddress(hcuda, "paf_cuda_init");
                paf_cuda_hash_flat_fn hash_flat  = (paf_cuda_hash_flat_fn)(uintptr_t)GetProcAddress(hcuda, "paf_cuda_hash_flat");

                if (cuda_init && hash_flat && cuda_init() == 0) {
                    g_paf_cuda_hash_flat = hash_flat;
                    g_cuda_avail = 1;
                } else {
                    FreeLibrary(hcuda);
                }
            }
        }
#endif
    }

    /* --- DirectStorage --- */
    if (!env_is_disabled("PAF_DISABLE_DSTORAGE")) {
        HMODULE hds = LoadLibraryA("dstorage.dll");
        if (hds) {
            if (GetProcAddress(hds, "DStorageGetFactory") != NULL)
                g_dstorage_avail = 1;
            /* Keep loaded so PafDirectStorage can also LoadLibrary it without re-mapping.
               The OS reference-counts the module so two LoadLibrary calls are cheap. */
            FreeLibrary(hds);
        }
    }

    /* --- Vulkan: probe now so the result feeds into the bitmask.
         paf_vulkan_init() is idempotent; state lives in paf_vulkan.c. --- */
    if (!env_is_disabled("PAF_DISABLE_VULKAN"))
        paf_vulkan_init();

    return (g_cuda_avail     ? PAF_GPU_CUDA     : 0)
         | (g_dstorage_avail ? PAF_GPU_DSTORAGE : 0)
         | (paf_vulkan_is_available() ? PAF_GPU_VULKAN : 0);
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

/* Non-Windows platforms: Vulkan only (libvulkan.so.1 resolved at runtime) */
int paf_gpu_init(void) {
    paf_vulkan_init();
    return paf_vulkan_is_available() ? PAF_GPU_VULKAN : 0;
}

int paf_cuda_is_available(void)     { return 0; }
int paf_dstorage_is_available(void) { return 0; }

#endif
