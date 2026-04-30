// True NVMe→GPU DMA via DirectStorage with a D3D12 resource destination.
// Compiled only for the full GPU Windows build (not PAF_CI_BUILD).
#ifdef _WIN32
#ifndef PAF_CI_BUILD

#include <windows.h>
#include "dstorage.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <stdint.h>

using Microsoft::WRL::ComPtr;

typedef HRESULT (WINAPI *PFN_DSTORAGE_GET_FACTORY)(REFIID riid, void** ppv);

static PFN_DSTORAGE_GET_FACTORY s_GetFactory = nullptr;
static HMODULE                  s_hDS        = nullptr;

static bool LoadDStorageD3D12Once() {
    if (s_GetFactory) return true;
    s_hDS = LoadLibraryA("dstorage.dll");
    if (!s_hDS) return false;
    s_GetFactory = (PFN_DSTORAGE_GET_FACTORY)(uintptr_t)
                   GetProcAddress(s_hDS, "DStorageGetFactory");
    if (!s_GetFactory) {
        FreeLibrary(s_hDS);
        s_hDS = nullptr;
        return false;
    }
    return true;
}

extern "C" int paf_gpu_direct_load_d3d12(const char* path, uint64_t offset,
                                           uint64_t size,
                                           void* d3d12_device,
                                           void* d3d12_resource,
                                           uint64_t resource_offset) {
    if (!path || !d3d12_device || !d3d12_resource || size == 0) return -1;
    if (!LoadDStorageD3D12Once()) return -1;

    ComPtr<IDStorageFactory> factory;
    if (FAILED(s_GetFactory(IID_PPV_ARGS(&factory)))) return -1;

    DSTORAGE_QUEUE_DESC queueDesc = {};
    queueDesc.Capacity   = DSTORAGE_MAX_QUEUE_CAPACITY;
    queueDesc.Priority   = DSTORAGE_PRIORITY_NORMAL;
    queueDesc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    queueDesc.Device     = (ID3D12Device*)d3d12_device;

    ComPtr<IDStorageQueue> queue;
    if (FAILED(factory->CreateQueue(&queueDesc, IID_PPV_ARGS(&queue)))) return -1;

    wchar_t wpath[1024];
    mbstowcs(wpath, path, sizeof(wpath) / sizeof(wpath[0]) - 1);
    wpath[sizeof(wpath) / sizeof(wpath[0]) - 1] = L'\0';

    ComPtr<IDStorageFile> file;
    if (FAILED(factory->OpenFile(wpath, IID_PPV_ARGS(&file)))) return -1;

    DSTORAGE_REQUEST request = {};
    request.Options.SourceType                  = DSTORAGE_REQUEST_SOURCE_FILE;
    request.Options.DestinationType             = DSTORAGE_REQUEST_DESTINATION_BUFFER;
    request.Source.File.Source                  = file.Get();
    request.Source.File.Offset                  = offset;
    request.Source.File.Size                    = (UINT32)size;
    request.Destination.Buffer.Resource         = (ID3D12Resource*)d3d12_resource;
    request.Destination.Buffer.Offset           = resource_offset;
    request.Destination.Buffer.Size             = (UINT32)size;

    queue->EnqueueRequest(&request);

    ComPtr<ID3D12Fence> fence;
    if (FAILED(((ID3D12Device*)d3d12_device)->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
        return -1;

    const UINT64 kSignal = 1;
    queue->EnqueueSignal(fence.Get(), kSignal);
    queue->Submit();

    if (fence->GetCompletedValue() < kSignal) {
        HANDLE evt = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!evt) return -1;
        HRESULT hr = fence->SetEventOnCompletion(kSignal, evt);
        if (SUCCEEDED(hr)) WaitForSingleObject(evt, 10000 /* ms */);
        CloseHandle(evt);
        if (FAILED(hr)) return -1;
    }

    return 0;
}

#endif // !PAF_CI_BUILD
#endif // _WIN32
