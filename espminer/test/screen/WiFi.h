#pragma once
#include <Arduino.h>
#define WIFI_STA 1
#define WIFI_AP 2
extern String g_testIp;
class IPAddress { public: String toString() const { return g_testIp; } };
class WiFiClass {
public:
    int getMode() { return WIFI_STA; }
    IPAddress localIP() { return IPAddress(); }
    IPAddress softAPIP() { return IPAddress(); }
};
extern WiFiClass WiFi;
