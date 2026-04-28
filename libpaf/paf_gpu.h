#ifndef PAF_GPU_H
#define PAF_GPU_H

#include "paf.h"

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
int paf_gpu_get_info(paf_gpu_info_t* info);

/**
 * Calculate the optimal batch size based on available VRAM and file statistics.
 * @param available_vram Available memory in bytes.
 * @param total_files Total number of files in the archive.
 * @param avg_file_size Average size of a single file in bytes.
 */
paf_batch_config_t paf_gpu_calculate_batch(uint64_t available_vram, uint32_t total_files, uint64_t avg_file_size);

/**
 * High-performance NVMe -> GPU Direct Load.
 * Uses DirectStorage on Windows or GDS on Linux.
 */
/**
 * GPU-Accelerated Search across all file paths.
 * Returns the number of matches found.
 */
int paf_gpu_search_files(paf_extractor_t* ext, const char* pattern, uint32_t* out_indices, uint32_t max_results);

#endif // PAF_GPU_H
