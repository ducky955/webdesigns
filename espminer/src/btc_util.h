// Pure-C helpers shared by the miner: hex conversion, difficulty <-> target
// math and hash comparison. No Arduino dependencies, so test/test_host.cpp
// can compile and verify this on a PC.
#pragma once
#include <stdint.h>
#include <stddef.h>

// Difficulty-1 target: 0x00000000FFFF0000... == 0xFFFF * 2^208
extern const double BTC_DIFF1;

int  hex_value(char c);
// Returns the number of bytes written, or 0 if the input is not valid hex.
size_t hex2bin(uint8_t *out, const char *hex, size_t out_len);
void bin2hex(char *out, const uint8_t *in, size_t len);  // writes len*2 + NUL

// target = floor(0xFFFF * 2^208 / difficulty), big-endian.
void diff_to_target(double difficulty, uint8_t target[32]);
// The compact "nBits" encoding used in the block header, big-endian.
void bits_to_target(uint32_t bits, uint8_t target[32]);

// hash is the raw double-SHA256 output (little-endian numerically, i.e.
// the reverse of what a block explorer shows). True when hash <= target.
bool hash_meets_target(const uint8_t hash[32], const uint8_t target[32]);

// How hard was this hash, in pool-difficulty units?
double hash_difficulty(const uint8_t hash[32]);
