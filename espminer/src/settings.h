// Runtime settings: defaults come from config.h, anything changed through
// the web UI is stored in NVS (flash) and wins on the next boot.
#pragma once
#include <Arduino.h>

struct MinerSettings {
    String   wifiSsid;
    String   wifiPassword;
    String   btcAddress;
    String   workerName;
    String   poolHost;
    uint16_t poolPort;
    String   poolPassword;
    double   suggestDifficulty;
};

void           settings_load();
void           settings_save();
void           settings_factory_reset();
MinerSettings &settings();

// The stratum username is "<address>.<worker>" on every pool worth using.
String settings_stratum_user();

// All-time best share difficulty, kept across reboots.
double settings_best_difficulty();
void   settings_update_best(double difficulty);
