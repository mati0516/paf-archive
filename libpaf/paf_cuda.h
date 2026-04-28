#ifndef PAF_CUDA_H
#define PAF_CUDA_H

#include <stdint.h>

/**
 * Run parallel SHA-256 hashing on GPU for a batch of files.
 * @param d_data Pointer to data block in GPU memory.
 * @param d_offsets Array of offsets for each file in the data block (in GPU memory).
 * @param d_sizes Array of sizes for each file (in GPU memory).
 * @param d_hashes Output array for 32-byte hashes (in GPU memory).
 * @param count Number of files in this batch.
 */
extern "C" void paf_cuda_sha256_batch(const uint8_t* d_data, const uint64_t* d_offsets, const uint64_t* d_sizes, uint8_t* d_hashes, uint32_t count);

#endif // PAF_CUDA_H
