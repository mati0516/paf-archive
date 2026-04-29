#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    unsigned long long bitlen;
    uint32_t state[8];
} sha256_context_t;

void sha256_init(sha256_context_t *ctx);
void sha256_update(sha256_context_t *ctx, const uint8_t data[], size_t len);
void sha256_final(sha256_context_t *ctx, uint8_t hash[]);

#ifdef __cplusplus
}
#endif

#endif // SHA256_H
