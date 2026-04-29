#ifdef _WIN32
#include <windows.h>
#include "dstorage.h"
#include <wrl/client.h>
#include <vector>
#include <stdint.h>
#include <string>

using Microsoft::WRL::ComPtr;

// Timeout for synchronous wait in milliseconds
static const DWORD DSTORAGE_WAIT_TIMEOUT_MS = 5000;

class PafDirectStorage {
private:
    ComPtr<IDStorageFactory> m_factory;
    ComPtr<IDStorageQueue> m_queue;
    ComPtr<IDStorageFile> m_file;
    std::wstring m_current_path;

    HRESULT OpenFile(const wchar_t* path) {
        m_file.Reset();
        HRESULT hr = m_factory->OpenFile(path, IID_PPV_ARGS(&m_file));
        if (SUCCEEDED(hr)) m_current_path = path;
        return hr;
    }

public:
    HRESULT Initialize(const wchar_t* path) {
        HRESULT hr = DStorageGetFactory(IID_PPV_ARGS(&m_factory));
        if (FAILED(hr)) return hr;

        DSTORAGE_QUEUE_DESC queueDesc = {};
        queueDesc.Capacity = DSTORAGE_MAX_QUEUE_CAPACITY;
        queueDesc.Priority = DSTORAGE_PRIORITY_NORMAL;
        queueDesc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        queueDesc.Device = nullptr;

        hr = m_factory->CreateQueue(&queueDesc, IID_PPV_ARGS(&m_queue));
        if (FAILED(hr)) return hr;

        return OpenFile(path);
    }

    bool IsInitialized() const { return m_factory != nullptr; }

    HRESULT EnsureFile(const wchar_t* path) {
        if (m_current_path != path) return OpenFile(path);
        return S_OK;
    }

    HRESULT EnqueueAndWait(uint64_t offset, uint32_t size, void* destination) {
        DSTORAGE_REQUEST request = {};
        request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
        request.Source.File.Source = m_file.Get();
        request.Source.File.Offset = offset;
        request.Source.File.Size = size;
        request.Destination.Memory.Buffer = destination;
        request.Destination.Memory.Size = size;

        m_queue->EnqueueRequest(&request);

        ComPtr<IDStorageStatusArray> statusArray;
        HRESULT hr = m_factory->CreateStatusArray(1, nullptr, IID_PPV_ARGS(&statusArray));
        if (FAILED(hr)) return hr;

        m_queue->EnqueueStatus(statusArray.Get(), 0);
        m_queue->Submit();

        DWORD elapsed = 0;
        while (!statusArray->IsComplete(0)) {
            if (elapsed >= DSTORAGE_WAIT_TIMEOUT_MS) return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
            Sleep(1);
            elapsed++;
        }

        return statusArray->GetHResult(0);
    }
};

extern "C" int paf_io_directstorage_load(const wchar_t* path, uint64_t offset, uint64_t size, void* destination) {
    static PafDirectStorage ds;

    if (!ds.IsInitialized()) {
        if (FAILED(ds.Initialize(path))) return -1;
    } else {
        if (FAILED(ds.EnsureFile(path))) return -1;
    }

    return SUCCEEDED(ds.EnqueueAndWait(offset, (uint32_t)size, destination)) ? 0 : -1;
}
#endif
