#ifndef PAF_PATCH_HPP
#define PAF_PATCH_HPP

#include <windows.h>
#include <dstorage.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include "paf_delta.h"

namespace paf {

/**
 * High-performance patch application engine using DirectStorage v1.2.2.
 */
class PatchEngine {
public:
    PatchEngine();
    ~PatchEngine();

    /**
     * Extracts updated/added files from a new PAF and aggregates them into a sequential buffer.
     */
    HRESULT ApplyDeltaToBuffer(
        const std::wstring& new_paf_path,
        const paf_delta_t& delta,
        void* out_buffer,
        uint64_t* out_total_copied_size
    );

private:
    Microsoft::WRL::ComPtr<IDStorageFactory> m_factory;
    Microsoft::WRL::ComPtr<IDStorageQueue> m_queue;
    bool m_initialized;
};

} // namespace paf

#endif // PAF_PATCH_HPP
