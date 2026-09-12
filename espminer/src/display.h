// Optional 128x64 SSD1306 status screen. Everything here compiles away
// unless the firmware is built with -DUSE_OLED=1 (see platformio.ini).
#pragma once
#include <Arduino.h>

void display_begin();
void display_message(const String &line1, const String &line2);
void display_update();
