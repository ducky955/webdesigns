// Minimal Stratum V1 client: subscribe, authorize, receive jobs, submit
// shares. Everything is non-blocking and driven from loop().
#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "miner.h"

enum StratumState {
    STRATUM_DISCONNECTED = 0,
    STRATUM_CONNECTING,
    STRATUM_SUBSCRIBING,
    STRATUM_AUTHORIZING,
    STRATUM_MINING
};

class StratumClient {
public:
    void begin(const String &host, uint16_t port, const String &user,
               const String &pass, double suggestDifficulty);
    void loop();
    void disconnect();

    StratumState state() const { return _state; }
    const char  *stateName() const;
    bool         isMining() const { return _state == STRATUM_MINING; }
    uint32_t     lastJobAgeSeconds() const;
    const String &poolHost() const { return _host; }
    uint16_t      poolPort() const { return _port; }

private:
    bool connectToPool();
    void handleLine(char *line);
    void handleNotify(JsonArrayConst params);
    bool sendLine(const String &json);
    void setState(StratumState next);
    void submitPendingShares();

    WiFiClient _client;
    String     _host, _user, _pass;
    uint16_t   _port = 0;
    double     _suggestDifficulty = 0;

    StratumState _state = STRATUM_DISCONNECTED;
    uint32_t     _nextId = 4;
    uint32_t     _lastConnectAttempt = 0;
    uint32_t     _lastJobMs = 0;
    uint32_t     _lastRxMs = 0;
    uint32_t     _stateSince = 0;
    uint32_t     _backoffMs = 2000;

    char   _rx[5120];
    size_t _rxLen = 0;

    char    _extranonce1[33] = {0};
    uint8_t _extranonce2Size = 4;

    static const uint8_t MAX_PENDING = 8;
    uint32_t _pendingIds[MAX_PENDING] = {0};
    double   _pendingDiff[MAX_PENDING] = {0};
    uint8_t  _pendingCount = 0;
};

extern StratumClient stratum;
