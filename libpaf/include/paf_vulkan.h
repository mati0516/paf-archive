#ifndef PAF_VULKAN_H
#define PAF_VULKAN_H

#include <stdint.h>
#include "paf.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns non-zero if the Vulkan compute path is usable.
// Requires libvulkan (Linux) or vulkan-1.dll (Windows) plus paf_sha256.spv
// in the same directory as libpaf.
PAF_API int paf_vulkan_init(void);
PAF_API int paf_vulkan_is_available(void);

// Batch SHA-256 using Vulkan compute.
// Same signature as paf_cuda_hash_flat — drop-in fallback when CUDA is absent.
PAF_API int paf_vulkan_hash_flat(const uint8_t* host_buf,
                                  const uint64_t* offsets,
                                  const uint64_t* sizes,
                                  uint32_t count,
                                  uint8_t* out_hashes);

PAF_API void paf_vulkan_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // PAF_VULKAN_H
