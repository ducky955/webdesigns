// =====================================================================
//  ESPMiner - configuration
// =====================================================================
//  EDIT THE VALUES IN THIS FILE, or (better) copy secrets.h.example to
//  secrets.h and put your private values there - secrets.h is listed in
//  .gitignore so your Wi-Fi password never ends up in the repository.
//
//  Anything set here is only a DEFAULT: once the miner is running you
//  can change every setting from the built-in web UI
//  (http://espminer.local or the IP printed on the serial console) and
//  the new values are stored in flash (NVS), surviving reflashes.
// =====================================================================
#pragma once

#if defined(__has_include)
#  if __has_include("secrets.h")
#    include "secrets.h"
#  endif
#endif

// ---------------------------------------------------------------------
// 1. Wi-Fi  (2.4 GHz only - the ESP32 cannot join a 5 GHz network)
// ---------------------------------------------------------------------
#ifndef WIFI_SSID
#define WIFI_SSID           "YOUR_WIFI_NAME"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"
#endif

// ---------------------------------------------------------------------
// 2. Payout address  -  THIS is where the 3.125 BTC would land.
//    Use an address you control the private keys for. Native segwit
//    (bc1q...), taproot (bc1p...) and legacy (1.../3...) all work as
//    long as your pool accepts them.
// ---------------------------------------------------------------------
#ifndef BTC_ADDRESS
#define BTC_ADDRESS         "bc1qYOURADDRESSGOESHERE"
#endif

// Worker name - shows up on the pool dashboard. Keep it short, no dots.
#ifndef WORKER_NAME
#define WORKER_NAME         "esp32"
#endif

// ---------------------------------------------------------------------
// 3. Pool  (must be a SOLO pool if you want the whole block reward)
//
//    public-pool.io      solo, open source, nice dashboard
//        host "public-pool.io"        port 21496
//    ckpool solo         the classic, 2% fee, no registration
//        host "solo.ckpool.org"       port 3333
//    nerdminers.org      solo pool aimed at low-power miners
//        host "pool.nerdminers.org"   port 3333
//
//    Dashboard for public-pool: https://web.public-pool.io/#/app/<BTC_ADDRESS>
// ---------------------------------------------------------------------
#ifndef POOL_HOST
#define POOL_HOST           "public-pool.io"
#endif
#ifndef POOL_PORT
#define POOL_PORT           21496
#endif
// Stratum password. Almost every pool ignores it; "x" is the convention.
#ifndef POOL_PASSWORD
#define POOL_PASSWORD       "x"
#endif

// Ask the pool for this share difficulty (0 = accept whatever it gives).
// Lower = more shares = a livelier dashboard; it does NOT change your
// odds of finding a block.
#ifndef SUGGESTED_DIFFICULTY
#define SUGGESTED_DIFFICULTY 0
#endif

// ---------------------------------------------------------------------
// 4. Setup access point
//    If the Wi-Fi above cannot be joined, the miner starts its own
//    network with this name so you can enter the right credentials from
//    a phone. Password must be >= 8 characters.
// ---------------------------------------------------------------------
#define AP_SSID             "ESPMiner-Setup"
// Must be at least 8 characters, or the ESP32 refuses to start the AP.
// Set to "" for an open network with no password.
#define AP_PASSWORD         "bitcoin123"
// 2.4 GHz channel for the setup network. 1, 6 or 11 are the sane choices.
#define AP_CHANNEL          1

// Hold this pin low during the Wi-Fi connect to force setup mode, even if
// the configured network is working. On almost every dev board it is the
// BOOT button. Press it while the dots are scrolling on the serial console
// (not during reset - that puts the chip into flashing mode). Set to -1
// to disable.
#define AP_FORCE_PIN        0

// Hostname for mDNS + DHCP -> http://espminer.local
#define MINER_HOSTNAME      "espminer"

// ---------------------------------------------------------------------
// 5. Tuning
// ---------------------------------------------------------------------
// Hashing tasks. The ESP32 has two cores; 2 is the sweet spot. Use 1 if
// you want a core free for something else (or to run cooler).
#define MINER_TASK_COUNT    2

// Seconds to wait for Wi-Fi before falling back to the setup AP.
#define WIFI_CONNECT_TIMEOUT 25

// Seconds between stat lines on the serial console (0 = never).
#define STATS_INTERVAL       10

// Built-in web dashboard on port 80.
#define ENABLE_WEB_UI        1

// Over-the-air updates (PlatformIO: upload_protocol = espota).
#define ENABLE_OTA           1

// Serial console speed.
#define SERIAL_BAUD          115200

// OLED pins, only used when built with -DUSE_OLED=1. Any two free GPIOs
// work - I2C is not tied to specific pins on the ESP32. See the wiring
// table in README.md. Override from platformio.ini with -DOLED_SDA=...
#ifndef OLED_SDA
#  if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2)
#    define OLED_SDA         8
#  else
#    define OLED_SDA         21
#  endif
#endif
#ifndef OLED_SCL
#  if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2)
#    define OLED_SCL         9
#  else
#    define OLED_SCL         22
#  endif
#endif
// 0x3C for almost every module; a few are strapped to 0x3D.
#ifndef OLED_ADDRESS
#  define OLED_ADDRESS       0x3C
#endif

#define FIRMWARE_VERSION     "1.0.0"
