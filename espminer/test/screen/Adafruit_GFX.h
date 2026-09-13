#pragma once
#include <Arduino.h>
#include "fake_panel.h"
class Adafruit_GFX {
public:
    int16_t cx = 0, cy = 0;
    uint8_t tsize = 1;
    void setTextColor(uint16_t) {}
    void setTextSize(uint8_t s) { tsize = s ? s : 1; }
    void setTextWrap(bool) {}
    void setRotation(uint8_t) {}
    void setCursor(int16_t x, int16_t y) { cx = x; cy = y; }

    void print(const char *s) {
        DrawOp op;
        op.kind = "text"; op.isText = true; op.text = s ? s : "";
        op.x = cx; op.y = cy; op.size = tsize;
        op.w = (int)op.text.size() * 6 * tsize;
        op.h = 8 * tsize;
        panel.add(op);
        cx += op.w;
    }
    void print(const String &s) { print(s.c_str()); }
    void println(const char *s) { print(s); cx = 0; cy += 8 * tsize; }
    void println(const String &s) { println(s.c_str()); }

    void drawPixel(int16_t x, int16_t y, uint16_t) { prim("pixel", x, y, 1, 1); }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t) {
        int16_t x = x0 < x1 ? x0 : x1, y = y0 < y1 ? y0 : y1;
        prim("line", x, y, (x0 > x1 ? x0 - x1 : x1 - x0) + 1,
             (y0 > y1 ? y0 - y1 : y1 - y0) + 1);
    }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t) { prim("rect", x, y, w, h); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t) { prim("fillRect", x, y, w, h); }
    void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t) { prim("circle", x - r, y - r, 2 * r + 1, 2 * r + 1); }
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t) { prim("fillCircle", x - r, y - r, 2 * r + 1, 2 * r + 1); }

private:
    void prim(const char *kind, int16_t x, int16_t y, int16_t w, int16_t h) {
        DrawOp op;
        op.kind = kind; op.x = x; op.y = y; op.w = w; op.h = h;
        panel.add(op);
    }
};
