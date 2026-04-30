#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <stdint.h>
#include <stdlib.h>

#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))
#define CH(x,y,z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2)  ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6)  ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x)(ROTRIGHT(x,7)  ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x)(ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

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
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];
    #pragma unroll
    for (uint32_t i = 0; i < 16; ++i)
        m[i] = ((uint32_t)data[i*4]<<24)|((uint32_t)data[i*4+1]<<16)
              |((uint32_t)data[i*4+2]<<8)|(uint32_t)data[i*4+3];
    #pragma unroll
    for (uint32_t i = 16; i < 64; ++i)
        m[i] = SIG1(m[i-2]) + m[i-7] + SIG0(m[i-15]) + m[i-16];
    a=state[0]; b=state[1]; c=state[2]; d=state[3];
    e=state[4]; f=state[5]; g=state[6]; h=state[7];
    #pragma unroll
    for (uint32_t i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

__global__ void paf_sha256_kernel(const uint8_t* data, const uint64_t* offsets,
                                   const uint64_t* sizes, uint8_t* hashes,
                                   uint32_t count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    const uint8_t* ptr = data + offsets[idx];
    uint64_t size = sizes[idx];
    uint32_t state[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    uint8_t buf[64];
    uint64_t rem = size;

    while (rem >= 64) {
        #pragma unroll
        for (uint32_t i = 0; i < 64; i++) buf[i] = ptr[i];
        sha256_transform(state, buf);
        ptr += 64; rem -= 64;
    }
    memset(buf, 0, 64);
    for (uint32_t i = 0; i < (uint32_t)rem; i++) buf[i] = ptr[i];
    buf[rem] = 0x80;
    if (rem >= 56) {
        sha256_transform(state, buf);
        memset(buf, 0, 64);
    }
    uint64_t bits = size * 8;
    for (int i = 0; i < 8; i++)
        buf[63 - i] = (uint8_t)(bits >> (i * 8));
    sha256_transform(state, buf);

    #pragma unroll
    for (uint32_t i = 0; i < 8; i++) {
        hashes[idx*32 + i*4 + 0] = (state[i] >> 24) & 0xff;
        hashes[idx*32 + i*4 + 1] = (state[i] >> 16) & 0xff;
        hashes[idx*32 + i*4 + 2] = (state[i] >>  8) & 0xff;
        hashes[idx*32 + i*4 + 3] =  state[i]        & 0xff;
    }
}

extern "C" void paf_cuda_sha256_batch(const uint8_t* d_data, const uint64_t* d_offsets,
                                       const uint64_t* d_sizes, uint8_t* d_hashes,
                                       uint32_t count) {
    uint32_t threads = 256;
    uint32_t blocks  = (count + threads - 1) / threads;
    paf_sha256_kernel<<<blocks, threads>>>(d_data, d_offsets, d_sizes, d_hashes, count);
}

extern "C" int paf_cuda_init() {
    return (cudaFree(0) == cudaSuccess) ? 0 : -1;
}

// ── Persistent device buffer pool ───────────────────────────────────────────
// Grow-only: avoids cudaMalloc/cudaFree on every batch call.
// Sizes are tracked separately so we only reallocate when the new request
// doesn't fit in the existing allocation.

static uint8_t  *s_d_data    = NULL;
static uint8_t  *s_d_hashes  = NULL;
static uint64_t *s_d_offsets = NULL;
static uint64_t *s_d_sizes   = NULL;
static size_t    s_d_data_cap   = 0;  // bytes
static size_t    s_d_meta_cap   = 0;  // element count

static cudaStream_t s_stream = NULL;

static int ensure_device_buffers(size_t data_bytes, uint32_t count) {
    if (!s_stream) {
        if (cudaStreamCreate(&s_stream) != cudaSuccess) return -1;
    }
    if (data_bytes > s_d_data_cap) {
        // Grow by at least 2× to amortise future reallocs
        size_t new_cap = (s_d_data_cap * 2 > data_bytes) ? s_d_data_cap * 2 : data_bytes;
        cudaFree(s_d_data);
        if (cudaMalloc(&s_d_data, new_cap) != cudaSuccess) {
            s_d_data = NULL; s_d_data_cap = 0; return -1;
        }
        s_d_data_cap = new_cap;
    }
    if ((size_t)count > s_d_meta_cap) {
        size_t new_cap = (s_d_meta_cap * 2 > (size_t)count) ? s_d_meta_cap * 2 : (size_t)count;
        cudaFree(s_d_hashes);  cudaFree(s_d_offsets); cudaFree(s_d_sizes);
        s_d_hashes = NULL; s_d_offsets = NULL; s_d_sizes = NULL;
        if (cudaMalloc(&s_d_hashes,  new_cap * 32)               != cudaSuccess) goto fail;
        if (cudaMalloc(&s_d_offsets, new_cap * sizeof(uint64_t)) != cudaSuccess) goto fail;
        if (cudaMalloc(&s_d_sizes,   new_cap * sizeof(uint64_t)) != cudaSuccess) goto fail;
        s_d_meta_cap = new_cap;
    }
    return 0;
fail:
    cudaFree(s_d_hashes);  s_d_hashes  = NULL;
    cudaFree(s_d_offsets); s_d_offsets = NULL;
    cudaFree(s_d_sizes);   s_d_sizes   = NULL;
    s_d_meta_cap = 0;
    return -1;
}

// Host-to-host batch hash.
// When host_buf is pinned (cudaMallocHost) the async copies are true DMA —
// otherwise CUDA uses an internal staging buffer but the API still works.
extern "C" int paf_cuda_hash_flat(const uint8_t* host_buf,
                                   const uint64_t* host_offsets,
                                   const uint64_t* host_sizes,
                                   uint32_t count,
                                   uint8_t* host_out_hashes) {
    if (count == 0) return 0;

    size_t buf_size = (size_t)(host_offsets[count-1] + host_sizes[count-1]);

    if (ensure_device_buffers(buf_size, count) != 0) return -1;

    cudaMemcpyAsync(s_d_data,    host_buf,     buf_size,
                    cudaMemcpyHostToDevice, s_stream);
    cudaMemcpyAsync(s_d_offsets, host_offsets, (size_t)count * sizeof(uint64_t),
                    cudaMemcpyHostToDevice, s_stream);
    cudaMemcpyAsync(s_d_sizes,   host_sizes,   (size_t)count * sizeof(uint64_t),
                    cudaMemcpyHostToDevice, s_stream);

    uint32_t threads = 256;
    uint32_t blocks  = (count + threads - 1) / threads;
    paf_sha256_kernel<<<blocks, threads, 0, s_stream>>>(
        s_d_data, s_d_offsets, s_d_sizes, s_d_hashes, count);

    cudaMemcpyAsync(host_out_hashes, s_d_hashes, (size_t)count * 32,
                    cudaMemcpyDeviceToHost, s_stream);

    cudaStreamSynchronize(s_stream);
    return (cudaGetLastError() == cudaSuccess) ? 0 : -1;
}

// Allocate/free pinned (page-locked) host memory for zero-copy DMA transfers.
extern "C" void* paf_cuda_pinned_alloc(size_t size) {
    void* p = NULL;
    return (cudaMallocHost(&p, size) == cudaSuccess) ? p : NULL;
}
extern "C" void paf_cuda_pinned_free(void* p) {
    if (p) cudaFreeHost(p);
}

extern "C" int paf_cuda_hash_batch(const uint8_t** host_data_ptrs,
                                    const uint64_t* host_sizes,
                                    uint32_t count,
                                    uint8_t* host_out_hashes) {
    if (count == 0) return 0;

    uint64_t total = 0;
    for (uint32_t i = 0; i < count; i++) total += host_sizes[i];

    if (ensure_device_buffers((size_t)total, count) != 0) return -1;

    uint64_t* host_offsets = (uint64_t*)malloc(count * sizeof(uint64_t));
    if (!host_offsets) return -1;

    uint64_t off = 0;
    for (uint32_t i = 0; i < count; i++) {
        host_offsets[i] = off;
        cudaMemcpyAsync(s_d_data + off, host_data_ptrs[i], (size_t)host_sizes[i],
                        cudaMemcpyHostToDevice, s_stream);
        off += host_sizes[i];
    }
    cudaMemcpyAsync(s_d_offsets, host_offsets, count * sizeof(uint64_t),
                    cudaMemcpyHostToDevice, s_stream);
    cudaMemcpyAsync(s_d_sizes,   host_sizes,   count * sizeof(uint64_t),
                    cudaMemcpyHostToDevice, s_stream);

    uint32_t threads = 256;
    paf_sha256_kernel<<<(count+threads-1)/threads, threads, 0, s_stream>>>(
        s_d_data, s_d_offsets, s_d_sizes, s_d_hashes, count);

    cudaMemcpyAsync(host_out_hashes, s_d_hashes, count * 32,
                    cudaMemcpyDeviceToHost, s_stream);
    cudaStreamSynchronize(s_stream);
    free(host_offsets);
    return (cudaGetLastError() == cudaSuccess) ? 0 : -1;
}
