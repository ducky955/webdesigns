#include "display.h"
#include "config.h"
#include "miner.h"
#include "stratum.h"
#include "settings.h"

#if defined(USE_OLED) && USE_OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(128, 64, &Wire, -1);
static bool s_ready = false;

void display_begin() {
    Wire.begin(OLED_SDA, OLED_SCL);
    s_ready = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
    if (!s_ready) return;
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
    oled.setCursor(0, 36);
    oled.printf("shares %lu/%lu\n", (unsigned long)st.sharesAccepted,
                (unsigned long)st.sharesFound);
    oled.printf("best   %.3f\n", st.bestDifficulty);
    uint32_t up = millis() / 1000;
    oled.printf("up     %luh%02lum", (unsigned long)(up / 3600),
                (unsigned long)((up % 3600) / 60));
    oled.display();
}

#else   // ---- no display configured -------------------------------------

void display_begin() {}
void display_message(const String &, const String &) {}
void display_update() {}

#endif
