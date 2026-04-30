#ifndef PAF_SHA256_HW_H
#define PAF_SHA256_HW_H

#include <stdint.h>
#include <stddef.h>

// Single-shot SHA-256: hashes data[0..len) and writes 32-byte digest to out.
// Dispatches at runtime to the best available implementation:
//   ARM SHA extensions  (aarch64 with HWCAP_SHA2 / hw.optional.arm.FEAT_SHA256)
//   x86 SHA-NI          (x86-64 with CPUID.07H.EBX[29])
//   generic C           (always available, portable fallback)
//
// Initialisation is lazy — the first call detects hardware and sets the
// function pointer; subsequent calls pay only one indirect branch.
void paf_sha256_compute(const uint8_t* data, size_t len, uint8_t out[32]);

#endif // PAF_SHA256_HW_H
