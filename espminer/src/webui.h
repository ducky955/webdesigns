// Built-in dashboard + settings page, served on port 80.
#pragma once
#include <Arduino.h>

void webui_begin();
void webui_loop();
bool webui_restart_requested();
