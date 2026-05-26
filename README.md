# OTGW-firmware (ESP8266) for NodoShop OpenTherm Gateway

> ⚠️ **This is the development branch (`dev`)**: the 1.5.x maintenance line, currently tracking the next stable release as **1.6.0** prereleases (`1.6.0-beta.N`).
> For the current stable release, see the [`main` branch](https://github.com/rvdbreemen/OTGW-firmware/tree/main) or the [v1.5.0 release](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.5.0).

[![Join the Discord chat](https://img.shields.io/discord/812969634638725140.svg?style=flat-square)](https://discord.gg/zjW3ju7vGQ)

This repository contains the **ESP8266 firmware for the NodoShop OpenTherm Gateway (OTGW)**. It runs on the ESP8266 "devkit" that is part of the NodoShop OTGW and turns the gateway into a standalone network device.

## What's new on dev (since v1.5.0-fix2)

Dev currently builds as `1.6.0-beta.N` (latest cut: `1.6.0-beta.22`). The list below summarises the user-visible changes that have landed on `dev` since the last public stable, [v1.5.0-fix2](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.5.0-fix2). Field testers can flash these builds from the [Releases page](https://github.com/rvdbreemen/OTGW-firmware/releases) (look for the most recent `v1.6.0-beta.*` prerelease).

**MQTT and Home Assistant discovery**
- **HA availability now reflects the MQTT link, not the OpenTherm bus** (ADR-074, regression fix). Entities like `DHW Control` and `Thermostat` no longer flap `unavailable` when the boiler stops talking; OT-bus liveness lives on the dedicated `otgw_connected` sensor. **Contract change:** consumers reading the base `<toptopic>/value/<nodeid>` topic as OT-bus liveness must migrate to `otgw_connected`.
- **Pure JIT MQTT discovery** (ADR-073, supersedes ADR-041): only non-OT pseudo-IDs queue at boot; OT MsgID discovery configs publish on first MsgID reception. Stalled-discovery edge case fixed by aligning the JIT trigger with the force-path `hasConfig` filter.
- **Proxy-answer (no-B) routing fix** (ADR-075): MsgIDs without a boiler response now route to the correct worldview topic instead of going silent.
- **MsgID 0 Status canonical publish gated on boiler-side worldview** so the canonical topic no longer flaps on thermostat-only frames.
- **HA PIC-control entities**: new `button` and `select` discovery configs under pseudo-ID 251 expose the PIC reset and mode controls as proper HA entities.
- **Standalone HA discovery topic wiper**: one-shot helper for cleaning stale retained discovery topics out of the broker (TASK-611).
- **HA capability-flag binary sensors for bits 2-5 no longer stuck at `unknown`** (ADR-076, PR #614): the global MQTT status fanout rate gate suppressed per-bit publishes on subsequent MsgID 5 frames; the rate gate is dropped and the per-bit publish is scoped to all three pending types so cooling, OTC active, CH2 active, and summer/winter all reach their retained topics on every status change.

**Settings and networking**
- **Static IP address support** (TASK-548): configure a fixed IP, subnet, gateway, and up to two DNS servers in the firmware settings. Persisted across reboots and applied before WiFiManager connects, so the device lands on a predictable address every time.

**Web UI and diagnostics**
- **Statistics table drag-to-resize columns** (TASK-703): grab any column header edge in the Statistics tab to resize it. Width preferences are saved in localStorage and survive page reloads.
- **LittleFS size display fixed** (TASK-701): the device-info API and Web UI were showing 1 MB filesystem instead of the correct 2 MB; fixed by reading the partition size from the LittleFS descriptor.
- **OT log auto-scroll preserved** (TASK-701): switching tabs or navigating back to the main page no longer resets the scroll position in the OT log.
- **Statistics column proportions and badge styling refined** (TASK-705, TASK-706): column widths are better balanced after the support-map feature landed; the "boiler unsupported" badge is visually consistent.
- **Bilateral OT-bus support map** (TASK-686, PR #640): the gateway now tracks which OpenTherm MsgIDs are seen from the thermostat side, the boiler side, or both. The telnet view labels each data point "T / B / T+B"; a new `GET /api/v2/otgw/support-map` endpoint exposes the bitmaps; the Web UI shows which data points your system is actually exchanging.
- **Heap drop counters now show lifetime totals** (TASK-697, PR #642): the per-minute `logHeapStats` line was showing window-scoped drop counters (which reset after each throttle warning) instead of the monotonic lifetime totals. Now consistent with MQTT stats topics and `/api/v2/heap`.
- **Telnet diagnostic noise reduced** (TASK-683, PR #640): `onNotFound` now logs accurate HTTP status; the firmware-file-list API no longer mirrors its JSON to telnet; `checklittlefshash` is silent on a hash match.
- **FSexplorer "Update Firmware" button** is visible again on touch-capable desktops; the touch-class media query no longer hides the upload control.
- **Set-command debug surfacing**: silently-dropped set-commands now appear in the default debug stream instead of being swallowed.

**Tooling and build**
- **Flash scripts hardened**: `flash_otgw.sh` / `flash_otgw.bat` now mirror spec parity, verify SHA256 integrity, and pick the binary that matches the requested version. The `.bat` variant detects COM ports through the Windows registry and auto-downloads binaries when not found locally.
- **`build.py` auto-initialises missing git submodules** so a fresh clone or a stale checkout builds without manual `git submodule update`.
- **`evaluate.py`** false-positive and stale checks fixed; the gate is now meaningful again.
- **`/beta-prerelease` skill + GitHub Action** for tag-driven (and, after #609, workflow-dispatch-driven) beta publishing, with draft-first asset attachment to satisfy GitHub's immutable-releases policy. The release body now inlines a "What's new since the last public release" digest sourced from `RELEASE_NOTES_<base>-beta.md` above a `<!-- digest:end -->` sentinel, and the `/beta-prerelease` skill gates the README + CHANGELOG staleness check before the version bump (#612).

**Performance**
- **Mainloop responsiveness audit** (TASK-651, TASK-652, PR #617): all blocking `delay()` / `delayMs()` calls on the cooperative path replaced with non-blocking timer checks so `doBackgroundTasks()` keeps running at full cadence under load.
- **Mainloop Tier-1 follow-up** (TASK-671, PR #626): `handleOTGW()` drain loops are now bounded (max 4 lines per call) so a noisy PIC cannot starve the rest of the loop; the dead `executeCommand` path is removed; a stray `delay(1)` on the cooperative path is replaced with a `yield()`.
- **Mainloop Tier-1 follow-up #2** (TASK-673, PR #633): `String` usage removed from hot paths in `helperStuff.ino` / `webhook.ino`; `emergencyHeapRecovery()` is now a real recovery routine (drops the OTGWstream client and pauses the discovery drip for one tick when heap is critical, see ADR-079); the always-on `BGTRACE` instrumentation is dropped from production builds.
- **Mainloop Tier-2 dispositions** (TASK-674, PR #635): webhook HTTP timeout tightened from 1000 ms to 500 ms; the existing webhook retry state machine absorbs the slack. The remaining synchronous blocker, `MQTTclient.connect()` with a 15 s socket timeout, is documented and accepted in ADR-080 (15 s worst-case stall every 42 s during a broker outage; bounded `handleOTGW()` drains keep the PIC serial path safe).

**Code hygiene**
- **Dead and orphaned code paths cleaned out of `dev`** (#586, #589): inactive subsystem code and its matching scaffolding in `OTGW-firmware.h` removed, since neither is reachable on the 1.5.x / 1.6.x line.

**Documentation**
- New integration guides for **openHAB** and **Domoticz**.
- New Dutch beginner guide for cleaning up stale MQTT topics in MQTT Explorer.
- PIC and ESP firmware guides split into EN/NL language variants, with PIC guide scope restored and ESP-flash docs routed to `FLASH_GUIDE.md`.
- `MQTT_STALE_TOPICS_CLEANUP.md`: added a "Recovering missing HA entities" section distinguishing JIT progressive appearance from upgrade stale-topic cleanup.
- API and ADR docs refreshed mid-cycle: `docs/api/MQTT.md` documents the boot vs. JIT discovery split per ADR-073, `docs/api/README.md` corrects the `/discovery/verify` endpoint description, and `docs/adr/README.md` gains the ADR-041 (Superseded) and ADR-073 entries.
- Release-notes housekeeping: `RELEASE_NOTES_1.5.0.md` moved out of the repo root into `docs/releases/`; older `1.3.3` and `1.3.4` notes archived under `docs/releases/archive/`.
- Documentation link paths normalised; markdown link-validation guardrail introduced (#573) and its CI scope extended to `docs/guides/` and `docs/process/` in `.github/workflows/evaluate.yml` (#581) to keep documentation cross-links honest.

Full per-commit detail lives in [`CHANGELOG.md`](CHANGELOG.md) under `## [Unreleased]`. Architectural rationale lives in the linked ADRs under [`docs/adr/`](docs/adr/).

## What's New in v1.5.0

v1.5.0 is the first stable release of the `1.5.x` long-term-support line on **Arduino Core 2.7.4**. It ships 29 beta builds worth of fixes, MQTT improvements, and Home Assistant discovery refinements.

- **Sibling-suffix MQTT topic shape (ADR-070/071)**: `TSet_thermostat` / `TSet_boiler` instead of `TSet/thermostat` / `TSet/boiler` — source-variant entities now actually register in HA for the first time.
- **Worldview semantics for /thermostat and /boiler (ADR-069)**: each sub-topic reflects the correct actor perspective.
- **Human-readable HA discovery friendly names (ADR-072)**: entity names use spaces and Title Case; `unique_id` unchanged so automations are unaffected.
- **HA discovery for diagnostic topics**: PIC and firmware metrics publish proper HA sensors.
- **GET /api/v2/debug**: one-call diagnostic dump for troubleshooting and field support.
- **MQTT topic flapping fixed (ADR-066)**: base topic uses Read-Ack and Write-Data only; `bSlaveEchoesValue` per-MsgID flag gates the boiler echo path.
- **TSet flip for heat-pump stability**: MsgID 1 `bSlaveEchoesValue=false` stops flapping on non-echoing boilers.
- **No-Python flash and build scripts**: `flash_otgw.sh` / `flash_otgw.bat` and `build.sh` / `build.bat`.
- **Arduino Core 2.7.4 baseline**: partition layout retained at `eesz=4M2M` — no filesystem partition reformat needed when upgrading from v1.4.1.

Full release notes: [RELEASE_NOTES_1.5.0.md](docs/releases/RELEASE_NOTES_1.5.0.md)  
Breaking changes: [docs/BREAKING_CHANGES.md](docs/BREAKING_CHANGES.md)

## Latest stable release: v1.5.0-fix2

`v1.5.0-fix2` is the current stable release tag on `main`. It keeps the `v1.5.0` Arduino Core 2.7.4 baseline and includes maintenance fixes on top of the stable 1.5.0 release.

Full release notes: [RELEASE_NOTES_1.5.0.md](docs/releases/RELEASE_NOTES_1.5.0.md)  
Maintenance release tags: [v1.5.0-fix](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.5.0-fix), [v1.5.0-fix2](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.5.0-fix2)

## Previous stable release: v1.4.1

`v1.4.1` was the previous stable release on the Arduino Core 3.1.2 line. Highlights: SimpleTelnet migration, MQTT HA discovery streaming rewrite (309 configs across 80+ msgIds), WiFi reconnect hardening after router reboot, heap-aware discovery drip with fragmentation gates, retained-discovery self-heal ([ADR-062](docs/adr/ADR-062-retained-discovery-verification.md)), unified time-boundary dispatcher ([ADR-064](docs/adr/ADR-064-time-boundary-single-caller-contract.md)), OpenTherm v4.2 alignment fixes.

> **Upgrade warning for v1.4.1**: the Arduino Core 3.1.2 upgrade changed the LittleFS partition from 1 MB to 2 MB. Flash the **filesystem binary first** (`*.littlefs.bin`), then the firmware binary, to preserve your settings. Reverse order triggers a 5-10 minute partition reformat on boot and all settings are lost. Full procedure in the [v1.4.1 release notes](docs/releases/RELEASE_NOTES_1.4.1.md).

Full release notes: [docs/releases/RELEASE_NOTES_1.4.1.md](docs/releases/RELEASE_NOTES_1.4.1.md).

---

## What was new in v1.3.4

Version 1.3.4 fixes MQTT throttle slot suppression, adds Debug Info tooltips, renames "OTGW Connected" to "OpenTherm Active", and adds thermostat-only MQTT support. Full release notes: [RELEASE_NOTES_1.3.4.md](docs/releases/archive/RELEASE_NOTES_1.3.4.md)

## What was new in v1.3.3

Version 1.3.3 adds PIC-less OTGW support and fixes the dashboard showing empty values for unsupported OpenTherm message IDs. Full release notes: [RELEASE_NOTES_1.3.3.md](docs/releases/archive/RELEASE_NOTES_1.3.3.md)

## What was new in v1.3.2

Version 1.3.2 fixes the persistent file explorer failures reported after v1.3.1. Full release notes: [RELEASE_NOTES_1.3.2.md](docs/releases/archive/RELEASE_NOTES_1.3.2.md)

## What was new in v1.3.1

Version 1.3.1 was a stability release fixing command queue reliability, CS override interference, and serial coordination issues reported after v1.3.0. Full release notes: [RELEASE_NOTES_1.3.1.md](docs/releases/archive/RELEASE_NOTES_1.3.1.md)

## What was new in v1.3.0

Version 1.3.0 is a major feature release building on v1.2.0 with PIC settings visibility, safer upgrades, better recovery, optional admin protection, fuller `PS=1` integration, and significantly lower RAM pressure. Full release notes: [RELEASE_NOTES_1.3.0.md](docs/releases/archive/RELEASE_NOTES_1.3.0.md) / [Breaking Changes Log](docs/BREAKING_CHANGES.md)

### Highlights

- **PIC Gateway Settings Panel:** All 15 PIC configuration registers (setpoint override, GPIO, LEDs, tweaks, smart power, thermostat detection, etc.) are now exposed via REST API (`/api/v2/pic/settings`), MQTT, and a new "Gateway Settings" section in the Web UI. Settings are read on-demand from the PIC (one PR= every 3s, full cycle ~45s) and cached in the browser with localStorage for up to 7 days. Live values show in green, cached in amber.
- **Single-Click GitHub Release OTA:** The update page now lists GitHub releases with Installed/Update/Rollback badges. One-click download and flash with semver-aware version comparison including pre-release tags.
- **Optional Protected Admin Endpoints:** Settings, maintenance, file-management, reboot, and OTA routes can now be protected with HTTP Basic Auth.
- **Configurable MQTT Publish Gating:** OpenTherm and `PS=1` summary publishing can now be rate-limited to reduce MQTT broker load and WiFi chatter, with better status republish behavior after boot and reconnect.
- **Full `PS=1` Summary Integration:** `PS=1` output is now translated into the normal data pipeline, published to MQTT, and exposed to Home Assistant discovery.
- **Web UI Enhancements:** Light/dark theme toggle, one-shot OTGW PIC commands from the monitor page, richer settings tooltips, gateway mode indicator, WebSocket connection status with tooltips, simulation badge, and improved heap/device reporting.
- **Safer OTA / LittleFS Updates:** Reboot verification via `/api/v2/health`, browser backups of `settings.ini` and `dallas_labels.ini`, Dallas labels auto-preserved through localStorage, hardened filesystem flashing against WiFi reconnect corruption.
- **Triple-Reset WiFi Recovery:** Three quick hardware resets within 10 seconds clear stored WiFi credentials and reopen the captive portal without requiring a reflash.
- **Non-Blocking WiFi Reconnect:** The blocking 30-second reconnect loop is replaced with a state machine, preventing main-loop freezes on a heating system controller.
- **Security Hardening:** Centralized HTTP Basic Auth enforcement for all POST/PUT API endpoints. CORS wildcard replaced with dynamic origin validation. Webhook hostname SSRF prevention via DNS resolution. XSS fix in statistics table. Boot command and MQTT payload validation. ~450 lines of dead code removed.
- **Memory and Stability:** ArduinoJson removed, settings/state reorganized into structs, String class eliminated from hot paths including CSRF validation. MQTT autodiscovery memory reduced via streaming. ~1,400 bytes of stack pressure eliminated through centralized buffers. Fixed `millis()` wraparound bug, f8.8 negative value encoding, and OT message parse validation.
- **No New Breaking Changes:** For v1.2.0 users, this release adds features and hardening without introducing new MQTT topic, REST API, or settings-format breaks.

## What was new in v1.2.0

Version 1.2.0 was the protocol-alignment and discovery release. It expanded Home Assistant coverage across the OpenTherm specification and tightened MQTT, REST API, and Web UI behavior. Full release notes: [RELEASE_NOTES_1.2.0.md](docs/releases/archive/RELEASE_NOTES_1.2.0.md)

### Highlights

- **Complete Home Assistant discovery expansion:** 309 auto-discovery configurations across 80+ OpenTherm message IDs, covering heating, cooling, solar, DHW, ventilation, CH2, humidity, counters, and system status.
- **OpenTherm v4.2 alignment:** Added missing IDs `39` and `93-97`, corrected types and units, and introduced compatibility handling for legacy IDs `50-63`.
- **MQTT / webhook / diagnostics improvements:** Added optional source-separated MQTT topics, webhook support, safer MQTT auto-configuration, and richer serial/WebSocket diagnostics.
- **v2-only API baseline:** `/api/v0/` and `/api/v1/` were removed in favor of `/api/v2/`, with related device-info key updates for raw API consumers.
- **Upgrade note:** v1.2.0 introduced real migration items for MQTT topics, Home Assistant entities, and some raw API fields. See [RELEASE_NOTES_1.2.0.md](docs/releases/archive/RELEASE_NOTES_1.2.0.md) and [docs/fixes/opentherm-v42-mqtt-breaking-changes.md](docs/fixes/opentherm-v42-mqtt-breaking-changes.md).

## What was new in v1.1.0

Version 1.1.0 builds on the stable v1.0.0 foundation with Dallas temperature sensor enhancements, a complete RESTful API v2, WebUI data persistence, and 20 bug fixes from a comprehensive codebase review. Full release notes: [RELEASE_NOTES_1.1.0.md](docs/releases/archive/RELEASE_NOTES_1.1.0.md)

### Dallas Sensors, RESTful API v2, and 20-Bug Codebase Overhaul

**v1.1.0 delivers custom labels and real-time graphs for Dallas temperature sensors, a fully RESTful API v2 with 13 new endpoints (compliance score 5.4 → 8.5/10), and resolution of 20 bugs spanning memory safety, data integrity, concurrency, and security.**

- **Dallas Sensor Custom Labels & Graphs** — Inline label editing in the Web UI, stored in `/dallas_labels.ini` with zero backend RAM, automatic backup/restore during filesystem flash, and real-time graph visualization with 16-color palette. REST API: `GET/POST /api/v2/sensors/labels`.
- **RESTful API v2** — 13 new endpoints with consistent JSON errors, proper HTTP status codes (202 for async), CORS/OPTIONS support, RESTful resource naming (`messages/{id}`, `commands`, `device/info`). All frontend calls migrated to v2. See [ADR-035](docs/adr/ADR-035-restful-api-compliance-strategy.md).
- **20-bug codebase review** — Memory safety (OOB write, stack overflow), data integrity (MQTT hour bitmask, −127°C sensor published to MQTT), concurrency (ISR race in S0 counter), security (reflected XSS), reliability (file descriptor leak, null pointer crash, 750ms blocking sensor read), GPIO output feature fix, flash wear reduction (20 writes → 1). Full details: [Codebase Review](docs/reviews/2026-02-13_codebase-review/CODEBASE_REVIEW.md).
- **WebUI Data Persistence** — Automatic `localStorage` persistence with debounced saves, dynamic memory management, normal/capture modes, and auto-restoration on page load.
- **Heap Memory Monitoring** — 4-level health system (CRITICAL/WARNING/LOW/HEALTHY) with adaptive throttling and WebSocket backpressure control ([ADR-030](docs/adr/ADR-030-heap-memory-monitoring-emergency-recovery.md)).
- **Browser Debug Console (`otgwDebug`)** — Full diagnostic toolkit in the browser console: `status()`, `info()`, `settings()`, `wsStatus()`, `logs()`, `api()`, `health()`, `sendCmd()`, `exportLogs()`, and more.
- **PS Mode detection** — Automatic detection of `PS=1`; hides the OT log section, disables WebSocket streaming, suppresses time-sync commands.
- **MQTT auth fix** — Whitespace automatically trimmed from MQTT credentials, fixing auth failures when upgrading from v0.10.x.

### Notes for upgraders from v1.0.x

No breaking API or MQTT changes. A filesystem flash and hard browser refresh (Ctrl+F5) are recommended. The v0 and unversioned REST API endpoints deprecated in this release were removed in v1.2.0 (return 410 Gone).

## 🏁 Introduced in v1.0.0

Version 1.0.0 was a major milestone delivering improved stability, a modern user interface, and robust integration.

> 📝 Full release notes: [RELEASE_NOTES_1.0.0.md](docs/releases/archive/RELEASE_NOTES_1.0.0.md)

### Highlights

- **Real-Time Graphs & Statistics**: Live boiler data visualization (temperatures, setpoints) with responsive graphs and a long-term statistics dashboard.
- **Modern Web UI**: Fully integrated Dark Mode, responsive mobile design, redesigned File System Explorer, and WebSocket-based live log viewer.
- **Improved Flashing**: Reliable web-based firmware and filesystem flashing with health-check reboot verification. New `flash_esp.py` script for easy updates.
- **MQTT Auto Discovery**: Added Outside Temperature override (`outside`) support; static 1350-byte MQTT buffer prevents heap fragmentation.
- **Binary Safety**: Critical fix for Exception (2) crashes during PIC flashing (`strncmp_P` → `memcmp_P`).
- **Connectivity & Security**: Rewritten Wi-Fi logic with improved watchdog handling; CSRF protection, masked password fields, input sanitization.
- **Gateway Mode**: Reliable detection using `PR=M` command. New `NTPsendtime` setting.

---

## Features at a glance

### Home Assistant integration via MQTT

The recommended way to integrate with Home Assistant. The firmware publishes all OpenTherm data to MQTT and supports automatic discovery of entities.

- **309 auto-discovery configurations** across 80+ OpenTherm message IDs -- heating, cooling, solar thermal, DHW, ventilation, CH2, humidity, operational counters, fault diagnostics.
- Climate entity with temperature override support.
- Configurable publish interval to reduce broker load while keeping data fresh.
- Source-separated MQTT topics (optional) for per-device breakdown.
- Webhook support for triggering HTTP calls on status bit changes (flame on, fault detected, etc.).

See [Setting up MQTT with Home Assistant](#setting-up-mqtt-with-home-assistant) below for configuration steps.

### Web interface

- Live OpenTherm message log via WebSocket (port 81), with filtering, pausing, and raw message decoding.
- Real-time graphs for boiler temperatures, setpoints, water pressure, and modulation level ([ECharts](https://echarts.apache.org/)).
- Dallas temperature sensors shown in graphs with custom labels and a 16-color palette.
- Dark/light theme toggle with per-browser persistence.
- PIC gateway settings panel -- all 15 PIC configuration registers readable from the browser.
- File system explorer with upload, download, and delete.
- Firmware and filesystem OTA updates with health-check verification after reboot.

### REST API

A documented, versioned REST API for automation and integration beyond MQTT.

- All endpoints under `/api/v2/` with consistent JSON responses and proper HTTP status codes.
- OpenTherm data queries by message ID or label.
- Command submission, settings management, sensor label CRUD, PIC settings readout.
- CORS support for browser-based tools.
- **[REST API reference](docs/api/README.md)** -- full endpoint documentation with examples for Home Assistant, Python, and JavaScript.
- **[OpenAPI 3.0 specification](docs/api/openapi.yaml)** -- machine-readable spec for Swagger UI, Postman, or code generation.

### MQTT reference

Full MQTT topic documentation including namespace conventions, published topics, command topics, and Home Assistant discovery details.

- **[MQTT topic reference](docs/api/MQTT.md)**

### Ser2net / OTmonitor

- TCP serial socket on port **25238** for OTmonitor and other tools that speak the OTGW serial protocol.
- Command queue coordination: the firmware detects ser2net traffic and pauses its own queued commands to avoid PIC serial bus conflicts ([ADR-059](docs/adr/ADR-059-ser2net-queue-awareness.md)).
- `NTPsendtime` setting available to disable time synchronization when your ser2net workflow handles time independently.

### Dallas temperature sensors

- DS18B20/DS18S20/DS1822 support with Home Assistant auto-discovery.
- Custom labels editable in the Web UI (click to rename). Stored in `/dallas_labels.ini` with zero backend RAM usage.
- Automatic backup and restore of labels during filesystem flash.
- REST API: `GET/POST /api/v2/sensors/labels`. See [Dallas sensor API](docs/api/DALLAS_SENSOR_LABELS_API.md) and [sensor documentation](docs/features/dallas-temperature-sensors.md).

### S0 pulse counter

- kWh meter pulse counting on a configurable GPIO pin.

### Stability and memory

- Extensive use of PROGMEM to keep string literals in flash, not RAM.
- ArduinoJson removed; `String` class eliminated from all hot paths.
- Non-blocking WiFi reconnect state machine -- heating continues while WiFi recovers.
- Triple-reset WiFi recovery: three quick resets reopen the captive portal without reflashing.
- Heap monitoring with adaptive throttling and WebSocket backpressure.
- Optional HTTP Basic Auth for settings and maintenance endpoints.

---

## Setting up MQTT with Home Assistant

### Prerequisites

- Home Assistant with MQTT integration installed (Settings > Devices & Services > MQTT).
- An MQTT broker running (e.g., Mosquitto add-on in Home Assistant, or an external broker).
- Your OTGW device connected to the same network.

### Step 1: Configure MQTT in the OTGW Web UI

Open `http://<device-ip>/` in your browser and go to **Settings**.

| Setting | What to enter | Example |
| --- | --- | --- |
| **MQTT Broker** | IP address or hostname of your broker | `192.168.1.100` |
| **MQTT Port** | Broker port (usually 1883) | `1883` |
| **MQTT User** | Broker username (if authentication is enabled) | `mqttuser` |
| **MQTT Password** | Broker password | `••••••` |
| **MQTT Top Topic** | Prefix for all topics published by the gateway | `OTGW` |
| **MQTT Unique ID** | Unique identifier for this device | `otgw` |
| **HA Discovery** | Enable Home Assistant MQTT auto-discovery | Checked |

Click **Save** and the device will connect to your broker. The status bar at the bottom of the Web UI shows MQTT connection state.

### Step 2: Verify in Home Assistant

After saving, Home Assistant should discover the OTGW device within a few seconds:

1. Go to **Settings > Devices & Services > MQTT**.
2. Look for a new device named after your OTGW.
3. Click it to see all discovered entities -- heating status, temperatures, setpoints, modulation, flame, DHW, and more.

If your boiler supports cooling, solar thermal, ventilation, or a second heating circuit, those entities appear automatically too. No manual YAML configuration needed.

### Step 3: Tune the publish interval

By default (`0`), the gateway publishes every OpenTherm message as it arrives -- multiple times per second. This is the freshest data but creates high MQTT traffic.

Set the **Publish Interval** (under Settings > MQTT) to a value like `60` seconds. The gateway will then:
- Publish immediately when a value **changes**.
- Re-publish unchanged values once per interval as a heartbeat (so Home Assistant does not mark sensors as unavailable).

A value of `10`-`60` is a good starting point. Adjust based on how responsive you need your automations to be.

### Step 4: Optional -- send commands from Home Assistant

The gateway accepts commands on its MQTT subscribe topic. The topic structure is:

```
<TopTopic>/set/<UniqueId>/<command>
```

Common commands:

| Command | Description | Example payload |
| --- | --- | --- |
| `setpoint` | Temporary temperature override (TT) | `21.5` |
| `constant` | Constant temperature override (TC) | `22.0` |
| `outside` | Override outside temperature (OT) | `15.5` |
| `hotwater` | DHW control: `0`=off, `1`=on, `P`=push, `A`=auto | `1` |
| `maxchsetpt` | Max CH water setpoint (SH) | `60` |
| `maxdhwsetpt` | Max DHW setpoint (SW) | `55` |

**Example automation -- sync outside temperature from another sensor:**

```yaml
automation:
  - alias: "Sync Outside Temperature to OTGW"
    trigger:
      - platform: state
        entity_id: sensor.outdoor_temperature
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw/outside"
          payload: "{{ states('sensor.outdoor_temperature') }}"
```

For more examples: [Outside Temperature Override](example-api/outside_temperature_override_examples.md) | [Hot Water Control](example-api/hotwater_examples.md)

### Alternative: OpenTherm Gateway integration (no MQTT)

If you prefer not to use MQTT, Home Assistant has a built-in [OpenTherm Gateway integration](https://www.home-assistant.io/integrations/opentherm_gw/) that connects directly via the TCP serial socket:

```
socket://<device-ip>:25238
```

Use port **25238**, not 23. Port 23 is the telnet debug console.

This integration provides basic thermostat control but does not expose the full range of OpenTherm data that MQTT auto-discovery covers.

---

## Quick start

1. **Flash the firmware** to your ESP8266.
   - **Recommended**: Use the included script: `python3 flash_esp.py` (downloads and flashes the latest release).
   - `python3 flash_esp.py --build` to build from source instead.
   - See [FLASH_GUIDE.md](docs/guides/FLASH_GUIDE.md) for detailed instructions.
2. **Connect to WiFi**: The device starts an AP named `<hostname>-<mac>`. Connect and configure your WiFi credentials.
3. **Open the Web UI** at `http://<device-ip>/` and configure MQTT (see [above](#setting-up-mqtt-with-home-assistant)).
4. **Check Home Assistant** for auto-discovered entities.

## Hardware support

Starting with hardware version 2.3, the included ESP8266 devkit changed from NodeMCU to a Wemos D1 mini. Both are supported.

| NodoShop OTGW version | ESP8266 devkit |
| --- | --- |
| 1.x--2.0 | NodeMCU ESP8266 devkit |
| 2.3--2.x | Wemos D1 mini ESP8266 devkit |

For NodoShop boards in the **Wemos D1 mini family**, the firmware assumes the standard ESP8266 **UART0** pins (**TX/GPIO1**, **RX/GPIO3**) for PIC communication plus **D5/GPIO14** for PIC reset. A **Wemos D1 mini Pro** uses the same D1 mini-family footprint and pin mapping, so there is **no separate board profile or pin-remap option** in the firmware. If a Mini Pro still reports `picavailable=false`, the next step is a boot-time serial capture and hardware continuity/orientation check rather than a firmware pin-definition change.

## Connectivity summary

| Port | Protocol | Purpose |
| --- | --- | --- |
| 80 | HTTP | Web UI and REST API |
| 23 | Telnet | Debug logging |
| 25238 | TCP | OTGW serial interface (OTmonitor, HA OpenTherm integration) |
| -- | MQTT | Home automation integration (recommended) |

The firmware also exposes a Wi-Fi configuration portal (AP mode) when it cannot connect to a saved network.

## Security note

The Web UI and APIs are designed for use on a trusted local network. Do not expose the device directly to the internet; use a VPN for remote access. A reverse proxy can help with HTTP UI/API access, but WebSocket features assume plain HTTP/WS and may not work through an HTTPS proxy.

### Protected endpoints (optional)

Set an **endpoint password** in Settings (field: `httppasswd`) to require HTTP Basic Auth for:

- Settings (reading and changing device configuration)
- File management (upload/delete)
- Reboot and wireless reset
- OTA firmware updates
- Webhook test

Read-only monitoring (sensor values, device status, WebSocket connection) stays open.

## Documentation

| Topic | Link |
| --- | --- |
| Wiki (recommended starting point) | <https://github.com/rvdbreemen/OTGW-firmware/wiki> |
| REST API reference | [docs/api/README.md](docs/api/README.md) |
| OpenAPI specification | [docs/api/openapi.yaml](docs/api/openapi.yaml) |
| MQTT topic reference | [docs/api/MQTT.md](docs/api/MQTT.md) |
| Dallas sensor labels API | [docs/api/DALLAS_SENSOR_LABELS_API.md](docs/api/DALLAS_SENSOR_LABELS_API.md) |
| Webhook documentation | [docs/features/webhook.md](docs/features/webhook.md) |
| Flash guide | [docs/guides/FLASH_GUIDE.md](docs/guides/FLASH_GUIDE.md) |
| Local build guide | [docs/guides/BUILD.md](docs/guides/BUILD.md) |
| Code quality checker | [docs/process/EVALUATION.md](docs/process/EVALUATION.md) |
| Architecture Decision Records | [docs/adr/README.md](docs/adr/README.md) |
| WebSocket architecture | [docs/api/WEBSOCKET_FLOW.md](docs/api/WEBSOCKET_FLOW.md) |
| Upgrading from 0.9.x / 0.10.y | [docs/archive/upgrade-from-0.x.md](docs/archive/upgrade-from-0.x.md) |
| Release notes index (current + archive) | [docs/releases/README.md](docs/releases/README.md) |
| Documentation link policy | [docs/process/DOCUMENTATION_LINKS_POLICY.md](docs/process/DOCUMENTATION_LINKS_POLICY.md) |

## Important warnings

- **Do not flash OTGW PIC firmware over Wi-Fi using OTmonitor.** You can brick the PIC. Use the built-in PIC firmware upgrade feature instead.
- **Dallas GPIO default changed in v1.0.0**: Default pin moved from GPIO 13 (D7) to GPIO 10 (SD3). If upgrading from an older version, verify your wiring or change the setting back to 13.
- **REST API v0/v1 removed in v1.2.0**: Only `/api/v2/` remains. See the [REST API reference](docs/api/README.md).
- **MQTT topic spelling corrections in v1.2.0**: A few typos were fixed (`eletric_production` -> `electric_production`, etc.). Delete orphaned HA entities and let discovery recreate them. See [breaking changes details](docs/fixes/opentherm-v42-mqtt-breaking-changes.md).

## History and scope

The OpenTherm Gateway itself (hardware + PIC firmware + OTmonitor tooling) originates from **Schelte Bron's OTGW project**. This firmware builds on that ecosystem by running on the ESP8266 inside the **NodoShop OTGW** to expose OTGW data and controls over the network.

This project is primarily designed for the NodoShop OTGW hardware with an ESP8266 (NodeMCU / Wemos D1 mini). If you have a different OTGW build, it may work, but NodoShop OTGW compatibility is the main target.

## Release history

Release notes for all versions are in [docs/releases/](docs/releases/). Prebuilt firmware binaries are on the [GitHub releases page](https://github.com/rvdbreemen/OTGW-firmware/releases).

For historical versions (`v1.3.x` and older), links intentionally point to [docs/releases/archive/](docs/releases/archive/) because those notes are archived.

<details><summary>Version history (click to expand)</summary>

| Version | Highlights |
| --- | --- |
| **1.5.x** | Stable LTS line on Arduino Core 2.7.4. `v1.5.0` introduced reboot reliability hardening, tighter MQTT publish gating, HA discovery for stats topics, WebUI design system, and boot/loop diagnostics. [1.5.0](docs/releases/RELEASE_NOTES_1.5.0.md) [v1.5.0-fix2](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.5.0-fix2) |
| **1.4.x** | Arduino Core 3.1.2 baseline, SimpleTelnet migration, MQTT HA discovery streaming rewrite (309 configs / 80+ msgIds), WiFi reconnect hardening, heap-aware discovery drip, retained-discovery self-heal, unified time-boundary dispatcher, OpenTherm v4.2 alignment. [1.4.1](docs/releases/RELEASE_NOTES_1.4.1.md) |
| **1.3.x** | PIC gateway settings panel, optional HTTP Basic Auth, configurable MQTT publish gating, full PS=1 integration, triple-reset WiFi recovery, non-blocking WiFi reconnect, MQTT uptime/version publishing, PIC-less OTGW support, ser2net command queue coordination. [1.3.0](docs/releases/archive/RELEASE_NOTES_1.3.0.md) [1.3.1](docs/releases/archive/RELEASE_NOTES_1.3.1.md) [1.3.2](docs/releases/archive/RELEASE_NOTES_1.3.2.md) [1.3.3](docs/releases/archive/RELEASE_NOTES_1.3.3.md) [1.3.4](docs/releases/archive/RELEASE_NOTES_1.3.4.md) [1.3.5](docs/releases/RELEASE_NOTES_1.3.5.md) |
| **1.2.0** | Complete HA discovery expansion (309 configs, 80+ message IDs), OpenTherm v4.2 alignment, webhook support, source-separated MQTT topics, v0/v1 API removed. [Notes](docs/releases/archive/RELEASE_NOTES_1.2.0.md) |
| **1.1.0** | Dallas sensor custom labels and graphs, RESTful API v2 (13 new endpoints), WebUI data persistence, browser debug console, PS mode detection, 20 bug fixes. [Notes](docs/releases/archive/RELEASE_NOTES_1.1.0.md) |
| **1.0.0** | Milestone release: real-time graphs, modern Web UI with dark mode, WebSocket live log, MQTT auto-discovery, interactive flashing tool, PROGMEM memory safety. [Notes](docs/releases/archive/RELEASE_NOTES_1.0.0.md) |
| 0.10.3 | MQTT password masking, HA discovery template improvements, status function fixes. |
| 0.10.2 | PIC firmware update fix, filesystem update with latest PIC firmware. |
| 0.10.1 | Build process improvements, VH status parsing fix, WiFi quality indicator. |
| 0.10.0 | PIC16F1847 (6.x firmware) support, DHCP NTP override, S0 pulse counter, Dallas auto-configure. |
| 0.9.x | JIT HA auto-discovery, climate entity, MQTT set commands, time setup, NTP improvements. |
| 0.8.x | MQTT topic convention change, HA device grouping, climate entity, PIC firmware integration, Dallas sensors, command queue. |
| 0.7.x | LittleFS migration, ser2net on port 25238, ventilation/heat recovery message IDs, PIC reset on boot. |
| 0.6.x | Standalone Web UI, OTA support. |
| 0.5.x | REST API v1, settings UI. |
| 0.4.x | Ser2net, REST API v0. |
| 0.2--0.3 | MQTT integration, serial stream. |
| 0.0.1 | Initial OT protocol parsing. |

</details>

## Community and support

- Discord: <https://discord.gg/zjW3ju7vGQ>
- Issues / bug reports: <https://github.com/rvdbreemen/OTGW-firmware/issues>

## Credits

Shoutout to early adopters helping me out testing and discussing the firmware in development. For pushing features, testing and living on the edge.

Reaching version 1.0.0 wouldn't have been possible without the community. So shoutout to the following people for the collaboration on development:

- @hvxl for all his work on the OTGW hardware, PIC firmware and ESP coding.
- @sjorsjuhmaniac for improving the MQTT naming convention and HA integration, adding climate entity and otgw device
- @vampywiz17 early adopter and tester
- @Stemplar reporting issues realy on
- @proditaki for creating Domiticz plugin for OTGW-firmware
- @tjfsteele for endless hours of testing
- @DaveDavenport for fixing all known and unknown issues with the codebase, it's stable with you
- @DutchessNicole for fixing the Web UI over time
- @RobR for his work in the s0 counter implementation
- @GeorgeZ83 for improving Home Assistant MQTT integration and climate entity support

And for all those people that keep reporting issue, pushing for more and helping other in the community all the time.

A big thank should goto **Schelte Bron** @hvxl for amazing work on the OpenTherm Gateway project and for providing access to the upgrade routines of the PIC. Enabling this custom firmware a reliable way to upgrade you PIC firmware. If you want to thank Schelte Bron for his work on the OpenTherm Gateway project, just head over to his homepage and donate to him: <https://otgw.tclcode.com/>

## Buy me a coffee

In case you want to buy me a coffee, head over here:

[![Buy me a coffee](https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20coffee&emoji=&slug=rvdbreemen&button_colour=5F7FFF&font_colour=ffffff&font_family=Cookie&outline_colour=000000&coffee_colour=FFDD00)](https://www.buymeacoffee.com/rvdbreemen)

## License

MIT. See `LICENSE`.
