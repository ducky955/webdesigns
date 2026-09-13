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
static bool      s_wifiWasUp = true;
static bool      s_apUp = false;
static uint32_t  s_lastApRetry = 0;

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
static bool startAccessPoint() {
    s_apMode = true;

    // WiFi.begin() keeps retrying in the background long after our connect
    // loop gives up, and bringing an AP up underneath an in-flight station
    // connect is the classic way to get a softAP() that silently does
    // nothing. Tear the station side down properly first.
    WiFi.disconnect(true, true);
    delay(200);
    WiFi.mode(WIFI_OFF);
    delay(200);
    WiFi.mode(WIFI_AP);
    delay(200);

    const char *pass = (strlen(AP_PASSWORD) >= 8) ? AP_PASSWORD : nullptr;
    if (!pass && strlen(AP_PASSWORD) > 0)
        MLOG("wifi: AP_PASSWORD is shorter than 8 characters - starting an open AP");

    s_apUp = WiFi.softAP(AP_SSID, pass, AP_CHANNEL);
    if (!s_apUp) {
        // Some cores/boards refuse a protected AP but will start an open
        // one. A reachable setup page beats an unreachable secure one.
        MLOG("wifi: softAP('%s') failed, retrying without a password", AP_SSID);
        delay(500);
        s_apUp = WiFi.softAP(AP_SSID, nullptr, AP_CHANNEL);
        if (s_apUp) pass = nullptr;
    }

    if (!s_apUp) {
        MLOG("wifi: COULD NOT START THE SETUP AP - will keep retrying");
        display_message("AP failed", "retrying...");
        return false;
    }

    IPAddress ip = WiFi.softAPIP();
    dnsServer.start(53, "*", ip);

    Serial.println();
    MLOG("=====================================================");
    MLOG(" SETUP MODE - the miner needs your wi-fi details");
    MLOG("   1. join the network  '%s'", AP_SSID);
    MLOG("      password          '%s'", pass ? pass : "(none - open network)");
    MLOG("   2. open              http://%s", ip.toString().c_str());
    MLOG("   (channel %d, ap mac %s)", AP_CHANNEL, WiFi.softAPmacAddress().c_str());
    MLOG("=====================================================");
    Serial.println();

    display_setup_mode(AP_SSID, pass ? pass : "", ip.toString());
    display_update();
    return true;
}

static bool connectWifi() {
    MinerSettings &c = settings();

    // Nothing configured yet? Don't burn 25 seconds failing to join a
    // placeholder - go straight to the setup network.
    if (c.wifiSsid.length() == 0 || c.wifiSsid == "YOUR_WIFI_NAME") {
        MLOG("wifi: no network configured yet");
        return false;
    }

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);              // sleep costs us shares, not power we care about
    WiFi.setHostname(MINER_HOSTNAME);
    WiFi.begin(c.wifiSsid.c_str(), c.wifiPassword.c_str());

    MLOG("wifi: joining '%s'", c.wifiSsid.c_str());
    display_message("Connecting to", c.wifiSsid);

    uint32_t deadline = millis() + WIFI_CONNECT_TIMEOUT * 1000UL;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
#if AP_FORCE_PIN >= 0
        if (digitalRead(AP_FORCE_PIN) == LOW) {
            Serial.println();
            MLOG("wifi: BOOT button held - forcing setup mode");
            return false;
        }
#endif
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
    if (s_apMode) {
        if (s_apUp)
            MLOG("setup mode: join '%s', then open http://%s  (%u client%s connected)",
                 AP_SSID, WiFi.softAPIP().toString().c_str(),
                 (unsigned)WiFi.softAPgetStationNum(),
                 WiFi.softAPgetStationNum() == 1 ? "" : "s");
        else
            MLOG("setup mode: the access point is NOT running - retrying");
        return;
    }

    MinerStats st = miner_get_stats();
    double rate = st.hashrate;
    const char *unit = "H/s";
    if (rate >= 1000.0) { rate /= 1000.0; unit = "kH/s"; }

    MLOG("%s | %.2f %s | shares %lu ok / %lu bad | best diff %g | pool diff %g | heap %u",
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
    display_set_options(settings().screen);
#if AP_FORCE_PIN >= 0
    pinMode(AP_FORCE_PIN, INPUT_PULLUP);
#endif

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
        if (!s_apUp && millis() - s_lastApRetry > 10000) {
            s_lastApRetry = millis();
            startAccessPoint();
        }
        dnsServer.processNextRequest();
    } else {
        // Wi-Fi dropped? Let the ESP32 reconnect, and park the miner meanwhile.
        if (millis() - s_lastWifiCheck > 5000) {
            s_lastWifiCheck = millis();
            bool up = (WiFi.status() == WL_CONNECTED);
            if (!up) {
                if (s_wifiWasUp) {           // log the transition, not every retry
                    MLOG("wifi: link lost, reconnecting");
                    stratum.disconnect();
                }
                WiFi.reconnect();
            } else if (!s_wifiWasUp) {
                MLOG("wifi: back up, ip %s", WiFi.localIP().toString().c_str());
            }
            s_wifiWasUp = up;
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

    // display_update() rate-limits itself: 8 fps while an animation is
    // running, once a second otherwise.
    display_update();

#if STATS_INTERVAL > 0
    if (millis() - s_lastStats > STATS_INTERVAL * 1000UL) {
        s_lastStats = millis();
        printStats();
    }
#endif

    delay(5);
}
