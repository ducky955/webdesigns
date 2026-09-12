// The hashing engine: owns the current job, runs one FreeRTOS task per
// core and hands finished shares back to the stratum client.
#pragma once
#include <Arduino.h>
#include <stdint.h>

#define MAX_MERKLE_BRANCHES 24
#define MAX_COINB_HEX       1024

struct StratumJob {
    char     jobId[48];
    uint32_t version;                              // header value, host order
    uint8_t  prevhash[32];                         // already in header order
    uint32_t ntime;
    uint32_t nbits;
    char     coinb1[MAX_COINB_HEX + 1];
    char     coinb2[MAX_COINB_HEX + 1];
    uint8_t  merkle[MAX_MERKLE_BRANCHES][32];
    uint8_t  merkleCount;
    char     extranonce1[33];
    uint8_t  extranonce2Size;
    bool     valid;
};

struct FoundShare {
    char     jobId[48];
    char     extranonce2[33];
    uint32_t ntime;
    uint32_t nonce;
    double   difficulty;
    bool     isBlock;
};

struct MinerStats {
    uint64_t totalHashes;
    uint32_t sharesFound;
    uint32_t sharesAccepted;
    uint32_t sharesRejected;
    uint32_t blocksFound;
    uint32_t jobsReceived;
    double   bestDifficulty;
    double   hashrate;        // H/s, smoothed over a few seconds
    double   poolDifficulty;
};

void miner_begin(uint8_t taskCount);
void miner_set_job(const StratumJob &job);
void miner_set_difficulty(double difficulty);
void miner_set_extranonce(const char *extranonce1, uint8_t extranonce2Size);
void miner_clear_job();                    // e.g. pool connection lost

bool miner_pop_share(FoundShare &out);     // non-blocking
void miner_report_share(bool accepted);

void miner_note_difficulty(double difficulty);

void       miner_tick();                   // call about once a second
MinerStats miner_get_stats();
