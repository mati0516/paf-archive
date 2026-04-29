#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <stdint.h>

// SHA-256 constants and macros optimized for CUDA
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

__constant__ uint32_t k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

__device__ void sha256_transform(uint32_t state[8], const uint8_t data[64]) {
    uint32_t a, b, c, d, e, f, g, h, i, t1, t2, m[64];
    
    #pragma unroll
    for (i = 0; i < 16; ++i)
        m[i] = (data[i * 4] << 24) | (data[i * 4 + 1] << 16) | (data[i * 4 + 2] << 8) | (data[i * 4 + 3]);
    
    #pragma unroll
    for (i = 16; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
        
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    
    #pragma unroll
    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

__global__ void paf_sha256_kernel(const uint8_t* data, const uint64_t* offsets, const uint64_t* sizes, uint8_t* hashes, uint32_t count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    uint64_t offset = offsets[idx];
    uint64_t size = sizes[idx];
    const uint8_t* file_ptr = data + offset;

    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    uint8_t buffer[64];
    uint64_t remaining = size;
    uint64_t bitlen = size * 8;
    
    // Process full blocks
    while (remaining >= 64) {
        #pragma unroll
        for(int i=0; i<64; i++) buffer[i] = file_ptr[i];
        sha256_transform(state, buffer);
        file_ptr += 64;
        remaining -= 64;
    }

    // Final padding
    memset(buffer, 0, 64);
    for(int i=0; i<remaining; i++) buffer[i] = file_ptr[i];
    buffer[remaining] = 0x80;
    
    if (remaining >= 56) {
        sha256_transform(state, buffer);
        memset(buffer, 0, 64);
    }
    
    // Append bit length (big-endian)
    buffer[63] = (uint8_t)(bitlen & 0xff);
    buffer[62] = (uint8_t)((bitlen >> 8) & 0xff);
    buffer[61] = (uint8_t)((bitlen >> 16) & 0xff);
    buffer[60] = (uint8_t)((bitlen >> 24) & 0xff);
    buffer[59] = (uint8_t)((bitlen >> 32) & 0xff);
    buffer[58] = (uint8_t)((bitlen >> 40) & 0xff);
    buffer[57] = (uint8_t)((bitlen >> 48) & 0xff);
    buffer[56] = (uint8_t)((bitlen >> 56) & 0xff);
    sha256_transform(state, buffer);

    // Write out results
    #pragma unroll
    for (int i = 0; i < 8; i++) {
        hashes[idx * 32 + i * 4 + 0] = (state[i] >> 24) & 0xff;
        hashes[idx * 32 + i * 4 + 1] = (state[i] >> 16) & 0xff;
        hashes[idx * 32 + i * 4 + 2] = (state[i] >> 8) & 0xff;
        hashes[idx * 32 + i * 4 + 3] = (state[i] & 0xff);
    }
}

extern "C" void paf_cuda_sha256_batch(const uint8_t* d_data, const uint64_t* d_offsets, const uint64_t* d_sizes, uint8_t* d_hashes, uint32_t count) {
    uint32_t threads_per_block = 256;
    uint32_t blocks = (count + threads_per_block - 1) / threads_per_block;
    paf_sha256_kernel<<<blocks, threads_per_block>>>(d_data, d_offsets, d_sizes, d_hashes, count);
}

extern "C" int paf_cuda_init() {
    return (cudaFree(0) == cudaSuccess) ? 0 : -1;
}

extern "C" int paf_cuda_hash_batch(const uint8_t** host_data_ptrs, const uint64_t* host_sizes, uint32_t count, uint8_t* host_out_hashes) {
    if (count == 0) return 0;

    uint64_t total_data_size = 0;
    for (uint32_t i = 0; i < count; i++) total_data_size += host_sizes[i];

    uint8_t *d_data = NULL, *d_hashes = NULL;
    uint64_t *d_offsets = NULL, *d_sizes = NULL;

    if (cudaMalloc(&d_data, total_data_size) != cudaSuccess) return -1;
    if (cudaMalloc(&d_hashes, count * 32) != cudaSuccess) { cudaFree(d_data); return -1; }
    if (cudaMalloc(&d_offsets, count * sizeof(uint64_t)) != cudaSuccess) { cudaFree(d_data); cudaFree(d_hashes); return -1; }
    if (cudaMalloc(&d_sizes, count * sizeof(uint64_t)) != cudaSuccess) { cudaFree(d_data); cudaFree(d_hashes); cudaFree(d_offsets); return -1; }

    uint64_t* host_offsets = (uint64_t*)malloc(count * sizeof(uint64_t));
    if (!host_offsets) { cudaFree(d_data); cudaFree(d_hashes); cudaFree(d_offsets); cudaFree(d_sizes); return -1; }
    uint64_t current_offset = 0;
    for (uint32_t i = 0; i < count; i++) {
        host_offsets[i] = current_offset;
        cudaMemcpy(d_data + current_offset, host_data_ptrs[i], host_sizes[i], cudaMemcpyHostToDevice);
        current_offset += host_sizes[i];
    }

    cudaMemcpy(d_offsets, host_offsets, count * sizeof(uint64_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_sizes, host_sizes, count * sizeof(uint64_t), cudaMemcpyHostToDevice);

    paf_cuda_sha256_batch(d_data, d_offsets, d_sizes, d_hashes, count);
    cudaDeviceSynchronize();

    cudaMemcpy(host_out_hashes, d_hashes, count * 32, cudaMemcpyDeviceToHost);

    cudaFree(d_data);
    cudaFree(d_hashes);
    cudaFree(d_offsets);
    cudaFree(d_sizes);
    free(host_offsets);

    return 0;
}
