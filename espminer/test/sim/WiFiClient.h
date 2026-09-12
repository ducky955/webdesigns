// A WiFiClient wired to the in-process fake pool in sim_main.cpp.
#pragma once
#include <Arduino.h>
#include <string>

// Implemented by the harness.
void sim_pool_send_from_client(const std::string &line);
bool sim_pool_next_byte(int &out);

class WiFiClient {
public:
    bool connect(const char *, uint16_t, int32_t = 0) { _connected = true; return true; }
    bool connected() { return _connected; }
    void stop() { _connected = false; }
    void setTimeout(uint32_t) {}
    void setNoDelay(bool) {}

    int available() {
        if (!_connected) return 0;
        if (_pending < 0 && !sim_pool_next_byte(_pending)) return 0;
        return 1;
    }
    int read() {
        if (_pending < 0 && !sim_pool_next_byte(_pending)) return -1;
        int c = _pending;
        _pending = -1;
        return c;
    }
    void print(const String &s) { print(s.c_str()); }
    void print(const char *s) {
        for (const char *p = s; *p; p++) {
            if (*p == '\n') { sim_pool_send_from_client(_out); _out.clear(); }
            else _out += *p;
        }
    }

private:
    bool        _connected = false;
    int         _pending = -1;
    std::string _out;
};
