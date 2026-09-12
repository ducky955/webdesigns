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
- Optionally a 128x64 SSD1306 OLED on I2C.

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

The math is verified on your PC, no hardware needed:

```bash
cd test && make run
```

It checks SHA-256 against the standard vectors, rebuilds **real block
125552** from its stratum-style fields and confirms the known hash (both via
the plain path and the midstate fast path), checks `nBits` and difficulty
targets — including fractional difficulties like 0.001 — and runs a full
coinbase → merkle → header pipeline against a reference implementation.

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
