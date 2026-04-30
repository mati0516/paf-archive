#ifdef _WIN32
#include <windows.h>
#include "dstorage.h"
#include <wrl/client.h>
#include <stdint.h>
#include <string>

using Microsoft::WRL::ComPtr;

typedef HRESULT (WINAPI *PFN_DSTORAGE_GET_FACTORY)(REFIID riid, void** ppv);

static PFN_DSTORAGE_GET_FACTORY s_DStorageGetFactory = nullptr;
static HMODULE                  s_hDStorage           = nullptr;

static bool LoadDStorageOnce() {
    if (s_DStorageGetFactory) return true;
    s_hDStorage = LoadLibraryA("dstorage.dll");
    if (!s_hDStorage) return false;
    s_DStorageGetFactory = (PFN_DSTORAGE_GET_FACTORY)(uintptr_t)
                           GetProcAddress(s_hDStorage, "DStorageGetFactory");
    if (!s_DStorageGetFactory) {
        FreeLibrary(s_hDStorage);
        s_hDStorage = nullptr;
        return false;
    }
    return true;
}

// Capacity per sub-batch: leave 2 slots for status + event entries.
static const uint32_t DS_BATCH_CAP = DSTORAGE_MAX_QUEUE_CAPACITY - 2;

class PafDirectStorage {
    ComPtr<IDStorageFactory> m_factory;
    ComPtr<IDStorageQueue>   m_queue;
    ComPtr<IDStorageQueue1>  m_queue1;   // non-null if SDK >= 1.1 (EnqueueSetEvent)
    ComPtr<IDStorageFile>    m_file;
    std::wstring             m_current_path;

    HRESULT OpenFile(const wchar_t* path) {
        m_file.Reset();
        HRESULT hr = m_factory->OpenFile(path, IID_PPV_ARGS(&m_file));
        if (SUCCEEDED(hr)) m_current_path = path;
        return hr;
    }

    // Submit one sub-batch of up to DS_BATCH_CAP already-enqueued requests and
    // wait for completion.  Uses EnqueueSetEvent (zero-CPU-spin) when available,
    // falls back to statusArray + Sleep(0) yield loop.
    HRESULT SubmitAndWait() {
        ComPtr<IDStorageStatusArray> status;
        HRESULT hr = m_factory->CreateStatusArray(1, nullptr, IID_PPV_ARGS(&status));
        if (FAILED(hr)) return hr;
        m_queue->EnqueueStatus(status.Get(), 0);

        if (m_queue1) {
            HANDLE hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!hEvent) return E_OUTOFMEMORY;
            m_queue1->EnqueueSetEvent(hEvent);
            m_queue->Submit();
            WaitForSingleObject(hEvent, INFINITE);
            CloseHandle(hEvent);
        } else {
            m_queue->Submit();
            // Sleep(0) yields the time slice instead of burning 1ms per poll.
            while (!status->IsComplete(0)) Sleep(0);
        }
        return status->GetHResult(0);
    }

public:
    HRESULT Initialize(const wchar_t* path) {
        if (!LoadDStorageOnce()) return E_NOTIMPL;

        HRESULT hr = s_DStorageGetFactory(IID_PPV_ARGS(&m_factory));
        if (FAILED(hr)) return hr;

        DSTORAGE_QUEUE_DESC qd = {};
        qd.Capacity   = DSTORAGE_MAX_QUEUE_CAPACITY;
        qd.Priority   = DSTORAGE_PRIORITY_NORMAL;
        qd.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        qd.Device     = nullptr;

        hr = m_factory->CreateQueue(&qd, IID_PPV_ARGS(&m_queue));
        if (FAILED(hr)) return hr;

        // Opportunistically get IDStorageQueue1 (SDK >= 1.1) for EnqueueSetEvent.
        m_queue.As(&m_queue1);

        return OpenFile(path);
    }

    bool IsInitialized() const { return m_factory != nullptr; }

    HRESULT EnsureFile(const wchar_t* path) {
        if (m_current_path != path) return OpenFile(path);
        return S_OK;
    }

    // Load a batch of N file regions into flat[dst_offsets[i]..].
    // Enqueues DS_BATCH_CAP requests at a time, submitting + waiting between
    // sub-batches so we never overflow the queue.
    // io_failed[i] is set to 1 on per-sub-batch failure (coarse granularity).
    HRESULT LoadBatch(const uint64_t* paf_offsets, const uint64_t* sizes,
                      uint8_t* flat, const uint64_t* dst_offsets,
                      uint32_t count, uint8_t* io_failed)
    {
        for (uint32_t base = 0; base < count; ) {
            uint32_t n = count - base;
            if (n > DS_BATCH_CAP) n = DS_BATCH_CAP;

            uint32_t enqueued = 0;
            for (uint32_t i = 0; i < n; i++) {
                if (sizes[base + i] == 0) continue;
                DSTORAGE_REQUEST req = {};
                req.Options.SourceType      = DSTORAGE_REQUEST_SOURCE_FILE;
                req.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
                req.Source.File.Source      = m_file.Get();
                req.Source.File.Offset      = paf_offsets[base + i];
                req.Source.File.Size        = (UINT32)sizes[base + i];
                req.Destination.Memory.Buffer = flat + dst_offsets[base + i];
                req.Destination.Memory.Size   = (UINT32)sizes[base + i];
                m_queue->EnqueueRequest(&req);
                enqueued++;
            }

            if (enqueued > 0) {
                HRESULT hr = SubmitAndWait();
                if (FAILED(hr)) {
                    for (uint32_t i = 0; i < n; i++)
                        io_failed[base + i] = 1;
                }
            }
            base += n;
        }
        return S_OK;
    }
};

// ─── Single-file API (kept for compatibility) ────────────────────────────────

extern "C" int paf_io_directstorage_load(const wchar_t* path, uint64_t offset,
                                          uint64_t size, void* destination) {
    static PafDirectStorage ds;
    if (!ds.IsInitialized()) {
        if (FAILED(ds.Initialize(path))) return -1;
    } else {
        if (FAILED(ds.EnsureFile(path))) return -1;
    }
    uint64_t dst_off = 0;
    uint8_t  failed  = 0;
    return SUCCEEDED(ds.LoadBatch(&offset, &size,
                                  (uint8_t*)destination, &dst_off,
                                  1, &failed)) && !failed ? 0 : -1;
}

// ─── Batch API: N regions → flat buffer ────────────────────────────────────
// paf_offsets[i] : absolute byte offset within the PAF file
// sizes[i]       : byte count to read (0 entries are skipped)
// flat           : contiguous destination buffer
// dst_offsets[i] : byte offset within flat for file i
// io_failed[i]   : set to 1 on failure (output, must be zero-initialised by caller)

extern "C" int paf_io_directstorage_load_batch(const wchar_t* path,
                                                const uint64_t* paf_offsets,
                                                const uint64_t* sizes,
                                                uint8_t* flat,
                                                const uint64_t* dst_offsets,
                                                uint32_t count,
                                                uint8_t* io_failed) {
    static PafDirectStorage ds_batch;
    if (!ds_batch.IsInitialized()) {
        if (FAILED(ds_batch.Initialize(path))) return -1;
    } else {
        if (FAILED(ds_batch.EnsureFile(path))) return -1;
    }
    return SUCCEEDED(ds_batch.LoadBatch(paf_offsets, sizes, flat,
                                        dst_offsets, count, io_failed)) ? 0 : -1;
}

#endif // _WIN32
