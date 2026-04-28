#include "paf_gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")
#elif defined(__ANDROID__) || defined(__iphone__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/mman.h>
// For Mobile GPU detection (Vulkan is standard)
// #include <vulkan/vulkan.h> 
#endif

int paf_gpu_get_info(paf_gpu_info_t* info) {
    if (!info) return -1;
    memset(info, 0, sizeof(paf_gpu_info_t));

#ifdef _WIN32
    IDXGIFactory* factory;
    if (SUCCEEDED(CreateDXGIFactory(&IID_IDXGIFactory, (void**)&factory))) {
        IDXGIAdapter* adapter;
        if (SUCCEEDED(factory->lpVtbl->EnumAdapters(factory, 0, &adapter))) {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(adapter->lpVtbl->GetDesc(adapter, &desc))) {
                info->total_vram = desc.DedicatedVideoMemory;
                info->available_vram = desc.DedicatedVideoMemory;
                wcstombs(info->device_name, desc.Description, sizeof(info->device_name));
            }
            adapter->lpVtbl->Release(adapter);
        }
        factory->lpVtbl->Release(factory);
        return 0;
    }
#elif defined(__ANDROID__) || defined(__APPLE__)
    // Mobile Implementation
    strcpy(info->device_name, "Mobile GPU (Vulkan/Metal Ready)");
    info->total_vram = 4ULL * 1024 * 1024 * 1024; // Common mobile VRAM (Shared)
    info->available_vram = 2ULL * 1024 * 1024 * 1024;
    return 0;
#else
    // General Linux / Other
    strcpy(info->device_name, "Generic Unix GPU");
    info->total_vram = 2ULL * 1024 * 1024 * 1024;
    info->available_vram = 1ULL * 1024 * 1024 * 1024;
    return 0;
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
    printf("[DirectStorage] Loading %s (Offset: %llu, Size: %llu) -> GPU Buffer %p\n", path, offset, size, gpu_buffer);
    
    // Simulation: Just a normal read for now if we don't have DS ready
    // return 0;
    return 0;
}
