#include "settings.h"
#include "config.h"
#include "mlog.h"
#include <Preferences.h>

static Preferences   prefs;
static MinerSettings s;
static double        s_bestAllTime = 0.0;

MinerSettings &settings() { return s; }

void settings_load() {
    // 1. compile-time defaults
    s.wifiSsid          = WIFI_SSID;
    s.wifiPassword      = WIFI_PASSWORD;
    s.btcAddress        = BTC_ADDRESS;
    s.workerName        = WORKER_NAME;
    s.poolHost          = POOL_HOST;
    s.poolPort          = POOL_PORT;
    s.poolPassword      = POOL_PASSWORD;
    s.suggestDifficulty = SUGGESTED_DIFFICULTY;
    s.screen.mode       = display_mode_from_name(SCREEN_MODE);
    s.screen.anim       = display_anim_from_name(SCREEN_ANIM);
    s.screen.flip       = SCREEN_FLIP;
    s.screen.dim        = SCREEN_DIM;

    // 2. anything saved from the web UI overrides them
    prefs.begin("espminer", true);
    s.wifiSsid          = prefs.getString("ssid", s.wifiSsid);
    s.wifiPassword      = prefs.getString("pass", s.wifiPassword);
    s.btcAddress        = prefs.getString("btc", s.btcAddress);
    s.workerName        = prefs.getString("worker", s.workerName);
    s.poolHost          = prefs.getString("host", s.poolHost);
    s.poolPort          = prefs.getUShort("port", s.poolPort);
    s.poolPassword      = prefs.getString("ppass", s.poolPassword);
    s.suggestDifficulty = prefs.getDouble("sdiff", s.suggestDifficulty);
    s.screen.mode       = prefs.getUChar("scrmode", s.screen.mode);
    s.screen.anim       = prefs.getUChar("scranim", s.screen.anim);
    s.screen.flip       = prefs.getBool("scrflip", s.screen.flip);
    s.screen.dim        = prefs.getBool("scrdim", s.screen.dim);
    s_bestAllTime       = prefs.getDouble("best", 0.0);
    prefs.end();

    if (s.workerName.length() == 0) s.workerName = "esp32";
    if (s.poolPort == 0) s.poolPort = 3333;
    if (s.screen.mode >= SCREEN_MODE_COUNT) s.screen.mode = SCREEN_FULL;
    if (s.screen.anim >= ANIM_COUNT) s.screen.anim = ANIM_NONE;
}

void settings_save() {
    prefs.begin("espminer", false);
    prefs.putString("ssid", s.wifiSsid);
    prefs.putString("pass", s.wifiPassword);
    prefs.putString("btc", s.btcAddress);
    prefs.putString("worker", s.workerName);
    prefs.putString("host", s.poolHost);
    prefs.putUShort("port", s.poolPort);
    prefs.putString("ppass", s.poolPassword);
    prefs.putDouble("sdiff", s.suggestDifficulty);
    prefs.end();
    settings_save_screen();
    MLOG("settings saved to flash");
}

void settings_factory_reset() {
    prefs.begin("espminer", false);
    prefs.clear();
    prefs.end();
    MLOG("settings cleared - back to the values compiled into config.h");
}

void settings_save_screen() {
    prefs.begin("espminer", false);
    prefs.putUChar("scrmode", s.screen.mode);
    prefs.putUChar("scranim", s.screen.anim);
    prefs.putBool("scrflip", s.screen.flip);
    prefs.putBool("scrdim", s.screen.dim);
    prefs.end();
}

String settings_stratum_user() {
    String user = s.btcAddress;
    if (s.workerName.length()) {
        user += ".";
        user += s.workerName;
    }
    return user;
}

double settings_best_difficulty() { return s_bestAllTime; }

void settings_update_best(double difficulty) {
    if (difficulty <= s_bestAllTime) return;
    s_bestAllTime = difficulty;
    prefs.begin("espminer", false);
    prefs.putDouble("best", s_bestAllTime);
    prefs.end();
}
