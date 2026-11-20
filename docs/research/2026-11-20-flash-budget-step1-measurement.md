# Flash budget, step 1: where the bytes go (beta.401)

Date: 2026-11-20. Author: otgw-architect. This is a measurement only. Step 2 (trim candidates with a risk class), step 3 (a policy ADR through adr-kit) and step 4 (an epic) follow from it.

## 0. Prior work this builds on

- **TASK-1639** (GitHub #694) did the first size breakdown at beta.89, at 93.5 %. Robert chose only the siscanf reduction then.
- **TASK-1955** (Done, 2026-11-15) brought headroom back to about 157 KB at beta.323, mainly through ADR-247: the replay fixture and settings help are stored deflated, saving 22,304 B. The coordinator parked three further options, each with a trigger:
  - pack `mqttHaSensors` (5-8 KB) when the headroom falls below 96 KB;
  - replace WiFiManager and the Arduino WebServer with an async portal (30-40 KB) when the headroom falls below 64 KB, or when a feature over 40 KB is planned;
  - shorten our debug strings (5-10 KB), but only alongside other work in the same files.
- **TASK-1993** (Done): `build.py` logs every image against the app slot, with WARN above 95 % and FAIL above 98 %.

**What this report adds:**

- The headroom at beta.401 is 139,984 B (93.9 %). None of the three triggers is reached.
- The merged string pool is now attributed. Of its 250 KB, about 134 KB is ours. TASK-1955 estimated our debug formats at about 50 KB, using a different method (string-content heuristics). This report does not settle which number is closer. The two are not directly comparable, because the 134 KB counts all our literals, not just the debug formats.
- The trend now covers all 35 releases.

## 1. The budget

| item | bytes | source |
|---|---|---|
| `app0` partition | 2,293,760 (0x230000) | `partitions_otgw_esp32.csv` (ADR-191 layout) |
| beta.401 app image (`...beta.401+0bee309.ino.bin`) | 2,153,776 | GitHub release asset, sha256 checked against the release's SHA256SUMS |
| **headroom** | **139,984 (6.1 %)** | |

The safeboot updater has its own 960 KB partition. An OTA update needs only `app0` to fit, so `app0` is the limit that counts.

### Trend

These are the combo `.ino.bin` sizes of all 35 published 2.x releases (GitHub release assets):

| release | date | bytes |
|---|---|---|
| beta.1 | 2026-11-01 | 2,128,432 |
| beta.105 | 2026-11-06 | 2,151,184 |
| beta.118 | 2026-11-07 | 2,123,984 (-27 KB) |
| beta.294 | 2026-11-15 | 2,155,936 (largest so far) |
| beta.323 | 2026-11-15 | 2,136,816 (-19 KB) |
| beta.401 | 2026-11-20 | 2,153,776 |

- Since beta.323, growth is +16,960 B in 5 days, about 3.4 KB per day. At that rate the headroom is gone in about 41 days.
- Since beta.1, net growth is +25 KB in 19 days, about 1.3 KB per day, which leaves about 107 days. The two drops (beta.118 and beta.323) were one-off trims.
- These are estimates from a line through the points, not a forecast.

## 2. The image by section

From the beta.401 ELF (`xtensa-esp-elf-size -A`):

| section | bytes | what |
|---|---|---|
| `.flash.text` | 1,477,732 | code run from flash |
| `.flash.rodata` | 542,328 | constants and strings |
| `.iram0.text` + vectors | 101,075 | code in IRAM |
| `.dram0.data` | 32,196 | initialised RAM data, stored in the image |

## 3. Where the code and constants come from

**Method.** I read every symbol in the beta.401 ELF that has a size, with its DWARF source line (`nm -S -l`), and grouped it by source path. That covers 1,673 KB of the 2,154 KB image (78 %). The rest is mostly the merged string pool (section 4) and padding.

| origin | bytes | share of the attributed bytes |
|---|---|---|
| ESP-IDF, prebuilt with debug info (lwip 109 K, mbedtls 63 K, wpa_supplicant 57 K, mdns 26 K, littlefs 23 K, hal, freertos, nvs, drivers) | 527,805 | 31.5 % |
| no source line: the prebuilt WiFi, PHY and BT controller blobs (`ieee80211*`, `pp*`, `lmac*`, `llm*`/`lld*`/`btdm*`) and other symbols without debug info | 466,940 | 27.9 % |
| **ours: `src/OTGW-firmware`** | **325,221** | **19.4 %** |
| libdeps | 116,488 | 7.0 % |
| Arduino core and libraries | 92,145 | 5.5 % |
| toolchain (newlib, libgcc) | 83,985 | 5.0 % |
| other (mostly `std_function.h` instantiations, 33 K) | 41,477 | 2.5 % |
| vendored libraries (espMqttClient, OTGWSerial, SimpleTelnet, Platform, OpenTherm) | 18,981 | 1.1 % |

**Our code by file.** These are the largest; the build uses LTO, so inlined code counts toward the file that defines the symbol.

| file | size | file | size |
|---|---|---|---|
| SATcontrol | 50 K | OTGW-Core | 42 K |
| restAPI | 32 K | OTGW-firmware | 29 K |
| MQTTstuff | 24 K | networkStuff | 21 K |
| OTDirect | 21 K | settingStuff | 19 K |
| SATcycles | 8 K | FSexplorer | 7 K |
| MQTTHaDiscovery | 6 K | SATble | 5 K |
| webServerCompat.h | 5 K | sensors_ext | 4 K |

**libdeps.** NimBLE-Arduino 52 K, WiFiManager 24 K, ESPAsyncWebServer 21 K, AceTime 6 K, AsyncTCP 4 K, EthernetESP32 2 K.

**Arduino.** Of the 92 K:

- core 27 K;
- WebServer 11 K, the synchronous server. Our HTTP server is async (`webServerCompat.h`), so it is most likely pulled in by WiFiManager. That is not verified;
- Network 10 K, WiFi 8 K, HTTPClient 7 K, Wire 7 K, Hash 5 K.

**Our largest constant objects in `.flash.rodata`.** All of them are named PROGMEM arrays:

- `UpdateServerIndex` 16,132;
- `mqttHaSensors` 11,340;
- `SETTINGS_HELP_GZ` 6,959;
- `OTmap` 3,752;
- `FAVICON_ICO` 3,627;
- `HTTP_STYLE` 2,969;
- `mqttHaBinSensors` 2,712;
- `UpdateServerSuccess` 1,332;
- `HTTP_HELP` 1,137;
- `kSatMqttCmds` 972.

## 4. The merged string pool: 250,641 B (11.6 % of the image)

The linker merges every NUL-terminated literal (`.rodata.*.str1.1`) from all objects into one pool. In the map it appears under the first object that contributed one.

- In `firmware.map` that object is `libesp_driver_gpio.a(gpio.c.obj)`: 0x3d311 bytes, while its own contribution "before relaxing" was 0x2f.
- The pool is the largest single item in `.flash.rodata`.

**How I split it.**

1. Each string in the pool was looked up as a complete string in the 186 prebuilt archives: the IDF libs, the `qio_qspi` libs and the toolchain libs.
2. A string not found there comes from LTO-compiled code: ours, the libdeps and the Arduino core.
3. For this I took the pool bytes from the beta.403 ELF (`objcopy`), because only beta.403 still had a map. See section 6.

| part | bytes |
|---|---|
| strings found in prebuilt archives (lwip 12 K, esp_eth 7 K, esp_common 6 K, mbedcrypto 6 K, littlefs 4 K, ...) | 95,600 |
| LTO side: Arduino core and library logs (`[E][file:line]` formats, `__FILE__`) | about 20,000 |
| **LTO side: the rest, mostly ours** (telnet and debug log formats, HA discovery JSON templates, REST JSON, settings and state keys) | **about 134,000** |
| short strings (< 4 B), unattributed | 811 |

The split between "Arduino" and "ours" on the LTO side is a heuristic on the string content. A sample of the list confirms it is mostly our text, but some WiFiManager HTML sits in it too.

## 5. What this means for the budget

| part of the image | about | can we change it? |
|---|---|---|
| our code | 325 K | yes, directly |
| our string literals | about 134 K | yes, directly (log formats, JSON templates) |
| our PROGMEM objects (section 3) | about 50 K | yes, but `UpdateServerIndex` and `SETTINGS_HELP_GZ` are there on purpose, for recovery and offline help |
| libdeps + Arduino + vendored | about 228 K | partly: a choice of library or feature |
| IDF, WiFi/BT blobs, toolchain | about 1.1 M | effectively fixed: prebuilt libs; only sdkconfig changes plus a library rebuild would move it, which is not for 2.0.0 |

About a quarter of the image, roughly 510 K, is in our hands. The 140 K of headroom is about 6.5 % of the image, and about a quarter of what we control.

## 6. Method, evidence and caveats

- **Inputs.**
  - The beta.401 ELF and `.ino.bin` came from the GitHub release; the sha256 matched the release's SHA256SUMS.
  - The beta.403 ELF and `firmware.map` were read-only copies from the beta.403+ce65ac7 build. No beta.401 map was left, and beta.403 is two prereleases later.
  - The section-3 numbers are beta.401; the string-pool numbers are beta.403.
- **Commands.**
  - `xtensa-esp-elf-size -A`;
  - `xtensa-esp-elf-nm -S -l --defined-only`;
  - `xtensa-esp-elf-objcopy -O binary --only-section=.flash.rodata`;
  - Python scripts to parse the map and split the pool, kept with the measurement in the architect's scratchpad.
- **LTO.** The map shows our code only as `ltrans*.o` objects, so the per-file numbers come from DWARF (section 3), not from the map. Inlining moves bytes between files, so per-file sizes are approximate. The per-origin totals are robust.
- **Not verified.**
  - The exact split of the 467 K without a source line between WiFi, BT and other code. It is all prebuilt and not changeable either way.
  - Whether the sync `WebServer` is linked only because of WiFiManager.
  - How much of "about 134 K ours" is debug and telnet output, as opposed to protocol text that has to stay.
