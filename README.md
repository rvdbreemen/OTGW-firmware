# OTGW-firmware (ESP8266) for NodoShop OpenTherm Gateway

[![Join the Discord chat](https://img.shields.io/discord/812969634638725140.svg?style=flat-square)](https://discord.gg/zjW3ju7vGQ)

This repository contains the **ESP8266 firmware for the NodoShop OpenTherm Gateway (OTGW)**. It runs on the ESP8266 "devkit" that is part of the NodoShop OTGW and turns the gateway into a standalone network device.

**Current version: v1.3.5** -- [Release notes](RELEASE_NOTES_1.3.5.md) | [Previous releases](docs/releases/)

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
