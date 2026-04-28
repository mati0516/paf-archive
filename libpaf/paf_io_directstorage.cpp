#include <windows.h>
#include <dstorage.h>
#include <wrl/client.h>
#include <vector>
#include <stdint.h>

using Microsoft::WRL::ComPtr;

class PafDirectStorage {
private:
    ComPtr<IDStorageFactory> m_factory;
    ComPtr<IDStorageQueue> m_queue;
    ComPtr<IDStorageFile> m_file;

public:
    HRESULT Initialize(const wchar_t* path) {
        HRESULT hr = DStorageGetFactory(IID_PPV_ARGS(&m_factory));
        if (FAILED(hr)) return hr;

        DSTORAGE_QUEUE_DESC queueDesc = {};
        queueDesc.Capacity = DSTORAGE_MAX_QUEUE_CAPACITY;
        queueDesc.Priority = DSTORAGE_PRIORITY_NORMAL;
        queueDesc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        queueDesc.Device = nullptr; // GPU Device would go here if using GPU destination

        hr = m_factory->CreateQueue(&queueDesc, IID_PPV_ARGS(&m_queue));
        if (FAILED(hr)) return hr;

        hr = m_factory->OpenFile(path, IID_PPV_ARGS(&m_file));
        return hr;
    }

    void EnqueueRequest(uint64_t offset, uint32_t size, void* destination) {
        DSTORAGE_REQUEST request = {};
        request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
        request.File.Source = m_file.Get();
        request.File.Offset = offset;
        request.File.Size = size;
        request.Memory.Buffer = destination;
        request.Memory.Size = size;

        m_queue->EnqueueRequest(&request);
    }

    HRESULT SubmitAndWait() {
        m_queue->Submit();
        
        IDStorageStatusArray* statusArray; // Simplified status check
        // In real usage, we would use a fence or status array
        return S_OK;
    }
};

extern "C" int paf_io_directstorage_load(const wchar_t* path, uint64_t offset, uint64_t size, void* destination) {
    static PafDirectStorage ds;
    static bool initialized = false;
    
    if (!initialized) {
        if (FAILED(ds.Initialize(path))) return -1;
        initialized = true;
    }

    ds.EnqueueRequest(offset, (uint32_t)size, destination);
    return 0;
}
