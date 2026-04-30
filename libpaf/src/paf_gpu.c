#define LIBPAF_EXPORTS
#include "paf_gpu.h"
#include "paf_extractor.h"
#include "paf_gpu_loader.h"
#include "fnmatch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(PAF_CI_BUILD) && !defined(__ANDROID__) && !defined(__linux__)
#include <windows.h>
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__ANDROID__) || defined(__iphone__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/mman.h>
#endif

#if defined(_WIN32) && !defined(PAF_CI_BUILD) && !defined(__ANDROID__) && !defined(__linux__)
int paf_io_directstorage_load(const wchar_t* path, uint64_t offset,
                               uint64_t size, void* destination);
#endif

int paf_gpu_get_info(paf_gpu_info_t* info) {
    if (!info) return -1;
    memset(info, 0, sizeof(paf_gpu_info_t));

#if defined(_WIN32) && !defined(PAF_CI_BUILD) && !defined(__ANDROID__) && !defined(__linux__)
    IDXGIFactory* factory = NULL;
    if (SUCCEEDED(CreateDXGIFactory(&IID_IDXGIFactory, (void**)&factory))) {
        IDXGIAdapter* adapter = NULL;
        if (SUCCEEDED(factory->lpVtbl->EnumAdapters(factory, 0, &adapter))) {
            DXGI_ADAPTER_DESC desc;
            adapter->lpVtbl->GetDesc(adapter, &desc);
            info->total_vram = desc.DedicatedVideoMemory;
            wcstombs(info->device_name, desc.Description,
                     sizeof(info->device_name) - 1);
            info->device_name[sizeof(info->device_name) - 1] = '\0';
            info->supports_vulkan    = 1;
            info->supports_direct_io = 1;
            info->supports_cuda      = (strstr(info->device_name, "NVIDIA") != NULL);
            adapter->lpVtbl->Release(adapter);
        }
        factory->lpVtbl->Release(factory);
    }
#elif defined(_WIN32)
    snprintf(info->device_name, sizeof(info->device_name),
             "Windows Generic (CI Build)");
    info->total_vram = 0;
#elif defined(__ANDROID__) || defined(__APPLE__)
    info->supports_vulkan    = 1;
    info->supports_direct_io = 0;
    snprintf(info->device_name, sizeof(info->device_name), "Mobile Integrated GPU");
#else
    info->supports_vulkan    = 1;
    info->supports_direct_io = 1;
#endif
    return 0;
}

paf_batch_config_t paf_gpu_calculate_batch(uint64_t available_vram,
                                            uint32_t total_files,
                                            uint64_t avg_file_size) {
    paf_batch_config_t config;
    uint64_t safe_vram = (available_vram > 256ULL * 1024 * 1024)
                         ? (available_vram - 256ULL * 1024 * 1024) : 0;
    uint64_t bytes_per_file = 64 + avg_file_size;

    if (bytes_per_file == 0) {
        config.files_per_batch = total_files;
        config.data_per_batch  = 0;
        return config;
    }

    uint32_t batch = (uint32_t)(safe_vram / bytes_per_file);
    if (batch > total_files) batch = total_files;
    if (batch == 0) batch = 1;

    config.files_per_batch = batch;
    config.data_per_batch  = (uint64_t)batch * avg_file_size;
    return config;
}

// Load a byte range from a PAF file into a caller-supplied buffer.
// Uses DirectStorage on Windows when available; falls back to fread.
// gpu_buffer is treated as host-accessible memory. A true NVMe→GPU path
// requires a D3D12 resource destination and is not yet implemented.
int paf_gpu_direct_load(const char* path, uint64_t offset,
                         uint64_t size, void* gpu_buffer) {
    if (!path || !gpu_buffer || size == 0) return -1;

#if defined(_WIN32) && !defined(PAF_CI_BUILD) && !defined(__ANDROID__) && !defined(__linux__)
    if (paf_dstorage_is_available()) {
        wchar_t wpath[1024];
        mbstowcs(wpath, path, sizeof(wpath) / sizeof(wpath[0]) - 1);
        wpath[sizeof(wpath) / sizeof(wpath[0]) - 1] = L'\0';
        if (paf_io_directstorage_load(wpath, offset, size, gpu_buffer) == 0)
            return 0;
    }
#endif

    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;
    int ok = fseek(fp, (long)offset, SEEK_SET) == 0 &&
             fread(gpu_buffer, 1, (size_t)size, fp) == (size_t)size;
    fclose(fp);
    return ok ? 0 : -1;
}

// Linear scan of all file paths in the archive against a fnmatch pattern.
// Returns the number of matching entries written to out_indices.
int paf_gpu_search_files(paf_extractor_t* ext, const char* pattern,
                          uint32_t* out_indices, uint32_t max_results) {
    if (!ext || !pattern || !out_indices || max_results == 0) return -1;

    uint32_t found = 0;
    char path_buf[1024];

    for (uint32_t i = 0; i < ext->header.file_count && found < max_results; i++) {
        const paf_index_entry_t* e = &ext->entries[i];
        uint32_t plen = e->path_length;
        if (plen == 0 || plen >= sizeof(path_buf)) continue;

        long seek_pos = (long)(ext->header.path_offset + e->path_buffer_offset);
        if (fseek(ext->fp, seek_pos, SEEK_SET) != 0) continue;
        if (fread(path_buf, 1, plen, ext->fp) != plen) continue;
        path_buf[plen] = '\0';

        if (fnmatch(pattern, path_buf, 0) == 0)
            out_indices[found++] = i;
    }

    return (int)found;
}
