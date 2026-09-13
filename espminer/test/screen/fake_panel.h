// A recording stand-in for the SSD1306, used to check the layouts in
// display.cpp without hardware: every draw is logged with its bounding
// box, so anything that would fall off the 128x64 panel or land on top of
// text is caught, and the result can be printed as ASCII art.
#pragma once
#include <stdint.h>
#include <string>
#include <vector>

struct DrawOp {
    std::string kind;     // "text" or a primitive name
    int x = 0, y = 0, w = 0, h = 0;
    std::string text;
    int size = 1;
    bool isText = false;
};

struct Panel {
    static const int W = 128, H = 64;
    std::vector<DrawOp> ops;
    char occupancy[H][W];          // ' ', 'T' for text, 'G' for graphics

    void reset();
    void add(const DrawOp &op);
    int  outOfBounds(std::vector<std::string> &why) const;
    int  textCollisions(std::vector<std::string> &why) const;
    void render(const char *title) const;
};

extern Panel panel;
