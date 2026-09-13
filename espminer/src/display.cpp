#include "display.h"
#include "config.h"
#include "mlog.h"
#include "miner.h"
#include "stratum.h"
#include <WiFi.h>
#include <string.h>

// ---------------------------------------------------------------------
// names (always compiled, the web UI needs them with or without a panel)
// ---------------------------------------------------------------------
static const char *MODE_NAMES[SCREEN_MODE_COUNT] = {
    "full", "big", "stats", "minimal", "rotate", "off"};
static const char *ANIM_NAMES[ANIM_COUNT] = {
    "none", "pickaxe", "spinner", "bars", "pulse", "chain"};

const char *display_mode_name(uint8_t m) {
    return m < SCREEN_MODE_COUNT ? MODE_NAMES[m] : MODE_NAMES[0];
}
const char *display_anim_name(uint8_t a) {
    return a < ANIM_COUNT ? ANIM_NAMES[a] : ANIM_NAMES[0];
}
uint8_t display_mode_from_name(const char *n) {
    for (uint8_t i = 0; i < SCREEN_MODE_COUNT; i++)
        if (n && !strcasecmp(n, MODE_NAMES[i])) return i;
    return SCREEN_FULL;
}
uint8_t display_anim_from_name(const char *n) {
    for (uint8_t i = 0; i < ANIM_COUNT; i++)
        if (n && !strcasecmp(n, ANIM_NAMES[i])) return i;
    return ANIM_NONE;
}

#if defined(USE_OLED) && USE_OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCR_W 128
#define SCR_H 64

static Adafruit_SSD1306 oled(SCR_W, SCR_H, &Wire, -1);
static bool          s_ready = false;
static ScreenOptions s_opt;
static uint32_t      s_frame = 0;
static uint32_t      s_lastDraw = 0;
static uint32_t      s_rotateAt = 0;
static uint8_t       s_rotateIndex = 0;

static bool   s_setupMode = false;
static String s_apSsid, s_apPass, s_apIp;

// ---------------------------------------------------------------------
// text helpers. The built-in font is exactly 6 px wide and 8 px tall per
// size step, so every layout below can be checked with plain arithmetic -
// which is what test/screen does.
// ---------------------------------------------------------------------
static inline int16_t tw(const char *s, uint8_t size) {
    return (int16_t)strlen(s) * 6 * size;
}

static void txt(int16_t x, int16_t y, uint8_t size, const char *s) {
    if (x < 0) x = 0;
    oled.setTextSize(size);
    oled.setCursor(x, y);
    oled.print(s);
}
static void txtRight(int16_t right, int16_t y, uint8_t size, const char *s) {
    txt(right - tw(s, size), y, size, s);
}
static void txtCenter(int16_t y, uint8_t size, const char *s) {
    txt((SCR_W - tw(s, size)) / 2, y, size, s);
}

// Largest of the offered sizes whose rendering still fits maxWidth.
static uint8_t fitSize(const char *s, int16_t maxWidth, uint8_t biggest) {
    for (uint8_t size = biggest; size > 1; size--)
        if (tw(s, size) <= maxWidth) return size;
    return 1;
}

static void fmtRate(double rate, char *buf, size_t n, const char **unit) {
    const char *u = "H/s";
    if (rate >= 1e6)      { rate /= 1e6;  u = "MH/s"; }
    else if (rate >= 1e3) { rate /= 1e3;  u = "kH/s"; }
    snprintf(buf, n, "%.2f", rate);
    *unit = u;
}

static void fmtUptime(uint32_t s, char *buf, size_t n) {
    if (s < 3600)       snprintf(buf, n, "%lum%02lus", (unsigned long)(s / 60), (unsigned long)(s % 60));
    else if (s < 86400) snprintf(buf, n, "%luh%02lum", (unsigned long)(s / 3600), (unsigned long)((s % 3600) / 60));
    else                snprintf(buf, n, "%lud%02luh", (unsigned long)(s / 86400), (unsigned long)((s % 86400) / 3600));
}

// ---------------------------------------------------------------------
// animations - each one draws inside the rectangle it is handed
// ---------------------------------------------------------------------
static void animPickaxe(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t f) {
    // the block being worked on
    int16_t blockH = 6;
    int16_t by = y + h - blockH;
    oled.fillRect(x + w / 2 - 5, by, 10, blockH, SSD1306_WHITE);

    // handle swings through four positions, striking on the last
    static const int8_t HX[4] = {-6, -3, 1, 3};
    static const int8_t HY[4] = {-8, -9, -6, -2};
    uint8_t p = f & 3;
    int16_t px = x + w / 2, py = by - 1;          // pivot, just above the block
    int16_t hx = px + HX[p], hy = py + HY[p];
    oled.drawLine(px, py, hx, hy, SSD1306_WHITE);
    oled.drawLine(hx - 2, hy + 1, hx + 2, hy - 1, SSD1306_WHITE);   // the head

    if (p == 3) {                                  // sparks on impact
        oled.drawPixel(px - 3, by - 2, SSD1306_WHITE);
        oled.drawPixel(px + 4, by - 3, SSD1306_WHITE);
    }
}

static void animSpinner(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t f) {
    int16_t r = (w < h ? w : h) / 2 - 1;
    if (r < 3) r = 3;
    int16_t cx = x + w / 2, cy = y + h / 2;
    // eighth-turn steps, scaled by radius - no trig needed
    static const int8_t DX[8] = {0, 5, 7, 5, 0, -5, -7, -5};
    static const int8_t DY[8] = {-7, -5, 0, 5, 7, 5, 0, -5};
    uint8_t i = f & 7;
    oled.drawCircle(cx, cy, r, SSD1306_WHITE);
    oled.drawLine(cx, cy, cx + DX[i] * r / 7, cy + DY[i] * r / 7, SSD1306_WHITE);
}

static void animBars(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t f) {
    static const uint8_t LEVEL[8] = {2, 5, 8, 6, 3, 7, 4, 6};
    int16_t bars = w / 5;
    if (bars > 5) bars = 5;
    if (bars < 1) bars = 1;
    int16_t bw = (w - (bars - 1)) / bars;
    if (bw < 1) bw = 1;
    for (int16_t i = 0; i < bars; i++) {
        uint8_t lv = LEVEL[(f + i * 3) & 7];
        int16_t bh = 1 + (int16_t)lv * (h - 1) / 8;
        oled.fillRect(x + i * (bw + 1), y + h - bh, bw, bh, SSD1306_WHITE);
    }
}

static void animPulse(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t f) {
    int16_t maxR = (w < h ? w : h) / 2;
    if (maxR < 2) maxR = 2;
    int16_t cx = x + w / 2, cy = y + h / 2;
    oled.fillCircle(cx, cy, 1, SSD1306_WHITE);
    for (uint8_t ring = 0; ring < 2; ring++) {
        int16_t r = (int16_t)((f + ring * 4) % 8) * maxR / 8;
        if (r > 1) oled.drawCircle(cx, cy, r, SSD1306_WHITE);
    }
}

static void animChain(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t f) {
    int16_t size = h > 8 ? 8 : h;
    int16_t cy = y + (h - size) / 2;
    int16_t step = size + 3;
    int16_t shift = (int16_t)(f % (uint32_t)step);
    oled.drawLine(x, cy + size / 2, x + w - 1, cy + size / 2, SSD1306_WHITE);
    for (int16_t bx = x - step + shift; bx < x + w; bx += step) {
        if (bx < x || bx + size > x + w) continue;       // never draw past the box
        oled.drawRect(bx, cy, size, size, SSD1306_WHITE);
    }
}

static void drawAnim(int16_t x, int16_t y, int16_t w, int16_t h) {
    switch (s_opt.anim) {
        case ANIM_PICKAXE: animPickaxe(x, y, w, h, s_frame); break;
        case ANIM_SPINNER: animSpinner(x, y, w, h, s_frame); break;
        case ANIM_BARS:    animBars(x, y, w, h, s_frame);    break;
        case ANIM_PULSE:   animPulse(x, y, w, h, s_frame);   break;
        case ANIM_CHAIN:   animChain(x, y, w, h, s_frame);   break;
        default: break;
    }
}
static inline bool animOn() { return s_opt.anim != ANIM_NONE; }

// ---------------------------------------------------------------------
// screens. Every y coordinate below is chosen so the last row ends at or
// before y=63; test/screen asserts it rather than trusting the comment.
// ---------------------------------------------------------------------
static String ipString() {
    return (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP().toString()
                                       : WiFi.localIP().toString();
}

static void screenFull(const MinerStats &st) {
    char rate[16], line[32], up[16];
    const char *unit;
    fmtRate(st.hashrate, rate, sizeof(rate), &unit);
    (void)up;

    txt(0, 0, 1, "ESPMiner");
    txtRight(SCR_W, 0, 1, stratum.stateName());
    oled.drawLine(0, 9, SCR_W - 1, 9, SSD1306_WHITE);

    // The animation owns the right edge, so the number gets what is left -
    // and the number AND its unit have to fit in that, not just the number.
    int16_t avail = animOn() ? 102 : SCR_W;
    uint8_t size = (tw(rate, 2) + 3 + tw(unit, 1) <= avail) ? 2 : 1;
    txt(0, 12, size, rate);
    txt(tw(rate, size) + 3, 12 + (size == 2 ? 8 : 0), 1, unit);
    if (animOn()) drawAnim(106, 11, 20, 18);

    snprintf(line, sizeof(line), "sh %lu/%lu", (unsigned long)st.sharesAccepted,
             (unsigned long)st.sharesFound);
    txt(0, 32, 1, line);
    snprintf(line, sizeof(line), "d%g", st.poolDifficulty);
    txtRight(SCR_W, 32, 1, line);

    snprintf(line, sizeof(line), "best %g", st.bestDifficulty);
    txt(0, 41, 1, line);

    txt(0, 50, 1, ipString().c_str());
}

static void screenBig(const MinerStats &st) {
    char rate[16];
    const char *unit;
    fmtRate(st.hashrate, rate, sizeof(rate), &unit);

    txtCenter(0, 1, stratum.stateName());
    uint8_t size = fitSize(rate, SCR_W, 3);
    txtCenter(10, size, rate);                       // size 3 spans y 10..33
    txtCenter(36, 1, unit);
    if (animOn()) drawAnim(32, 46, 64, 16);
    else {
        char line[24];
        snprintf(line, sizeof(line), "%lu shares", (unsigned long)st.sharesAccepted);
        txtCenter(50, 1, line);
    }
}

static void screenStats(const MinerStats &st) {
    char rate[16], line[32], up[16];
    const char *unit;
    fmtRate(st.hashrate, rate, sizeof(rate), &unit);
    fmtUptime(millis() / 1000, up, sizeof(up));

    // Eight 8-pixel rows, exactly filling the panel.
    txt(0, 0, 1, "ESPMiner");
    txtRight(SCR_W, 0, 1, stratum.stateName());

    snprintf(line, sizeof(line), "rate %s %s", rate, unit);
    txt(0, 8, 1, line);
    snprintf(line, sizeof(line), "shrs %lu/%lu", (unsigned long)st.sharesAccepted,
             (unsigned long)st.sharesFound);
    txt(0, 16, 1, line);
    snprintf(line, sizeof(line), "best %g", st.bestDifficulty);
    txt(0, 24, 1, line);
    snprintf(line, sizeof(line), "diff %g", st.poolDifficulty);
    txt(0, 32, 1, line);
    snprintf(line, sizeof(line), "jobs %lu", (unsigned long)st.jobsReceived);
    txt(0, 40, 1, line);
    snprintf(line, sizeof(line), "up   %s", up);
    txt(0, 48, 1, line);
    txt(0, 56, 1, ipString().c_str());
}

static void screenMinimal(const MinerStats &st) {
    char rate[16], line[24];
    const char *unit;
    fmtRate(st.hashrate, rate, sizeof(rate), &unit);
    snprintf(line, sizeof(line), "%s %s", rate, unit);

    uint8_t size = fitSize(line, SCR_W, 2);
    txtCenter(2, size, line);
    if (animOn()) drawAnim(44, 22, 40, 40);
    else          txtCenter(30, 1, stratum.stateName());
}

static void screenSetup() {
    txt(0, 0, 1, "SETUP MODE");
    oled.drawLine(0, 9, SCR_W - 1, 9, SSD1306_WHITE);
    txt(0, 13, 1, "join this wi-fi:");
    txt(0, 22, 1, s_apSsid.c_str());
    char line[32];
    snprintf(line, sizeof(line), "pass: %s",
             s_apPass.length() ? s_apPass.c_str() : "(open)");
    txt(0, 31, 1, line);
    txt(0, 44, 1, "then browse to");
    txt(0, 53, 1, s_apIp.c_str());
}

// ---------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------
void display_set_options(const ScreenOptions &opts) {
    s_opt = opts;
    if (s_opt.mode >= SCREEN_MODE_COUNT) s_opt.mode = SCREEN_FULL;
    if (s_opt.anim >= ANIM_COUNT) s_opt.anim = ANIM_NONE;
    if (!s_ready) return;

    oled.setRotation(s_opt.flip ? 2 : 0);
    oled.dim(s_opt.dim);
    s_lastDraw = 0;                   // redraw on the next tick
    if (s_opt.mode == SCREEN_OFF) {
        oled.clearDisplay();
        oled.display();
    }
}

ScreenOptions display_get_options() { return s_opt; }

void display_begin() {
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(400000);            // 100 kHz makes animation crawl
    s_ready = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);

    if (!s_ready) {
        // A handful of modules are strapped to the other address; try it
        // before giving up, then scan the bus so a wiring mistake shows up
        // in the serial log instead of as a mysteriously blank screen.
        uint8_t alt = (OLED_ADDRESS == 0x3C) ? 0x3D : 0x3C;
        s_ready = oled.begin(SSD1306_SWITCHCAPVCC, alt);
        if (s_ready)
            MLOG("oled: found at 0x%02X, not 0x%02X - set OLED_ADDRESS in config.h",
                 alt, OLED_ADDRESS);
    }

    if (!s_ready) {
        MLOG("oled: no display on SDA=%d SCL=%d, scanning the bus...", OLED_SDA, OLED_SCL);
        int found = 0;
        for (uint8_t addr = 1; addr < 127; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                MLOG("oled:   something answered at 0x%02X", addr);
                found++;
            }
        }
        if (!found)
            MLOG("oled:   nothing on the bus - check VCC, GND, and that SDA/SCL "
                 "are not swapped");
        return;   // mining carries on happily without a screen
    }

    MLOG("oled: ready on SDA=%d SCL=%d", OLED_SDA, OLED_SCL);
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextWrap(false);          // a fixed layout must never reflow
    oled.setRotation(s_opt.flip ? 2 : 0);
    oled.dim(s_opt.dim);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println(F("ESPMiner " FIRMWARE_VERSION));
    oled.display();
}

void display_message(const String &line1, const String &line2) {
    if (!s_ready || s_opt.mode == SCREEN_OFF) return;
    oled.clearDisplay();
    oled.setTextWrap(false);
    txt(0, 0, 1, line1.c_str());
    txt(0, 12, 1, line2.c_str());
    oled.display();
    s_lastDraw = millis();
}

void display_setup_mode(const String &ssid, const String &password, const String &ip) {
    s_setupMode = ssid.length() > 0;
    s_apSsid = ssid;
    s_apPass = password;
    s_apIp = ip;
    s_lastDraw = 0;
}

void display_update() {
    if (!s_ready) return;

    uint32_t now = millis();
    if (s_opt.mode == SCREEN_OFF && !s_setupMode) return;

    // 8 fps while something is moving, otherwise once a second.
    uint32_t interval = (animOn() && !s_setupMode) ? 125 : 1000;
    if (s_lastDraw && now - s_lastDraw < interval) return;
    s_lastDraw = now;
    s_frame++;

    oled.clearDisplay();
    oled.setTextWrap(false);
    oled.setTextColor(SSD1306_WHITE);

    if (s_setupMode) {
        screenSetup();
        oled.display();
        return;
    }

    MinerStats st = miner_get_stats();

    uint8_t mode = s_opt.mode;
    if (mode == SCREEN_ROTATE) {
        static const uint8_t CYCLE[3] = {SCREEN_FULL, SCREEN_BIG, SCREEN_STATS};
        if (now - s_rotateAt > 5000) {
            s_rotateAt = now;
            s_rotateIndex = (uint8_t)((s_rotateIndex + 1) % 3);
        }
        mode = CYCLE[s_rotateIndex];
    }

    switch (mode) {
        case SCREEN_BIG:     screenBig(st);     break;
        case SCREEN_STATS:   screenStats(st);   break;
        case SCREEN_MINIMAL: screenMinimal(st); break;
        default:             screenFull(st);    break;
    }
    oled.display();
}

#else   // ---- no display configured -------------------------------------

static ScreenOptions s_opt;

void display_begin() {}
void display_set_options(const ScreenOptions &opts) { s_opt = opts; }
ScreenOptions display_get_options() { return s_opt; }
void display_message(const String &, const String &) {}
void display_setup_mode(const String &, const String &, const String &) {}
void display_update() {}

#endif
