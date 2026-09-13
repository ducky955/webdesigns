// Renders every screen layout and animation against a virtual 128x64
// panel, checks nothing falls off the edge or lands on top of text, and
// prints each one as ASCII art so the layout can actually be eyeballed.
//
//   make run          all layouts, one animation frame each
//   ./screen_test -q  checks only, no art
#include "fake_panel.h"
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "../../src/display.h"
#include "../../src/miner.h"
#include "../../src/stratum.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---- doubles for the bits of firmware the screens read ---------------
String        g_testIp = "192.168.86.41";
TwoWire       Wire;
WiFiClass     WiFi;
SerialClass   Serial;
EspClass      ESP;
StratumClient stratum;

static MinerStats g_stats;
MinerStats miner_get_stats() { return g_stats; }
const char *StratumClient::stateName() const { return "mining"; }

static unsigned long g_millis = 23640000;   // 6h34m, as in the photo
unsigned long millis() { return g_millis; }
void delay(unsigned long) {}

// ---- the checks ------------------------------------------------------
static int failures = 0;

static void checkPanel(const char *what, bool showArt) {
    std::vector<std::string> why;
    int oob = panel.outOfBounds(why);
    int hit = panel.textCollisions(why);

    if (showArt) panel.render(what);

    if (oob || hit) {
        printf("  FAIL  %s\n", what);
        for (const std::string &w : why) printf("          %s\n", w.c_str());
        failures++;
    } else {
        printf("  ok    %-34s %2zu draws, all inside 128x64\n", what, panel.ops.size());
    }
}

static void renderOnce(uint8_t mode, uint8_t anim, uint32_t frame) {
    ScreenOptions o;
    o.mode = mode;
    o.anim = anim;
    display_set_options(o);
    // Each call advances one animation frame; the rate limiter is driven
    // off millis(), which we step manually.
    for (uint32_t i = 0; i <= frame; i++) {
        g_millis += 1000;
        display_update();
    }
}

int main(int argc, char **argv) {
    bool quiet = (argc > 1 && !strcmp(argv[1], "-q"));

    g_stats.hashrate = 55100.0;      // 55.10 kH/s
    g_stats.sharesAccepted = 12;
    g_stats.sharesFound = 14;
    g_stats.sharesRejected = 2;
    g_stats.bestDifficulty = 1.308;
    g_stats.poolDifficulty = 0.001;
    g_stats.jobsReceived = 47;
    g_stats.totalHashes = 1300000000ULL;
    g_stats.blocksFound = 0;

    display_begin();

    printf("\nlayouts (animation: pickaxe)\n");
    for (uint8_t m = 0; m < SCREEN_MODE_COUNT; m++) {
        if (m == SCREEN_OFF) continue;
        renderOnce(m, ANIM_PICKAXE, 0);
        char label[64];
        snprintf(label, sizeof(label), "layout '%s'", display_mode_name(m));
        checkPanel(label, !quiet);
    }

    printf("\nanimations (layout: full, 4 frames each)\n");
    for (uint8_t a = 0; a < ANIM_COUNT; a++) {
        for (uint32_t f = 0; f < 4; f++) {
            renderOnce(SCREEN_FULL, a, f);
            char label[64];
            snprintf(label, sizeof(label), "anim '%s' frame %u",
                     display_anim_name(a), (unsigned)f);
            checkPanel(label, false);
        }
    }

    printf("\nminimal layout with each animation\n");
    for (uint8_t a = 1; a < ANIM_COUNT; a++) {
        renderOnce(SCREEN_MINIMAL, a, 2);
        char label[64];
        snprintf(label, sizeof(label), "minimal + '%s'", display_anim_name(a));
        checkPanel(label, !quiet && a == ANIM_SPINNER);
    }

    printf("\nawkward values (long strings must still fit)\n");
    struct Case { const char *name; double rate; uint32_t acc, found; double best, diff; const char *ip; };
    const Case cases[] = {
        {"zero everything",      0.0,       0,      0, 0.0,      0.0,    "0.0.0.0"},
        {"megahash rate",        12345678., 999999, 999999, 123456.7, 65536.0, "192.168.100.200"},
        {"tiny difficulties",    41830.,    7,      9,  0.00012,  0.0001, "10.0.0.8"},
        {"long ip, big numbers", 999999.,   123456, 123456, 9999.99, 1024.0, "255.255.255.255"},
    };
    for (const Case &c : cases) {
        g_stats.hashrate = c.rate;
        g_stats.sharesAccepted = c.acc;
        g_stats.sharesFound = c.found;
        g_stats.bestDifficulty = c.best;
        g_stats.poolDifficulty = c.diff;
        g_testIp = c.ip;
        for (uint8_t m = 0; m < SCREEN_MODE_COUNT; m++) {
            if (m == SCREEN_OFF) continue;
            renderOnce(m, ANIM_BARS, 1);
            char label[96];
            snprintf(label, sizeof(label), "%s / %s", c.name, display_mode_name(m));
            checkPanel(label, false);
        }
    }

    printf("\nsetup screen\n");
    display_setup_mode("ESPMiner-Setup", "bitcoin123", "192.168.4.1");
    renderOnce(SCREEN_FULL, ANIM_NONE, 0);
    checkPanel("setup mode", !quiet);

    printf("\n%s (%d failure%s)\n", failures ? "SCREEN TESTS FAILED" : "ALL SCREENS FIT",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
