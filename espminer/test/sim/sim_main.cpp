// ---------------------------------------------------------------------
//  End-to-end test: runs the REAL miner.cpp and stratum.cpp against a
//  fake Stratum pool, on a PC.
//
//  It drives a full session - subscribe, authorize, set_difficulty,
//  notify, mine, submit, accept - then a second job to prove job
//  switching works. Everything the miner submits is written to
//  sim_result.json so verify_sim.py can independently confirm, with
//  Python's own hashlib, that the shares are real.
// ---------------------------------------------------------------------
#include <Arduino.h>
#include <ArduinoJson.h>

#include "../../src/stratum.h"
#include "../../src/miner.h"

#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <cstdio>

// ---- the job the fake pool hands out --------------------------------
struct SimJob {
    const char *id;
    const char *prevhash;   // stratum form: 4-byte words byte-reversed
    const char *coinb1;
    const char *coinb2;
    const char *merkle[3];
    const char *version;
    const char *nbits;
    const char *ntime;
};

static const SimJob JOBS[2] = {
    {"sim-job-1",
     "ab02cd818b9e567ee21793cddef299feb29ad444a41b85b8000008a300000000",
     "01000000010000000000000000000000000000000000000000000000000000000000000000"
     "ffffffff20020862",
     "ffffffff0100f2052a010000001976a91400112233445566778899aabbccddeeff0011223388ac"
     "00000000",
     {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"},
     "20000000", "1a44b9f2", "5e9c1234"},
    {"sim-job-2",
     "00000000000008a3a41b85b8b29ad444def299fe6de67f788f44ae2a5af6e219",
     "01000000010000000000000000000000000000000000000000000000000000000000000000"
     "ffffffff21030963",
     "ffffffff0100f2052a010000001976a914ffeeddccbbaa998877665544332211000badc0de88ac"
     "00000000",
     {"1111111111111111111111111111111111111111111111111111111111111111",
      "2222222222222222222222222222222222222222222222222222222222222222",
      "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"},
     "20000004", "1a44b9f2", "5e9c9999"}};

static const char *EXTRANONCE1 = "a1b2c3d4";
static const int   EXTRANONCE2_SIZE = 4;
// Low enough that a PC finds a share in well under a second, high enough
// that the two-zero-byte fast path in miner.cpp is the one being tested.
static const double DIFFICULTY = 0.0001;
static const char  *WORKER = "bc1qexampleaddressxxxxxxxxxxxxxxxxxxxxxxx.esp32";

// ---- fake pool plumbing ---------------------------------------------
static std::mutex        g_m;
static std::deque<char>  g_toClient;
static std::vector<std::string> g_fromClient;

struct Submission {
    std::string worker, jobId, extranonce2, ntime, nonce;
};
static std::vector<Submission> g_submissions;
static bool g_sawSubscribe = false, g_sawAuthorize = false;
static int  g_jobsSent = 0;
static bool g_rejectNext = false;     // make the pool refuse one share
static bool g_didReject = false;

static void poolSend(const std::string &line) {
    std::lock_guard<std::mutex> lock(g_m);
    for (char c : line) g_toClient.push_back(c);
    g_toClient.push_back('\n');
}

bool sim_pool_next_byte(int &out) {
    std::lock_guard<std::mutex> lock(g_m);
    if (g_toClient.empty()) return false;
    out = (unsigned char)g_toClient.front();
    g_toClient.pop_front();
    return true;
}

static void sendJob(int index, bool clean) {
    const SimJob &j = JOBS[index];
    std::string p = "{\"id\":null,\"method\":\"mining.notify\",\"params\":[";
    p += std::string("\"") + j.id + "\",";
    p += std::string("\"") + j.prevhash + "\",";
    p += std::string("\"") + j.coinb1 + "\",";
    p += std::string("\"") + j.coinb2 + "\",[";
    for (int i = 0; i < 3; i++) {
        p += std::string("\"") + j.merkle[i] + "\"";
        if (i < 2) p += ",";
    }
    p += "],";
    p += std::string("\"") + j.version + "\",";
    p += std::string("\"") + j.nbits + "\",";
    p += std::string("\"") + j.ntime + "\",";
    p += clean ? "true" : "false";
    p += "]}";
    poolSend(p);
    g_jobsSent++;
}

void sim_pool_send_from_client(const std::string &line) {
    {
        std::lock_guard<std::mutex> lock(g_m);
        g_fromClient.push_back(line);
    }
    printf("  -> pool received: %s\n", line.c_str());

    JsonDocument doc;
    if (deserializeJson(doc, line)) {
        printf("  !! client sent invalid JSON\n");
        return;
    }
    const char *method = doc["method"] | "";
    uint32_t id = doc["id"] | 0;

    if (!strcmp(method, "mining.subscribe")) {
        g_sawSubscribe = true;
        std::string r = "{\"id\":1,\"result\":[[[\"mining.set_difficulty\",\"1\"],"
                        "[\"mining.notify\",\"1\"]],\"";
        r += EXTRANONCE1;
        r += "\"," + std::to_string(EXTRANONCE2_SIZE) + "],\"error\":null}";
        poolSend(r);
    } else if (!strcmp(method, "mining.authorize")) {
        g_sawAuthorize = true;
        poolSend("{\"id\":2,\"result\":true,\"error\":null}");
        poolSend("{\"id\":null,\"method\":\"mining.set_difficulty\",\"params\":[" +
                 std::string("0.0001") + "]}");
        sendJob(0, true);
    } else if (!strcmp(method, "mining.submit")) {
        JsonArrayConst p = doc["params"].as<JsonArrayConst>();
        Submission s{p[0] | "", p[1] | "", p[2] | "", p[3] | "", p[4] | ""};
        {
            std::lock_guard<std::mutex> lock(g_m);
            g_submissions.push_back(s);
        }
        if (g_rejectNext) {
            g_rejectNext = false;
            g_didReject = true;
            poolSend("{\"id\":" + std::to_string(id) +
                     ",\"result\":false,\"error\":[23,\"Job not found\",null]}");
        } else {
            poolSend("{\"id\":" + std::to_string(id) + ",\"result\":true,\"error\":null}");
        }
    }
}

// ---- checks ----------------------------------------------------------
static int failures = 0;
static void check(const char *what, bool ok) {
    printf("  %-52s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) failures++;
}

static size_t submissionCount() {
    std::lock_guard<std::mutex> lock(g_m);
    return g_submissions.size();
}

// Pump the miner + client until `want` submissions have arrived.
static bool pumpUntil(size_t want, unsigned long timeoutMs) {
    unsigned long deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        stratum.loop();
        miner_tick();
        if (submissionCount() >= want) return true;
        delay(2);
    }
    return false;
}

int main() {
    printf("\nESPMiner end-to-end simulation\n");
    printf("(real miner.cpp + stratum.cpp, fake pool, difficulty %.4f)\n\n", DIFFICULTY);

    miner_begin(2);
    stratum.begin("sim-pool.invalid", 3333, WORKER, "x", 0);

    printf("\n-- phase 1: handshake and first job\n");
    bool got1 = pumpUntil(1, 45000);

    printf("\n-- results\n");
    check("client sent mining.subscribe", g_sawSubscribe);
    check("client sent mining.authorize", g_sawAuthorize);
    check("miner accepted the job and submitted a share", got1);

    if (got1) {
        printf("\n-- phase 2: new job, miner must switch to it\n");
        sendJob(1, true);
        // Drain anything still queued for the old job, then wait for a
        // submission that names the new one.
        unsigned long deadline = millis() + 45000;
        bool switched = false;
        while (millis() < deadline && !switched) {
            stratum.loop();
            miner_tick();
            {
                std::lock_guard<std::mutex> lock(g_m);
                for (auto &s : g_submissions)
                    if (s.jobId == JOBS[1].id) switched = true;
            }
            delay(2);
        }
        check("miner switched to the second job and submitted", switched);
    }

    printf("\n-- phase 3: error paths\n");
    {
        // 1. a line that is not JSON at all must not take the client down
        poolSend("<html>502 Bad Gateway</html>");
        poolSend("{\"id\":null,\"method\":\"mining.notify\",\"params\":[\"truncated\"]}");
        unsigned long until3 = millis() + 500;
        while (millis() < until3) { stratum.loop(); miner_tick(); delay(2); }
        check("survived garbage from the pool", stratum.isMining());

        // 2. vardiff: the pool raises the share difficulty mid-session
        poolSend("{\"id\":null,\"method\":\"mining.set_difficulty\",\"params\":[0.001]}");
        until3 = millis() + 500;
        while (millis() < until3) { stratum.loop(); miner_tick(); delay(2); }
        MinerStats mid = miner_get_stats();
        check("picked up the new pool difficulty",
              mid.poolDifficulty > 0.0009 && mid.poolDifficulty < 0.0011);

        // 3. a share the pool refuses must be counted, not silently dropped
        uint32_t rejectedBefore = mid.sharesRejected;
        g_rejectNext = true;
        until3 = millis() + 20000;
        while (millis() < until3 &&
               miner_get_stats().sharesRejected == rejectedBefore) {
            stratum.loop();
            miner_tick();
            delay(2);
        }
        check("counted the rejected share",
              g_didReject && miner_get_stats().sharesRejected == rejectedBefore + 1);
    }

    // Park the miner so the submission count stops moving, drain what is
    // already queued, then wait for the pool's replies to land.
    miner_clear_job();
    unsigned long until = millis() + 1000;
    while (millis() < until) { stratum.loop(); miner_tick(); delay(2); }
    size_t submitted = submissionCount();
    until = millis() + 5000;
    while (millis() < until && miner_get_stats().sharesAccepted < submitted) {
        stratum.loop();
        miner_tick();
        delay(2);
    }

    MinerStats st = miner_get_stats();
    printf("\n  hashrate %.0f H/s, %u found, %u accepted, %u rejected, best %.4f\n",
           st.hashrate, st.sharesFound, st.sharesAccepted, st.sharesRejected,
           st.bestDifficulty);

    check("every submission got a verdict",
          st.sharesAccepted + st.sharesRejected >= submitted);
    check("exactly one share was rejected (the forced one)", st.sharesRejected == 1);
    check("hash rate is being counted", st.hashrate > 0);
    check("best difficulty recorded", st.bestDifficulty >= DIFFICULTY);
    check("jobs counted", st.jobsReceived >= (uint32_t)g_jobsSent);
    check("stratum reached the mining state", stratum.isMining());

    // ---- hand everything to Python for an independent verdict --------
    FILE *f = fopen("sim_result.json", "w");
    fprintf(f, "{\n  \"difficulty\": %.10f,\n  \"extranonce1\": \"%s\",\n", DIFFICULTY,
            EXTRANONCE1);
    fprintf(f, "  \"jobs\": [\n");
    for (int i = 0; i < 2; i++) {
        const SimJob &j = JOBS[i];
        fprintf(f,
                "    {\"id\":\"%s\",\"prevhash\":\"%s\",\"coinb1\":\"%s\","
                "\"coinb2\":\"%s\",\"merkle\":[\"%s\",\"%s\",\"%s\"],"
                "\"version\":\"%s\",\"nbits\":\"%s\",\"ntime\":\"%s\"}%s\n",
                j.id, j.prevhash, j.coinb1, j.coinb2, j.merkle[0], j.merkle[1],
                j.merkle[2], j.version, j.nbits, j.ntime, i == 0 ? "," : "");
    }
    fprintf(f, "  ],\n  \"submissions\": [\n");
    {
        std::lock_guard<std::mutex> lock(g_m);
        for (size_t i = 0; i < g_submissions.size(); i++) {
            Submission &s = g_submissions[i];
            fprintf(f,
                    "    {\"worker\":\"%s\",\"job_id\":\"%s\",\"extranonce2\":\"%s\","
                    "\"ntime\":\"%s\",\"nonce\":\"%s\"}%s\n",
                    s.worker.c_str(), s.jobId.c_str(), s.extranonce2.c_str(),
                    s.ntime.c_str(), s.nonce.c_str(),
                    i + 1 < g_submissions.size() ? "," : "");
        }
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);

    printf("\n  %zu submission(s) written to sim_result.json\n", submissionCount());
    printf("\n%s (%d failure%s)\n", failures ? "SIMULATION FAILED" : "SIMULATION PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
