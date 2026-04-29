#include "paf_extractor.h"
#include "paf_gpu.h"
#if defined(_WIN32) && defined(PAF_USE_CUDA)
#include "paf_cuda.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External DirectStorage wrapper (only on Windows)
#ifdef _WIN32
int paf_io_directstorage_load(const wchar_t* path, uint64_t offset, uint64_t size, void* destination);
#else
static int paf_io_directstorage_load(const void* path, uint64_t offset, uint64_t size, void* destination) {
    (void)path; (void)offset; (void)size; (void)destination;
    return -1; 
}
#endif

int paf_extractor_gpu_run(paf_extractor_t* ext, const char* path) {
    if (!ext || !path) return -1;

    // 1. Get GPU Info
    paf_gpu_info_t gpu;
    paf_gpu_get_info(&gpu);
    printf("Detected Hardware: %s\n", gpu.device_name);
    printf(" - VRAM: %llu MB\n", (unsigned long long)gpu.total_vram / 1024 / 1024);
    printf(" - CUDA Support: %s\n", gpu.supports_cuda ? "Yes" : "No");
    printf(" - Vulkan Support: %s\n", gpu.supports_vulkan ? "Yes (AMD/Intel/Mobile)" : "No");
    printf(" - Direct IO: %s\n", gpu.supports_direct_io ? "Yes (NVMe -> GPU Direct Path)" : "No");

    // 2. Decide Execution Path
    int use_direct_io = gpu.supports_direct_io;
    int use_gpu_compute = (gpu.supports_cuda || gpu.supports_vulkan);

    if (use_gpu_compute) {
        printf("Mode: GPU-Accelerated Pipeline (%s)\n", gpu.supports_cuda ? "CUDA" : "Vulkan");
    } else {
        printf("Mode: CPU Multi-threaded Fallback\n");
    }

    // 3. Calculate Adaptive Batch Size
    paf_batch_config_t batch = paf_gpu_calculate_batch(gpu.total_vram, ext->header.file_count, 1024 * 1024);
    
    // ... loop with selected path

    // 3. Main Processing Loop
    uint32_t processed = 0;
    while (processed < ext->header.file_count) {
        uint32_t current_batch_count = (ext->header.file_count - processed < batch.files_per_batch) ? 
                                        (ext->header.file_count - processed) : batch.files_per_batch;

        printf("Processing batch %u to %u...\n", processed, processed + current_batch_count);

        // A. Allocate GPU memory for this batch (Simplification: using a pre-allocated pool in real impl)
        // B. Enqueue DirectStorage requests for all files in this batch
        for (uint32_t i = 0; i < current_batch_count; i++) {
            paf_index_entry_t* entry = &ext->entries[processed + i];
            // paf_io_directstorage_load(...)
        }

        // C. Launch CUDA Kernel for parallel SHA-256
        // paf_cuda_sha256_batch(...)

        processed += current_batch_count;
    }

    return 0;
}
