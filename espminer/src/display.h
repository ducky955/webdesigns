// Optional 128x64 SSD1306 status screen. Everything here compiles away
// unless the firmware is built with -DUSE_OLED=1 (see platformio.ini).
#pragma once
#include <Arduino.h>

void display_begin();
void display_message(const String &line1, const String &line2);

// Show the setup network instead of mining stats until the miner is
// configured. Passing an empty ssid returns the screen to the miner.
void display_setup_mode(const String &ssid, const String &password, const String &ip);
void display_update();
