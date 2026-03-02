# OTGW-firmware (ESP8266) for NodoShop OpenTherm Gateway

> **🔧 Development Release — v1.3.0-beta**
> This build delivers **triple-reset WiFi recovery**, one-shot OTGW PIC commands from the web UI, full PS=1 summary parsing with MQTT/HA discovery, heap status in device info, and a range of reliability fixes (boot-time service restart prevention, webhook payload truncation fix, String heap fragmentation reduction).<br>
> For the latest stable release, see the [releases page](https://github.com/rvdbreemen/OTGW-firmware/releases).

[![Join the Discord chat](https://img.shields.io/discord/812969634638725140.svg?style=flat-square)](https://discord.gg/zjW3ju7vGQ)

This repository contains the **ESP8266 firmware for the NodoShop OpenTherm Gateway (OTGW)**. It runs on the ESP8266 “devkit” that is part of the NodoShop OTGW and turns the gateway into a standalone network device.

## 🚀 What's New in v1.3.0-beta

Version 1.3.0-beta builds on v1.2.0-beta with focused reliability, usability, and memory improvements. Full release notes: [RELEASE_NOTES_1.3.0.md](RELEASE_NOTES_1.3.0.md)

### 🔁 Triple-Reset WiFi Recovery

The most practical new feature in v1.3.0 is **triple-click reset to re-enter WiFi setup mode**.

If the device loses its WiFi connection — router replaced, SSID changed, or credentials forgotten — triple-clicking the hardware reset button within 10 seconds clears the stored WiFi configuration and starts the WiFiManager captive portal, without needing to physically re-flash the device.

**How it works**: three resets within 10 seconds → WiFi credentials cleared → blue LED blinks → captive portal opens on `192.168.4.1` → connect and configure a new network.

See [docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md](docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md) for step-by-step instructions.

### What's included

- **One-shot OTGW PIC commands from the web UI** — A command bar in the OpenTherm Monitor lets you type raw PIC commands (e.g. `TT=20.5`, `SH=60`) and see the response in the log immediately, without needing a telnet/serial session.
- **PS=1 summary parsing with MQTT/HA discovery** — All `PS=1` summary fields are now fully parsed and published to MQTT with Home Assistant auto-discovery per field. PS mode users now get proper HA sensor entities automatically.
- **Heap status in device info** — Free heap, max contiguous block, and heap health level exposed in `/api/v2/device/info` and visible on the firmware flash page.
- **Boot reliability fix** — `readSettings()` no longer marks settings dirty or triggers MQTT/NTP/mDNS service restarts on the first `loop()` iteration. Services start cleanly; connections are not dropped immediately after boot.
- **Hostname dot-stripping fix** — `strchr()` in the hostname handler was targeting `settingMQTTtopTopic` instead of `settingHostname`; corrected.
- **Webhook payload truncation fix** — `valueBuf` widened to prevent truncation of long webhook payload strings on settings load.
- **String heap fragmentation reduction** — `writeSettings()` and `writeJsonStringPair()` no longer allocate temporary `String` objects per save, reducing heap fragmentation over time on the ESP8266.
- **REST API v2 fully complete** — The OTA/firmware flash page no longer calls any v1 endpoints; the v2 migration is 100% done across the entire firmware.

### No breaking changes

v1.3.0 introduces no MQTT topic renames, no API removals, and no settings format changes. Upgrading from v1.2.0-beta requires no migration steps.

Upgrading from v1.1.x or earlier: see [RELEASE_NOTES_1.2.0.md](RELEASE_NOTES_1.2.0.md) for v1.2.0 migration guidance (MQTT topic renames, REST API v0/v1 removal).

---

## What was new in v1.2.0-beta

Version 1.2.0-beta builds on the stable v1.0.0 foundation and includes two release increments of improvements. Full release notes: [RELEASE_NOTES_1.2.0.md](RELEASE_NOTES_1.2.0.md)

### Comprehensive Home Assistant Discovery — All OpenTherm Message Types

**v1.2.0-beta brings full MQTT auto-discovery for the complete OpenTherm protocol** — 309 discovery configurations across 80+ message IDs, covering central heating, cooling, solar thermal, DHW, ventilation/heat recovery, CH2, environment sensors, operational counters, and gateway status. No manual YAML required.

- **OpenTherm v4.2 protocol alignment** — Added missing IDs, corrected directions/types/units, reserved-ID compatibility for legacy IDs `50-63`.
- **Configurable source-separated MQTT/HA discovery** (`mqttseparatesources`) — nested source paths (`<metric>/thermostat`, `<metric>/boiler`, `<metric>/gateway`) while retaining legacy topics.
- **Webhook support** — HTTP call on OpenTherm status bit change (e.g. flame on/off), with configurable URL, payload, and content type per event direction. Local-network-only.
- **v0 and v1 REST API removed** — Only `/api/v2/` remains; `/api/v0/` and `/api/v1/` return **410 Gone**.
- **ArduinoJson removed** — Settings I/O replaced with lightweight bounded streaming helpers.
- **Gateway mode / API reliability** — Fixed `PR=M` parsing, explicit detecting state, standardised `otgwmode` / `wifiquality_text` device-info keys.
- **Serial robustness** — OT line buffer increased (`256` → `512`), safe overflow discard, richer WebSocket event logging.
- **Mobile-responsive UI** — Shared navigation shell, `index_common.css`, improved layout at `<=768px`.

### ⚠️ Migration required from v1.1.x

MQTT topic renames (OpenTherm v4.2 alignment), REST API v0/v1 removal, and device-info key changes. Full details: [RELEASE_NOTES_1.2.0.md](RELEASE_NOTES_1.2.0.md)

---

## What was new in v1.1.0-beta

Version 1.1.0-beta builds on the stable v1.0.0 foundation with Dallas temperature sensor enhancements, a complete RESTful API v2, WebUI data persistence, and 20 bug fixes from a comprehensive codebase review. Full release notes: [RELEASE_NOTES_1.1.0.md](RELEASE_NOTES_1.1.0.md)

### Dallas Sensors, RESTful API v2, and 20-Bug Codebase Overhaul

**v1.1.0-beta delivers custom labels and real-time graphs for Dallas temperature sensors, a fully RESTful API v2 with 13 new endpoints (compliance score 5.4 → 8.5/10), and resolution of 20 bugs spanning memory safety, data integrity, concurrency, and security.**

- **Dallas Sensor Custom Labels & Graphs** — Inline label editing in the Web UI, stored in `/dallas_labels.ini` with zero backend RAM, automatic backup/restore during filesystem flash, and real-time graph visualization with 16-color palette. REST API: `GET/POST /api/v2/sensors/labels`.
- **RESTful API v2** — 13 new endpoints with consistent JSON errors, proper HTTP status codes (202 for async), CORS/OPTIONS support, RESTful resource naming (`messages/{id}`, `commands`, `device/info`). All frontend calls migrated to v2. See [ADR-035](docs/adr/ADR-035-restful-api-compliance-strategy.md).
- **20-bug codebase review** — Memory safety (OOB write, stack overflow), data integrity (MQTT hour bitmask, −127°C sensor published to MQTT), concurrency (ISR race in S0 counter), security (reflected XSS), reliability (file descriptor leak, null pointer crash, 750ms blocking sensor read), GPIO output feature fix, flash wear reduction (20 writes → 1). Full details: [Codebase Review](docs/reviews/2026-02-13_codebase-review/CODEBASE_REVIEW.md).
- **WebUI Data Persistence** — Automatic `localStorage` persistence with debounced saves, dynamic memory management, normal/capture modes, and auto-restoration on page load.
- **Heap Memory Monitoring** — 4-level health system (CRITICAL/WARNING/LOW/HEALTHY) with adaptive throttling and WebSocket backpressure control ([ADR-030](docs/adr/ADR-030-heap-memory-monitoring.md)).
- **Browser Debug Console (`otgwDebug`)** — Full diagnostic toolkit in the browser console: `status()`, `info()`, `settings()`, `wsStatus()`, `logs()`, `api()`, `health()`, `sendCmd()`, `exportLogs()`, and more.
- **PS Mode detection** — Automatic detection of `PS=1`; hides the OT log section, disables WebSocket streaming, suppresses time-sync commands.
- **MQTT auth fix** — Whitespace automatically trimmed from MQTT credentials, fixing auth failures when upgrading from v0.10.x.

### Notes for upgraders from v1.0.x

No breaking API or MQTT changes. A filesystem flash and hard browser refresh (Ctrl+F5) are recommended. The v0 and unversioned REST API endpoints deprecated in this release were removed in v1.2.0-beta (return 410 Gone).

## 🏁 Introduced in v1.0.0

Version 1.0.0 was a major milestone delivering improved stability, a modern user interface, and robust integration.

### Major Features

- **Real-Time Graphs & Statistics**: Visualize boiler data (temperatures, setpoints) in real-time with responsive graphs and view long-term statistics in a dedicated dashboard.
- **Modern Web UI**: Features a fully integrated **Dark Mode**, responsive design for mobile devices, and a redesigned **File System Explorer**.
- **Live OpenTherm Message Streaming**: Real-time OpenTherm message log viewing using **WebSocket** on port 81 for instant visibility into gateway-boiler communication.
- **Improved Flashing**: Simplified and more reliable **web-based firmware and filesystem flashing** with explicit reboot verification via health checks.
- **Stream Logging**: Stream OpenTherm logs directly to local files for troubleshooting.
- **Device Health Monitoring**: `/api/v1/health` endpoint for operational status, uptime, and heap usage monitoring.
- **Gateway Mode**: Reliable detection using `PR=M` command, checks every 30s.
- **NTP Control**: New `NTPsendtime` setting.

### Integration (MQTT & HA)

- **Auto Discovery**: Added support for Outside Temperature override (`outside`).
- **Stability**: Static 1350-byte MQTT buffer to prevent heap fragmentation.

### Core Stability & Security

- **Binary Safety**: Critical fix for Exception (2) crashes during PIC flashing, replaced `strncmp_P` with `memcmp_P`.
- **Connectivity**: Rewritten Wi-Fi logic with improved watchdog handling.
- **Security**: CSRF protection on APIs, masked password fields, input sanitization.

---

## History and scope

The OpenTherm Gateway itself (hardware + PIC firmware + OTmonitor tooling) originates from **Schelte Bron’s OTGW project**. This firmware builds on that ecosystem by running on the ESP8266 inside the **NodoShop OTGW** to expose OTGW data and controls over the network.

This project is therefore **primarily designed for the NodoShop OTGW hardware with an ESP8266** (NodeMCU / Wemos D1 mini depending on hardware revision). If you have a different OTGW build, it may work, but NodoShop OTGW compatibility is the main target.

## Project Structure

The project code is organized as follows:

- `src/OTGW-firmware/`: Main firmware source code (`.ino`, `.h`) and filesystem data (`data/`).
- `libraries/`: External and internal libraries used by the firmware.
- `docs/`: Documentation, guides, and architectural decision records.
- `build/`: Build artifacts (created automatically during build).
- `scripts/`: Helper scripts for versioning and building.

## Hardware support

Starting with hardware version 2.3, the included ESP8266 devkit changed from NodeMCU to a Wemos D1 mini. Both are supported by this firmware.

| NodoShop OTGW version | ESP8266 devkit |
| --- | --- |
| 1.x–2.0 | NodeMCU ESP8266 devkit |
| 2.3–2.x | Wemos D1 mini ESP8266 devkit |

## Documentation and links

- Wiki / documentation (recommended starting point): <https://github.com/rvdbreemen/OTGW-firmware/wiki>
- **Flash guide** (platform-independent Python script): [FLASH_GUIDE.md](docs/FLASH_GUIDE.md)
- **Local build guide** (Windows/Mac): [BUILD.md](docs/BUILD.md)
- **Evaluation framework**: [EVALUATION.md](docs/EVALUATION.md) - Code quality and standards checker
- **Architecture Decision Records (ADRs)**: [docs/adr/README.md](docs/adr/README.md) - Documented architectural decisions
- **REST API Documentation**: [docs/api/README.md](docs/api/README.md) - Complete API reference with v2 endpoints
- **OpenAPI Specification**: [docs/api/openapi.yaml](docs/api/openapi.yaml) - Machine-readable API spec
- **Codebase Review (Feb 2026)**: [docs/reviews/2026-02-13_codebase-review/](docs/reviews/2026-02-13_codebase-review/) - Comprehensive review with 20 findings, all resolved
- **ADR Skill for Copilot**: [.github/skills/adr/](.github/skills/adr/) - Automated ADR management and compliance
- **WebSocket architecture**: [WEBSOCKET_FLOW.md](docs/WEBSOCKET_FLOW.md) - Complete WebSocket flow explanation
- **WebSocket quick reference**: [WEBSOCKET_QUICK_REFERENCE.md](docs/WEBSOCKET_QUICK_REFERENCE.md) - Short WebSocket overview
- **WiFi recovery (triple reset)**: [docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md](docs/guides/WIFI_RECOVERY_TRIPLE_RESET.md) - Force WiFi config portal when network credentials are invalid
- NodoShop OTGW product page: <https://www.nodo-shop.nl/nl/opentherm-gateway/211-opentherm-gateway.html>
- Original OTGW project site (Schelte Bron): <http://otgw.tclcode.com/>
- OTGW PIC firmware downloads: <http://otgw.tclcode.com/download.html>
- GitHub releases (prebuilt firmware binaries): <https://github.com/rvdbreemen/OTGW-firmware/releases>

## Quick start (high level)

The exact steps and screenshots live in the wiki, but the general flow is:

1. Flash the latest firmware release to your ESP8266 (and flash the matching LittleFS image when required by the release).
   - **Easy method (recommended)**: Use the included `flash_esp.py` script:
     - `python3 flash_esp.py` - Downloads and flashes the latest release (default)
     - `python3 flash_esp.py --build` - Builds from source and flashes (for developers)
     - See [FLASH_GUIDE.md](docs/FLASH_GUIDE.md) for detailed instructions
   - **Manual method**: Follow the wiki instructions
2. Connect the OTGW to your network and open the Web UI via `http://<device-ip>/`.
   If the device cannot connect, it starts a Wi-Fi configuration portal using an AP named `<hostname>-<mac>`.
  - **Recovery option (Wemos/ESP8266 reset-only)**: press the reset button **3 times within 10 seconds** to force the Wi-Fi config portal on next boot.
  - Forced recovery clears stored Wi-Fi credentials first, then starts the portal.
3. Configure MQTT (broker, credentials, topic prefix) and enable Home Assistant MQTT Auto Discovery.
4. Add the MQTT integration in Home Assistant; entities should appear automatically.

## Security note

The Web UI and APIs are designed for use on a trusted local network. Do not expose the device directly to the internet; use a VPN or a properly secured reverse proxy if remote access is needed.

## Community and support

- Discord: <https://discord.gg/zjW3ju7vGQ>
- Issues / bug reports: <https://github.com/rvdbreemen/OTGW-firmware/issues>

## What you can do with this firmware

### Web UI

- Configure the gateway via HTTP (default port 80).
- Manage settings stored on LittleFS.
- Perform PIC firmware maintenance (see warning below).
- View a live OpenTherm message log stream and download logs (WebSocket-based on port 81).
- **Stream logs directly to local files** using the File System Access API (Chrome/Edge/Opera desktop only) - see [Stream Logging.md](Stream%20Logging.md) for setup and technical details.
- Perform firmware and filesystem updates with automatic health verification after reboot (simplified XHR-based flow, no real-time progress polling).
- Toggle a dark theme with per-browser persistence.
- Supports reverse proxy deployments with automatic http/https detection for REST API and basic Web UI access; WebSocket-based features (such as the live OT message log) assume plain HTTP and may not work when accessed via an HTTPS reverse proxy.

### MQTT (including Home Assistant Auto Discovery)

- Publishes parsed OpenTherm values to MQTT using a configurable topic prefix.
- Supports Home Assistant MQTT Auto Discovery (Home Assistant Core v2021.2.0+).
- Accepts OTGW commands via MQTT (topic structure depends on your configured prefix; see the wiki for exact topics and examples).
- Publishes event topics for command responses/errors and thermostat connection/power state changes.

#### MQTT Commands

The firmware accepts commands via MQTT to control various OTGW functions. Commands are published to the topic:

```text
<mqtt-prefix>/set/<node-id>/<command>
```

**Available commands include:**

- `setpoint` - Temporary temperature override (maps to OTGW `TT` command)
- `constant` - Constant temperature override (maps to OTGW `TC` command)
- **`outside`** - **Override outside temperature sensor** (maps to OTGW `OT` command)
- `hotwater` - Control domestic hot water enable option (maps to OTGW `HW` command)
  - Valid values: `0` (off), `1` (on), `P` (DHW push - heat tank once), any other character (e.g., `A`) for auto/thermostat control
- `maxchsetpt` - Set maximum central heating water setpoint (maps to OTGW `SH` command)
- `maxdhwsetpt` - Set maximum DHW setpoint (maps to OTGW `SW` command)
- And many more (see MQTTstuff.ino for complete list)

#### Example: Override Outside Temperature

If your boiler's built-in outside temperature sensor is malfunctioning or not present, you can override it with a value from another sensor (e.g., a weather station or Home Assistant sensor):

```bash
# Using mosquitto_pub (replace with your MQTT prefix and node ID)
mosquitto_pub -h <broker-ip> -t "OTGW/set/otgw/outside" -m "15.5"
```

**Home Assistant Example:**

Create an automation to sync your preferred outside temperature sensor:

```yaml
automation:
  - alias: "Sync Outside Temperature to OTGW"
    trigger:
      - platform: state
        entity_id: sensor.outdoor_temperature  # Your actual outdoor sensor
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw/outside"  # Adjust to your MQTT prefix and node ID
          payload: "{{ states('sensor.outdoor_temperature') }}"
```

This allows the OTGW to use external temperature data for OpenTherm communication with your boiler, which is particularly useful when:

- Your boiler doesn't have an outside temperature sensor
- The built-in sensor is affected by sun/wind exposure
- You want to use a more accurate or better-positioned sensor

**For more detailed examples and use cases, see:** [Outside Temperature Override Examples](example-api/outside_temperature_override_examples.md)

**For hot water control examples and automation ideas, see:** [Hot Water Control Examples](example-api/hotwater_examples.md)

### REST API

- Read OpenTherm values via `/api/v2/` endpoints. v0 and v1 have been removed (return 410 Gone).
- **v2 API**: RESTful-compliant endpoints with consistent JSON errors, proper HTTP status codes, CORS support, and resource-based naming. See [API Documentation](docs/api/README.md).
- Send OTGW commands via `POST /api/v2/otgw/commands` (JSON body) or the alias `POST /api/v2/otgw/command/{cmd}` (URL-based).
- **Manage Dallas sensor labels** via `GET/POST /api/v2/sensors/labels` — fetch and update custom names for temperature sensors.
  - **Bulk operations only**: Frontend manages label lookup and modification using read-modify-write pattern.
  - **File-based storage**: Labels stored in `/dallas_labels.ini` with zero backend RAM usage.
- Full OpenAPI specification: [docs/api/openapi.yaml](docs/api/openapi.yaml).
- **Health check endpoint:** `GET /api/v2/health` — returns device status including uptime, heap usage, and operational status. Used by OTA flash verification to confirm device is fully operational after reboot.

**OTA Flash Improvements (v1.0.0-rc7):**
The firmware flash mechanism has been simplified for improved reliability:

- **Firmware upload** (`/update?cmd=0`) and **filesystem upload** (`/update?cmd=100`) endpoints now block until flash operations complete
- Backend returns HTTP 200 only after flash write succeeds, preventing false-success detection
- Frontend uses `/api/v2/health` health check polling to verify device is fully operational (status: "UP")
- **Removed:** Real-time WebSocket flash progress updates (simplified for reliability; flash operations complete in 10-30 seconds)
- See [ADR-029](docs/adr/ADR-029-simple-xhr-ota-flash.md) for architectural rationale

### TCP serial socket (OTmonitor compatible)

- Exposes a TCP socket on port `25238` for OTmonitor and other tools that speak the OTGW serial protocol.

### Extra sensors

- Dallas temperature sensors (e.g. DS18B20/DS18S20/DS1822) with Home Assistant discovery support.
  - **Custom labels**: Click sensor names in the Web UI to assign friendly labels (max 16 characters).
  - **Graph visualization**: Sensors appear automatically in the real-time graph with 16 unique colors per theme.
  - **File-based storage**: Labels stored in `/dallas_labels.ini` file with zero backend RAM usage.
  - **Bulk API**: GET/POST `/api/v2/sensors/labels` for fetching and updating all labels at once.
  - **Automatic backup & restore**: Labels automatically backed up before filesystem flash and restored after reboot.
- S0 pulse counter for kWh meters on a configurable GPIO.

#### Important Note for Dallas Sensors (v1.0.0)

**Breaking Change**: In previous versions (< v1.0), a bug in the code generated Dallas DS18B20 sensor IDs that were shorter than the standard format (e.g., `2FE7983B8` instead of `28F0E979970803B8`).

Version 1.0.0 fixes this bug, ensuring that all sensors now report their correct, unique 16-character hexadecimal address.

**However**, this change breaks existing Home Assistant automations that rely on the old, truncated IDs. To support users upgrading from older versions without forcing a reconfiguration, we have added a **Legacy Format** setting:

- **Standard (Default)**: Use the correct 16-character IDs. Recommended for new installations.
- **Legacy Mode**: Check **"GPIO Sensors Legacy Format"** in Settings. This safely emulates the old truncated ID format, restoring compatibility with existing Home Assistant entities from older firmware versions.

## Connectivity options

This firmware provides multiple ways to connect and interact with your OpenTherm Gateway:

| Port | Protocol | Purpose | Usage |
| --- | --- | --- | --- |
| 80 | HTTP | Web Interface & REST API | Access the configuration interface at `http://<ip>` |
| 23 | Telnet | Debug & Logging | Connect for real-time debugging: `telnet <ip>` |
| 25238 | Serial over TCP | OTGW Serial Interface | For OTmonitor app or Home Assistant OpenTherm Gateway integration: `socket://<ip>:25238` |
| MQTT | MQTT | Home Automation Integration | Recommended for Home Assistant (auto-discovery enabled) |

The firmware also exposes a Wi-Fi configuration portal (AP mode) when it cannot connect to a saved network.
Use it to set up Wi-Fi credentials and reboot into normal STA mode.

### Home Assistant integration

There are two ways to integrate with Home Assistant:

1. **Recommended: MQTT Auto-Discovery** - Configure MQTT settings in the Web UI. Home Assistant will automatically discover all sensors and controls — including heating, cooling, solar thermal, DHW, ventilation, and more. Full discovery details in [RELEASE_NOTES_1.2.0.md](RELEASE_NOTES_1.2.0.md).

2. **Alternative: OpenTherm Gateway Integration** - Use Home Assistant's [OpenTherm Gateway integration](https://www.home-assistant.io/integrations/opentherm_gw/) with the connection string: `socket://<ip>:25238` (where `<ip>` is your device's IP address).

   **Important:** Use port `25238`, not port `23`. Port 23 is for debugging only and will not work with the Home Assistant integration.

## Important warnings / breaking changes

- **Breaking changes (v1.2.0-beta):** OpenTherm v4.2 MQTT/HA alignment updates include `FanSpeed` HA entity split (`setpoint/actual`, `Hz`), `RelativeHumidity` payload format correction, suppression of legacy IDs `50-63` on v4.x systems in default `AUTO` mode, and source-specific MQTT/HA topic path normalization to nested `<metric>/<source>` paths (manual retained-topic cleanup recommended after upgrade). See [docs/fixes/opentherm-v42-mqtt-breaking-changes.md](docs/fixes/opentherm-v42-mqtt-breaking-changes.md).
- **Breaking change (v1.0.0):** Default GPIO pin for Dallas temperature sensors changed from **GPIO 13 (D7)** to **GPIO 10 (SD3)**. This aligns with the OTGW hardware default. If you're upgrading from a previous version and have sensors connected to GPIO 13, you'll need to either:
  - Reconnect your sensors to GPIO 10, OR
  - Change the GPIO pin setting back to 13 in the Settings page
- **REST API v0/v1 removed (v1.2.0):** Only `/api/v2/` remains. Requests to `/api/v0/` or `/api/v1/` return 410 Gone. See [docs/api/README.md](docs/api/README.md) for the v2 endpoint reference.
- **New API endpoint (v1.0.0):** A new optimized `/api/v2/otgw/otmonitor` endpoint uses a map-based JSON format. See [API_CHANGES_v1.0.0.md](example-api/API_CHANGES_v1.0.0.md) for details.
- **Do not flash OTGW PIC firmware over Wi-Fi using OTmonitor.** You can brick the PIC. Use the built-in PIC firmware upgrade feature instead (based on code by Schelte Bron).
- **Breaking change (v0.8.0):** MQTT topic naming conventions changed; MQTT-based integrations may need updates.
- **Breaking change (v0.7.2+):** LittleFS is used; switching required a full reflash via USB and settings are not preserved.

## Roadmap ideas

- InfluxDB client for direct logging
- Broader live Web UI updates beyond the OT message log (e.g. live sensor/value cards)

## Release notes

For release artifacts, see <https://github.com/rvdbreemen/OTGW-firmware/releases>. A running summary is kept below.

| Version | Release notes |
| --- | --- |
| 1.3.0-beta | **Triple-reset WiFi recovery, one-shot PIC commands, PS=1 parsing, heap status, and reliability fixes.**<br>• Triple-reset (3× within 10s) clears WiFi credentials and opens captive portal — no re-flash needed<br>• Command bar on OpenTherm Monitor for raw PIC commands with live response in the log<br>• PS=1 summary fields fully parsed — MQTT published + HA auto-discovery per field<br>• Heap status (free heap, max block, health level) in `/api/v2/device/info` and flash page<br>• Boot fix — no spurious MQTT/NTP/mDNS restarts on first `loop()` iteration after `readSettings()`<br>• String heap fragmentation eliminated from settings save path; REST API v2 100% complete<br>No breaking changes. Full notes: [RELEASE_NOTES_1.3.0.md](RELEASE_NOTES_1.3.0.md) |
| 1.2.0-beta | **Comprehensive Home Assistant discovery for all OpenTherm message types (309 configs, 80+ IDs).**<br>• OpenTherm v4.2 alignment — missing IDs added, directions/types/units corrected<br>• Source-separated MQTT/HA discovery (`mqttseparatesources`) with `<metric>/thermostat\|boiler\|gateway` paths<br>• Webhook support — HTTP call on OpenTherm status bit change, configurable URL/payload/type<br>• REST API v0/v1 removed — only `/api/v2/` remains (410 Gone for v0/v1)<br>• OT serial buffer 256→512, safe overflow discard, richer WebSocket event logging<br>• Mobile-responsive shared nav shell and `index_common.css`<br>**Migration**: MQTT topic renames, device-info key changes — see [RELEASE_NOTES_1.2.0.md](RELEASE_NOTES_1.2.0.md) |
| 1.1.0-beta | **Dallas sensor labels & graphs, RESTful API v2 (13 endpoints), and 20-bug codebase review.**<br>• Dallas Custom Labels — inline Web UI editing, `/dallas_labels.ini`, zero RAM, backup/restore on flash, REST API `GET/POST /api/v2/sensors/labels`<br>• REST API v2 — 13 new endpoints, consistent JSON errors, HTTP 202 for async, CORS, RESTful naming; compliance 5.4→8.5/10<br>• 20 bugs resolved: OOB write, stack overflow, MQTT hour bitmask, −127°C sensor, ISR race (S0), XSS, FD leak, null-ptr crash, blocking sensor read, GPIO output feature, flash wear 20→1 writes<br>• Heap monitoring — 4-level health system with adaptive throttling and WebSocket backpressure<br>• WebUI `localStorage` persistence, browser debug console (`otgwDebug`), non-blocking modals<br>No breaking API or MQTT changes. Full notes: [RELEASE_NOTES_1.1.0.md](RELEASE_NOTES_1.1.0.md) |
| 1.0.0 | **Milestone release: modern UI, real-time graphs, WebSocket streaming, and robust integration.**<br>• Real-time graphs & statistics for boiler data (temperatures, setpoints, DHW)<br>• Dark mode, responsive design, redesigned File System Explorer<br>• Live OpenTherm message log via WebSocket (port 81)<br>• Gateway mode detection (`PR=M`, 30s interval); `NTPsendtime` setting<br>• MQTT: Outside Temperature override auto-discovery; static 1350-byte buffer for heap stability<br>• Critical binary fix (Exception 2 during PIC flashing), rewritten Wi-Fi logic, CSRF protection<br>**Breaking**: Dallas sensor default pin GPIO 13→10 (SD3) |
| 0.10.3 | Web UI: Mask MQTT password field and support running behind a reverse proxy (auto-detect http/https)<br>Home Assistant: Improve discovery templates (remove empty unit_of_measurement and add additional sensors/boundary values)<br>Fix: Status functions and REST API status reporting<br>CI: Improved GitHub Actions build/release workflow and release artifacts. |
| 0.10.2 | Bugfix: issue #213 which caused 0 bytes after update of PIC firwmare (dropped to Adruino core 2.7.4)<br>Update to filesystem to include latest PIC firmware (6.5 and 5.8, released 12 march 2023)<br>Fix: Back to correct hostname to wifi (credits to @hvxl)<br>Fix: Adding a little memory for use with larger settings. |
| 0.10.1 | Beter build processes to generate consistant quality using aruidno-cli and github actions (Thx to @hvxl and @DaveDavenport)<br>Maintaince to sourcetree, removed cruft, time.h library, submodules<br>Fix: parsing VH Status Master correctly<br>Enhancement: Stopping send time commands on detections of PS=1 mode<br>Fix: Mistake in MQTT configuration of auto discovery template for OEM fault code<br>Added wifi quality indication (so you can understand better)<br>Remove: Boardtype, as it was static in compiletime building |
| 0.10.0 | Updated: Added support fox 6.x firmware (pic16f1847) (Thanks to @hvxl / Schelte Bron)<br>Added reporting of "firmware type"<br>Improved: DHCP can override NTP settings now<br>Improved: Sending SC command on the minute (00 second), after reset ESP all commands (SR 21, SR 22) are resend<br>Bugfix: bitwise not bytewise AND operation for ASF flags OEM codes<br>Readout S0 output from configurable GPIO, interupt rtn added for this, enhanced Dallas-type sensor logic (autoconfigure, code cleanup) (Thanks to @RobR)<br>Web UI improvements by @rlagerwij and @Nicole |
| 0.9.5 | Improved: WebUI improved by community<br>Bugfix: Device Online status indicator for Home Assistant<br>Improved: Update of 5.x series (pic16f88) firmwares, preparing for 6.x (pic16f1847) updates.<br>Bugfix: Prevent spamming OTGW firmware website in case of rebootloop<br>Added: Unique useragent |
| 0.9.4 | Update: New firmware included gateway version 5.3 for PIC P16F88.<br>Update: Preventing >5.x PIC firmwares to be detected, incompatible (for now) |
| 0.9.3 | Bugfix: Small buffer of serial input, broke the PS=1 command, causing integrations of Domoticz and HA to break<br>Added: Setting for HA reboot detections, this enables a user to change the behaviour of HA reboot detection<br>Bugfix: PIC version detection fixed<br>Improving: Top topics parsing broke with 0.9.2, now you can once more use "/Myhome/OTGW/" as your toptopics |
| 0.9.2 | New feature: Just In Time Home Assistant Auto Discovery topics. Now only sensors that actually have msgids from OpenTherm are send to Home Assistant. (thanks to @rlagerweij)<br>Improvement: Climate Entity (Home Assistant) got improved to detect Thermostat availablity (by @sergantd)<br>Bugfix: Alternating values on status bits (thanks @binsentsu)<br>Bugfix: Blue blinking leds of nodemcu should be off using WebUI (thanks @fsfikke)<br>New feature: Reset wifi button in webUI (thanks @DaveDavenport)<br>Improved: More UI improvements (thanks @rlagerweij)<br>Improved: Serial handling improvements<br>Fixed: Codecleanup (removal of errorprone string functions), removal of potential bufferoverflow, removed all warnings in code compile (thanks @DaveDavenport)<br>Improved: Reboot logging, now includes external watchdog reason. |
| 0.9.1 | New feature: Added new set commands topics for most OTGW features, read more on the wiki<br>New feature: Reset bootlog to filesystem, for debug purposes<br>Improved: Stability, due to removal of ESP based auto-wifi-reconnect<br>Improved: the OT decoding algoritm, so values on MQTT, REST and WebUI now should be more reliable<br>Added: Override decoding of B and T when followed by A and R of the same MsgID, because this means OTGW overrides messages<br>Improved: No messages on versions when not connected to internet<br>Added: Proper msgid 100: remote override room setpoint flags decoding<br>Added: Missing some msgids to OT decoding |
| 0.9.0 | New: Adding time setup commands for Thermostat<br>Fixed: Improved OT status (incl. VH and Solar) message decoding<br>Fixed: Statusbit decoding in webUI<br>Improved: Better wifi auto-reconnect (ESP based)<br>Improved: Wifi reconnection logic, reboot if 15 min not connected<br>New: NTP hostname setting in webUI<br>Changed: removed ezTime NTP library, moved to ConfigTime NTP and AceTime |
| 0.8.6 | Improving wifi reconnect (without reboot)<br>Fix: Double definition to a HA sensor<br>Adding: OEMDiagnosticCode topic to HA Discovery<br>Bugfix: UI now labels OEM DiagnosticCode correctly, and added the real OEM Fault code |
| 0.8.5 | Bugfix: Queue bug never sending the command (reporter: @jvinckers)<br>Small improvement to status parsing, only resturned status from slave gets parsed now. |
| 0.8.4 | Adding MsgID for Solar Storage<br>Verbose Status parsing for Ventlation / Heatrecovery<br>Adding msgid 113/114 unsuccessful burnerstart / flame too low<br>Added smartpower configruation detection<br>Added 2.3 spec status bits for (summer/winter time, dhw blocking, service indicator, electric production)<br>Adding PS=1 detection (WebUI notification)<br>Fix: restore settings issue |
| 0.8.3 | New feature: Unique ID is configurable (thanks to @RobR)<br>New feature: GPIO pins follow status bits (master/slave) (thanks to @sjorsjuhmaniac)<br>Improved: Detecting online status of thermostat and boiler<br>Improved: MQTT Debug error logging<br>Fixed bug: reconnect MQTT timer and changed wait for reconnect to 42 seconds<br>Added: Rest API command now uses queues for sending commands<br>Fixed bug: msgid 32/33 type switch around<br>Changed: Solar Storage and Collector now proper names (breaking change) |  
| 0.8.2 | Added: Command Queue to MQTT command topic<br>Bugfix: Values not updating in WebUI fixed<br>Added: verbose debug modes<br>Added check for littlefs githash<br>Added: Interval setting for sensor readout<br>Adding: Send OTGW commands on boot<br>Bugfix: Hostname now actually changes if needed. |  
| 0.8.1 | Improved ot msg processing<br>MQTT: added `otgw-firmware/version`, `otgw-firmware/reboot_count`, `otgw-firmware/version` and `otgw-firmware/uptime` (seconds)<br>Bugfix: typoo in topic name `master_low_off_pomp_control_function` -> `master_low_off_pump_control_function`<br>Bugfix: Home Assistant thermostat operation mode (flame icon) template<br>Feature: Add support for Dallas temperature sensors, defaults GPIO10, pushes data to `otgw-firmware/sensors/<Dallas-sensor-ID>` |
| 0.8.0 | **Breaking Change: MQTT topic naming convention has changed from `<mqtt top prefix>/<sensor>` to `<mqtt top prefix>/value/<node id>/<sensor>` for data published and `<mqtt top prefix>/set/<node id>/<command>` for subscriptions**<br>Update Home Assistant Discovery: add OTGW as a device and group all exposed entities as children<br>Update Home Assistant Discovery: add climate (thermostat) entity, uses temporary temperature override (OTGW `TT` command) (Home Assistant Core v2021.2.0+)<br>Bugfix #14: reduce MQTT connect timeout < the watchdog timeout to prevent reboot on a timeout<br>Adding LLMNR responder (<http://otgw/> will work now too)<br>New REST API: Telegraf endpoint (/api/v1/otgw/telegraf)<br>Fixing bugs in core OTGW message processor for ASF flags |
| 0.7.8 | Update Home Assistant Discovery<br>Flexible Home Assistant prefix<br>Bugfix: Removed hardcoded OTGW topic<br>Bugfix: NTP timezone discovery removed |
| 0.7.7 | UI improved: Only show updates values in web UI<br>Bugifx: Serial not found error when sending commands thru MQTT fixed |
| 0.7.6 | PIC firmware integration done.<br>New setting: NTP configurable<br>New setting: heartbeat led on/off<br>Update to REST API to include epoch of last update to message |
| 0.7.5 | Complete set of status bits in UI and Central Heating 2 information |
| 0.7.4 | Integration of the otgw-pic firmware upgrade code - upgrade to pic firmware version 5.0 (by Schelte Bron) |
| 0.7.3 | Adding MQTT disable/enable option<br>Adding MQTT long password (max. 100 chars)<br>Adding executeCommand API (verify and return response for commands)<br>Added uptime and otgw fwversion in devinfo UI |
| 0.7.2 | **Breaking change: Moving over to LittleFS. This means you need to reflash your device using a USB cable.** |
| 0.7.1 | Adding reset gateway to enter self-programming mode more reliable.<br>Changed to port 25238 for serial TCP connections (default of OTmonitor application by Schelte Bron)<br>Bugfix: Settings UI works even with "browserplugins". Thanks @STemplar |
| 0.7.0 | Added all Ventilation/Heat Recovery msgids (2.3b OT spec). Plus Remeha msgids. Thanks @STemplar<br>Added OTGW pic reset on bootup.<br>Translate dutch to english.<br>Bugfix: Serial flushing & writebuffer checking to prevent overflow during flashing. |
| 0.6.1 | Bugfix: setting page did not always work correctly, now it does. |
| 0.6.0 | Standalone UI for simple OT monitor purposes and deviceinformation, moved index.html to SPIFF<br>OTA is possible after flashing 0.6.0 (Hardware watchdog is fed, during flash uploads now) |
| 0.5.1 | REST APIs, v1, for OTmonitor values, GetByLabel, GetByID, POST `otgw/command/{command}` |
| 0.5.0 | Implemented the UI for settings (restapi, read/write file in json) |
| 0.4.2 | Bi-directional serial communication on port 25238 (aka ser2net) for use with OTmonitor application |
| 0.4.0 | RestAPI implemented - as simple as `<ip>/api/v0/otgw/{id}` to get the latest values |
| 0.3.1 | Bug: Open AP after configuration, change ESP to STA mode on StartWifi<br>No more default Debug to Serial, only to port 23 telnet |
| 0.3.0 | Read only Serial stream implementend on port 25238 (debug port remains on port 23 - telnet) |
| 0.2.0 | Auto-discovery through MQTT implemented for integration with Home Assistant |
| 0.1.0 | MQTT messaging implemented |
| 0.0.1 | parsing of OT protocol implemented (use telnet to see)<br>Watchdog feeding implemented |

</details>

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
