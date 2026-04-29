#include "paf_patch.hpp"
#include <iostream>

#pragma comment(lib, "dstorage.lib")

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

        // If queue capacity reached, submit and wait or use multiple queues
        if (request_count >= DSTORAGE_MAX_QUEUE_CAPACITY - 1) {
            // In a production scenario, we'd handle batching here.
            // For the prototype, we assume DSTORAGE_MAX_QUEUE_CAPACITY is enough 
            // or we could implement a more complex pooling.
        }
    }

    if (request_count == 0) return S_OK;

    // Monitor completion
    Microsoft::WRL::ComPtr<IDStorageStatusArray> statusArray;
    m_factory->CreateStatusArray(1, nullptr, IID_PPV_ARGS(&statusArray));
    m_queue->EnqueueStatus(statusArray.Get(), 0);
    m_queue->Submit();

    // In a real application, you would use an event or a poll thread.
    // For the prototype, we block until complete.
    while (!statusArray->IsComplete(0)) {
        Sleep(1);
    }

    if (out_total_copied_size) *out_total_copied_size = current_dst_offset;

    return statusArray->GetHResult(0);
}

} // namespace paf
