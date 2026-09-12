#include "display.h"
#include "config.h"
#include "mlog.h"
#include "miner.h"
#include "stratum.h"
#include <WiFi.h>

#if defined(USE_OLED) && USE_OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(128, 64, &Wire, -1);
static bool s_ready = false;

void display_begin() {
    Wire.begin(OLED_SDA, OLED_SCL);
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
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println(F("ESPMiner " FIRMWARE_VERSION));
    oled.display();
}

void display_message(const String &line1, const String &line2) {
    if (!s_ready) return;
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println(line1);
    oled.println(line2);
    oled.display();
}

void display_update() {
    if (!s_ready) return;
    MinerStats st = miner_get_stats();

    double rate = st.hashrate;
    const char *unit = "H/s";
    if (rate >= 1000.0) { rate /= 1000.0; unit = "kH/s"; }

    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print(F("ESPMiner  "));
    oled.println(stratum.stateName());

    oled.setTextSize(2);
    oled.setCursor(0, 14);
    oled.print(rate, 2);
    oled.setTextSize(1);
    oled.print(' ');
    oled.println(unit);

    oled.setTextSize(1);
    oled.setCursor(0, 34);
    oled.printf("shares %lu/%lu\n", (unsigned long)st.sharesAccepted,
                (unsigned long)st.sharesFound);
    oled.printf("best   %.3f\n", st.bestDifficulty);
    uint32_t up = millis() / 1000;
    oled.printf("up     %luh%02lum\n", (unsigned long)(up / 3600),
                (unsigned long)((up % 3600) / 60));
    oled.print(WiFi.localIP().toString());
    oled.display();
}

#else   // ---- no display configured -------------------------------------

void display_begin() {}
void display_message(const String &, const String &) {}
void display_update() {}

#endif
