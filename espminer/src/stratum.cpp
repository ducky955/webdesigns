#include "stratum.h"
#include "btc_util.h"
#include "config.h"
#include "mlog.h"
#include <ArduinoJson.h>

StratumClient stratum;

void StratumClient::begin(const String &host, uint16_t port, const String &user,
                          const String &pass, double suggestDifficulty) {
    _host = host;
    _port = port;
    _user = user;
    _pass = pass;
    _suggestDifficulty = suggestDifficulty;
    _state = STRATUM_DISCONNECTED;
    _lastConnectAttempt = 0;
    _backoffMs = 2000;
}

const char *StratumClient::stateName() const {
    switch (_state) {
        case STRATUM_CONNECTING:  return "connecting";
        case STRATUM_SUBSCRIBING: return "subscribing";
        case STRATUM_AUTHORIZING: return "authorizing";
        case STRATUM_MINING:      return "mining";
        default:                  return "offline";
    }
}

uint32_t StratumClient::lastJobAgeSeconds() const {
    if (_lastJobMs == 0) return 0;
    return (millis() - _lastJobMs) / 1000;
}

void StratumClient::disconnect() {
    if (_client.connected()) _client.stop();
    _state = STRATUM_DISCONNECTED;
    _rxLen = 0;
    _pendingCount = 0;
    miner_clear_job();
}

bool StratumClient::sendLine(const String &json) {
    if (!_client.connected()) return false;
    _client.print(json);
    _client.print("\n");
    return true;
}

bool StratumClient::connectToPool() {
    MLOG("stratum: connecting to %s:%u", _host.c_str(), _port);
    _client.stop();
    _client.setTimeout(5);
    if (!_client.connect(_host.c_str(), _port, 8000)) {
        MLOG("stratum: connection failed");
        return false;
    }
    _client.setNoDelay(true);
    _rxLen = 0;
    _pendingCount = 0;
    _nextId = 4;

    // id 1: mining.subscribe
    String msg = "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"ESPMiner/";
    msg += FIRMWARE_VERSION;
    msg += "\"]}";
    if (!sendLine(msg)) return false;

    _state = STRATUM_SUBSCRIBING;
    _lastRxMs = millis();
    return true;
}

void StratumClient::loop() {
    // ---- keep the connection up ------------------------------------
    if (_state != STRATUM_DISCONNECTED && !_client.connected()) {
        MLOG("stratum: disconnected by pool");
        disconnect();
    }

    if (_state == STRATUM_DISCONNECTED) {
        uint32_t now = millis();
        if (_lastConnectAttempt != 0 && now - _lastConnectAttempt < _backoffMs) return;
        _lastConnectAttempt = now;
        if (!connectToPool()) {
            _backoffMs = _backoffMs < 60000 ? _backoffMs * 2 : 60000;   // back off, cap at 1 min
            return;
        }
        _backoffMs = 2000;
        return;
    }

    // ---- read whole lines ------------------------------------------
    while (_client.available()) {
        int c = _client.read();
        if (c < 0) break;
        _lastRxMs = millis();
        if (c == '\r') continue;
        if (c == '\n') {
            _rx[_rxLen] = '\0';
            if (_rxLen) handleLine(_rx);
            _rxLen = 0;
            continue;
        }
        if (_rxLen < sizeof(_rx) - 1) {
            _rx[_rxLen++] = (char)c;
        } else {
            MLOG("stratum: line too long, dropping");
            _rxLen = 0;
        }
    }

    submitPendingShares();

    // ---- watchdog: pools push work at least every couple of minutes
    if (_state == STRATUM_MINING && millis() - _lastRxMs > 300000UL) {
        MLOG("stratum: no data for 5 minutes, reconnecting");
        disconnect();
    }
    if (_state != STRATUM_MINING && _state != STRATUM_DISCONNECTED &&
        millis() - _lastRxMs > 20000UL) {
        MLOG("stratum: handshake timed out, reconnecting");
        disconnect();
    }
}

void StratumClient::handleLine(char *line) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
        MLOG("stratum: bad JSON (%s)", err.c_str());
        return;
    }

    const char *method = doc["method"] | (const char *)nullptr;

    // ---------------- notifications ---------------------------------
    if (method) {
        JsonArrayConst params = doc["params"].as<JsonArrayConst>();
        if (!strcmp(method, "mining.notify")) {
            handleNotify(params);
        } else if (!strcmp(method, "mining.set_difficulty")) {
            if (params.size() >= 1) miner_set_difficulty(params[0].as<double>());
        } else if (!strcmp(method, "mining.set_extranonce")) {
            if (params.size() >= 2) {
                strncpy(_extranonce1, params[0] | "", sizeof(_extranonce1) - 1);
                _extranonce2Size = params[1].as<uint8_t>();
                miner_set_extranonce(_extranonce1, _extranonce2Size);
                MLOG("stratum: new extranonce1=%s size=%u", _extranonce1, _extranonce2Size);
            }
        } else if (!strcmp(method, "client.reconnect")) {
            MLOG("stratum: pool asked us to reconnect");
            disconnect();
        } else if (!strcmp(method, "client.show_message")) {
            MLOG("pool says: %s", params[0] | "");
        }
        return;
    }

    // ---------------- responses -------------------------------------
    uint32_t id = doc["id"] | 0;

    if (id == 1) {  // subscribe
        JsonArrayConst result = doc["result"].as<JsonArrayConst>();
        if (result.isNull() || result.size() < 3) {
            MLOG("stratum: subscribe rejected");
            disconnect();
            return;
        }
        strncpy(_extranonce1, result[1] | "", sizeof(_extranonce1) - 1);
        _extranonce2Size = result[2].as<uint8_t>();
        if (_extranonce2Size == 0 || _extranonce2Size > 16) _extranonce2Size = 4;
        miner_set_extranonce(_extranonce1, _extranonce2Size);
        MLOG("stratum: subscribed, extranonce1=%s extranonce2_size=%u",
             _extranonce1, _extranonce2Size);

        String msg = "{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"";
        msg += _user + "\",\"" + _pass + "\"]}";
        sendLine(msg);
        _state = STRATUM_AUTHORIZING;

        if (_suggestDifficulty > 0) {
            String sd = "{\"id\":3,\"method\":\"mining.suggest_difficulty\",\"params\":[";
            sd += String(_suggestDifficulty, 6) + "]}";
            sendLine(sd);
        }
        return;
    }

    if (id == 2) {  // authorize
        bool ok = doc["result"] | false;
        if (!ok) {
            const char *why = doc["error"][1] | "rejected";
            MLOG("stratum: authorize FAILED (%s)", why);
            MLOG("         check that '%s' is a valid address for this pool", _user.c_str());
            disconnect();
            _backoffMs = 30000;
            return;
        }
        MLOG("stratum: authorized as %s", _user.c_str());
        _state = STRATUM_MINING;
        return;
    }

    if (id == 3) return;  // suggest_difficulty, nothing to do

    // ---------------- share results ---------------------------------
    for (uint8_t i = 0; i < _pendingCount; i++) {
        if (_pendingIds[i] != id) continue;
        bool ok = doc["result"] | false;
        double d = _pendingDiff[i];
        for (uint8_t j = i; j + 1 < _pendingCount; j++) {
            _pendingIds[j] = _pendingIds[j + 1];
            _pendingDiff[j] = _pendingDiff[j + 1];
        }
        _pendingCount--;

        miner_report_share(ok);
        if (ok) {
            MLOG("share ACCEPTED  (difficulty %.3f)", d);
        } else {
            const char *why = doc["error"][1] | "unknown reason";
            MLOG("share rejected: %s", why);
        }
        return;
    }
}

void StratumClient::handleNotify(JsonArrayConst params) {
    if (params.size() < 9) {
        MLOG("stratum: malformed mining.notify");
        return;
    }

    // ~3 KB: keep it off the loop task's stack. Only ever touched here,
    // and handleNotify is called from a single task.
    static StratumJob job;
    memset(&job, 0, sizeof(job));

    strncpy(job.jobId, params[0] | "", sizeof(job.jobId) - 1);

    const char *prevhash = params[1] | "";
    if (strlen(prevhash) != 64) {
        MLOG("stratum: bad prevhash length");
        return;
    }
    uint8_t raw[32];
    if (!hex2bin(raw, prevhash, 32)) {
        MLOG("stratum: bad prevhash hex");
        return;
    }
    // Stratum sends the previous hash with each 4-byte word byte-reversed.
    for (int w = 0; w < 8; w++)
        for (int b = 0; b < 4; b++) job.prevhash[w * 4 + b] = raw[w * 4 + 3 - b];

    const char *c1 = params[2] | "";
    const char *c2 = params[3] | "";
    if (strlen(c1) > MAX_COINB_HEX || strlen(c2) > MAX_COINB_HEX) {
        MLOG("stratum: coinbase parts too big (%u/%u)", (unsigned)strlen(c1), (unsigned)strlen(c2));
        return;
    }
    strcpy(job.coinb1, c1);
    strcpy(job.coinb2, c2);

    JsonArrayConst branches = params[4].as<JsonArrayConst>();
    job.merkleCount = 0;
    for (JsonVariantConst b : branches) {
        if (job.merkleCount >= MAX_MERKLE_BRANCHES) {
            MLOG("stratum: too many merkle branches, skipping job");
            return;
        }
        const char *h = b | "";
        if (strlen(h) != 64 || !hex2bin(job.merkle[job.merkleCount], h, 32)) {
            MLOG("stratum: bad merkle branch");
            return;
        }
        job.merkleCount++;
    }

    job.version = strtoul(params[5] | "0", nullptr, 16);
    job.nbits   = strtoul(params[6] | "0", nullptr, 16);
    job.ntime   = strtoul(params[7] | "0", nullptr, 16);
    bool clean  = params[8] | false;

    strncpy(job.extranonce1, _extranonce1, sizeof(job.extranonce1) - 1);
    job.extranonce2Size = _extranonce2Size;
    job.valid = true;

    _lastJobMs = millis();
    miner_set_job(job);
    MLOG("job %s  merkle=%u  version=%08x  bits=%08x%s",
         job.jobId, job.merkleCount, job.version, job.nbits, clean ? "  (clean)" : "");
}

void StratumClient::submitPendingShares() {
    FoundShare share;
    while (miner_pop_share(share)) {
        if (_state != STRATUM_MINING) {
            MLOG("share dropped: not connected");
            continue;
        }
        uint32_t id = _nextId++;

        char ntimeHex[9], nonceHex[9];
        snprintf(ntimeHex, sizeof(ntimeHex), "%08x", share.ntime);
        snprintf(nonceHex, sizeof(nonceHex), "%08x", share.nonce);

        String msg = "{\"id\":";
        msg += String(id);
        msg += ",\"method\":\"mining.submit\",\"params\":[\"";
        msg += _user;       msg += "\",\"";
        msg += share.jobId; msg += "\",\"";
        msg += share.extranonce2; msg += "\",\"";
        msg += ntimeHex;    msg += "\",\"";
        msg += nonceHex;    msg += "\"]}";

        if (!sendLine(msg)) {
            MLOG("share dropped: send failed");
            continue;
        }
        MLOG("submitted share: job=%s nonce=%s diff=%.3f%s", share.jobId, nonceHex,
             share.difficulty, share.isBlock ? "  <-- BLOCK!" : "");

        if (_pendingCount < MAX_PENDING) {
            _pendingIds[_pendingCount] = id;
            _pendingDiff[_pendingCount] = share.difficulty;
            _pendingCount++;
        }
    }
}
