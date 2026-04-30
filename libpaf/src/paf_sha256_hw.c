#include "paf_sha256_hw.h"
#include "../include/sha256.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

static const uint32_t SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

// ── Generic C fallback ────────────────────────────────────────────────────

static void paf_sha256_generic(const uint8_t* data, size_t len, uint8_t out[32]) {
    sha256_context_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

// ── Padding helper ────────────────────────────────────────────────────────

static uint8_t* sha256_pad(const uint8_t* data, size_t len, size_t* out_len) {
    size_t p = ((len + 9 + 63) / 64) * 64;
    uint8_t* buf = (uint8_t*)calloc(1, p);
    if (!buf) return NULL;
    memcpy(buf, data, len);
    buf[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) buf[p - 1 - i] = (uint8_t)(bits >> (i * 8));
    *out_len = p;
    return buf;
}

// ── ARM SHA-2 extensions (aarch64 only) ───────────────────────────────────

#if defined(__aarch64__)

#if defined(__linux__)
#  include <sys/auxv.h>
#  include <asm/hwcap.h>
#  ifndef HWCAP_SHA2
#    define HWCAP_SHA2 (1 << 6)
#  endif
static int arm_sha2_available(void) { return (getauxval(AT_HWCAP) & HWCAP_SHA2) != 0; }
#elif defined(__APPLE__)
#  include <sys/sysctl.h>
static int arm_sha2_available(void) {
    int v = 0; size_t sz = sizeof(v);
    return sysctlbyname("hw.optional.arm.FEAT_SHA256", &v, &sz, NULL, 0) == 0 && v;
}
#else
static int arm_sha2_available(void) { return 0; }
#endif

#if defined(__ARM_FEATURE_CRYPTO) || defined(__ARM_FEATURE_SHA2)
#include <arm_neon.h>

__attribute__((target("crypto")))
static void paf_sha256_arm(const uint8_t* data, size_t len, uint8_t out[32]) {
    uint32x4_t abcd = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au};
    uint32x4_t efgh = {0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};

    size_t padded;
    uint8_t* buf = sha256_pad(data, len, &padded);
    if (!buf) { paf_sha256_generic(data, len, out); return; }

    for (size_t off = 0; off < padded; off += 64) {
        const uint8_t* b = buf + off;
        uint32x4_t sv_a = abcd, sv_e = efgh;
        uint32x4_t m0 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(b     )));
        uint32x4_t m1 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(b + 16)));
        uint32x4_t m2 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(b + 32)));
        uint32x4_t m3 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(b + 48)));

#define AR(a,e,w,i) { uint32x4_t t=vaddq_u32((w),vld1q_u32(&SHA256_K[i])); \
        (e)=vsha256hq_u32((e),(a),t); (a)=vsha256h2q_u32((a),(e),t); }

        AR(abcd,efgh,m0, 0) AR(abcd,efgh,m1, 4)
        AR(abcd,efgh,m2, 8) AR(abcd,efgh,m3,12)
        m0=vsha256su1q_u32(vsha256su0q_u32(m0,m1),m2,m3); AR(abcd,efgh,m0,16)
        m1=vsha256su1q_u32(vsha256su0q_u32(m1,m2),m3,m0); AR(abcd,efgh,m1,20)
        m2=vsha256su1q_u32(vsha256su0q_u32(m2,m3),m0,m1); AR(abcd,efgh,m2,24)
        m3=vsha256su1q_u32(vsha256su0q_u32(m3,m0),m1,m2); AR(abcd,efgh,m3,28)
        m0=vsha256su1q_u32(vsha256su0q_u32(m0,m1),m2,m3); AR(abcd,efgh,m0,32)
        m1=vsha256su1q_u32(vsha256su0q_u32(m1,m2),m3,m0); AR(abcd,efgh,m1,36)
        m2=vsha256su1q_u32(vsha256su0q_u32(m2,m3),m0,m1); AR(abcd,efgh,m2,40)
        m3=vsha256su1q_u32(vsha256su0q_u32(m3,m0),m1,m2); AR(abcd,efgh,m3,44)
        m0=vsha256su1q_u32(vsha256su0q_u32(m0,m1),m2,m3); AR(abcd,efgh,m0,48)
        m1=vsha256su1q_u32(vsha256su0q_u32(m1,m2),m3,m0); AR(abcd,efgh,m1,52)
        m2=vsha256su1q_u32(vsha256su0q_u32(m2,m3),m0,m1); AR(abcd,efgh,m2,56)
        m3=vsha256su1q_u32(vsha256su0q_u32(m3,m0),m1,m2); AR(abcd,efgh,m3,60)
#undef AR

        abcd = vaddq_u32(abcd, sv_a);
        efgh = vaddq_u32(efgh, sv_e);
    }
    free(buf);

    // Store big-endian
    uint32x4_t ab = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(abcd)));
    uint32x4_t ef = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(efgh)));
    vst1q_u32((uint32_t*)out,      ab);
    vst1q_u32((uint32_t*)(out+16), ef);
}
#else
static void paf_sha256_arm(const uint8_t* d, size_t l, uint8_t o[32]) { paf_sha256_generic(d,l,o); }
#endif

#endif // __aarch64__

// ── x86-64 SHA-NI ─────────────────────────────────────────────────────────

#if defined(__x86_64__) || defined(_M_X64)

#if defined(__GNUC__) || defined(__clang__)
#  include <cpuid.h>
static int x86_sha_ni_available(void) {
    unsigned a,b,c,d;
    if (!__get_cpuid_count(7,0,&a,&b,&c,&d)) return 0;
    return (b >> 29) & 1;
}
#elif defined(_MSC_VER)
#  include <intrin.h>
static int x86_sha_ni_available(void) { int i[4]; __cpuidex(i,7,0); return (i[1]>>29)&1; }
#else
static int x86_sha_ni_available(void) { return 0; }
#endif

#if defined(__GNUC__) || defined(__clang__)
#include <immintrin.h>
#include <tmmintrin.h>

// Precompute the full 64-word message schedule in scalar; then use sha256rnds2
// for the 64 rounds (2 rounds/call × 32 calls).  This is correct and avoids
// the tricky msg1/msg2 ordering; scalar schedule cost is minimal vs. the rounds.
#define ROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define S0(x) (ROTR(x,7)^ROTR(x,18)^((x)>>3))
#define S1(x) (ROTR(x,17)^ROTR(x,19)^((x)>>10))

__attribute__((target("sha,sse4.1,ssse3")))
static void paf_sha256_x86ni(const uint8_t* data, size_t len, uint8_t out[32]) {
    // STATE0 = {A,B,E,F} dword[3..0], STATE1 = {C,D,G,H}
    __m128i S0 = _mm_set_epi32((int)0x6a09e667,(int)0xbb67ae85,(int)0x510e527f,(int)0x9b05688c);
    __m128i S1 = _mm_set_epi32((int)0x3c6ef372,(int)0xa54ff53a,(int)0x1f83d9ab,(int)0x5be0cd19);

    size_t padded;
    uint8_t* buf = sha256_pad(data, len, &padded);
    if (!buf) { paf_sha256_generic(data, len, out); return; }

    for (size_t off = 0; off < padded; off += 64) {
        const uint8_t* b = buf + off;
        __m128i sv0 = S0, sv1 = S1;

        // Build W[0..63] in scalar
        uint32_t W[64];
        const uint8_t* blk = b;
        for (int i = 0; i < 16; i++)
            W[i] = ((uint32_t)blk[i*4]<<24)|((uint32_t)blk[i*4+1]<<16)|
                   ((uint32_t)blk[i*4+2]<<8)|(uint32_t)blk[i*4+3];
        for (int i = 16; i < 64; i++)
            W[i] = S1(W[i-2]) + W[i-7] + S0(W[i-15]) + W[i-16];

        // 64 rounds via sha256rnds2 (2 rounds per call, 32 calls total)
        for (int r = 0; r < 64; r += 4) {
            __m128i MSG = _mm_set_epi32((int)(W[r+1]+SHA256_K[r+1]),
                                        (int)(W[r  ]+SHA256_K[r  ]),
                                        (int)(W[r+1]+SHA256_K[r+1]),
                                        (int)(W[r  ]+SHA256_K[r  ]));
            // Only low 2 dwords used by sha256rnds2; high 2 are ignored
            MSG = _mm_set_epi32(0, 0, (int)(W[r+1]+SHA256_K[r+1]), (int)(W[r]+SHA256_K[r]));
            S1 = _mm_sha256rnds2_epu32(S1, S0, MSG);
            MSG = _mm_shuffle_epi32(MSG, 0x0e);
            S0 = _mm_sha256rnds2_epu32(S0, S1, MSG);

            MSG = _mm_set_epi32(0, 0, (int)(W[r+3]+SHA256_K[r+3]), (int)(W[r+2]+SHA256_K[r+2]));
            S1 = _mm_sha256rnds2_epu32(S1, S0, MSG);
            MSG = _mm_shuffle_epi32(MSG, 0x0e);
            S0 = _mm_sha256rnds2_epu32(S0, S1, MSG);
        }

        S0 = _mm_add_epi32(S0, sv0);
        S1 = _mm_add_epi32(S1, sv1);
    }
    free(buf);

    // Rearrange: S0={A,B,E,F}, S1={C,D,G,H} → digest order ABCDEFGH
    __m128i tmp = _mm_shuffle_epi32(S0, 0x1b);  // {F,E,B,A}
    S1          = _mm_shuffle_epi32(S1, 0xb1);  // {D,C,H,G}
    S0          = _mm_blend_epi16(tmp, S1, 0xf0); // {D,C,B,A}
    S1          = _mm_alignr_epi8(S1, tmp, 8);    // {H,G,F,E}
    _mm_storeu_si128((__m128i*)out,      S0);
    _mm_storeu_si128((__m128i*)(out+16), S1);
}
#undef ROTR
#undef S0
#undef S1

#else
static void paf_sha256_x86ni(const uint8_t* d, size_t l, uint8_t o[32]) { paf_sha256_generic(d,l,o); }
#endif // GCC/Clang

#endif // x86_64

// ── Runtime dispatch ──────────────────────────────────────────────────────

typedef void (*sha256_fn_t)(const uint8_t*, size_t, uint8_t[32]);
static sha256_fn_t g_sha256_fn = NULL;

static void paf_sha256_detect(void) {
#if defined(__aarch64__)
    if (arm_sha2_available()) { g_sha256_fn = paf_sha256_arm; return; }
#endif
#if defined(__x86_64__) || defined(_M_X64)
    if (x86_sha_ni_available()) { g_sha256_fn = paf_sha256_x86ni; return; }
#endif
    g_sha256_fn = paf_sha256_generic;
}

void paf_sha256_compute(const uint8_t* data, size_t len, uint8_t out[32]) {
    if (!g_sha256_fn) paf_sha256_detect();
    g_sha256_fn(data, len, out);
}
