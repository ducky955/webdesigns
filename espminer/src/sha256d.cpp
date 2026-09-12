#include "sha256d.h"
#include <string.h>

static const uint32_t K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL, 0x59f111f1UL,
    0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL,
    0x0fc19dc6UL, 0x240ca1ccUL, 0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
    0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL, 0xa2bfe8a1UL, 0xa81a664bUL,
    0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL,
    0x5b9cca4fUL, 0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x) (ROR(x, 2) ^ ROR(x, 13) ^ ROR(x, 22))
#define BSIG1(x) (ROR(x, 6) ^ ROR(x, 11) ^ ROR(x, 25))
#define SSIG0(x) (ROR(x, 7) ^ ROR(x, 18) ^ ((x) >> 3))
#define SSIG1(x) (ROR(x, 17) ^ ROR(x, 19) ^ ((x) >> 10))

static inline uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

void sha256_init_state(uint32_t s[8]) {
    s[0] = 0x6a09e667UL; s[1] = 0xbb67ae85UL; s[2] = 0x3c6ef372UL; s[3] = 0xa54ff53aUL;
    s[4] = 0x510e527fUL; s[5] = 0x9b05688cUL; s[6] = 0x1f83d9abUL; s[7] = 0x5be0cd19UL;
}

void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) w[i] = be32(block + i * 4);
    for (int i = 16; i < 64; i++) w[i] = SSIG1(w[i - 2]) + w[i - 7] + SSIG0(w[i - 15]) + w[i - 16];

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + BSIG1(e) + CH(e, f, g) + K[i] + w[i];
        uint32_t t2 = BSIG0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static void store_state(const uint32_t s[8], uint8_t out[32]) {
    for (int i = 0; i < 8; i++) {
        out[i * 4 + 0] = (uint8_t)(s[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(s[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(s[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(s[i]);
    }
}

void sha256(const uint8_t *data, size_t len, uint8_t out[32]) {
    uint32_t st[8];
    sha256_init_state(st);

    size_t full = len / 64;
    for (size_t i = 0; i < full; i++) sha256_transform(st, data + i * 64);

    uint8_t tail[128];
    size_t rem = len - full * 64;
    memcpy(tail, data + full * 64, rem);
    tail[rem++] = 0x80;
    size_t tlen = (rem <= 56) ? 64 : 128;
    memset(tail + rem, 0, tlen - rem);

    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) tail[tlen - 1 - i] = (uint8_t)(bits >> (8 * i));

    for (size_t i = 0; i < tlen / 64; i++) sha256_transform(st, tail + i * 64);
    store_state(st, out);
}

void sha256d(const uint8_t *data, size_t len, uint8_t out[32]) {
    uint8_t first[32];
    sha256(data, len, first);
    sha256(first, 32, out);
}

void sha256_midstate(const uint8_t block0[64], uint32_t midstate[8]) {
    sha256_init_state(midstate);
    sha256_transform(midstate, block0);
}

void sha256d_tail(const uint32_t midstate[8], const uint8_t tail[16], uint8_t out[32]) {
    // --- finish the first hash: 16 bytes of header left, total 80 bytes
    uint8_t block[64];
    memcpy(block, tail, 16);
    block[16] = 0x80;
    memset(block + 17, 0, 45);
    block[62] = 0x02;  // 80 bytes = 640 bits = 0x0280
    block[63] = 0x80;

    uint32_t st[8];
    memcpy(st, midstate, sizeof(st));
    sha256_transform(st, block);

    // --- second hash over those 32 bytes
    store_state(st, block);
    block[32] = 0x80;
    memset(block + 33, 0, 29);
    block[62] = 0x01;  // 32 bytes = 256 bits = 0x0100
    block[63] = 0x00;

    sha256_init_state(st);
    sha256_transform(st, block);
    store_state(st, out);
}
