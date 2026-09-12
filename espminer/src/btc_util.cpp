#include "btc_util.h"
#include <string.h>
#include <math.h>

const double BTC_DIFF1 = 26959535291011309493156476344723991336010898738574164086137773096960.0;

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

size_t hex2bin(uint8_t *out, const char *hex, size_t out_len) {
    for (size_t i = 0; i < out_len; i++) {
        int hi = hex_value(hex[i * 2]);
        int lo = hi < 0 ? -1 : hex_value(hex[i * 2 + 1]);
        if (lo < 0) return 0;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return out_len;
}

void bin2hex(char *out, const uint8_t *in, size_t len) {
    static const char *d = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = d[in[i] >> 4];
        out[i * 2 + 1] = d[in[i] & 0x0f];
    }
    out[len * 2] = '\0';
}

// Shift a big-endian byte array left by `bits`, dropping what falls off
// the top. Used to build the numerator of the target division.
static void shift_left(uint8_t *a, size_t n, int bits) {
    if (bits <= 0) return;
    size_t byteShift = (size_t)bits / 8;
    int    bitShift  = bits % 8;
    if (byteShift >= n) {
        memset(a, 0, n);
        return;
    }
    if (byteShift) {
        memmove(a, a + byteShift, n - byteShift);
        memset(a + n - byteShift, 0, byteShift);
    }
    if (bitShift) {
        for (size_t i = 0; i + 1 < n; i++)
            a[i] = (uint8_t)((a[i] << bitShift) | (a[i + 1] >> (8 - bitShift)));
        a[n - 1] = (uint8_t)(a[n - 1] << bitShift);
    }
}

void diff_to_target(double difficulty, uint8_t target[32]) {
    if (!(difficulty > 0.0)) difficulty = 1.0;

    // Split the difficulty into mantissa * 2^exp with an exact integer
    // mantissa, so fractional difficulties (solo pools like to hand out
    // 0.001) come out exact instead of merely close:
    //
    //   target = 0xFFFF * 2^208 / (m * 2^(e-53))
    //          = (0xFFFF * 2^(261-e)) / m
    int    exp2 = 0;
    double frac = frexp(difficulty, &exp2);      // difficulty = frac * 2^exp2, 0.5 <= frac < 1
    uint64_t m = (uint64_t)ldexp(frac, 53);      // always an exact 53-bit integer
    if (m == 0) m = 1;

    int shift = 261 - exp2;
    if (shift < 0) {                             // difficulty beyond anything real
        memset(target, 0, 32);
        return;
    }
    if (shift > 303) shift = 303;                // keep the numerator inside 40 bytes

    uint8_t num[40];
    memset(num, 0, sizeof(num));
    num[38] = 0xFF;                              // start with the value 0xFFFF ...
    num[39] = 0xFF;
    shift_left(num, sizeof(num), shift);         // ... then scale it up

    // Long division by m. m < 2^53, so rem * 256 can never overflow.
    uint8_t  q[40];
    uint64_t rem = 0;
    for (int i = 0; i < 40; i++) {
        rem = (rem << 8) | num[i];
        q[i] = (uint8_t)(rem / m);
        rem %= m;
    }

    for (int i = 0; i < 8; i++) {
        if (q[i]) {  // difficulty so low the target overflows: anything wins
            memset(target, 0xFF, 32);
            return;
        }
    }
    memcpy(target, q + 8, 32);
}

void bits_to_target(uint32_t bits, uint8_t target[32]) {
    memset(target, 0, 32);
    uint32_t exponent = bits >> 24;
    uint32_t mantissa = bits & 0x007fffff;  // bit 23 is a sign flag, never set in practice
    if (exponent <= 3) {
        mantissa >>= 8 * (3 - exponent);
        exponent = 3;
    }
    // The mantissa's least significant byte sits at 2^(8*(exponent-3)),
    // i.e. index 32-(exponent-3)-1 in a big-endian array.
    int lsb_index = 32 - (int)(exponent - 3) - 1;
    for (int i = 0; i < 3; i++) {
        int idx = lsb_index - i;
        if (idx < 0 || idx > 31) continue;
        target[idx] = (uint8_t)(mantissa >> (8 * i));
    }
}

bool hash_meets_target(const uint8_t hash[32], const uint8_t target[32]) {
    // hash is little-endian; walk it from the most significant byte down.
    for (int i = 0; i < 32; i++) {
        uint8_t h = hash[31 - i];
        if (h < target[i]) return true;
        if (h > target[i]) return false;
    }
    return true;  // exactly equal still wins
}

double hash_difficulty(const uint8_t hash[32]) {
    double value = 0.0;
    for (int i = 31; i >= 0; i--) value = value * 256.0 + (double)hash[i];
    if (value <= 0.0) return INFINITY;
    return BTC_DIFF1 / value;
}
