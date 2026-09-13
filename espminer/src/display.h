// Optional 128x64 SSD1306 status screen.
//
// Everything here compiles away unless the firmware is built with
// -DUSE_OLED=1, but the option types and name lookups stay available so
// settings and the web UI do not need to care.
#pragma once
#include <Arduino.h>

enum ScreenMode : uint8_t {
    SCREEN_FULL = 0,   // hash rate + the numbers that matter
    SCREEN_BIG,        // one huge hash rate
    SCREEN_STATS,      // dense table, everything at once
    SCREEN_MINIMAL,    // mostly animation
    SCREEN_ROTATE,     // cycle through the above
    SCREEN_OFF,        // blank the panel
    SCREEN_MODE_COUNT
};

enum ScreenAnim : uint8_t {
    ANIM_NONE = 0,
    ANIM_PICKAXE,      // a pick chipping away at a block
    ANIM_SPINNER,      // sweeping hand around a dial
    ANIM_BARS,         // bouncing activity bars
    ANIM_PULSE,        // expanding rings
    ANIM_CHAIN,        // blocks marching along a chain
    ANIM_COUNT
};

struct ScreenOptions {
    uint8_t mode = SCREEN_FULL;
    uint8_t anim = ANIM_PICKAXE;
    bool    flip = false;    // rotate 180 degrees for upside-down mounting
    bool    dim  = false;    // lower contrast
};

// Names used by the web UI and config.h. Always available.
const char *display_mode_name(uint8_t mode);
const char *display_anim_name(uint8_t anim);
uint8_t     display_mode_from_name(const char *name);
uint8_t     display_anim_from_name(const char *name);

void display_begin();
void display_set_options(const ScreenOptions &opts);   // applies immediately
ScreenOptions display_get_options();

void display_message(const String &line1, const String &line2);

// Show the setup network instead of mining stats until the miner is
// configured. An empty ssid returns the screen to the miner.
void display_setup_mode(const String &ssid, const String &password, const String &ip);

// Call this every loop; it rate-limits itself (8 fps when animating,
// once a second when not).
void display_update();
