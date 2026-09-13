#pragma once
#include <Arduino.h>
class TwoWire {
public:
    bool begin(int, int) { return true; }
    void setClock(uint32_t) {}
    void beginTransmission(uint8_t) {}
    uint8_t endTransmission() { return 1; }
};
extern TwoWire Wire;
