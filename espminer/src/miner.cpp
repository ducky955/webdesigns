#include "miner.h"
#include "sha256d.h"
#include "btc_util.h"
#include "config.h"
#include "mlog.h"
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// ---------------------------------------------------------------------
// shared state
// ---------------------------------------------------------------------
static SemaphoreHandle_t s_jobMutex = nullptr;
static QueueHandle_t     s_shareQueue = nullptr;

static StratumJob s_job;                 // guarded by s_jobMutex
static uint8_t    s_target[32];          // pool share target, guarded too
static double     s_poolDiff = 1.0;
static volatile uint32_t s_jobVersion = 0;   // bumped on every job change

static volatile uint32_t s_hashCounter[4];   // per-task, free-running
static uint32_t s_lastCounter[4];
static uint8_t  s_taskCount = 0;

static uint64_t s_totalHashes = 0;
static double   s_hashrate = 0.0;
static uint32_t s_lastTickMs = 0;

static volatile uint32_t s_sharesFound = 0;
static volatile uint32_t s_blocksFound = 0;
static uint32_t s_sharesAccepted = 0;
static uint32_t s_sharesRejected = 0;
static uint32_t s_jobsReceived = 0;
static double   s_bestDifficulty = 0.0;

static inline void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

// ---------------------------------------------------------------------
// the hashing task
// ---------------------------------------------------------------------
static void minerTask(void *arg) {
    const uint8_t id = (uint8_t)(uintptr_t)arg;

    StratumJob *job = (StratumJob *)malloc(sizeof(StratumJob));
    uint8_t    *coinbase = (uint8_t *)malloc(MAX_COINB_HEX + 64);
    if (!job || !coinbase) {
        MLOG("miner task %u: out of memory", id);
        vTaskDelete(nullptr);
        return;
    }
    memset(job, 0, sizeof(StratumJob));

    uint8_t  target[32];
    uint8_t  networkTarget[32];
    uint32_t localVersion = 0;
    // Give every task its own slice of the extranonce2 space so two tasks
    // never grind the identical search space.
    uint32_t extranonce2 = (uint32_t)id << 28;
    uint32_t hashes = 0;
    bool     fastReject = true;

    for (;;) {
        // ---- pick up the current job -------------------------------
        if (s_jobVersion != localVersion) {
            xSemaphoreTake(s_jobMutex, portMAX_DELAY);
            memcpy(job, &s_job, sizeof(StratumJob));
            memcpy(target, s_target, 32);
            localVersion = s_jobVersion;
            xSemaphoreGive(s_jobMutex);
            bits_to_target(job->nbits, networkTarget);
            // The cheap pre-check below only holds while the target has two
            // zero bytes on top, i.e. difficulty above roughly 1/65536.
            fastReject = (target[0] == 0 && target[1] == 0);
        }
        if (!job->valid) {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        // ---- coinbase -> merkle root -------------------------------
        char e2hex[33];
        uint8_t e2size = job->extranonce2Size;
        if (e2size > 16) e2size = 16;
        uint8_t e2bin[16];
        memset(e2bin, 0, sizeof(e2bin));
        for (uint8_t i = 0; i < 4 && i < e2size; i++)
            e2bin[e2size - 1 - i] = (uint8_t)(extranonce2 >> (8 * i));
        bin2hex(e2hex, e2bin, e2size);
        extranonce2++;

        size_t c1 = strlen(job->coinb1);
        size_t e1 = strlen(job->extranonce1);
        size_t c2 = strlen(job->coinb2);
        size_t hexLen = c1 + e1 + e2size * 2 + c2;
        if (hexLen % 2 || hexLen / 2 > MAX_COINB_HEX + 64) {
            MLOG("coinbase too large (%u hex chars)", (unsigned)hexLen);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        {   // assemble the full coinbase transaction, then hash it
            char *hex = (char *)malloc(hexLen + 1);
            if (!hex) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
            memcpy(hex, job->coinb1, c1);
            memcpy(hex + c1, job->extranonce1, e1);
            memcpy(hex + c1 + e1, e2hex, e2size * 2);
            memcpy(hex + c1 + e1 + e2size * 2, job->coinb2, c2);
            hex[hexLen] = '\0';
            size_t written = hex2bin(coinbase, hex, hexLen / 2);
            free(hex);
            if (written == 0) {
                MLOG("bad coinbase hex from pool");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
        }

        uint8_t merkleRoot[32];
        sha256d(coinbase, hexLen / 2, merkleRoot);
        for (uint8_t i = 0; i < job->merkleCount; i++) {
            uint8_t pair[64];
            memcpy(pair, merkleRoot, 32);
            memcpy(pair + 32, job->merkle[i], 32);
            sha256d(pair, 64, merkleRoot);
        }

        // ---- block header ------------------------------------------
        uint8_t header[80];
        put_le32(header + 0, job->version);
        memcpy(header + 4, job->prevhash, 32);
        memcpy(header + 36, merkleRoot, 32);
        put_le32(header + 68, job->ntime);
        put_le32(header + 72, job->nbits);
        put_le32(header + 76, 0);

        uint32_t midstate[8];
        sha256_midstate(header, midstate);       // first 64 bytes never change
        uint8_t tail[16];
        memcpy(tail, header + 64, 16);

        // ---- grind -------------------------------------------------
        uint8_t hash[32];
        for (uint32_t nonce = 0;; nonce++) {
            put_le32(tail + 12, nonce);
            sha256d_tail(midstate, tail, hash);
            hashes++;

            // 65535 of every 65536 hashes fail on the two top bytes alone,
            // so check those before the full comparison.
            bool hit = (!fastReject || (hash[31] | hash[30]) == 0) &&
                       hash_meets_target(hash, target);
            if (hit) {
                FoundShare share;
                memcpy(share.jobId, job->jobId, sizeof(share.jobId));
                memcpy(share.extranonce2, e2hex, sizeof(e2hex));
                share.ntime = job->ntime;
                share.nonce = nonce;
                share.difficulty = hash_difficulty(hash);
                share.isBlock = hash_meets_target(hash, networkTarget);

                s_sharesFound++;
                miner_note_difficulty(share.difficulty);
                if (share.isBlock) {
                    s_blocksFound++;
                    MLOG("*** BLOCK CANDIDATE FOUND *** job=%s nonce=%08x", share.jobId, nonce);
                }
                if (s_shareQueue) xQueueSend(s_shareQueue, &share, 0);
            }

            if ((nonce & 0x3FFF) == 0x3FFF) {
                s_hashCounter[id] += hashes;
                hashes = 0;
                // Let the idle task run so the watchdog stays happy.
                vTaskDelay(1);
                if (s_jobVersion != localVersion) break;   // new job, restart
            }
            if (nonce == 0xFFFFFFFFu) break;               // roll extranonce2
        }
    }
}

// ---------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------
void miner_begin(uint8_t taskCount) {
    if (taskCount < 1) taskCount = 1;
    if (taskCount > 4) taskCount = 4;
    s_taskCount = taskCount;

    s_jobMutex = xSemaphoreCreateMutex();
    s_shareQueue = xQueueCreate(8, sizeof(FoundShare));
    memset(&s_job, 0, sizeof(s_job));
    diff_to_target(s_poolDiff, s_target);
    s_lastTickMs = millis();

    for (uint8_t i = 0; i < taskCount; i++) {
        char name[16];
        snprintf(name, sizeof(name), "miner%u", i);
        // Priority 1 keeps us below the Wi-Fi/TCP stack; pinning spreads the
        // tasks over both cores.
        xTaskCreatePinnedToCore(minerTask, name, 6144, (void *)(uintptr_t)i, 1,
                                nullptr, i % 2);
    }
    MLOG("started %u mining task(s)", taskCount);
}

void miner_set_job(const StratumJob &job) {
    xSemaphoreTake(s_jobMutex, portMAX_DELAY);
    char e1[33];
    uint8_t e2 = s_job.extranonce2Size;
    strncpy(e1, s_job.extranonce1, sizeof(e1));
    e1[sizeof(e1) - 1] = '\0';

    s_job = job;
    if (s_job.extranonce1[0] == '\0' && e1[0] != '\0') {
        strncpy(s_job.extranonce1, e1, sizeof(s_job.extranonce1));
        s_job.extranonce2Size = e2;
    }
    s_jobsReceived++;
    s_jobVersion++;
    xSemaphoreGive(s_jobMutex);
}

void miner_set_difficulty(double difficulty) {
    if (!(difficulty > 0)) return;
    xSemaphoreTake(s_jobMutex, portMAX_DELAY);
    s_poolDiff = difficulty;
    diff_to_target(difficulty, s_target);
    s_jobVersion++;
    xSemaphoreGive(s_jobMutex);
    MLOG("pool difficulty set to %.6f", difficulty);
}

void miner_set_extranonce(const char *extranonce1, uint8_t extranonce2Size) {
    xSemaphoreTake(s_jobMutex, portMAX_DELAY);
    strncpy(s_job.extranonce1, extranonce1, sizeof(s_job.extranonce1) - 1);
    s_job.extranonce1[sizeof(s_job.extranonce1) - 1] = '\0';
    s_job.extranonce2Size = extranonce2Size;
    s_jobVersion++;
    xSemaphoreGive(s_jobMutex);
}

void miner_clear_job() {
    xSemaphoreTake(s_jobMutex, portMAX_DELAY);
    s_job.valid = false;
    s_jobVersion++;
    xSemaphoreGive(s_jobMutex);
}

bool miner_pop_share(FoundShare &out) {
    if (!s_shareQueue) return false;
    return xQueueReceive(s_shareQueue, &out, 0) == pdTRUE;
}

void miner_report_share(bool accepted) {
    if (accepted) s_sharesAccepted++;
    else          s_sharesRejected++;
}

void miner_tick() {
    uint32_t now = millis();
    uint32_t elapsed = now - s_lastTickMs;
    if (elapsed < 500) return;
    s_lastTickMs = now;

    uint32_t delta = 0;
    for (uint8_t i = 0; i < s_taskCount; i++) {
        uint32_t c = s_hashCounter[i];
        delta += c - s_lastCounter[i];     // unsigned math handles wrap-around
        s_lastCounter[i] = c;
    }
    s_totalHashes += delta;

    double instant = (double)delta * 1000.0 / (double)elapsed;
    s_hashrate = (s_hashrate <= 0.0) ? instant : (s_hashrate * 0.8 + instant * 0.2);
}

MinerStats miner_get_stats() {
    MinerStats st;
    st.totalHashes    = s_totalHashes;
    st.sharesFound    = s_sharesFound;
    st.sharesAccepted = s_sharesAccepted;
    st.sharesRejected = s_sharesRejected;
    st.blocksFound    = s_blocksFound;
    st.jobsReceived   = s_jobsReceived;
    st.bestDifficulty = s_bestDifficulty;
    st.hashrate       = s_hashrate;
    st.poolDifficulty = s_poolDiff;
    return st;
}

// Called by the stratum client when it learns a share's real difficulty.
void miner_note_difficulty(double d) {
    if (d > s_bestDifficulty) s_bestDifficulty = d;
}
