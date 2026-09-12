// ---------------------------------------------------------------------
//  Host stand-in for the Arduino core.
//
//  This is NOT firmware. It exists so the real miner.cpp and stratum.cpp
//  can be compiled and RUN on a PC against a fake pool, which is the only
//  way to exercise the whole pipeline without an ESP32 on the bench.
// ---------------------------------------------------------------------
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <cstring>
#include <cstdlib>

#define PROGMEM
#define F(x) String(x)
#define FPSTR(x) String((const char *)(x))

class String {
public:
    std::string s;
    String() {}
    String(const char *v) : s(v ? v : "") {}
    String(const std::string &v) : s(v) {}
    String(char c) { s = std::string(1, c); }
    String(int v) { s = std::to_string(v); }
    String(unsigned v) { s = std::to_string(v); }
    String(long v) { s = std::to_string(v); }
    String(unsigned long v) { s = std::to_string(v); }
    String(uint16_t v) { s = std::to_string((unsigned)v); }
    String(double v, int prec = 2) { char b[64]; snprintf(b, sizeof b, "%.*f", prec, v); s = b; }
    const char *c_str() const { return s.c_str(); }
    size_t length() const { return s.size(); }
    void reserve(size_t n) { s.reserve(n); }
    void trim() {
        size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
        s = (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
    }
    void replace(const String &a, const String &b) {
        size_t p;
        while ((p = s.find(a.s)) != std::string::npos) s.replace(p, a.s.size(), b.s);
    }
    bool startsWith(const String &p) const { return s.rfind(p.s, 0) == 0; }
    long toInt() const { return atol(s.c_str()); }
    double toDouble() const { return atof(s.c_str()); }
    char operator[](size_t i) const { return s[i]; }
    String &operator+=(const String &o) { s += o.s; return *this; }
    String &operator+=(const char *o) { s += o; return *this; }
    String &operator+=(char o) { s += o; return *this; }
    bool operator==(const char *o) const { return s == o; }
    bool operator==(const String &o) const { return s == o.s; }
};
inline String operator+(const String &a, const String &b) { String r(a); r += b; return r; }
inline String operator+(const String &a, const char *b) { String r(a); r += b; return r; }

struct SerialClass {
    void begin(unsigned long) {}
    void print(const char *v) { fputs(v, stdout); }
    void print(char v) { fputc(v, stdout); }
    void print(const String &v) { fputs(v.c_str(), stdout); }
    void print(double v, int p = 2) { printf("%.*f", p, v); }
    void println() { fputc('\n', stdout); }
    void println(const char *v) { puts(v); }
    void println(const String &v) { puts(v.c_str()); }
    int printf(const char *fmt, ...) {
        va_list ap; va_start(ap, fmt);
        int n = vprintf(fmt, ap);
        va_end(ap); fflush(stdout);
        return n;
    }
};
extern SerialClass Serial;

unsigned long millis();
void delay(unsigned long ms);
unsigned getCpuFrequencyMhz();

struct EspClass {
    uint32_t getFreeHeap() { return 200000; }
    uint8_t  getChipCores() { return 2; }
    void     restart() {}
};
extern EspClass ESP;
