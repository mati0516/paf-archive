#include "paf_patch.hpp"

#pragma comment(lib, "dstorage.lib")

static const DWORD PATCH_WAIT_TIMEOUT_MS = 30000;

namespace paf {

PatchEngine::PatchEngine() : m_initialized(false) {
    if (FAILED(DStorageGetFactory(IID_PPV_ARGS(&m_factory)))) {
        return;
    }

    DSTORAGE_QUEUE_DESC queueDesc = {};
    queueDesc.Capacity = DSTORAGE_MAX_QUEUE_CAPACITY;
    queueDesc.Priority = DSTORAGE_PRIORITY_NORMAL;
    queueDesc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    queueDesc.Device = nullptr; // Destination is CPU memory

    if (SUCCEEDED(m_factory->CreateQueue(&queueDesc, IID_PPV_ARGS(&m_queue)))) {
        m_initialized = true;
    }
}

PatchEngine::~PatchEngine() {
    // ComPtrs will release automatically
}

HRESULT PatchEngine::ApplyDeltaToBuffer(
    const std::wstring& new_paf_path,
    const paf_delta_t& delta,
    void* out_buffer,
    uint64_t* out_total_copied_size) 
{
    if (!m_initialized) return E_FAIL;
    if (out_total_copied_size) *out_total_copied_size = 0;

    Microsoft::WRL::ComPtr<IDStorageFile> sourceFile;
    HRESULT hr = m_factory->OpenFile(new_paf_path.c_str(), IID_PPV_ARGS(&sourceFile));
    if (FAILED(hr)) return hr;

    uint64_t current_dst_offset = 0;
    uint32_t request_count = 0;

    for (uint32_t i = 0; i < delta.count; ++i) {
        const auto& entry = delta.entries[i];

        // Process only ADDED or UPDATED
        if (entry.status == PAF_DELTA_DELETED) continue;

        DSTORAGE_REQUEST request = {};
        request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
        
        // Source: File in the new PAF
        request.Source.File.Source = sourceFile.Get();
        request.Source.File.Offset = entry.new_offset;
        request.Source.File.Size = (uint32_t)entry.data_size;

        // Destination: Sequential position in the large buffer
        request.Destination.Memory.Buffer = (uint8_t*)out_buffer + current_dst_offset;
        request.Destination.Memory.Size = (uint32_t)entry.data_size;

        m_queue->EnqueueRequest(&request);
        
        current_dst_offset += entry.data_size;
        request_count++;

        if (request_count >= DSTORAGE_MAX_QUEUE_CAPACITY - 1) {
            ComPtr<IDStorageStatusArray> midStatus;
            m_factory->CreateStatusArray(1, nullptr, IID_PPV_ARGS(&midStatus));
            m_queue->EnqueueStatus(midStatus.Get(), 0);
            m_queue->Submit();
            DWORD elapsed = 0;
            while (!midStatus->IsComplete(0)) {
                if (elapsed >= PATCH_WAIT_TIMEOUT_MS) return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
                Sleep(1);
                elapsed++;
            }
            HRESULT midHr = midStatus->GetHResult(0);
            if (FAILED(midHr)) return midHr;
            request_count = 0;
        }
    }

    if (request_count == 0) return S_OK;

    // Monitor completion
    Microsoft::WRL::ComPtr<IDStorageStatusArray> statusArray;
    m_factory->CreateStatusArray(1, nullptr, IID_PPV_ARGS(&statusArray));
    m_queue->EnqueueStatus(statusArray.Get(), 0);
    m_queue->Submit();

    DWORD elapsed = 0;
    while (!statusArray->IsComplete(0)) {
        if (elapsed >= PATCH_WAIT_TIMEOUT_MS) return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        Sleep(1);
        elapsed++;
    }

    if (out_total_copied_size) *out_total_copied_size = current_dst_offset;

    return statusArray->GetHResult(0);
}

} // namespace paf
