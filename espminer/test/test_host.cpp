// Host-side unit tests for the parts of ESPMiner that are plain C++:
// SHA-256, the header/midstate fast path, difficulty <-> target math and
// the coinbase -> merkle root -> block hash pipeline.
//
//   cd test && make && ./test_host
//
// The expected values come from real Bitcoin block 125552 and from a
// Python reference implementation (see README).
#include "../src/sha256d.h"
#include "../src/btc_util.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

static int failures = 0;

static std::string toHex(const uint8_t *b, size_t n) {
    char buf[256];
    bin2hex(buf, b, n);
    return std::string(buf);
}

static void check(const char *name, const std::string &got, const std::string &want) {
    if (got == want) {
        printf("  ok    %s\n", name);
    } else {
        printf("  FAIL  %s\n        got  %s\n        want %s\n", name, got.c_str(), want.c_str());
        failures++;
    }
}

static void checkBool(const char *name, bool got, bool want) {
    check(name, got ? "true" : "false", want ? "true" : "false");
}

// The header is little-endian; write the 32-bit fields accordingly.
static void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

// Reverse a hash for display, the way block explorers show it.
static std::string displayHash(const uint8_t h[32]) {
    uint8_t r[32];
    for (int i = 0; i < 32; i++) r[i] = h[31 - i];
    return toHex(r, 32);
}

int main() {
    printf("SHA-256\n");
    {
        uint8_t out[32];
        sha256((const uint8_t *)"abc", 3, out);
        check("sha256(\"abc\")", toHex(out, 32),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

        sha256((const uint8_t *)"", 0, out);
        check("sha256(\"\")", toHex(out, 32),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

        // 56 bytes forces the two-block padding path
        const char *msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        sha256((const uint8_t *)msg, strlen(msg), out);
        check("sha256(56-byte msg)", toHex(out, 32),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

        sha256d((const uint8_t *)"abc", 3, out);
        check("sha256d(\"abc\")", toHex(out, 32),
              "4f8b42c22dd3729b519ba6f68d2da7cc5b2d606d05daed5ad5128cc03e6c6358");
    }

    printf("\nBlock 125552 (real block, real nonce)\n");
    {
        // What the pool would put in mining.notify, and what we must turn
        // it into: prevhash arrives with each 4-byte word byte-reversed.
        const char *notifyPrev = "ab02cd818b9e567ee21793cddef299feb29ad444a41b85b8000008a300000000";
        uint8_t raw[32], headerPrev[32];
        hex2bin(raw, notifyPrev, 32);
        for (int w = 0; w < 8; w++)
            for (int b = 0; b < 4; b++) headerPrev[w * 4 + b] = raw[w * 4 + 3 - b];

        uint8_t header[80];
        put_le32(header + 0, 1);                       // version
        memcpy(header + 4, headerPrev, 32);
        uint8_t merkle[32];
        // merkle root, display order -> header order
        uint8_t merkleDisplay[32];
        hex2bin(merkleDisplay, "2b12fcf1b09288fcaff797d71e950e71ae42b91e8bdb2304758dfcffc2b620e3", 32);
        for (int i = 0; i < 32; i++) merkle[i] = merkleDisplay[31 - i];
        memcpy(header + 36, merkle, 32);
        put_le32(header + 68, 0x4dd7f5c7);             // ntime
        put_le32(header + 72, 0x1a44b9f2);             // nbits
        put_le32(header + 76, 0x9546a142);             // the winning nonce

        uint8_t hash[32];
        sha256d(header, 80, hash);
        check("plain sha256d of the header", displayHash(hash),
              "00000000000000001e8d6829a8a21adc5d38d0a473b144b6765798e61f98bd1d");

        // Same thing through the miner's midstate fast path.
        uint32_t midstate[8];
        sha256_midstate(header, midstate);
        uint8_t tail[16];
        memcpy(tail, header + 64, 16);
        put_le32(tail + 12, 0x9546a142);
        uint8_t fast[32];
        sha256d_tail(midstate, tail, fast);
        check("midstate fast path agrees", toHex(fast, 32), toHex(hash, 32));

        // A wrong nonce must not produce the block hash.
        put_le32(tail + 12, 0x9546a141);
        sha256d_tail(midstate, tail, fast);
        checkBool("wrong nonce differs", memcmp(fast, hash, 32) != 0, true);

        // And it really does beat the network target of that era.
        uint8_t netTarget[32];
        bits_to_target(0x1a44b9f2, netTarget);
        check("bits 0x1a44b9f2 -> target", toHex(netTarget, 32),
              "00000000000044b9f20000000000000000000000000000000000000000000000");
        checkBool("block beats its own target", hash_meets_target(hash, netTarget), true);
    }

    printf("\nDifficulty -> target\n");
    {
        uint8_t t[32];
        diff_to_target(1.0, t);
        check("difficulty 1", toHex(t, 32),
              "00000000ffff0000000000000000000000000000000000000000000000000000");
        diff_to_target(2.0, t);
        check("difficulty 2", toHex(t, 32),
              "000000007fff8000000000000000000000000000000000000000000000000000");
        diff_to_target(512.0, t);
        check("difficulty 512", toHex(t, 32),
              "00000000007fff80000000000000000000000000000000000000000000000000");
        // Fractional difficulties must be exact, not merely close: these
        // are floor(0xFFFF * 2^208 / d) with d as the IEEE double the pool
        // actually sent.
        diff_to_target(0.001, t);
        check("difficulty 0.001", toHex(t, 32),
              "000003e7fc17fffffffa2405dc00000008c9f735fffffff2d10d2f00000013c6");
        diff_to_target(0.0001, t);
        check("difficulty 0.0001", toHex(t, 32),
              "0000270fd8efffffff791d46e3400001d1c6e73746fff9b79d29951ed415b1f5");
        diff_to_target(1e14, t);
        check("difficulty 1e14 (network scale)", toHex(t, 32),
              "00000000000000000002d090a040b49f3ac9edd4c4f468c7b68383f0bb918148");
    }

    printf("\nShare acceptance\n");
    {
        uint8_t t[32];
        diff_to_target(1.0, t);

        // hash is little-endian: byte 31 is the most significant one.
        // 00000000fffeffff... - just under the difficulty-1 target
        uint8_t easy[32];
        memset(easy, 0xFF, 32);
        easy[31] = easy[30] = easy[29] = easy[28] = 0x00;
        easy[26] = 0xFE;
        checkBool("hash just under the target wins", hash_meets_target(easy, t), true);

        // 00000000ffffffff... - just over it
        uint8_t nearMiss[32];
        memset(nearMiss, 0xFF, 32);
        nearMiss[31] = nearMiss[30] = nearMiss[29] = nearMiss[28] = 0x00;
        checkBool("hash just over the target loses", hash_meets_target(nearMiss, t), false);

        // exactly on target still counts
        uint8_t exact[32];
        for (int i = 0; i < 32; i++) exact[i] = t[31 - i];
        checkBool("hash exactly on target wins", hash_meets_target(exact, t), true);

        uint8_t hard[32];
        memset(hard, 0x11, 32);
        checkBool("a random hash does not", hash_meets_target(hard, t), false);

        // hash_difficulty should invert diff_to_target.
        uint8_t atTarget[32];
        for (int i = 0; i < 32; i++) atTarget[i] = t[31 - i];
        double d = hash_difficulty(atTarget);
        printf("  info  hash at the difficulty-1 target scores %.6f\n", d);
        checkBool("difficulty of a target-1 hash is ~1", d > 0.999 && d < 1.001, true);
    }

    printf("\nFull job pipeline (coinbase -> merkle -> header)\n");
    {
        const char *coinb1 = "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff20020862";
        const char *e1     = "a1b2c3d4";
        const char *e2     = "00000001";
        const char *coinb2 = "ffffffff0100f2052a010000001976a91400112233445566778899aabbccddeeff0011223388ac00000000";

        std::string hex = std::string(coinb1) + e1 + e2 + coinb2;
        uint8_t coinbase[512];
        size_t n = hex2bin(coinbase, hex.c_str(), hex.size() / 2);
        checkBool("coinbase hex parsed", n == hex.size() / 2, true);

        uint8_t root[32];
        sha256d(coinbase, n, root);

        const char *branches[3] = {
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"};
        for (int i = 0; i < 3; i++) {
            uint8_t pair[64];
            memcpy(pair, root, 32);
            hex2bin(pair + 32, branches[i], 32);
            sha256d(pair, 64, root);
        }
        check("merkle root", toHex(root, 32),
              "53223accf703f1eff96ee46075902eff2705647562b412bbf1a043c6622dd4e7");

        const char *notifyPrev = "ab02cd818b9e567ee21793cddef299feb29ad444a41b85b8000008a300000000";
        uint8_t raw[32], prev[32];
        hex2bin(raw, notifyPrev, 32);
        for (int w = 0; w < 8; w++)
            for (int b = 0; b < 4; b++) prev[w * 4 + b] = raw[w * 4 + 3 - b];

        uint8_t header[80];
        put_le32(header + 0, 2);
        memcpy(header + 4, prev, 32);
        memcpy(header + 36, root, 32);
        put_le32(header + 68, 0x5e9c1234);
        put_le32(header + 72, 0x1a44b9f2);
        put_le32(header + 76, 0);

        uint32_t midstate[8];
        sha256_midstate(header, midstate);
        uint8_t tail[16];
        memcpy(tail, header + 64, 16);
        put_le32(tail + 12, 0xdeadbeef);

        uint8_t hash[32];
        sha256d_tail(midstate, tail, hash);
        check("header hash for nonce 0xdeadbeef", toHex(hash, 32),
              "70b307728dda835aacc8f523c13f5cfd33cb39601b867658200bab98be9b0213");
    }

    printf("\nHex helpers\n");
    {
        uint8_t b[4];
        checkBool("rejects odd characters", hex2bin(b, "zz00", 2) == 0, true);
        checkBool("accepts upper case", hex2bin(b, "DEADBEEF", 4) == 4, true);
        check("round trips", toHex(b, 4), "deadbeef");
    }

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL TESTS PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
