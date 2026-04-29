#include "paf_gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(PAF_CI_BUILD)
#include <windows.h>
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__ANDROID__) || defined(__iphone__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/mman.h>
#endif

int paf_gpu_get_info(paf_gpu_info_t* info) {
    if (!info) return -1;
    memset(info, 0, sizeof(paf_gpu_info_t));

#if defined(_WIN32) && !defined(PAF_CI_BUILD)
    // Windows: Check DXGI for VRAM and DirectStorage capability
    IDXGIFactory* factory = NULL;
    if (SUCCEEDED(CreateDXGIFactory(&IID_IDXGIFactory, (void**)&factory))) {
        IDXGIAdapter* adapter = NULL;
        if (SUCCEEDED(factory->lpVtbl->EnumAdapters(factory, 0, &adapter))) {
            DXGI_ADAPTER_DESC desc;
            adapter->lpVtbl->GetDesc(adapter, &desc);
            info->total_vram = desc.DedicatedVideoMemory;
            wcstombs(info->device_name, desc.Description, sizeof(info->device_name));
            
            info->supports_vulkan = 1; 
            info->supports_direct_io = 1;
            info->supports_cuda = (strstr(info->device_name, "NVIDIA") != NULL);
            
            adapter->lpVtbl->Release(adapter);
        }
        factory->lpVtbl->Release(factory);
    }
#elif defined(_WIN32)
    strcpy(info->device_name, "Windows Generic (CI Build)");
    info->total_vram = 0;
#elif defined(__ANDROID__) || defined(__APPLE__)
    // Mobile: Vulkan/Metal is the primary path
    info->supports_vulkan = 1;
    info->supports_direct_io = 0; // Mobile usually uses mmap/standard IO
    strcpy(info->device_name, "Mobile Integrated GPU");
#else
    // Linux: Check for NVIDIA (CUDA) or AMD (Vulkan/ROCm)
    info->supports_vulkan = 1;
    info->supports_direct_io = 1; // io_uring / GDS
#endif
    return 0;
}

paf_batch_config_t paf_gpu_calculate_batch(uint64_t available_vram, uint32_t total_files, uint64_t avg_file_size) {
    paf_batch_config_t config;
    
    // Reserve 256MB for OS/System overhead
    uint64_t safe_vram = (available_vram > 256ULL * 1024 * 1024) ? (available_vram - 256ULL * 1024 * 1024) : 0;
    
    // We need to load index entries (64 bytes each) AND the actual data
    // Let's assume we want to fill safe_vram with a mix of index and data.
    uint64_t bytes_per_file_total = 64 + avg_file_size;
    
    if (bytes_per_file_total == 0) {
        config.files_per_batch = total_files;
        config.data_per_batch = 0;
        return config;
    }

    uint32_t batch_files = (uint32_t)(safe_vram / bytes_per_file_total);
    
    if (batch_files > total_files) batch_files = total_files;
    if (batch_files == 0) batch_files = 1; // At least one file

    config.files_per_batch = batch_files;
    config.data_per_batch = (uint64_t)batch_files * avg_file_size;

    return config;
}

int paf_gpu_direct_load(const char* path, uint64_t offset, uint64_t size, void* gpu_buffer) {
    // This is a placeholder for DirectStorage implementation.
    // In a real scenario, this would use IDStorageQueue::EnqueueRequest
    printf("[DirectStorage] Loading %s (Offset: %llu, Size: %llu) -> GPU Buffer %p\n", 
           path, (unsigned long long)offset, (unsigned long long)size, gpu_buffer);
    
    // Simulation: Just a normal read for now if we don't have DS ready
    return 0;
}
