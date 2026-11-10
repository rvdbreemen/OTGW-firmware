## Chapter 8: Developer Guide

This chapter is the primary reference for contributors and developers working with the OTGW-firmware source code. It covers the project layout, build system, architecture model, key coding patterns, and the step-by-step workflows for extending the firmware with new REST endpoints, MQTT topics, and settings. Read this chapter before writing a single line of code.

---

### Project Layout

The repository root contains the firmware source, build tooling, documentation, tests, and reference projects. The primary source tree is under `src/OTGW-firmware/`.

#### Source Files

All firmware code lives in `src/OTGW-firmware/` as Arduino `.ino` files plus a small number of `.cpp` translation units for large generated tables. Arduino concatenates the main sketch and every sibling `.ino` file into a single translation unit; `.cpp` files are compiled separately. Each file owns one logical domain:

| File | Domain |
|------|--------|
| `OTGW-firmware.ino` | Main entry point: `setup()`, `loop()`, boot sequence, LED management |
| `OTGW-Core.ino` | OpenTherm protocol decoding, PIC serial I/O, command queue, MQTT throttle |
| `OTGW-Core.h` | OpenTherm data structures: `OTdataStruct`, `OTLibMessageID`, frame type enums |
| `OTGW-firmware.h` | Top-level aggregator: includes the per-component `*types.h` headers, defines the aggregate `OTGWSettings` / `OTGWState` structs, forward declarations, shared constants |
| `*types.h` | Per-component state + settings struct headers (ADR-079 / ADR-081). One file per subsystem: `SATtypes.h`, `OTDirecttypes.h`, `Devicetypes.h`, `Hardwaretypes.h`, `Networktypes.h`, `PICtypes.h`, `OTBustypes.h`, `Flashtypes.h`, `Uptimetypes.h`, `NTPtypes.h`, `Sensorstypes.h`, `S0types.h`, `Outputstypes.h`, `Webhooktypes.h`, `UItypes.h` |
| `MQTTstuff.ino` | MQTT client state machine, HA auto-discovery, command subscriptions, `sendMQTTData()` |
| `networkStuff.ino` | WiFi, mDNS, LLMNR, NTP, OTA, network platform abstraction |
| `restAPI.ino` | REST API v2 dispatch table, all resource handlers, HTTP Basic Auth, CSRF, file serving |
| `FSexplorer.ino` | LittleFS browser: file listing, upload/delete endpoints |
| `jsonStuff.ino` | Low-level JSON formatting helpers: `sendJsonMapEntry()`, `sendJsonKVLine()`, streaming map builders |
| `settingStuff.ino` | Settings load/save (LittleFS JSON), `updateSetting()` validation, debounced flush, side-effect dispatch |
| `webSocketStuff.ino` | WebSocket server on port 81, OT log streaming, heap backpressure gate |
| `helperStuff.ino` | Heap health monitor, `canSendWebSocket()`, `canPublishMQTT()`, LittleFS status helpers |
| `OTDirect.ino` | OT-Direct engine (used only on hardware without a PIC): OTDirect operating modes, command queue coalesce-by-MsgID |
| `SATcontrol.ino` | SAT control loop, heating curve, boiler state machine, setpoint injection, master enable/disable |
| `SATpid.ino` | PID v3 implementation: proportional, integral, derivative with deadband |
| `SATcycles.ino` | Cycle classification, overshoot detection, anti-cycling |
| `SATpressure.ino` | SAT boiler pressure monitoring, low-pressure warning, trend detection |
| `SATweather.ino` | Open-Meteo weather fetch, outdoor temperature for SAT heating curve |
| `SATble.ino` | ESP32 BLE room-temperature sensor scanner on NimBLE-Arduino (ADR-092), BTHome v2 protocol |
| `MQTTHaDiscovery.cpp` | Generated PROGMEM tables for MQTT Home Assistant auto-discovery (sensors, binary sensors, climate, number entities) |
| `mqtt_discovery_verify.cpp` | Compile-time sanity checks for the discovery tables (no runtime cost) |
| `sensors_ext.ino`, `s0PulseCount.ino`, `OLED.ino` | Dallas DS18B20 temperature sensors, S0 pulse counter, OLED display |
| `Ethernet.ino` | Ethernet runtime probe and failover (only with Ethernet hardware) |
| `safeTimers.h` | Timer macros: `DECLARE_TIMER_SEC()`, `DUE()`, `RESTART_TIMER()` |

The platform abstraction lives outside the sketch, in the project's own Platform library under `src/libraries/Platform/src/`:

| File | Domain |
|------|--------|
| `platform.h` | Dispatcher, included by application code |
| `platform_esp32.h` | ESP32-S3 includes, `platformXxx()` shims, type aliases |
| `boards.h` | Board-specific pin maps and feature flags (`HAS_PIC`, `HAS_DIRECT_OT`, `HAS_ETH_CAPABLE`, `HAS_OLED_CAPABLE`, ...) |

#### Web Assets

The browser SPA lives in `src/OTGW-firmware/data/`. These files are flashed to LittleFS:

| File | Purpose |
|------|---------|
| `index.html` | Single-page application shell (~11 KB; always stream, never load into RAM) |
| `index.js` | Main SPA JavaScript: live OT log, WebSocket client, settings forms |
| `graph.js` | ECharts-based real-time temperature graphs |
| `index.css` | Stylesheet with dark/light theme support |

#### Build Infrastructure

| File / Directory | Purpose |
|-----------------|---------|
| `build.bat` / `build.sh` | Build entrypoints: bootstrap their own Python venv, then run `build.py` |
| `build.py` | Build script behind the wrappers: invokes PlatformIO internally, packages firmware and LittleFS artifacts, produces the distribution zip |
| `flash_otgw.bat` / `flash_otgw.sh` | Flash entrypoints, shipped in the release zip: first USB flash, `--update`, `--migrate` |
| `flash_esp.py` | Python flasher: detects port, writes firmware + filesystem to the ESP32-S3 |
| `platformio.ini` | PlatformIO project: `esp32`, `esp32-classic` and `esp32-combo` environments; `esp32-combo` is the target that is built and shipped |
| `evaluate.py` | Static code quality checker: PROGMEM usage, unsafe patterns, String class audit |
| `scripts/` | PlatformIO pre-build scripts: version injection, library patching |
| `src/libraries/` | The Platform library and vendored Arduino libraries |
| `docs/` | C4 architecture docs, ADRs, API references, feature docs |
| `other-projects/` | Read-only reference codebases (PIC firmware, OTmonitor, SAT) |
| `backlog/` | Project task management (Backlog.md) |

---

### Build System

#### build.bat / build.sh (release builds)

Build with the wrappers: `build.bat` on Windows, `./build.sh` on Linux and macOS. They bootstrap their own Python venv (`.build-python`) and then run `build.py`, which invokes PlatformIO internally and handles version embedding, filesystem packaging and artifact collection. Never call `python build.py` directly: the host default Python may be a version PlatformIO rejects.

```bash
./build.sh                         # Firmware + filesystem, esp32-combo (build.bat on Windows)
./build.sh --firmware              # Build firmware only (skip filesystem)
./build.sh --filesystem            # Build filesystem only
./build.sh --clean                 # Remove build artifacts, keep libdeps cache
./build.sh --distclean             # Remove build artifacts + cores + library cache
./build.sh --no-merged             # Skip the merged binaries (faster dev builds)
./build.sh --compress              # Gzip-compress the merged binary
./build.sh --no-zip                # Skip the distribution zip
./build.sh --no-color              # Disable coloured output
./build.sh --help                  # Show help
```

By default the build produces `esp32-combo`, the target that is built and shipped, including the merged binaries and the distribution zip. The script reads the Git tag to embed the version string and places output in the `build/` directory.

**One build at a time**: `build.py` takes a machine-wide build lock (`%LOCALAPPDATA%/OTGW-build/build.lock`, shared by every worktree), so a second build waits until the first ends. It runs at most 4 compiler jobs.

**esptool**: `build.py` uses esptool v5 for merged-binary creation. It is installed automatically on first use via `pip install esptool` if not already present.

#### PlatformIO

`build.py` drives PlatformIO. Three environments are defined in `platformio.ini`: `esp32` (the base environment), `esp32-classic` and `esp32-combo`; see Build Targets and Boards (2.0.0) below. Do not run `pio run` directly: it bypasses the build lock.

PlatformIO's pre-build scripts in `scripts/` handle version injection (`platformio_version.py`) and library compatibility patching (`patch_pio_libs.py`).

##### ESP32-specific PlatformIO notes

PlatformIO's ctags scanner processes all `.ino` files regardless of `#if` guards and generates forward declarations for every function it finds. In the `esp32` environment (`HAS_PIC=0`), `OTGWSerial.h` is never included, so `OTGWFirmware` (an enum type used as a parameter in `fwreportinfo()`) is unknown at ctags forward-declaration time. The flag `-DOTGWFirmware=int` stubs it out so the generated forward declaration compiles. The actual `fwreportinfo()` body is inside a `#if HAS_PIC` block and is never compiled in that environment. `esp32-classic` and `esp32-combo` include `OTGWSerial.h` and do not use this stub.

#### evaluate.py

Run the code quality checker after making changes to catch common firmware mistakes:

```bash
python evaluate.py                  # Full evaluation (all files)
python evaluate.py --quick          # Essential checks only
python evaluate.py --report         # Generate detailed JSON report
python evaluate.py --output FILE    # Custom report output path (default: evaluation-report.json)
python evaluate.py --verbose        # Show all checks, not just failures
python evaluate.py --no-color       # Disable coloured output
```

The checker performs comprehensive evaluation including:
- Code quality metrics (PROGMEM usage, unsafe patterns, String class audit)
- Build system validation
- Dependency health checks
- Documentation coverage
- Security analysis
- Memory and resource analysis

Key patterns it flags:
- `Serial.print()` calls (must never appear after init)
- String literal not wrapped in `F()` or `PSTR()`
- `String` class usage in non-setup code
- `strcpy`/`sprintf` without bounds (should use `strlcpy`/`snprintf_P`)
- `strncmp_P`/`strstr_P` on binary data (use `memcmp_P`)

#### tools/generate_mqttha_data.py (legacy regenerator)

The MQTT Home Assistant auto-discovery metadata lives in compiled form as `src/OTGW-firmware/MQTTHaDiscovery.cpp`. That file is the **source of truth** and is checked into the repository. It contains structured PROGMEM arrays for sensors, binary sensors, climate, and number entities, keyed by OT message ID.

The text template `mqttha.cfg` that originally generated this file has been archived to `docs/archive/mqttha.cfg` and is no longer part of the build. If you need to regenerate `MQTTHaDiscovery.cpp` from an updated template, restore `mqttha.cfg` to `src/OTGW-firmware/data/mqttha.cfg` and run:

```bash
python tools/generate_mqttha_data.py
```

For normal development, edit `MQTTHaDiscovery.cpp` directly and keep it consistent with the streaming discovery consumer in `MQTTstuff.ino`. The legacy generators `tools/generate_mqttha_progmem.py` and `tools/generate_mqttha_readable.py` are retained for reference but are superseded by `generate_mqttha_data.py`.

The generated file is deliberately placed in its own `.cpp` translation unit to avoid the Xtensa single-TU section/relocation explosion that occurs when large PROGMEM data is placed in the main sketch.

---

### Board Hardware Variants

The supported 2.x hardware is the NodoShop OTGW Classic with a LOLIN S3 Mini or S3 Mini Pro in the Wemos D1 mini socket; the PIC stays in place. Board definitions live in `src/libraries/Platform/src/boards.h`. Each PlatformIO environment sets exactly one `BOARD_*` preprocessor flag in `platformio.ini`, and `boards.h` turns that flag into `HAS_*` capability flags.

**These flags are board-level, not MCU-level.** The presence of a PIC, Ethernet hardware, or a direct OpenTherm interface is a property of the specific Nodoshop PCB, not of the MCU family. Application code tests only the `HAS_*` flags, never the raw `BOARD_*` macros.

#### Feature Flags

Four compile-time flags in `boards.h` describe the hardware capabilities of the board:

| Flag | Type | OTGW Classic + S3 Mini | Meaning |
|------|------|------------------------|---------|
| `HAS_PIC` | `0` / `1` | `1` | Board has a PIC co-processor that handles the OpenTherm electrical bus over UART |
| `HAS_DIRECT_OT` | `0` / `1` | `0` | OT-Direct engine compiled in (used only on hardware without a PIC) |
| `HAS_ETH_CAPABLE` | `0` / `1` | `0` | Ethernet support compiled in (only with Ethernet hardware; the Classic has none) |
| `HAS_OLED_CAPABLE` | `0` / `1` | `1` | Optional SSD1306 128x64 I2C OLED on the Classic I2C header (runtime probe at address `0x3C`) |

The shipped `esp32-combo` image links both OpenTherm engines and picks one at boot; it sets `HAS_RUNTIME_HW_DETECT=1` and gates the PIC-versus-OT-Direct choice at runtime instead of at compile time.

Use these flags in conditional compilation:

```cpp
#if HAS_PIC
  // PIC-specific code: addCommandToQueue(), OTGWSerial, fwreportinfo(), etc.
#endif

#if HAS_DIRECT_OT
  // OTDirect GPIO code: loopOTDirect(), OTDirectMode, etc.
#endif

#if HAS_ETH_CAPABLE
  // Ethernet code: loopEthernet(), state.hw.bEthernetPresent, etc.
#endif
```

---

### Build Targets and Boards (2.0.0)

The `dev` line (firmware 2.0.0) is ESP32-S3-only, built on an async + FreeRTOS stack. The 1.x line continues only on the `otgw-1.x.x` maintenance branch. On 2.0.0 the build is PlatformIO-only and every target is an ESP32-S3.

#### Build Targets (ADR-127)

The two fixed single-engine targets do no runtime hardware detection; each links exactly one OpenTherm engine. The `esp32-combo` target links both and detects the hardware at boot (ADR-127). Three environments are defined in `platformio.ini`:

| Target (`--target`) | Hardware | OpenTherm engine |
|---|---|---|
| `esp32` | No PIC | OTDirect only |
| `esp32-classic` | OTGW Classic + LOLIN S3 Mini | PIC (OTGWSerial over UART) |
| `esp32-combo` | OTGW Classic + LOLIN S3 Mini or S3 Mini Pro, engine picked at boot | Both engines linked in |

`esp32-combo` is the target that is built and shipped, and the only one built on `dev`. Build it with the wrapper, without a target argument:

```bash
./build.sh                             # firmware + filesystem, esp32-combo (build.bat on Windows)
```

`esp32` is the default PlatformIO environment. `esp32-classic` and `esp32-combo` extend it (`extends = env:esp32`), inheriting the platform, board, partition table, `lib_deps` and `build_unflags`, and override only `build_flags` and `lib_ignore`.

#### Combo Boot Detection (ADR-127 / ADR-158 / ADR-201)

The `esp32-combo` binary links both OT engines and selects one at boot. The `iBoardMode` setting picks the mode:

| `iBoardMode` | Mode | Hardware |
|---|---|---|
| `0` | AUTO | re-probe PIC vs OTDirect (and S3 Mini vs S3 Mini Pro) at boot |
| `1` | Force PIC | LOLIN S3 Mini in the Classic socket |
| `2` | OT-Direct mode | only on hardware without a PIC; probes once per boot, and a PIC that answers corrects the mode to `1` or `3` |
| `3` | Force PIC | LOLIN S3 Mini Pro in the Classic socket |

The S3 Mini Pro is distinguished from the plain S3 Mini by its on-board QMI8658C IMU. The Pro reroutes its Classic I2C headers and needs a different Classic pin map; `state.hw.bClassicPro` records the result. AUTO probes for the PIC up to 3 times per boot. A found PIC is persisted as `1` (S3 Mini) or `3` (S3 Mini Pro). A miss runs OT-Direct for that boot only and is never written to `iBoardMode`, so the next boot probes again (ADR-201). Nothing in the firmware writes `2` on its own.

#### PSRAM

Every S3 build defines `BOARD_HAS_PSRAM` and selects the `qio_qspi` memory type (`board_build.arduino.memory_type`). PSRAM is therefore detected at runtime, not per board: `psramFound()` is the single discriminator and drives the PSRAM-aware BLE default (`bleActive()` in `SATble.ino`). A board without PSRAM boots gracefully (BOOT_INIT is off, so `psramInit()` returns false with no abort).

#### Async Web Stack Tuning

The async stack replaces the synchronous `WebServer` + `PubSubClient` path (ADR-123). `platformio.ini` tunes it in the global `[env]` so all three targets inherit the settings:

- `CONFIG_ASYNC_TCP_STACK_SIZE=16384`: a controlled load-ramp A/B proved the old 4096-byte stack overflowed in AsyncTCP's low-heap cleanup path under a concurrent burst, wedging port 80 permanently. 16384 lets the task survive that path so the server recovers once load drops.
- NimBLE is trimmed to a passive-scan observer-only footprint (`ROLE_PERIPHERAL`/`BROADCASTER`/`CENTRAL_DISABLED`, one connection slot, no bonds or CCCDs, 23-byte MTU) to reclaim internal DRAM. SATble only scans (`setActiveScan(false)`); it never connects or advertises.

#### Partition Layout

The ESP32-S3 partition table `partitions_otgw_esp32.csv` is used by every 4 MB target and is a safeboot layout (ADR-191): a 960 KB safeboot updater (factory) at `0x10000`, a 2240 KB main firmware slot (`app0`, ota_0) at `0x100000`, a 768 KB LittleFS (label `spiffs`) at `0x330000`, plus a 64 KB coredump. Firmware OTA over WiFi goes through the safeboot updater; the main firmware never writes its own app slot.

#### Flashing (flash_esp.py)

`flash_esp.py` reads the LittleFS offset from the partition CSV (`_read_fs_offset_from_csv()`) instead of hardcoding it, so the flash offset can never drift from the firmware build. It detects the installed esptool major version and picks the matching subcommand form: esptool v5 uses the hyphenated `write-flash`/`erase-flash`, while v4 and earlier accept only the underscore form. ESP32-S3 full-erase is opt-in (`-e`), so a normal flash preserves NVS (saved WiFi credentials); the merged image carries a fresh partition table and otadata, so it still boots cleanly. Each build is archived (bin/elf/zip) so a later crash backtrace can be decoded with addr2line.

#### Relevant ADRs

| ADR | Topic |
|-----|-------|
| ADR-127 | Single `esp32-combo` binary with runtime PIC/OT-Direct boot detection; the fixed `esp32` and `esp32-classic` targets remain |
| ADR-158 | LOLIN S3 Mini Pro Classic variant (IMU-detected) |
| ADR-201 | Combo detection persists only a positive PIC verdict and re-probes every boot |
| ADR-130 | PIC-UART FreeRTOS task |
| ADR-146 | Streaming JsonEmit writer (ArduinoJson removed) |
| ADR-147 / ADR-149 | Async static-serve config and LWIP TCP-pcb limits |

---

### C4 Architecture Overview

The firmware uses a four-level C4 model documented in `docs/c4/`. Read the relevant level before touching code.

#### Level 1: Context (`c4-context.md`)

The OTGW-firmware sits between the OpenTherm heating bus and the home automation network. External actors are: Room Thermostat, Boiler (via PIC co-processor, or OTDirect in OT-Direct mode), MQTT Broker, Home Assistant, NTP Server, Web Browser, OTmonitor TCP client, Dallas/S0/BLE sensors.

#### Level 2: Container (`c4-container.md`)

The 2.x firmware runs on an ESP32-S3 (LOLIN S3 Mini or S3 Mini Pro) in the NodoShop OTGW Classic. The PIC16F co-processor handles the OpenTherm electrical bus; the ESP32-S3 communicates with it over UART at 9600 baud.

#### Level 3: Component (`c4-component.md`)

Seven components, each with a dedicated doc in `docs/c4/c4-component-*.md`:

| Component | Primary files | Responsibility |
|-----------|--------------|----------------|
| OpenTherm Core | `OTGW-Core.ino`, `OTDirect.ino` | Protocol decode, PIC UART, command queue |
| Network and Connectivity | `networkStuff.ino` | WiFi, NTP, mDNS, Ethernet failover |
| Integration Layer | `MQTTstuff.ino`, `restAPI.ino`, `jsonStuff.ino` | MQTT, REST API, HA discovery |
| Configuration and State | `settingStuff.ino`, `OTGW-firmware.h` | Settings persistence, OTGWSettings/State |
| Smart Thermostat (SAT) | `SAT*.ino`, `SATcontrol.ino`, `SATpid.ino` | Heating curve, PID, cycle management |
| Sensors and Hardware | `sensorStuff.ino` | Dallas, S0, GPIO relay, OLED |
| Web Interface | `webSocketStuff.ino`, `helperStuff.ino`, `data/` | WebSocket, telnet, heap gate, browser SPA |

#### Level 4: Code (`c4-code-*.md`)

Detailed function signatures, line numbers, and inter-file dependencies for every module. Read the relevant `c4-code-*.md` file before modifying any non-trivial function.

---

### Key Architectural Patterns

#### Cooperative Scheduling

The application code runs cooperatively in the Arduino `loop()` task. The firmware itself runs on FreeRTOS (the async web stack, ADR-123, and the PIC-UART task, ADR-130, run in their own tasks), but the main `loop()` calls `doBackgroundTasks()`, which in turn calls all subsystem handlers one after the other. `delay()` blocks that loop and must never be used. Long operations must be broken into state machines checked on each `loop()` iteration.

`feedWatchDog()` services the hardware watchdog. It does not call `yield()` (that line is intentionally commented out to prevent re-entrancy of `doBackgroundTasks()`). The background task re-entrancy that does exist comes from MQTT autoconfig's file-reading loop calling `handleOTGW()` mid-iteration.

#### Static Buffers (ADR-053 / ADR-180)

Dynamic allocation fragments the heap and leads to crashes after days of operation. This rule comes from the 1.x line and still holds on 2.x (ADR-180 for the `String` class). All buffers are statically allocated:

- Use `char buf[SIZE]` with `snprintf_P(buf, sizeof(buf), PSTR(...))`.
- Never use `String` class in hot paths (ISR, MQTT publish, OT processing loop).
- `String` is tolerated only in `setup()` / one-off init code, and only when the ESP SDK forces it (e.g., `ESP.getResetReason()`).
- Key buffer constants: `CMSG_SIZE` = 512 bytes (general scratch), `SLINE_SIZE` = 1200 bytes (MQTT autoconfig line buffer).

#### PROGMEM (ADR-009)

All string literals must live in flash (PROGMEM), not RAM:

```cpp
DebugTln(F("Starting MQTT"));                         // F() macro: flash-resident string
DebugTf(PSTR("Heap: %u bytes\r\n"), freeHeap);        // PSTR(): for printf-style format strings
snprintf_P(buf, sizeof(buf), PSTR("val=%d"), v);      // snprintf_P: format from flash
const char label[] PROGMEM = "boilertemperature";     // PROGMEM constant in flash
```

For comparisons use `strcmp_P(ram_ptr, PSTR("keyword"))` or `strcasecmp_P()`. Never pass a PROGMEM pointer to a function expecting a RAM pointer.

Binary data (non-null-terminated buffers) must use `memcmp_P()`. Using `strncmp_P` or `strstr_P` on binary data causes an Exception 2 crash.

The MQTT path adds a PROGMEM-safety helper in `MQTTstuff.h`: `pgm_strncmp_PP()` (byte-by-byte PROGMEM compare). Use it when the operand lives in flash; never pass a `PGM_P` to `printf("%s", ...)` or `MQTTclient.write()`.

#### Platform Abstraction Layer (ADR-061)

All platform SDK specifics are isolated in the Platform library under `src/libraries/Platform/src/`: `platform.h` (dispatcher), `platform_esp32.h` (shims) and `boards.h` (pin maps and `HAS_*` capability flags). Application code includes only `platform.h`, calls the `platformXxx()` shims and gates optional features with `HAS_*` flags. Never call platform-specific SDK functions directly in `.ino` files, and never write a raw `#ifdef ESP32` outside these files: `evaluate.py` (`check_esp_abstraction_boundary()`) fails on new violations.

Key functions provided by the abstraction layer:

| Function | Description |
|----------|-------------|
| `platformSetHostname(hostname)` | Set the WiFi station hostname for DHCP |
| `platformGetHostname()` | Read back the current hostname |
| `platformFreeHeap()` | Free heap in bytes |
| `platformMaxFreeBlock()` | Largest contiguous free block (fragmentation indicator) |
| `platformRestart()` | Platform-safe reboot |
| `platformCoreVersion()` | Arduino core version string |
| `platformChipId()` | Unique chip identifier |
| `platformGetMacAddress(mac)` | Fill a 6-byte buffer with the MAC address |

`platform_esp32.h` defines `MDNS_NEEDS_UPDATE` as `0`: mDNS runs on a background task, so the main loop does not call `MDNS.update()`. The main loop guards that call with `#if MDNS_NEEDS_UPDATE`.

The `PlatformDir` class in `platform.h` provides a unified directory iteration interface over LittleFS. Use `PlatformDir` instead of calling filesystem APIs directly.

#### No ArduinoJson

Do not add ArduinoJson to this project. JSON is:
- **Built**: using `snprintf_P`, `sendJsonMapEntry()`, and chunked `httpServer.sendContent()`.
- **Parsed**: using the project's own `parseJsonKVLine()` helper (key-value line scanner, sufficient for the flat settings JSON and MQTT command payloads).

#### Heap Backpressure Gate

Two gate functions protect heap health during normal operation:

```cpp
bool canPublishMQTT();     // false when heap is below HEAP_LOW threshold
bool canSendWebSocket();   // false when heap is below HEAP_WARNING threshold
```

Always check the appropriate gate before sending MQTT messages in background tasks. The heap monitor in `helperStuff.ino` defines four levels: `HEAP_HEALTHY` (>8 KB), `HEAP_LOW` (5-8 KB), `HEAP_WARNING` (3-5 KB), `HEAP_CRITICAL` (<3 KB).

#### Settings / State Separation (ADR-051, amended by ADR-079 and ADR-081)

All persistent configuration lives in `OTGWSettings settings` (serialised to `/settings.ini` on LittleFS). All transient runtime state lives in `OTGWState state`. Both structs use two-level named sub-sections with Hungarian prefixes:

```cpp
settings.mqtt.sBroker          // persistent: MQTT broker hostname
settings.sat.fHeatingCurveCoeff // persistent: SAT heating curve coefficient
state.otgw.bOnline             // transient: PIC serial link alive
state.pic.sFwversion           // transient: PIC firmware version string
```

Per ADR-079, each subsystem owns its section structs in a `*types.h` header (for example `SATtypes.h` declares both `SATRuntimeSection` and `SATSection`; `OTDirecttypes.h` declares both `OTDirectSection` and `OTDirectSettingsSection`). `OTGW-firmware.h` now just `#include`s those headers and defines the aggregate `OTGWSettings` / `OTGWState` plus the global `settings` and `state` instances. ADR-081 keeps the rule simple: if a component already has a `<Component>stuff.h` sibling, the types fold into that file instead of creating a separate `<Component>types.h`. Field access is unchanged: still `settings.sat.<field>` / `state.sat.<field>`.

Never persist transient state. Never access `state` fields from `writeSettings()`. When adding a new subsystem prefer a fresh `<Component>types.h` over growing `OTGW-firmware.h`.

---

### Adding a New REST Endpoint

The REST API v2 uses a centralized dispatch table (ADR-050) in `restAPI.ino`. Adding an endpoint requires:

1. Write the handler function.
2. Add one entry to `kV2Routes[]`.
3. No changes to the router.

#### Step 1: Write the Handler

Handler signature:

```cpp
void handleMyFeature(const char words[][API_WORD_LEN], uint8_t wc,
                     HTTPMethod method, const char* originalURI)
```

Parameters:
- `words`: URI tokens after `/api/v2/`, e.g., `words[0]` = `"myfeature"`, `words[1]` = `"status"`.
- `wc`: Word count.
- `method`: `HTTP_GET`, `HTTP_POST`, etc.
- `originalURI`: The full original URI string.

Example handler for `GET /api/v2/myfeature/status`:

```cpp
void handleMyFeature(const char words[][API_WORD_LEN], uint8_t wc,
                     HTTPMethod method, const char* originalURI) {
  if (method != HTTP_GET) {
    httpServer.send(405, F("application/json"),
                    F("{\"error\":{\"status\":405,\"message\":\"Method Not Allowed\"}}"));
    return;
  }
  // Require auth for all endpoints that expose sensitive data
  if (!checkHttpAuth()) return;

  char buf[CMSG_SIZE];
  snprintf_P(buf, sizeof(buf),
    PSTR("{\"myvalue\":%d,\"status\":\"ok\"}"),
    myGlobalValue);
  httpServer.send(200, F("application/json"), buf);
}
```

Rules:
- Never use `Serial.print()` for debug. Use `DebugTln()` or `DebugTf()`.
- Use `CMSG_SIZE` (512 bytes) as the standard response buffer unless you need a larger response.
- For responses larger than `CMSG_SIZE`, use `httpServer.sendContent()` in a chunked loop with `httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN)`.
- All mutation endpoints (POST, PUT, DELETE) must call `checkHttpAuth()` and return immediately if it returns false.
- Use `sendApiError(httpCode, F("message"))` to return consistent JSON error responses.
- File serving must use `httpServer.streamFile()` for any file larger than 2 KB.

#### Step 2: Register in the Dispatch Table

Locate `kV2Routes[]` in `restAPI.ino` (search for `kV2Routes`). Add one entry:

```cpp
{ PSTR("myfeature"), handleMyFeature },
```

The first field is the PROGMEM resource name matching the first URI token after `/api/v2/`. The second is the handler function pointer. Order in the table does not affect routing.

That is all. The router in `processAPI()` matches `words[0]` against the table and dispatches automatically.

---

### Adding a New MQTT Topic

#### Publish Side

All MQTT publishing flows through `sendMQTTData()` in `MQTTstuff.ino`:

```cpp
void sendMQTTData(const char* topic, const char* value, const bool retain = false);
void sendMQTTData(const __FlashStringHelper* topic, const char* value, const bool retain = false);
void sendMQTTData(const __FlashStringHelper* topic, const __FlashStringHelper* value,
                  const bool retain = false);
```

The function prepends the configured topic namespace (`{TopTopic}/value/{UniqueId}/`) automatically. It also checks `canPublishMQTT()` before sending.

Example, publish a new float value:

```cpp
if (canPublishMQTT()) {
  char payload[16];
  dtostrf(myFloatValue, 0, 2, payload);
  sendMQTTData(F("mynewtopic"), payload, true);  // retain=true for sensor values
}
```

For source-separated topics (when `settings.mqtt.bSeparateSources` is enabled), use `publishToSourceTopic()` instead:

```cpp
publishToSourceTopic(label, valueStr, sourceFlag);  // sourceFlag: 'T', 'B', or 'A' (Answer)
```

For periodically published values, call `sendMQTTData()` inside a `DUE()` check so you do not flood the broker.

#### Subscribe Side

The firmware subscribes to `{TopTopic}/set/{UniqueId}/#` on MQTT connect. Incoming messages are dispatched in `mqttCallback()` in `MQTTstuff.ino`. The dispatch table is `setcmds[]`, an array of `MQTT_set_cmd_t` structs (ADR-078):

```cpp
const MQTT_set_cmd_t setcmds[] PROGMEM = {
  { PSTR("setpoint"),    PSTR("TT"), PSTR("temperature") },
  { PSTR("mycommand"),   PSTR("XX"), PSTR("raw")         },  // new entry
  // ...
};
```

Fields:
- `setcmd`: The MQTT topic suffix (what comes after `{UniqueId}/`).
- `otgwcmd`: The two-character OTGW PIC command code sent to `addCommandToQueue()`.
- `ottype`: Value type: `"temperature"`, `"on/off"`, `"level"`, `"raw"`, or `"int"`.

For commands that do not map to a PIC command (e.g., SAT-specific topics), add a separate handler in `mqttCallback()` before the table dispatch. SAT command topics are handled this way.

#### Home Assistant Auto-Discovery

HA discovery payloads are built at runtime from compile-time PROGMEM tables in `MQTTHaDiscovery.cpp`. The streaming discovery emitter in `MQTTstuff.ino` (`doAutoConfigure()` / `doAutoConfigureMsgid()`) walks those tables and streams each discovery message directly to the broker without buffering a full payload in RAM. `MQTTHaDiscovery.cpp` is the source of truth; the older `mqttha.cfg` template has been archived to `docs/archive/`.

Discovery is **just-in-time** (ADR-100): the OT-specific configs are only published once the matching MsgID is actually seen on the bus, with a broker-restart heuristic gating republish on reconnect. Topics themselves follow the **flat per-value** shape (ADR-101): one MQTT topic per value, never aggregated JSON. ADR-106 governs the default topic naming (self-describing); the legacy aggregated shape stays reachable via the `bUseLegacyOtTopics` toggle.

To add a new discoverable entity, append a new entry to the appropriate PROGMEM array (sensors, binary sensors, climate, number) in `MQTTHaDiscovery.cpp`, using the existing entries as templates. Fields that vary per entity (device class, unit, state class, icon, entity category) are encoded as enum values to keep flash usage bounded. Rebuild firmware and force discovery (press `F` on the telnet debug console) to republish the new entity.

---

### Adding a New Setting

Adding a setting requires changes in four places: the struct definition, the serializer, the deserializer/validator, and (optionally) the web UI.

#### Step 1: Add to the Settings Struct

Open the relevant per-component header (for SAT: `SATtypes.h`; for OTDirect: `OTDirecttypes.h`; for the HA device identity: `Devicetypes.h`; new subsystems get a fresh `<Component>types.h`). Find the section struct (`SATSection`, `OTDirectSettingsSection`, `DeviceSection`, ...) and add the new field with its type and default value:

```cpp
struct MySection {
  bool   bMyNewFlag   = false;     // b prefix for bool
  char   sMyString[32] = "";       // s prefix for char array
  int    iMyInteger   = 0;         // i prefix for int
  float  fMyFloat     = 1.0f;      // f prefix for float
};
```

If adding to an existing section, add to the appropriate struct. If creating a new section, add a new `<Component>types.h` containing the struct, include it from `OTGW-firmware.h`, and reference it from `OTGWSettings`:

```cpp
// OTGW-firmware.h
#include "Mytypes.h"

struct OTGWSettings {
  // ... existing sections ...
  MySection my;   // access as settings.my.bMyNewFlag
};
```

#### Step 2: Add to writeSettings()

In `settingStuff.ino`, find `writeSettings()`. Add the serialization call in the appropriate section block:

```cpp
writeJsonBoolKV(f, PSTR("mynewflag"), settings.my.bMyNewFlag);
writeJsonStringKV(f, PSTR("mystring"), settings.my.sMyString);
writeJsonIntKV(f, PSTR("myinteger"), settings.my.iMyInteger);
writeJsonFloatKV(f, PSTR("myfloat"), settings.my.fMyFloat, 2);  // 2 decimal places
```

#### Step 3: Add to updateSetting()

In `settingStuff.ino`, find `updateSetting()`. Add a case for the new field. Use `strcasecmp_P` for case-insensitive matching:

```cpp
else if (strcasecmp_P(field, PSTR("mynewflag")) == 0) {
  settings.my.bMyNewFlag = EVALBOOLEAN(newValue);
  settingsDirty = true;
}
else if (strcasecmp_P(field, PSTR("mystring")) == 0) {
  strlcpy(settings.my.sMyString, newValue, sizeof(settings.my.sMyString));
  settingsDirty = true;
  // If this setting requires a service restart, set the side-effect bit:
  pendingSideEffects |= SIDE_EFFECT_MQTT;  // or SIDE_EFFECT_NTP, SIDE_EFFECT_MDNS
}
```

The `settingsDirty` flag triggers a deferred LittleFS write (typically after a 2-second debounce timer). `writeSettings()` skips the actual flash write when the serialised content matches the on-disk copy, so a no-op `updateSetting()` round-trip does not wear the flash.

#### Step 4: Expose via REST API (Optional)

`GET /api/v2/settings` returns all settings. The response is built in `sendDeviceSettings()`. Add the new field to the JSON output there. For `POST /api/v2/settings`, the existing `postSettings()` function iterates the JSON body and calls `updateSetting()` for each key-value pair, so new settings are automatically accepted without code changes in the POST handler.

#### Step 5: Web UI Field (Optional)

Settings form fields live in `data/index.js`. Add an entry to the settings form configuration object following the existing pattern. For a boolean toggle:

```javascript
{ key: 'mynewflag', label: 'My New Feature', type: 'checkbox', section: 'device' }
```

For a text field:

```javascript
{ key: 'mystring', label: 'My String Value', type: 'text', section: 'device', maxlength: 31 }
```

---

### Coding Conventions

#### Naming

| Kind | Convention | Example |
|------|-----------|---------|
| Variables (local, member) | camelCase | `lastReset`, `msgCount` |
| Global settings fields | `setting` prefix + camelCase | `settingMqttBroker` (legacy), `settings.mqtt.sBroker` (current) |
| Constants and `#define` | UPPER_CASE | `CMSG_SIZE`, `ON`, `NTP_HOST_DEFAULT` |
| Functions | camelCase | `startMQTT()`, `processOT()`, `handleOTGW()` |
| Enums (class) | PascalCase type, UPPER_CASE values | `HeapHealthLevel::HEAP_HEALTHY` |
| Struct types | PascalCase + `Section` suffix | `MQTTSection`, `SATSection` |

#### Safety Rules

- Never write to `Serial` after the boot sequence. The UART is the PIC serial link. Use `DebugTln(F("..."))` instead.
- Never send OTGW commands directly to the serial port. Always use `addCommandToQueue(cmd, len)`.
- Always validate buffer sizes before string operations. Use `strlcpy()`, `snprintf_P()`, never `strcpy()` or `sprintf()`.
- Do not write raw platform conditionals (`#ifdef ESP32` and the like) in application code. Call a `platformXxx()` shim or gate on a `HAS_*` flag; `evaluate.py` (`check_esp_abstraction_boundary()`) enforces this.
- Use `enum class` for internal discriminators, not string tokens.

---

### Debug Tools

#### Telnet Debug Console (Port 23)

The firmware streams diagnostic output to Telnet port 23 via the SimpleTelnet library (`SimpleTelnet<1> debugTelnet`, declared extern in `OTGW-firmware.h` and defined in `networkStuff.ino`). SimpleTelnet replaces the older TelnetStream and ESPTelnet libraries. Connect with any Telnet client:

```bash
telnet otgw2.local 23
# or by IP:
telnet 192.168.1.x 23
```

Output includes OpenTherm message decoding, MQTT state transitions, heap stats, sensor readings, and any text output from `DebugTln()`/`DebugTf()`. The stream is plain text, one line per event. Each line is prefixed with timestamp, free heap, and max free block size.

When you connect, a banner is printed listing all available debug toggle keys. Press `h` to redisplay the full menu. The current key bindings are:

| Key | Action |
|-----|--------|
| `1` | Toggle OT message parsing log (`state.debug.bOTmsg`) |
| `2` | Toggle REST API handler log (`state.debug.bRestAPI`) |
| `3` | Toggle MQTT module log (`state.debug.bMQTT`) |
| `4` | Toggle sensor module log (`state.debug.bSensors`) |
| `5` | Toggle SAT control loop log (`state.debug.bSAT`) |
| `6` | Toggle OTDirect frame log (`state.debug.bOTDirect`) |
| `g` | Toggle MQTT interval gating log (`state.debug.bMQTTGate`) |
| `d` | Toggle Dallas sensor simulation (`state.debug.bSensorSim`) |
| `s` / `S` | Toggle OTGW serial simulation replay |
| `r` | Reconnect WiFi, Telnet, OTGW stream, MQTT |
| `F` | Force MQTT HA discovery for all message IDs |
| `q` | Force re-read of settings from LittleFS |
| `p` | Reset PIC manually |
| `h` | Print the full debug help menu |

#### Debug Macros

```cpp
DebugTln(F("Message text"));                  // Print with newline, string in flash
DebugTf(PSTR("Value: %d\r\n"), myInt);        // Printf-style, format string in flash
DebugT(F("Partial: "));                       // Print without newline
```

The macros send output to the `debugTelnet` instance (a `SimpleTelnet<1>` on port 23). They are never written to `Serial`. Module-specific conditional macros follow a per-module flag in `state.debug`:

| Flag | Module macros | Description |
|------|--------------|-------------|
| `state.debug.bOTmsg` | `OTDebugTln()`, `OTDebugTf()` | OpenTherm message parsing |
| `state.debug.bRestAPI` | `RESTDebugTln()`, `RESTDebugTf()` | REST API handler logging |
| `state.debug.bMQTT` | `MQTTDebugTln()`, `MQTTDebugTf()` | MQTT client and publish/subscribe |
| `state.debug.bMQTTGate` | (inline checks) | MQTT interval gating decisions |
| `state.debug.bSensors` | `SensorDebugTln()`, `SensorDebugTf()` | Dallas, S0, sensor polling |
| `state.debug.bSAT` | `SATDebugTln()`, `SATDebugTf()` | SAT control loop, cycles, heating curve |
| `state.debug.bOTDirect` | `OTDDebugTln()`, `OTDDebugTf()` | OTDirect frame handling (OT-Direct mode) |

#### Browser Console Debug

When `window.otgwDebug = true` is set in the browser console before page load, the JavaScript SPA logs verbose diagnostics. The SAT subsystem has a separate flag: `window.SAT_DEBUG = true`.

#### evaluate.py

After making changes, always run:

```bash
python evaluate.py
```

This checks for the patterns most likely to cause crashes on the device. Address all warnings before submitting a pull request.

---

### Timer Management

The firmware uses the `safeTimers.h` macros for all periodic tasks. These are overflow-safe and handle the 49-day `millis()` rollover correctly.

#### Declaring a Timer

```cpp
DECLARE_TIMER_SEC(timerMQTTpublish, 30, SKIP_MISSED_TICKS);   // fire every 30 seconds
DECLARE_TIMER_MS(timerLedBlink, 500, CATCH_UP_MISSED_TICKS);  // fire every 500 ms
DECLARE_TIMER_MIN(timerNTPsync, 30, SKIP_MISSED_TICKS);       // fire every 30 minutes
```

Timer type options:
- `SKIP_MISSED_TICKS`: Advance `due` once past the current tick. Use for most periodic tasks.
- `CATCH_UP_MISSED_TICKS`: Advance `due` one interval at a time until current. Use for tasks that must account for every missed cycle.
- `SKIP_MISSED_TICKS_WITH_SYNC`: O(1) sync then skip. For tasks that benefit from phase synchronization.

#### Checking and Firing

```cpp
if (DUE(timerMQTTpublish)) {
  // publish periodic MQTT data
}
```

`DUE()` returns `true` (the current `millis()` value) when the interval has elapsed, and `false` otherwise. It also updates the timer's `_due` field.

#### Modifying a Timer

```cpp
CHANGE_INTERVAL_SEC(timerMQTTpublish, 60, SKIP_MISSED_TICKS);  // change to 60 seconds
RESTART_TIMER(timerMQTTpublish);                                 // reset countdown from now
```

---

### Command Queue

All commands to the OTGW PIC must go through `addCommandToQueue()`. Direct UART writes are forbidden because the PIC requires response matching and command ordering. In OT-Direct mode (hardware without a PIC), the same API funnels into the OTDirect queue, which **coalesces by MsgID** (TASK-494): if a frame for the same OT MsgID is already pending, the older entry is replaced in place, so multiple producers issuing the same MsgID within one OT cycle (~115 ms) never flood the bus.

```cpp
// Signature:
void addCommandToQueue(const char* buf, int len,
                       const bool forceQueue = false,
                       const int16_t delay = 1000);   // inter-command send delay, milliseconds

// Examples:
addCommandToQueue(PSTR("TT=21.50"), 8);            // temporary temperature setpoint
addCommandToQueue(PSTR("CS=55.00"), 8, true);      // force-queue: bypass deduplication
addCommandToQueue(PSTR("GW=1"), 4);                // gateway mode on
addCommandToQueue(PSTR("PR=A"), 4, false, 500);    // custom 500 ms send delay
```

The queue is processed in the OTGW-Core background task. Responses from the PIC are matched against outstanding commands and published to MQTT. In OT-Direct mode, the queue is handled internally by `loopOTDirect()`, which also exposes a high-water-mark diagnostic on the telnet console.

Never call `addCommandToQueue()` from an ISR or from within the PIC response handler.

---

### OpenTherm Message Processing Flow

Understanding this pipeline is essential before modifying any OT message handling:

```
Boiler/Thermostat
       |
  OpenTherm bus (2-wire)
       |
  PIC co-processor (PIC gateway mode) or OTDirect engine (OT-Direct mode)
       |
  UART 9600 baud ASCII line (e.g., "BA000001\r")   [PIC gateway mode]
  or decoded OT frame struct                        [OT-Direct mode]
       |
  handleOTGW() / OTGW-Core.ino
    - reads serial line byte by byte into sRead[256] (static)
    - on CR: dispatches to processOT()
       |
  processOT(buf, len) / OTGW-Core.ino:3689
    - parses frame type prefix (T=thermostat, B=boiler, R=response, A=answer, E=error)
    - extracts 8-hex-digit data word
    - decodes message ID (bits 23-16)
    - decodes message type (bits 31-28)
    - extracts typed value via OTValueType (f8.8, s16, u16, flag8, status, datetime, ...)
    - updates OTcurrentSystemState (the global OTdataStruct)
    - checks mqttlastsent[msgId] throttle timestamp
    - calls sendMQTTData(label, value) if throttle allows
    - calls sendEventToWebSocket(frame) for every frame
    - calls publishToSourceTopic() when source-separated topics enabled
       |
  sendMQTTData(topic, value) / MQTTstuff.ino
    - checks canPublishMQTT()
    - prepends {TopTopic}/value/{UniqueId}/ namespace
    - calls PubSubClient.publish()
       |
  MQTT broker
       |
  Home Assistant / automation clients
```

The `sRead[256]` buffer in `handleOTGW()` is a `static` local, making it re-entrancy-safe. The function can be called from both the main loop and from within MQTT autoconfig's file-reading yield points.

---

### Architecture Decision Records

ADRs live in `docs/adr/`. There are 100+ ADRs covering every significant design decision. ADRs are immutable once Accepted.

#### When to Read ADRs

Before: modifying architecture, changing API contracts, adding dependencies, touching build tooling.

Not required for: pure bug fixes, minor features within existing patterns.

#### Key ADRs to Know

| ADR | Topic | Impact |
|-----|-------|--------|
| ADR-003 | HTTP/WS only, no HTTPS/WSS | Plain HTTP and WebSocket only; trusted LAN security model |
| ADR-009 | PROGMEM string literals | All literals in flash via `F()` / `PSTR()` |
| ADR-016 | OpenTherm command queue | Always use `addCommandToQueue()` |
| ADR-019 | REST API versioning | `/api/v2/` is current; v0/v1 return 410 Gone |
| ADR-028 | File streaming | Files >2 KB must use `streamFile()`, never load |
| ADR-040 | MQTT source-specific topics | Source separation topic structure |
| ADR-049 | String prohibition on protocol paths | No `String` class on MQTT/REST/WebSocket hot paths |
| ADR-050 | Centralized API dispatch | Single `kV2Routes[]` table; no per-route router changes |
| ADR-051 | Dual encapsulating structs | `OTGWSettings` / `OTGWState` architecture |
| ADR-053 | Large feature buffer allocation | Static allocation for large per-feature buffers |
| ADR-061 | Platform abstraction | Use `platform.h` abstractions, not direct SDK calls |
| ADR-064 | OTDirect operating-mode architecture | Gateway / Monitor / Thermostat / Off modes in OT-Direct mode |
| ADR-079 | Per-component type headers | Each subsystem owns its state + settings structs in `<Component>types.h` |
| ADR-081 | Types merge into `<Component>stuff.h` | Amendment to ADR-079: fold into the existing stuff sibling when one exists |
| ADR-083 | PlatformIO as build system | `platformio.ini` is the build configuration; on 2.x build through `build.bat` / `./build.sh` |
| ADR-090 | Re-entrancy guard pattern for shared scratch buffers | `MQTTAutoConfigSessionLock` and the `inUse` flag on `publishToSourceTopic` |
| ADR-092 | NimBLE-Arduino for BLE scanner | SAT BLE scanner uses NimBLE-Arduino 2.x instead of Bluedroid |
| ADR-101 | Flat per-value MQTT topics | One topic per value; no aggregated JSON payloads |
| ADR-106 | Self-describing MQTT topic names | New default topic naming; legacy shape behind `bUseLegacyOtTopics` |
| ADR-146 | Streaming JsonEmit writer | No ArduinoJson; REST JSON goes through the `JsonEmit` streaming writer (`jsonEmit.h`) |
| ADR-174 | Republish on-change MQTT state when Home Assistant comes back online | `offline` to `online` on `homeassistant/status` calls `requestMQTTRepublishAll()`; JIT discovery stays as in ADR-100 |
| ADR-180 | Keep the `String` class out of hot-path source files | Guideline-level; `evaluate.py` reports WARN with the call site |
| ADR-197 | SAT PID v3 controller | Current PID control law for Smart Autotune Thermostat, following thermo-nova `pid.py` |

#### Creating a New ADR

Create a new ADR when your change: introduces a new dependency, modifies an API contract, changes the build system, or shifts architectural boundaries. Use the format:

```
# ADR-NNN-Short-Title
## Status: Proposed
## Context
## Decision
## Consequences
## Related
```

To reverse an Accepted ADR: create a new ADR that supersedes it and mark the old one "Superseded by ADR-NNN".

---

### Contributing Workflow

1. Fork the repository on GitHub.
2. Create a feature branch from `dev`: `git checkout -b feature/my-feature dev`.
3. Search for an existing backlog task: `backlog search "my topic" --plain`. Create one if absent.
4. Set the task in progress and assign to yourself: `backlog task edit <id> -s "In Progress" -a @yourname`.
5. Write an implementation plan in the task before writing code.
6. Implement the change. Mark acceptance criteria done as you go.
7. Run `python evaluate.py` and fix all reported issues.
8. Build and confirm no compile errors: `build.bat` (Windows) or `./build.sh` (Linux/macOS), which builds `esp32-combo`.
9. Test on real hardware if possible. When hardware is unavailable, use the OTGW serial-simulation replay (`s` / `S` on the telnet debug console, backed by `state.debug.bOTGWSimulation`) or the Dallas sensor simulation (`d`).
10. Write a final summary in the backlog task.
11. Open a pull request against the `dev` branch, not `main`.
12. Include the backlog task ID in the PR description.

Pull requests to `main` are not accepted directly. All merges to `main` go through `dev` and are coordinated by the maintainer.
