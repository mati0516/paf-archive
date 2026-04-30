#ifndef PAF_GPU_H
#define PAF_GPU_H

#include "paf.h"

#ifdef __cplusplus
extern "C" {
#endif

struct paf_extractor_t;
typedef struct paf_extractor_t paf_extractor_t;

typedef struct {
    uint64_t total_vram;
    uint64_t available_vram;
    uint32_t compute_units;
    char device_name[256];
    int supports_cuda;
    int supports_vulkan;      // AMD/Intel/Mobile Support
    int supports_direct_io;   // DirectStorage/GDS Support
} paf_gpu_info_t;

typedef struct {
    uint32_t files_per_batch;
    uint64_t data_per_batch;
} paf_batch_config_t;

/**
 * Get GPU information.
 * Implementation will use CUDA or Vulkan depending on platform.
 */
PAF_API int paf_gpu_get_info(paf_gpu_info_t* info);

/**
 * Calculate the optimal batch size based on available VRAM and file statistics.
 * @param available_vram Available memory in bytes.
 * @param total_files Total number of files in the archive.
 * @param avg_file_size Average size of a single file in bytes.
 */
PAF_API paf_batch_config_t paf_gpu_calculate_batch(uint64_t available_vram, uint32_t total_files, uint64_t avg_file_size);

// Load a byte range from a PAF file into host-accessible memory.
// Uses DirectStorage on Windows when available; falls back to fread.
PAF_API int paf_gpu_direct_load(const char* path, uint64_t offset,
                                 uint64_t size, void* gpu_buffer);

// True NVMe→GPU DMA path via DirectStorage with a D3D12 resource destination.
// d3d12_device   : ID3D12Device* (cast to void* for C linkage)
// d3d12_resource : ID3D12Resource* destination buffer on the GPU
// resource_offset: byte offset within the D3D12 resource
// Returns 0 on success; -1 on failure or when D3D12/DirectStorage is absent.
PAF_API int paf_gpu_direct_load_d3d12(const char* path, uint64_t offset,
                                        uint64_t size,
                                        void* d3d12_device,
                                        void* d3d12_resource,
                                        uint64_t resource_offset);

// GPU-Accelerated Search across all file paths.
// Returns the number of matches found.
PAF_API int paf_gpu_search_files(paf_extractor_t* ext, const char* pattern, uint32_t* out_indices, uint32_t max_results);

#ifdef __cplusplus
}
#endif

#endif // PAF_GPU_H
