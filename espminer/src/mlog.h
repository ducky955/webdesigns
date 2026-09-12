// One-line logger so the serial console stays readable regardless of the
// core debug level the sketch is built with.
#pragma once
#include <Arduino.h>

#define MLOG(fmt, ...) Serial.printf("[%7lu] " fmt "\n", (unsigned long)(millis() / 1000), ##__VA_ARGS__)
