# OTGW-firmware v2.0.0

[![Join the Discord chat](https://img.shields.io/discord/812969634638725140.svg?style=flat-square)](https://discord.gg/zjW3ju7vGQ)
[![GitHub release](https://img.shields.io/github/v/release/rvdbreemen/OTGW-firmware?style=flat-square)](https://github.com/rvdbreemen/OTGW-firmware/releases)
[![License: GPLv3](https://img.shields.io/badge/License-GPLv3-blue.svg?style=flat-square)](LICENSE)

OTGW-firmware turns the NodoShop OpenTherm Gateway into a networked smart heating controller. It runs on the ESP32 Wi-Fi module inside the OTGW device, monitors the OpenTherm bus between your thermostat and boiler, and connects your entire heating system to your home automation platform via MQTT, a browser-based web interface, and a REST API. With v2.0.0, the firmware also runs natively on the OTGW32 board with an ESP32, adding direct GPIO OpenTherm control, Ethernet, BLE room sensors, and a OLED display.

---

## What's New in v2.0.0

Version 2.0.0 is a major platform release. It targets the ESP32/OTGW32 platform; the ESP8266 is no longer a 2.0.0 build target. The firmware builds from a single source tree via PlatformIO. If you are coming from a v1.x ESP8266 build, see [Migrating from ESP8266 (1.x)](#migrating-from-esp8266-1x).

### Highlights

- **OTGW32 / ESP32 platform**: Native ESP32 support. The OTGW32 board combines an ESP32 with onboard OpenTherm circuitry so the PIC gateway chip is optional. Build with `pio run -e esp32`.
- **OTDirect**: ESP32-only direct GPIO OpenTherm master/slave. Five operating modes let the ESP32 act as thermostat, boiler, gateway, monitor, or a combined master+slave pair, all without a PIC co-processor.
- **Ethernet (W5500)**: Wired Ethernet via W5500 SPI on the OTGW32 board. The firmware auto-detects cable presence and switches between Ethernet and Wi-Fi, with transparent failover in both directions.
- **BLE temperature sensors**: On ESP32, up to four Xiaomi LYWSD03MMC sensors are read passively over Bluetooth LE using the BTHome v2 protocol. Room temperature and humidity feed directly into SAT and Home Assistant.
- **OLED display**: 128x64 SSD1306 I2C display on both platforms. Four information pages (status, temperatures, network, boiler). Button cycles pages; display auto-off after configurable timeout.
- **SAT enhancements**: Summer Simmer Index, improved solar gain compensation, expanded multi-zone support, and BLE sensor integration.
- **250+ Home Assistant entities**: Full climate entity, all SAT entities, pressure monitoring, BLE sensor entities, and OLED status — all via MQTT auto-discovery.
- **Wi-Fi resilience**: AP fallback mode (beta) keeps the web UI reachable when the home network is unavailable. Wi-Fi signal quality indicator in the web UI header. Triple-reset credential recovery.
- **PlatformIO build**: `platformio.ini` builds the `esp32` environment (default). The whole codebase builds from a single source tree.

**One small MQTT breaking change**: three OT-bus presence topics (`boiler_connected`, `thermostat_connected`, `otgw_connected`) moved out of the hardware-specific `otgw-pic/*` and `otgw-otdirect/*` subtrees into the generic value namespace, regardless of hardware variant. Home Assistant users are unaffected (entity `unique_id`s are stable, discovery auto-republishes). The firmware self-heals retained payloads on the deprecated topics at first MQTT reconnect. Custom MQTT consumers should update their topic patterns: see the [migration guide](docs/api/MQTT.md#migration-from-14x--pre-release-200-ot-bus-state-topics) and [ADR-084](docs/adr/ADR-084-generic-ot-bus-state-topics.md). REST API and `settings.ini` format are unchanged from v1.x.

---

## Features at a Glance

### Core features

- Full OpenTherm bus monitoring: all 80+ message IDs decoded in real time
- MQTT integration with 250+ Home Assistant auto-discovery entities
- Web interface: live OT log, real-time graphs, dark/light theme, settings panel
- REST API v2 with consistent JSON and proper HTTP status codes
- SAT (Smart Autotune Thermostat): weather-compensated PID heating controller
- DS18B20 Dallas temperature sensors with custom labels (up to 16)
- S0 pulse-counting energy meter
- SSD1306 OLED display (128x64, I2C, 4 pages)
- OTA firmware and filesystem updates via the web interface
- TCP serial bridge on port 25238 (OTmonitor, HA built-in integration)
- Telnet debug log on port 23
- Webhook callbacks on OpenTherm status bit changes
- Triple-reset Wi-Fi credential recovery
- Optional HTTP Basic Auth for settings and maintenance endpoints

### OTGW32 hardware features

- OTDirect: direct GPIO OpenTherm with five operating modes (no PIC required)
- W5500 Ethernet with auto-failover to/from Wi-Fi
- BLE room temperature and humidity sensors (Xiaomi/BTHome v2, up to 4)
- AP fallback mode (beta): web UI stays reachable when home network is down

---

## Hardware Support

### Platform overview

| Feature | ESP32 OTGW (with PIC) | ESP32 / OTGW32 |
|---|---|---|
| OpenTherm via PIC co-processor | Yes (required) | Yes (optional) |
| OpenTherm via direct GPIO (OTDirect) | No | Yes |
| Wi-Fi | Yes | Yes |
| Ethernet (W5500) | No | Yes |
| BLE sensors | No | Yes (up to 4) |
| OLED display | Yes | Yes |
| SAT thermostat | Yes | Yes |
| Dallas sensors | Yes | Yes |
| S0 pulse counter | Yes | Yes |

### Migrating from ESP8266 (1.x)

Firmware v1.x ran on the ESP8266 module that shipped on earlier NodoShop OTGW boards:

| NodoShop OTGW version | ESP8266 module (v1.x) |
|---|---|
| 1.x through 2.0 | NodeMCU ESP8266 |
| 2.3 and later | Wemos D1 mini ESP8266 |

The ESP8266 is no longer a 2.0.0 build target. To move an existing OTGW to v2.0.0, swap the Wemos D1 mini ESP8266 for a LOLIN S3 Mini (ESP32-S3) drop-in: it keeps the same OTGW board and PIC co-processor, and the firmware builds for it from the same source tree. Settings files from v1.3.x load without modification.

The OTGW32 is a separate board from NodoShop with an ESP32 and onboard OpenTherm interface. It can run with or without the PIC firmware chip installed.

---

## SAT: Smart Autotune Thermostat

SAT is an embedded heating controller that runs entirely on the device. It replaces the room thermostat's role in controlling the boiler: it computes the optimal boiler flow temperature from outdoor temperature and room temperature error, learns your heating system's behaviour over time, and keeps your home at the target temperature without needing Home Assistant or any external controller to be involved in the control loop.

### What SAT does

SAT combines a **weather-compensated heating curve** with a **PID v3 controller**. The heating curve calculates a base flow setpoint from outdoor temperature using a configurable coefficient. The PID loop adds a correction on top, based on the difference between actual and target room temperature. Over several days, the auto-tune function analyses heating cycles and adjusts PID gains automatically.

### Key capabilities

- **Weather compensation**: configurable heating curve coefficient; curve recommendation based on error statistics
- **PID v3 control**: proportional + integral + derivative correction with automatic gain tuning
- **Two control modes**: continuous modulation (direct boiler modulation) and PWM flame cycling (on/off duty cycle)
- **Heating system types**: radiators, underfloor heating, heat pump
- **Six presets**: comfort, eco, away, sleep, activity, home, each with a configurable target temperature
- **Multi-zone room temperature**: averages readings from up to four sources (OT, MQTT, BLE, Dallas)
- **BLE room sensor** (ESP32 only): reads temperature and humidity from a Xiaomi LYWSD03MMC via BTHome v2
- **Solar gain compensation**: detects solar gain from indoor temperature rise rate and sun elevation angle; reduces setpoint to prevent overheating
- **Summer Simmer Index**: calculates perceived outdoor heat; suppresses heating when the index stays above a threshold
- **Summer suppression**: independent outdoor-temperature-based heating cutoff with configurable duration threshold
- **Pressure monitoring**: tracks system pressure; raises alarm on low pressure or fast pressure drop
- **OPV calibration**: finds the boiler's Overpressure Valve temperature so SAT stays below it
- **Six safety layers**: flame health, CH circuit sync, setpoint mismatch, pressure, cycle classification, overshoot detection
- **Open-Meteo integration**: optional free weather API for outdoor temperature when no local source is available
- **OpenWeatherMap (OWM) support**: optional API key for outdoor temperature fallback, rate limited to one call per 15 minutes

### Quick start

1. Open the OTGW web interface and go to **SAT** in the navigation.
2. Enable SAT and set your target room temperature.
3. Select your heating system type (radiators / underfloor / heat pump).
4. Enter your boiler capacity in kW.
5. Set an initial heating curve coefficient (start with 1.0 for radiators, 0.6 for underfloor; the curve recommendation will guide further tuning).
6. Save. SAT takes over the flow setpoint immediately. Let auto-tune run for a few days to optimize PID gains.

For detailed setup including OPV calibration and Home Assistant automation examples, see the [SAT integration guide](backlog/docs/doc-3%20-%20sat-integration-guide.md).

### Home Assistant integration

When SAT is enabled and MQTT is configured, Home Assistant receives:

- A `climate` entity with target temperature control and heat/off mode selection
- 40+ sensor and binary_sensor entities for SAT state, diagnostics, and settings
- BLE sensor entities (ESP32 only): temperature and humidity per sensor

Commands via MQTT or the REST API (`/api/v2/sat/*`) control all SAT parameters at runtime.

| Reference | Link |
|---|---|
| MQTT topics | [docs/api/MQTT.md](docs/api/MQTT.md) |
| Full SAT topic list | [backlog/docs/doc-1 - sat-mqtt-topics.md](backlog/docs/doc-1%20-%20sat-mqtt-topics.md) |
| REST API | [docs/api/openapi.yaml](docs/api/openapi.yaml) |
| OPV calibration guide | [backlog/docs/doc-2 - sat-opv-calibration.md](backlog/docs/doc-2%20-%20sat-opv-calibration.md) |
| Preset configuration | [backlog/docs/doc-4 - sat-preset-configuration.md](backlog/docs/doc-4%20-%20sat-preset-configuration.md) |

---

## Quick Start

### 1. Flash the firmware

**Recommended** (downloads and flashes the latest release):

```bash
python3 flash_esp.py
```

Build from source and flash:

```bash
pio run -e esp32 --target upload    # ESP32 / OTGW32
```

See [docs/FLASH_GUIDE.md](docs/FLASH_GUIDE.md) for full instructions including filesystem flashing.

### 2. Connect to Wi-Fi

On first boot the device creates an access point named `<hostname>-<mac>`. Connect to it. A captive portal opens where you enter your home Wi-Fi credentials. On ESP32/OTGW32 with a W5500 and an Ethernet cable plugged in, wired Ethernet is used directly and the Wi-Fi setup is optional.

### 3. Open the Web UI

Navigate to `http://<device-ip>/` or `http://otgw.local/`. The dashboard shows the live OpenTherm log, temperatures, and boiler status.

### 4. Configure MQTT

Go to **Settings** and fill in your MQTT broker details. Enable **HA Discovery**. Click **Save**. Home Assistant discovers the device within seconds.

### 5. Check Home Assistant

Go to **Settings > Devices and Services > MQTT**. The OTGW device appears with 250+ entities already created and ready to use.

---

## Setting Up MQTT with Home Assistant

### Prerequisites

- Home Assistant with the MQTT integration installed (Settings > Devices and Services > MQTT).
- An MQTT broker running (Mosquitto add-on, or an external broker).
- Your OTGW device connected to the same network.

### Step 1: Configure MQTT in the web UI

Open `http://<device-ip>/` and go to **Settings**.

| Setting | Description | Example |
|---|---|---|
| MQTT Broker | IP or hostname of your broker | `192.168.1.100` |
| MQTT Port | Broker port | `1883` |
| MQTT User | Broker username | `mqttuser` |
| MQTT Password | Broker password | (your password) |
| MQTT Top Topic | Prefix for all published topics | `OTGW` |
| MQTT Unique ID | Identifier for this device in HA | `otgw` |
| HA Discovery | Enables auto-discovery payloads | checked |

### Step 2: Verify in Home Assistant

After saving, Home Assistant discovers the OTGW device. Go to **Settings > Devices and Services > MQTT** to see all entities: temperatures, setpoints, modulation, flame, DHW, faults, and more. No manual YAML required.

### Step 3: Tune the publish interval

By default, **MQTT Publish On-Change** (Settings > MQTT) is enabled: the gateway publishes immediately when an OpenTherm value changes and republishes unchanged values every `60` seconds as a heartbeat. This keeps data fresh while avoiding the high MQTT traffic of publishing every repeated OpenTherm frame.

With **MQTT Publish On-Change** enabled the gateway will publish immediately when a value **changes** and re-publish unchanged values once per **MQTT Interval** as a heartbeat (so Home Assistant does not mark sensors as unavailable). A value of `10`-`60` seconds is a good starting point. Unticking **MQTT Publish On-Change** restores legacy behaviour: every OpenTherm frame is published as it arrives. When upgrading from a release that published every frame (`MQTTinterval=0`), the gateway migrates that setting once to `60` seconds with Publish On-Change enabled.

### Step 4: Send commands from Home Assistant

The gateway subscribes to a command topic. Structure:

```
<TopTopic>/set/<UniqueId>/<command>
```

Common commands:

| Command | Description | Example payload |
|---|---|---|
| `setpoint` | Temporary temperature override (TT) | `21.5` |
| `constant` | Constant temperature override (TC) | `22.0` |
| `outside` | Override outside temperature (OT) | `15.5` |
| `hotwater` | DHW control: `0`=off, `1`=on, `P`=push, `A`=auto | `1` |
| `maxchsetpt` | Max CH water setpoint (SH) | `60` |
| `maxdhwsetpt` | Max DHW setpoint (SW) | `55` |

Example: sync outdoor temperature from another HA sensor:

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

More examples: [Outside Temperature Override](example-api/outside_temperature_override_examples.md) | [Hot Water Control](example-api/hotwater_examples.md)

### Alternative: built-in HA OpenTherm Gateway integration

The Home Assistant [OpenTherm Gateway integration](https://www.home-assistant.io/integrations/opentherm_gw/) can connect directly via the TCP serial bridge without MQTT:

```
socket://<device-ip>:25238
```

This provides basic thermostat control but does not expose the full range of data that MQTT auto-discovery covers.

---

## Connectivity Summary

| Port | Protocol | Purpose |
|---|---|---|
| 80 | HTTP | Web UI and REST API |
| 81 | WebSocket | Live OpenTherm log stream |
| 23 | Telnet | Debug log output |
| 25238 | TCP | OTGW serial bridge (OTmonitor, HA built-in integration) |
| 1883 | MQTT (outbound) | Home automation integration |

The device also exposes a Wi-Fi configuration portal (AP mode) on first boot or after triple-reset recovery.

---

## Security Notes

The web UI and APIs are designed for use on a trusted local network. Do not expose the device directly to the internet. Use a VPN for remote access. A reverse proxy works for the HTTP/REST interface, but WebSocket (live log) requires plain `ws://` and does not work through an HTTPS-terminating proxy.

### Protected endpoints (optional)

Set an **endpoint password** in Settings (`httppasswd`) to require HTTP Basic Auth for:

- Reading and changing device settings
- File upload, download, and delete
- Reboot and wireless reset
- OTA firmware and filesystem updates
- Webhook test

Live monitoring, sensor values, and the WebSocket stream remain open without authentication.

---

## Documentation

| Topic | Link |
|---|---|
| Wiki (recommended starting point) | <https://github.com/rvdbreemen/OTGW-firmware/wiki> |
| Flash guide | [docs/FLASH_GUIDE.md](docs/FLASH_GUIDE.md) |
| Local build guide | [docs/BUILD.md](docs/BUILD.md) |
| REST API reference | [docs/api/README.md](docs/api/README.md) |
| OpenAPI specification | [docs/api/openapi.yaml](docs/api/openapi.yaml) |
| MQTT topic reference | [docs/api/MQTT.md](docs/api/MQTT.md) |
| WebSocket protocol | [docs/api/WEBSOCKET_FLOW.md](docs/api/WEBSOCKET_FLOW.md) |
| Dallas sensor labels API | [docs/api/DALLAS_SENSOR_LABELS_API.md](docs/api/DALLAS_SENSOR_LABELS_API.md) |
| Webhook documentation | [docs/features/webhook.md](docs/features/webhook.md) |
| Dallas sensor documentation | [docs/features/dallas-temperature-sensors.md](docs/features/dallas-temperature-sensors.md) |
| Architecture Decision Records | [docs/adr/README.md](docs/adr/README.md) |
| C4 architecture context | [docs/c4/c4-context.md](docs/c4/c4-context.md) |
| Code quality checker | [docs/EVALUATION.md](docs/EVALUATION.md) |
| Upgrading from 0.x | [docs/upgrade-from-0.x.md](docs/upgrade-from-0.x.md) |
| Release notes | [docs/releases/](docs/releases/) |

---

## Important Warnings

- **Do not flash PIC firmware over Wi-Fi using OTmonitor.** This can brick the PIC microcontroller. Use the built-in PIC firmware upgrade feature in the web UI instead.
- **Dallas GPIO default changed in v1.0.0**: The default pin moved from GPIO 13 (D7) to GPIO 10 (SD3). If upgrading from a version before v1.0.0, verify your wiring or change the setting back to 13.
- **REST API v0/v1 removed in v1.2.0**: Only `/api/v2/` endpoints remain. See the [REST API reference](docs/api/README.md).
- **MQTT topic corrections in v1.2.0**: A few spelling fixes changed topic names (`eletric_production` to `electric_production`, etc.). Delete orphaned Home Assistant entities and let discovery recreate them. Details: [breaking changes](docs/fixes/opentherm-v42-mqtt-breaking-changes.md).
- **ESP32 OTDirect note**: When OTDirect is active, the PIC UART is bypassed. Do not attempt PIC firmware upgrades while OTDirect is the active OpenTherm path.

---

## History and Scope

The OpenTherm Gateway hardware and PIC firmware originate from **Schelte Bron's OTGW project** (<https://otgw.tclcode.com/>). This firmware builds on that ecosystem by running on the ESP32 inside the NodoShop OTGW to expose the gateway's data and controls over the network.

Primary hardware targets are the NodoShop OTGW with an ESP32-S3 (LOLIN S3 Mini) and the NodoShop OTGW32 (ESP32). Earlier ESP8266 boards ran firmware v1.x; see [Migrating from ESP8266 (1.x)](#migrating-from-esp8266-1x). Other boards may work, but these are the tested and supported configurations.

---

## Community and Support

- Discord: <https://discord.gg/zjW3ju7vGQ>
- Issues and bug reports: <https://github.com/rvdbreemen/OTGW-firmware/issues>
- GitHub Releases (prebuilt binaries): <https://github.com/rvdbreemen/OTGW-firmware/releases>

---

## Credits

Reaching v2.0.0 would not have been possible without years of community testing, feedback, and contribution. Thank you to everyone who has reported issues, pushed for features, and helped others in the Discord.

Special thanks to:

- **@hvxl (Schelte Bron)** for all his work on the OTGW hardware, PIC firmware, and ESP coding, and for providing access to the PIC upgrade routines that make reliable PIC firmware flashing possible. If you want to thank Schelte for his work, head over to <https://otgw.tclcode.com/> and donate.
- **@sjorsjuhmaniac** for improving the MQTT naming convention and Home Assistant integration, and for the climate entity and OTGW device model.
- **@GeorgeZ83** for improving Home Assistant MQTT integration and climate entity support across many releases.
- **@DaveDavenport** for fixing known and unknown codebase issues and making the firmware substantially more stable.
- **@DutchessNicole** for improving the Web UI over multiple releases.
- **@vampywiz17** for early adoption and thorough testing.
- **@Stemplar** for early issue reporting.
- **@proditaki** for creating the Domoticz plugin for OTGW-firmware.
- **@tjfsteele** for endless hours of testing.
- **@RobR** for the S0 pulse counter implementation.

And to everyone who keeps reporting issues, pushing for more, and helping others in the community.

---

## Buy Me a Coffee

[![Buy me a coffee](https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20coffee&emoji=&slug=rvdbreemen&button_colour=5F7FFF&font_colour=ffffff&font_family=Cookie&outline_colour=000000&coffee_colour=FFDD00)](https://www.buymeacoffee.com/rvdbreemen)

---

## License

GNU General Public License v3.0. See [LICENSE](LICENSE).

Relicensed from MIT to GPLv3 on 2026-07-06. This applies to the project's own code (Robert van den Breemen, sole copyright holder); third-party vendored libraries (`src/libraries/OpenTherm`, `src/libraries/OTGWSerial`) and the LGPL-2.1-licensed portions of `FSexplorer.ino` keep their original licenses and copyright notices unchanged.
