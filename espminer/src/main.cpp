// =====================================================================
//  ESPMiner - a Bitcoin solo ("lottery") miner for the ESP32
//
//  Core 0/1 run the SHA-256 grinders, the Arduino loop keeps the pool
//  connection, the web dashboard and the optional OLED alive.
//
//  Realistically this will never find a block. That is the whole joke:
//  it is a lottery ticket that costs about 1 W. See README.md.
// =====================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>

#include "config.h"
#include "mlog.h"
#include "settings.h"
#include "miner.h"
#include "stratum.h"
#include "display.h"
#if ENABLE_WEB_UI
#include "webui.h"
#endif
#if ENABLE_OTA
#include <ArduinoOTA.h>
#endif

static DNSServer dnsServer;
static bool      s_apMode = false;
static uint32_t  s_lastStats = 0;
static uint32_t  s_lastWifiCheck = 0;

// ---------------------------------------------------------------------
static void banner() {
    Serial.println();
    Serial.println(F("======================================================"));
    Serial.println(F("  ESPMiner " FIRMWARE_VERSION " - ESP32 Bitcoin lottery miner"));
    Serial.println(F("======================================================"));
    MinerSettings &c = settings();
    MLOG("payout address : %s", c.btcAddress.c_str());
    MLOG("worker         : %s", c.workerName.c_str());
    MLOG("pool           : %s:%u", c.poolHost.c_str(), c.poolPort);
    MLOG("wifi ssid      : %s", c.wifiSsid.c_str());
    MLOG("mining tasks   : %d", MINER_TASK_COUNT);
    MLOG("cpu            : %u MHz, %u cores", (unsigned)getCpuFrequencyMhz(), ESP.getChipCores());

    if (c.btcAddress.startsWith("bc1qYOUR") || c.btcAddress.length() < 26)
        MLOG("!! the payout address looks like a placeholder - open the web UI and fix it");
    if (c.wifiSsid == "YOUR_WIFI_NAME")
        MLOG("!! wi-fi is not configured - joining the '%s' setup network instead", AP_SSID);
}

// ---------------------------------------------------------------------
static void startAccessPoint() {
    s_apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    dnsServer.start(53, "*", WiFi.softAPIP());

    MLOG("setup mode: join wi-fi '%s' (password '%s')", AP_SSID, AP_PASSWORD);
    MLOG("            then open http://%s", WiFi.softAPIP().toString().c_str());
    display_message("Setup mode", WiFi.softAPIP().toString());
}

static bool connectWifi() {
    MinerSettings &c = settings();
    if (c.wifiSsid.length() == 0) return false;

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);              // sleep costs us shares, not power we care about
    WiFi.setHostname(MINER_HOSTNAME);
    WiFi.begin(c.wifiSsid.c_str(), c.wifiPassword.c_str());

    MLOG("wifi: joining '%s'", c.wifiSsid.c_str());
    display_message("Connecting to", c.wifiSsid);

    uint32_t deadline = millis() + WIFI_CONNECT_TIMEOUT * 1000UL;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        MLOG("wifi: could not connect");
        return false;
    }
    MLOG("wifi: connected, ip %s (rssi %d dBm)", WiFi.localIP().toString().c_str(),
         (int)WiFi.RSSI());
    display_message("Wi-Fi OK", WiFi.localIP().toString());
    return true;
}

// ---------------------------------------------------------------------
static void printStats() {
    MinerStats st = miner_get_stats();
    double rate = st.hashrate;
    const char *unit = "H/s";
    if (rate >= 1000.0) { rate /= 1000.0; unit = "kH/s"; }

    MLOG("%s | %.2f %s | shares %lu ok / %lu bad | best diff %.3f | pool diff %.4f | heap %u",
         stratum.stateName(), rate, unit,
         (unsigned long)st.sharesAccepted, (unsigned long)st.sharesRejected,
         st.bestDifficulty, st.poolDifficulty, (unsigned)ESP.getFreeHeap());
}

// ---------------------------------------------------------------------
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);

    settings_load();
    banner();
    display_begin();

    if (!connectWifi()) startAccessPoint();

    if (!s_apMode) {
        if (MDNS.begin(MINER_HOSTNAME)) {
            MDNS.addService("http", "tcp", 80);
            MLOG("dashboard: http://%s.local  (or http://%s)", MINER_HOSTNAME,
                 WiFi.localIP().toString().c_str());
        }
#if ENABLE_OTA
        ArduinoOTA.setHostname(MINER_HOSTNAME);
        ArduinoOTA.onStart([]() { MLOG("OTA update starting"); });
        ArduinoOTA.begin();
#endif
    }

#if ENABLE_WEB_UI
    webui_begin();
#endif

    // No point hashing while we are only a setup access point.
    if (!s_apMode) {
        miner_begin(MINER_TASK_COUNT);
        stratum.begin(settings().poolHost, settings().poolPort, settings_stratum_user(),
                      settings().poolPassword, settings().suggestDifficulty);
    } else {
        MLOG("mining is paused until wi-fi is configured");
    }
}

// ---------------------------------------------------------------------
void loop() {
    if (s_apMode) {
        dnsServer.processNextRequest();
    } else {
        // Wi-Fi dropped? Let the ESP32 reconnect, and park the miner meanwhile.
        if (millis() - s_lastWifiCheck > 5000) {
            s_lastWifiCheck = millis();
            if (WiFi.status() != WL_CONNECTED) {
                MLOG("wifi: link lost, reconnecting");
                stratum.disconnect();
                WiFi.reconnect();
            }
        }
        if (WiFi.status() == WL_CONNECTED) stratum.loop();
#if ENABLE_OTA
        ArduinoOTA.handle();
#endif
    }

#if ENABLE_WEB_UI
    webui_loop();
    if (webui_restart_requested()) {
        MLOG("restarting...");
        delay(150);
        ESP.restart();
    }
#endif

    miner_tick();

    MinerStats st = miner_get_stats();
    if (st.bestDifficulty > settings_best_difficulty())
        settings_update_best(st.bestDifficulty);

#if STATS_INTERVAL > 0
    if (millis() - s_lastStats > STATS_INTERVAL * 1000UL) {
        s_lastStats = millis();
        printStats();
        display_update();
    }
#endif

    delay(5);
}
