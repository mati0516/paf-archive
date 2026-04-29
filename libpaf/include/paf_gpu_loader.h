#ifndef PAF_GPU_LOADER_H
#define PAF_GPU_LOADER_H

#include <stdint.h>
#include "paf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAF_GPU_CUDA     (1 << 0)
#define PAF_GPU_DSTORAGE (1 << 1)

// Call once at startup (or rely on lazy init).
// Returns bitmask of available GPU backends (PAF_GPU_CUDA | PAF_GPU_DSTORAGE).
PAF_API int paf_gpu_init(void);

// Returns non-zero if paf_cuda.dll is loaded and CUDA device is usable.
PAF_API int paf_cuda_is_available(void);

// Returns non-zero if dstorage.dll is present and DStorageGetFactory is exported.
PAF_API int paf_dstorage_is_available(void);

// Function pointer type: hash a flat batch buffer via GPU.
// host_buf   : contiguous data for all files
// offsets    : byte offset of each file within host_buf
// sizes      : byte size of each file
// count      : number of files
// out_hashes : output buffer, count * 32 bytes
typedef int (*paf_cuda_hash_flat_fn)(const uint8_t* host_buf, const uint64_t* offsets,
                                     const uint64_t* sizes, uint32_t count,
                                     uint8_t* out_hashes);

// Populated by paf_gpu_init() when paf_cuda.dll is available; NULL otherwise.
extern paf_cuda_hash_flat_fn g_paf_cuda_hash_flat;

#ifdef __cplusplus
}
#endif

#endif // PAF_GPU_LOADER_H
