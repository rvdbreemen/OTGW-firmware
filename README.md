# OTGW-firmware (ESP8266) for NodoShop OpenTherm Gateway

[![Join the Discord chat](https://img.shields.io/discord/812969634638725140.svg?style=flat-square)](https://discord.gg/zjW3ju7vGQ)

This repository contains the **ESP8266 firmware for the NodoShop OpenTherm Gateway (OTGW)**. It runs on the ESP8266 "devkit" that is part of the NodoShop OTGW and turns the gateway into a standalone network device.

## What's New in v1.4.1

**v1.4.1 is the first public release in the 1.4.x series.** A 1.4.0 milestone was tracked internally but was never published as a standalone release: development continued until the full body of work was stable enough to ship in one go. If you are upgrading from v1.3.5, v1.4.1 contains everything. There is no v1.4.0 to install or skip.

> **CRITICAL upgrade warning: flash filesystem FIRST, then firmware.** The Arduino Core 3.1.2 upgrade changed the LittleFS partition from 1 MB to 2 MB. All versions before v1.4.x shipped with Arduino Core 2.7.4 and a 1 MB filesystem. Flashing in the correct order (filesystem binary first, firmware binary second) preserves your settings. If you mistakenly flash the firmware first, the new firmware boots against the old 1 MB layout and spends 5-10 minutes reformatting the 2 MB partition on first boot — the device is unresponsive during that time and all your settings are lost. Always download both `*.ino.bin` and `*.littlefs.bin` and flash the filesystem binary first.

Full release notes: [RELEASE_NOTES_1.4.1.md](RELEASE_NOTES_1.4.1.md).

Key highlights:

- **SimpleTelnet migration**: formatted welcome banner on debug connect, structured key dispatch, clean session teardown.
- **ESP8266 Arduino Core 3.1.2**: updated lwIP, improved WiFi driver, systematic PROGMEM pointer safety audit.
- **MQTT HA discovery rewrite**: 309 auto-discovery configs via a streaming, bitmap-driven drip API. No static staging buffer. Runtime Dallas sensor discovery. Comprehensive icon heuristics.
- **WiFi reconnect hardening**: fixes a regression that prevented IP re-acquisition after a router reboot (no ESP reboot required).
- **Heap-aware discovery drip**: 2 s normal / 10 s slow-mode with fragmentation-aware gates, Status-burst cooldown, and hold-per-interval hysteresis. Eliminates mid-discovery watchdog resets on loaded gateways.
- **Retained-discovery self-heal**: node-scoped wildcard verify window counts what the broker actually has, re-announces missing configs. Daily auto-heal + on-demand via REST (`/api/v2/discovery`), telnet `V` key. See [ADR-062](docs/adr/ADR-062-retained-discovery-verification.md).
- **Hourly heap diagnostic MQTT topic**: `<topTopic>/otgw-firmware/stats/heap` (retained, 17-field JSON covering free heap, fragmentation, tier counters, discovery state).
- **Unified time-boundary dispatcher**: hour/day/year triggers wall-clock aligned via single caller contract. See [ADR-064](docs/adr/ADR-064-time-boundary-single-caller-contract.md).
- **OpenTherm v4.2 alignment**: fixes for MaxTSet/TdhwSet (0°C in HA), reserved ID range 58-69, WRITE_ACK handling.
- **Configurable device manufacturer/model** in MQTT device announcement (credit: Schelte Bron).
- **Nightly restart** with configurable hour, wired to the unified hourChanged dispatcher.

---

## What was new in v1.3.4

Version 1.3.4 fixes MQTT throttle slot suppression, adds Debug Info tooltips, renames "OTGW Connected" to "OpenTherm Active", and adds thermostat-only MQTT support. Full release notes: [RELEASE_NOTES_1.3.4.md](RELEASE_NOTES_1.3.4.md)

## What was new in v1.3.3

Version 1.3.3 adds PIC-less OTGW support and fixes the dashboard showing empty values for unsupported OpenTherm message IDs. Full release notes: [RELEASE_NOTES_1.3.3.md](RELEASE_NOTES_1.3.3.md)

## What was new in v1.3.2

Version 1.3.2 fixes the persistent file explorer failures reported after v1.3.1. Full release notes: [RELEASE_NOTES_1.3.2.md](RELEASE_NOTES_1.3.2.md)

## What was new in v1.3.1

Version 1.3.1 was a stability release fixing command queue reliability, CS override interference, and serial coordination issues reported after v1.3.0. Full release notes: [RELEASE_NOTES_1.3.1.md](RELEASE_NOTES_1.3.1.md)

## What was new in v1.3.0

Version 1.3.0 is a major feature release building on v1.2.0 with PIC settings visibility, safer upgrades, better recovery, optional admin protection, fuller `PS=1` integration, and significantly lower RAM pressure. Full release notes: [RELEASE_NOTES_1.3.0.md](RELEASE_NOTES_1.3.0.md) / [Breaking Changes Log](docs/BREAKING_CHANGES.md)

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

Version 1.2.0 was the protocol-alignment and discovery release. It expanded Home Assistant coverage across the OpenTherm specification and tightened MQTT, REST API, and Web UI behavior. Full release notes: [RELEASE_NOTES_1.2.0.md](RELEASE_NOTES_1.2.0.md)

### Highlights

- **Complete Home Assistant discovery expansion:** 309 auto-discovery configurations across 80+ OpenTherm message IDs, covering heating, cooling, solar, DHW, ventilation, CH2, humidity, counters, and system status.
- **OpenTherm v4.2 alignment:** Added missing IDs `39` and `93-97`, corrected types and units, and introduced compatibility handling for legacy IDs `50-63`.
- **MQTT / webhook / diagnostics improvements:** Added optional source-separated MQTT topics, webhook support, safer MQTT auto-configuration, and richer serial/WebSocket diagnostics.
- **v2-only API baseline:** `/api/v0/` and `/api/v1/` were removed in favor of `/api/v2/`, with related device-info key updates for raw API consumers.
- **Upgrade note:** v1.2.0 introduced real migration items for MQTT topics, Home Assistant entities, and some raw API fields. See [RELEASE_NOTES_1.2.0.md](RELEASE_NOTES_1.2.0.md) and [docs/fixes/opentherm-v42-mqtt-breaking-changes.md](docs/fixes/opentherm-v42-mqtt-breaking-changes.md).

## What was new in v1.1.0

Version 1.1.0 builds on the stable v1.0.0 foundation with Dallas temperature sensor enhancements, a complete RESTful API v2, WebUI data persistence, and 20 bug fixes from a comprehensive codebase review. Full release notes: [RELEASE_NOTES_1.1.0.md](RELEASE_NOTES_1.1.0.md)

### Dallas Sensors, RESTful API v2, and 20-Bug Codebase Overhaul

**v1.1.0 delivers custom labels and real-time graphs for Dallas temperature sensors, a fully RESTful API v2 with 13 new endpoints (compliance score 5.4 → 8.5/10), and resolution of 20 bugs spanning memory safety, data integrity, concurrency, and security.**

- **Dallas Sensor Custom Labels & Graphs** — Inline label editing in the Web UI, stored in `/dallas_labels.ini` with zero backend RAM, automatic backup/restore during filesystem flash, and real-time graph visualization with 16-color palette. REST API: `GET/POST /api/v2/sensors/labels`.
- **RESTful API v2** — 13 new endpoints with consistent JSON errors, proper HTTP status codes (202 for async), CORS/OPTIONS support, RESTful resource naming (`messages/{id}`, `commands`, `device/info`). All frontend calls migrated to v2. See [ADR-035](docs/adr/ADR-035-restful-api-compliance-strategy.md).
- **20-bug codebase review** — Memory safety (OOB write, stack overflow), data integrity (MQTT hour bitmask, −127°C sensor published to MQTT), concurrency (ISR race in S0 counter), security (reflected XSS), reliability (file descriptor leak, null pointer crash, 750ms blocking sensor read), GPIO output feature fix, flash wear reduction (20 writes → 1). Full details: [Codebase Review](docs/reviews/2026-02-13_codebase-review/CODEBASE_REVIEW.md).
- **WebUI Data Persistence** — Automatic `localStorage` persistence with debounced saves, dynamic memory management, normal/capture modes, and auto-restoration on page load.
- **Heap Memory Monitoring** — 4-level health system (CRITICAL/WARNING/LOW/HEALTHY) with adaptive throttling and WebSocket backpressure control ([ADR-030](docs/adr/ADR-030-heap-memory-monitoring.md)).
- **Browser Debug Console (`otgwDebug`)** — Full diagnostic toolkit in the browser console: `status()`, `info()`, `settings()`, `wsStatus()`, `logs()`, `api()`, `health()`, `sendCmd()`, `exportLogs()`, and more.
- **PS Mode detection** — Automatic detection of `PS=1`; hides the OT log section, disables WebSocket streaming, suppresses time-sync commands.
- **MQTT auth fix** — Whitespace automatically trimmed from MQTT credentials, fixing auth failures when upgrading from v0.10.x.

### Notes for upgraders from v1.0.x

No breaking API or MQTT changes. A filesystem flash and hard browser refresh (Ctrl+F5) are recommended. The v0 and unversioned REST API endpoints deprecated in this release were removed in v1.2.0 (return 410 Gone).

## 🏁 Introduced in v1.0.0

Version 1.0.0 was a major milestone delivering improved stability, a modern user interface, and robust integration.

> 📝 Full release notes: [RELEASE_NOTES_1.0.0.md](RELEASE_NOTES_1.0.0.md)

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
   - See [FLASH_GUIDE.md](docs/FLASH_GUIDE.md) for detailed instructions.
2. **Connect to WiFi**: The device starts an AP named `<hostname>-<mac>`. Connect and configure your WiFi credentials.
3. **Open the Web UI** at `http://<device-ip>/` and configure MQTT (see [above](#setting-up-mqtt-with-home-assistant)).
4. **Check Home Assistant** for auto-discovered entities.

## Hardware support

Starting with hardware version 2.3, the included ESP8266 devkit changed from NodeMCU to a Wemos D1 mini. Both are supported.

| NodoShop OTGW version | ESP8266 devkit |
| --- | --- |
| 1.x--2.0 | NodeMCU ESP8266 devkit |
| 2.3--2.x | Wemos D1 mini ESP8266 devkit |

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
| Flash guide | [docs/FLASH_GUIDE.md](docs/FLASH_GUIDE.md) |
| Local build guide | [docs/BUILD.md](docs/BUILD.md) |
| Code quality checker | [docs/EVALUATION.md](docs/EVALUATION.md) |
| Architecture Decision Records | [docs/adr/README.md](docs/adr/README.md) |
| WebSocket architecture | [docs/api/WEBSOCKET_FLOW.md](docs/api/WEBSOCKET_FLOW.md) |
| Upgrading from 0.9.x / 0.10.y | [docs/upgrade-from-0.x.md](docs/upgrade-from-0.x.md) |

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

<details><summary>Version history (click to expand)</summary>

| Version | Highlights |
| --- | --- |
| **1.3.x** | PIC gateway settings panel, optional HTTP Basic Auth, configurable MQTT publish gating, full PS=1 integration, triple-reset WiFi recovery, non-blocking WiFi reconnect, MQTT uptime/version publishing, PIC-less OTGW support, ser2net command queue coordination. [1.3.0](docs/releases/RELEASE_NOTES_1.3.0.md) [1.3.1](docs/releases/RELEASE_NOTES_1.3.1.md) [1.3.2](docs/releases/RELEASE_NOTES_1.3.2.md) [1.3.3](docs/releases/RELEASE_NOTES_1.3.3.md) [1.3.4](docs/releases/RELEASE_NOTES_1.3.4.md) [1.3.5](RELEASE_NOTES_1.3.5.md) |
| **1.2.0** | Complete HA discovery expansion (309 configs, 80+ message IDs), OpenTherm v4.2 alignment, webhook support, source-separated MQTT topics, v0/v1 API removed. [Notes](docs/releases/RELEASE_NOTES_1.2.0.md) |
| **1.1.0** | Dallas sensor custom labels and graphs, RESTful API v2 (13 new endpoints), WebUI data persistence, browser debug console, PS mode detection, 20 bug fixes. [Notes](docs/releases/RELEASE_NOTES_1.1.0.md) |
| **1.0.0** | Milestone release: real-time graphs, modern Web UI with dark mode, WebSocket live log, MQTT auto-discovery, interactive flashing tool, PROGMEM memory safety. [Notes](docs/releases/RELEASE_NOTES_1.0.0.md) |
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
