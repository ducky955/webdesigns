#include "webui.h"
#include "index_html.h"
#include "settings_html.h"
#include "settings.h"
#include "miner.h"
#include "stratum.h"
#include "config.h"
#include "mlog.h"

#include <WebServer.h>
#include <WiFi.h>

static WebServer server(80);
static bool      s_restart = false;
static uint32_t  s_restartAt = 0;

bool webui_restart_requested() {
    return s_restart && millis() > s_restartAt;
}

// Minimal HTML escaping for values echoed back into the form.
static String esc(const String &in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;
        }
    }
    return out;
}

static void handleRoot() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html", INDEX_HTML);
}

static void handleStatus() {
    MinerStats st = miner_get_stats();
    MinerSettings &cfg = settings();

    String j = "{";
    j += "\"hashrate\":" + String(st.hashrate, 2);
    j += ",\"totalHashes\":" + String((double)st.totalHashes, 0);
    j += ",\"shares\":{\"found\":" + String(st.sharesFound) +
         ",\"accepted\":" + String(st.sharesAccepted) +
         ",\"rejected\":" + String(st.sharesRejected) + "}";
    j += ",\"blocks\":" + String(st.blocksFound);
    j += ",\"best\":" + String(st.bestDifficulty, 3);
    j += ",\"bestAllTime\":" + String(settings_best_difficulty(), 3);
    j += ",\"poolDifficulty\":" + String(st.poolDifficulty, 6);
    j += ",\"jobs\":" + String(st.jobsReceived);
    j += ",\"address\":\"" + cfg.btcAddress + "\"";
    j += ",\"worker\":\"" + cfg.workerName + "\"";
    j += ",\"pool\":{\"host\":\"" + cfg.poolHost + "\",\"port\":" + String(cfg.poolPort) +
         ",\"state\":\"" + String(stratum.stateName()) + "\"}";
    j += ",\"wifi\":{\"ssid\":\"" + WiFi.SSID() + "\",\"rssi\":" + String(WiFi.RSSI()) +
         ",\"ip\":\"" + (WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString()
                                                   : WiFi.localIP().toString()) + "\"}";
    j += ",\"uptime\":" + String(millis() / 1000);
    j += ",\"heap\":" + String(ESP.getFreeHeap());
    j += ",\"version\":\"" FIRMWARE_VERSION "\"";
    j += "}";

    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", j);
}

static void handleSettings() {
    MinerSettings &cfg = settings();
    String page = FPSTR(SETTINGS_HTML);
    page.replace("%SSID%", esc(cfg.wifiSsid));
    page.replace("%PASSPH%", cfg.wifiPassword.length() ? "(unchanged)" : "(not set)");
    page.replace("%BTC%", esc(cfg.btcAddress));
    page.replace("%WORKER%", esc(cfg.workerName));
    page.replace("%HOST%", esc(cfg.poolHost));
    page.replace("%PORT%", String(cfg.poolPort));
    page.replace("%PPASS%", esc(cfg.poolPassword));
    page.replace("%SDIFF%", String(cfg.suggestDifficulty, 4));
    server.send(200, "text/html", page);
}

static void finishPage(const String &title, const String &body) {
    String p = F("<!doctype html><html><head><meta charset=\"utf-8\">"
                 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                 "<meta http-equiv=\"refresh\" content=\"8;url=/\">"
                 "<style>body{background:#0b0d10;color:#e8edf2;font-family:-apple-system,"
                 "Segoe UI,Roboto,sans-serif;padding:48px 20px;text-align:center}"
                 "h2{color:#f7931a}a{color:#f7931a}</style></head><body><h2>");
    p += title;
    p += F("</h2><p>");
    p += body;
    p += F("</p><p><a href=\"/\">back to the dashboard</a></p></body></html>");
    server.send(200, "text/html", p);
}

static void handleSave() {
    MinerSettings &cfg = settings();

    if (server.hasArg("ssid"))   cfg.wifiSsid = server.arg("ssid");
    if (server.hasArg("pass") && server.arg("pass").length())
        cfg.wifiPassword = server.arg("pass");
    if (server.hasArg("btc"))    cfg.btcAddress = server.arg("btc");
    if (server.hasArg("worker")) cfg.workerName = server.arg("worker");
    if (server.hasArg("host"))   cfg.poolHost = server.arg("host");
    if (server.hasArg("port"))   cfg.poolPort = (uint16_t)server.arg("port").toInt();
    if (server.hasArg("ppass"))  cfg.poolPassword = server.arg("ppass");
    if (server.hasArg("sdiff"))  cfg.suggestDifficulty = server.arg("sdiff").toDouble();

    cfg.btcAddress.trim();
    cfg.workerName.trim();
    cfg.poolHost.trim();
    if (cfg.poolPort == 0) cfg.poolPort = 3333;

    settings_save();
    finishPage("Saved", "Rebooting with the new settings...");
    s_restart = true;
    s_restartAt = millis() + 1200;
}

static void handleReset() {
    settings_factory_reset();
    finishPage("Settings erased", "Rebooting with the values from config.h...");
    s_restart = true;
    s_restartAt = millis() + 1200;
}

static void handleRestart() {
    finishPage("Rebooting", "Give it a few seconds.");
    s_restart = true;
    s_restartAt = millis() + 800;
}

static void handleNotFound() {
    // Captive-portal friendly: anything unknown goes to the dashboard.
    server.sendHeader("Location", "http://" + (WiFi.getMode() == WIFI_AP
                                               ? WiFi.softAPIP().toString()
                                               : WiFi.localIP().toString()) + "/", true);
    server.send(302, "text/plain", "");
}

void webui_begin() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/settings", HTTP_GET, handleSettings);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/reset", HTTP_GET, handleReset);
    server.on("/restart", HTTP_POST, handleRestart);
    server.on("/restart", HTTP_GET, handleRestart);
    server.onNotFound(handleNotFound);
    server.begin();
    MLOG("web UI listening on port 80");
}

void webui_loop() {
    server.handleClient();
}
