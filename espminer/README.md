# ESPMiner

A Bitcoin **solo ("lottery") miner** for the ESP32. It talks Stratum V1 to a
solo pool, grinds SHA-256d on both cores, and ships with a web dashboard so
you can watch it not find a block.

```
        ESP32                      solo pool                 Bitcoin
   +-------------+            +----------------+
   | core 0  ----|---\        |                |
   | core 1  ----|---+--------| mining.notify  |--- block template
   |             |   |        | mining.submit  |--> the whole 3.125 BTC
   | web UI :80  |   |        +----------------+     goes to YOUR address
   +-------------+   |
      http://espminer.local
```

## Read this part first

An ESP32 does roughly **20-60 kH/s**. The Bitcoin network does around
**10^21 H/s**. Your share of the network is about one part in twenty
quadrillion, which works out to somewhere north of *a hundred billion years*
per block on average.

This is not a money-making device. It is a lottery ticket that costs about a
watt, and unlike a real lottery ticket the odds never expire — every ten
minutes you get a fresh draw. If it ever does hit, **the entire block reward
goes to the address you configure**, because solo pools do not split rewards.
That is the whole point of solo mining.

Budget your expectations accordingly, and enjoy the blinkenlights.

## What you need

- Any ESP32 dev board (the $5 clones are fine). ESP32-S3 also works.
- A 2.4 GHz Wi-Fi network — the ESP32 cannot see 5 GHz networks.
- A Bitcoin address **you hold the private keys to**. Not an exchange deposit
  address you found in an email; if a block lands there you want to be able
  to spend it.
- Optionally a 128x64 SSD1306 OLED on I2C — see [Adding an OLED](#adding-an-oled-optional).

## Quick start

### 1. Get the code

```bash
git clone https://github.com/ducky955/webdesigns.git
cd webdesigns/espminer
```

### 2. Put in your details

Copy the template and fill it in — this file is git-ignored, so your Wi-Fi
password never gets committed:

```bash
cp src/secrets.h.example src/secrets.h
```

```c
#define WIFI_SSID      "MyHomeWiFi"
#define WIFI_PASSWORD  "hunter2hunter2"
#define BTC_ADDRESS    "bc1q...your own address..."
#define WORKER_NAME    "esp32"
```

If you would rather not rebuild to change things, skip this and use the web
UI in step 4 — everything is editable at runtime and stored in flash.
`src/config.h` has the rest of the knobs (pool, ports, task count, display).

### 3. Flash it

With [PlatformIO](https://platformio.org/install/cli):

```bash
pio run -e esp32dev -t upload      # build + flash over USB
pio device monitor                 # watch it work
```

Other targets: `esp32-s3`, `lolin32`, `esp32dev-oled` (adds the SSD1306 screen).

<details>
<summary>Arduino IDE instead of PlatformIO</summary>

1. Install the **esp32** boards package (Boards Manager) and the
   **ArduinoJson** library (Library Manager, version 7.x).
2. Make a sketch folder called `espminer`, copy everything from `src/` into
   it, and rename `main.cpp` to `espminer.ino`.
3. Select your ESP32 board and upload.

</details>

### 4. Watch it

The serial console prints the IP address it got. Open it — or
`http://espminer.local` — for the dashboard:

- live hash rate, accepted/rejected shares, best difficulty found
- the address and pool it is actually mining to
- a **Settings** page for Wi-Fi, payout address, worker name and pool,
  saved to flash and applied on reboot

If the configured Wi-Fi can't be joined, the miner starts its own network
called **ESPMiner-Setup** (password `bitcoin123`). Join it from a phone,
open any page, and you land on the same settings form.

## Adding an OLED (optional)

A 0.96" 128x64 SSD1306 on I2C — the four-pin kind (GND, VCC, SCL, SDA). The
seven-pin SPI version of the same panel is a different beast and is not
wired up here.

### Wiring

```
   ESP32 dev board                    SSD1306 128x64 (I2C)
   +----------------+                 +--------------------+
   |           3V3  o-----------------o VCC                |
   |           GND  o-----------------o GND                |
   |         GPIO21 o-----------------o SDA                |
   |         GPIO22 o-----------------o SCL                |
   +----------------+                 +--------------------+
```

| OLED pin | ESP32 | ESP32-S3 / S2 | Notes |
|---|---|---|---|
| `VCC` | `3V3` | `3V3` | most modules take 3.3-5 V; 3V3 is the safe choice |
| `GND` | `GND` | `GND` | |
| `SDA` | `GPIO21` | `GPIO8` | data |
| `SCL` | `GPIO22` | `GPIO9` | clock |

Four things worth knowing:

- **Check the silkscreen before you plug anything in.** These modules ship
  with the pins in at least two different orders — `GND VCC SCL SDA` and
  `VCC GND SCL SDA` are both common. Going by position rather than by label
  is how people put 3V3 into GND.
- **No pull-up resistors needed.** The module has them, and the ESP32
  enables its internal ones.
- **Any two free GPIOs work.** I2C on the ESP32 is not tied to particular
  pins. 21/22 are just the traditional default. Avoid the strapping pins
  (0, 2, 12, 15) and the input-only pins (34-39, which cannot drive SDA).
- **Swapping SDA and SCL is harmless**, just non-functional. Try the other
  way round if nothing appears.

### Build

```bash
pio run -e esp32dev-oled -t upload      # ESP32
pio run -e esp32-s3-oled -t upload      # ESP32-S3
```

For different pins, either edit `OLED_SDA` / `OLED_SCL` in `src/config.h` or
pass them in `platformio.ini`:

```ini
build_flags = ${common.build_flags} -DUSE_OLED=1 -DOLED_SDA=18 -DOLED_SCL=19
```

Arduino IDE users: add `#define USE_OLED 1` at the top of `config.h`, and
install the **Adafruit SSD1306** and **Adafruit GFX** libraries.

### What it shows

Six layouts, switchable **live from the dashboard** - no reboot, no
reflash - and remembered in flash:

```
  full                        big                      stats
  +---------------------+  +---------------------+  +---------------------+
  |ESPMiner       mining|  |       mining        |  |ESPMiner       mining|
  |55.10           /\   |  |                     |  |rate 55.10 kH/s      |
  |     kH/s      /  \  |  |     55.10           |  |shrs 12/14           |
  |               ####  |  |                     |  |best 1.308           |
  |sh 12/14      d0.001 |  |      kH/s           |  |diff 0.001           |
  |best 1.308           |  |                     |  |jobs 47              |
  |192.168.86.41        |  |     [ anim ]        |  |up   6h34m           |
  |                     |  |                     |  |192.168.86.41        |
  +---------------------+  +---------------------+  +---------------------+
```

- **full** - hash rate plus the numbers worth watching (the default)
- **big** - one huge hash rate, readable across the room
- **stats** - everything at once, eight dense rows
- **minimal** - mostly animation
- **rotate** - cycles full / big / stats every five seconds
- **off** - blanks the panel

And six animations: **none, pickaxe, spinner, bars, pulse, chain**. Pick
one from the Screen card on the dashboard; it applies instantly. Animated
screens redraw at 8 fps, static ones once a second.

Two more toggles there: **flip 180&deg;** for upside-down mounting, and
**dim** for a dark room.

Starting values live in `config.h` (`SCREEN_MODE`, `SCREEN_ANIM`,
`SCREEN_FLIP`, `SCREEN_DIM`) but the dashboard overrides them and the
choice survives a reboot.

Every layout is checked against a virtual 128x64 panel by `test/screen`,
including with awkward values - megahash rates, six-digit share counts,
a full-length IP - so nothing runs off the edge or lands on top of
anything else.

### If the screen stays blank

The firmware tells you what happened — check the serial console at boot:

- `oled: ready on SDA=21 SCL=22` — the display is alive; if it is still
  blank, that is a contrast/panel problem, not wiring.
- `oled: found at 0x3D, not 0x3C` — your module uses the other I2C address.
  It carries on working; set `OLED_ADDRESS` in `config.h` to silence it.
- `oled: something answered at 0x..` — a device is on the bus but it is not
  an SSD1306 at the address we tried. Usually an SH1106 panel, which needs a
  different driver library.
- `oled: nothing on the bus` — power or wiring. Check VCC and GND first,
  then try swapping SDA and SCL.

The miner does not care either way: if there is no display it logs the
problem once and keeps hashing.

## Pools

Any Stratum V1 **solo** pool works. The default is public-pool.io.

| Pool | Host | Port | Notes |
|---|---|---|---|
| public-pool.io | `public-pool.io` | 21496 | open source, nice dashboard, no registration |
| ckpool solo | `solo.ckpool.org` | 3333 | the classic, 2% fee |
| nerdminers.org | `pool.nerdminers.org` | 3333 | aimed at low-power miners |

Your stratum username is `<BTC_ADDRESS>.<WORKER_NAME>` — that is how a solo
pool knows where to send the reward. Watch your miner on public-pool at
`https://web.public-pool.io/#/app/<your address>`.

A regular (non-solo) pool will also accept the connection, but then you are
mining for *their* payout scheme, and an ESP32's share of that is
indistinguishable from zero. Solo is the only mode that makes sense here.

## How it works

Per job from the pool, each core:

1. builds the coinbase transaction — `coinb1 + extranonce1 + extranonce2 + coinb2`
2. double-SHA256s it and folds in the merkle branch to get the merkle root
3. assembles the 80-byte block header (version, prev hash, merkle root,
   ntime, nbits, nonce)
4. hashes the first 64 bytes **once** into a midstate, then sweeps all 2^32
   nonces through only the last 16 bytes — two SHA-256 transforms per hash
   instead of four
5. submits anything that beats the pool's share target, and shouts loudly if
   something beats the *network* target

The fiddly parts are byte order. Stratum sends `prevhash` with each 4-byte
word reversed, version/ntime/nbits as big-endian hex that the header wants
little-endian, and the merkle root already in internal order. Getting any of
these backwards produces a miner that runs happily forever and never
produces a valid share — which is why there are tests.

| File | What it does |
|---|---|
| `src/main.cpp` | boot, Wi-Fi, setup AP, the housekeeping loop |
| `src/miner.cpp` | job state and the per-core hashing tasks |
| `src/stratum.cpp` | Stratum V1 client: subscribe, authorize, notify, submit |
| `src/sha256d.cpp` | SHA-256 plus the midstate fast path |
| `src/btc_util.cpp` | hex, difficulty <-> target, share checking |
| `src/settings.cpp` | defaults from `config.h`, overrides in flash (NVS) |
| `src/webui.cpp` | dashboard, `/api/status`, settings form |
| `src/display.cpp` | optional SSD1306 screen |

## Tests

Both suites run on your PC, no hardware needed.

### Unit tests — the mining math

```bash
cd test && make run
```

Checks SHA-256 against the standard vectors, rebuilds **real block 125552**
from its stratum-style fields and confirms the known hash (both via the
plain path and the midstate fast path), checks `nBits` and difficulty
targets — including fractional ones like 0.001 — and runs a full
coinbase → merkle → header pipeline against a reference implementation.

### Screen layouts — no panel required

```bash
cd test/screen && make run
```

Renders every layout and animation frame against a virtual 128x64 panel,
prints each as ASCII art, and fails if any draw falls outside the panel or
a graphic lands on top of text. It runs the real `display.cpp`, so the
layouts it draws are the ones your OLED gets.

### Simulation — the firmware against a fake pool

```bash
cd test/sim && make run
```

This compiles the **real `miner.cpp` and `stratum.cpp`** for your PC — tasks
become threads, `WiFiClient` talks to an in-process pool — and runs a
complete session: subscribe, authorize, `set_difficulty`, `notify`, mine,
submit, accept. Then a second job to prove job switching, then the error
paths: a non-JSON line from the pool, a truncated `mining.notify`, a
mid-session difficulty change, and a share the pool refuses.

Every share it submits is written to `sim_result.json`, and `verify_sim.py`
rebuilds each one from scratch with Python's `hashlib` — coinbase, merkle
root, header, double SHA-256 — and checks it really beats the target:

```
  1. job sim-job-1  nonce 00020de0  extranonce2 00000000
     hash 00001f5b2d2cd0c90a6cff41b81a5ddb67270eb83c2671125ffd9d082753bba9
     difficulty 0.0001  -> VALID

PASSED: all 4 submissions are cryptographically valid shares
```

That check shares no code with the firmware, so agreement means the byte
order and merkle folding are actually right rather than merely
self-consistent. The difficulty is set low so a PC finds shares in under a
second.

The simulation is also clean under sanitizers, which is worth doing after
any change to the threading:

```bash
cd test/sim
g++ -std=gnu++17 -O1 -g -fsanitize=address,undefined -I. -I../../src \
    -o sim_asan sim_main.cpp sim_support.cpp ../../src/*.cpp -pthread   # then ./sim_asan
g++ -std=gnu++17 -O1 -g -fsanitize=thread -I. -I../../src \
    -o sim_tsan sim_main.cpp sim_support.cpp ../../src/*.cpp -pthread   # then ./sim_tsan
```

**What this does not cover:** stack sizes, the task watchdog, real Wi-Fi,
NVS, the web UI, the OLED, and actual hash rate. Those need the hardware.

## Troubleshooting

**`authorize FAILED`** — the pool rejected your username. Almost always a
typo in the Bitcoin address, or an address type that pool doesn't accept.

**Connects, then nothing** — check the serial log for `job <id>` lines. No
jobs means the pool isn't sending work; wrong port is the usual cause.

**Shares found but rejected** — usually "job not found": the share was for a
job the pool already replaced. A few of these is normal. All of them being
rejected means something is wrong; open an issue with the serial log.

**Wi-Fi won't connect** — 2.4 GHz only, and the ESP32 dislikes some WPA3-only
networks. Try a WPA2 or mixed-mode SSID.

**The ESPMiner-Setup network never appears** — watch the serial console at
boot. It prints a banner with the network name, password and address when
the AP is up, or `COULD NOT START THE SETUP AP` when it is not (it then
retries every 10 seconds, and falls back to an open network if a protected
one is refused). With an OLED attached, setup mode takes over the screen
and shows the same details.

If the miner already has working Wi-Fi saved, it joins that instead and
never starts the AP — that is deliberate. The dashboard is then on your LAN,
at the address shown on the screen and in the serial log, or at
`http://espminer.local`. To get the setup network anyway, **press and hold
the BOOT button while the connect dots are scrolling** on the serial
console. (Holding BOOT during reset does something else entirely — it puts
the chip in flashing mode.) `Settings → Factory reset` also clears the saved
Wi-Fi.

**Reboot loops / brownouts** — feed it from a decent USB supply. Both cores
hashing flat out draws more than some laptop ports like to give.

**Forgot what you configured** — the dashboard shows the live address and
pool. `Settings → Factory reset` puts everything back to `config.h`.

## Notes

- Mining is legal in most places; electricity is on you. This draws about as
  much as a night light.
- The firmware never sends your Wi-Fi password anywhere — it lives in flash
  and is never echoed back by the web UI. The dashboard has no password
  though, so anyone on your LAN can change the payout address. Don't run it
  on a network you don't trust.
- Prior art worth a look: [NerdMiner](https://github.com/BitMaker-hub/NerdMiner_v2)
  and [ESP-Miner](https://github.com/skot/ESP-Miner) (the Bitaxe firmware).
