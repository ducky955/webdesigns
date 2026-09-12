// Small, dependency-free SHA-256 with the pieces a Bitcoin miner needs:
// a raw block transform and a midstate-accelerated double hash of an
// 80-byte block header.
#pragma once
#include <stdint.h>
#include <stddef.h>

void sha256_init_state(uint32_t state[8]);
void sha256_transform(uint32_t state[8], const uint8_t block[64]);

// One-shot helpers (used for the coinbase and the merkle branch).
void sha256(const uint8_t *data, size_t len, uint8_t out[32]);
void sha256d(const uint8_t *data, size_t len, uint8_t out[32]);

// --- miner fast path -------------------------------------------------
// A block header is 80 bytes = one full 64-byte block (which does not
// depend on the nonce) + a 16-byte tail (which does). Hash the first
// block once per job, then only two transforms are needed per nonce.
void sha256_midstate(const uint8_t block0[64], uint32_t midstate[8]);
void sha256d_tail(const uint32_t midstate[8], const uint8_t tail[16], uint8_t out[32]);
