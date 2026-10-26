/*
 * sha256.h - Minimal, dependency-free SHA-256 (FIPS 180-4).
 *
 * Used to produce dump_sha256.txt and the per-file hashes in the manifest.
 * Streaming API so a multi-gigabyte dump can be hashed while it is written,
 * without holding it in memory.
 *
 * This implementation is placed in the public domain (see docs/THIRD_PARTY.md).
 */
#ifndef UDT_SHA256_H
#define UDT_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHA256_BLOCK_SIZE   64
#define SHA256_DIGEST_SIZE  32

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  buffer[SHA256_BLOCK_SIZE];
    size_t   buflen;
} sha256_ctx;

void sha256_init(sha256_ctx *ctx);
void sha256_update(sha256_ctx *ctx, const void *data, size_t len);
void sha256_final(sha256_ctx *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);

/* One-shot helper. */
void sha256(const void *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]);

/* Format a digest as 64 lowercase hex chars + NUL. out must be >= 65 bytes. */
void sha256_hex(const uint8_t digest[SHA256_DIGEST_SIZE], char out[65]);

#ifdef __cplusplus
}
#endif

#endif /* UDT_SHA256_H */
