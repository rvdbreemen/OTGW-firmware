v2.0.0 is the biggest release in the project's history: a unified ESP8266/ESP32 codebase, an embedded smart heating controller (SAT), native OpenTherm bus mastering on ESP32, wired Ethernet, OLED display, and BLE temperature sensors.

[Full release notes](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/releases/RELEASE_NOTES_2.0.0.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [API docs](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/api/README.md) | [MQTT topics](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/api/MQTT.md)

## Highlights

### SAT: Smart Autotune Thermostat

SAT turns your OTGW into a standalone embedded heating controller. It runs entirely on the ESP chip, needs no internet connection for core operation, and is disabled by default.

- **Weather-compensated heating curve**: calculates the boiler flow temperature needed to hold your target room temperature, based on the outdoor temperature. Outdoor temp comes from the OT bus, an MQTT push, or the Open-Meteo API (no API key required).
- **PID v3 controller**: auto-calculates its own gains from the heating curve. Deadband-gated integral prevents windup. PID state persists across reboots.
- **PWM and continuous modes**: SAT can duty-cycle the boiler (PWM) or run it continuously at a modulated setpoint. It switches between modes automatically based on observed modulation reliability.
- **Anti-cycling**: enforces minimum OFF time and configurable maximum cycles-per-hour to protect the burner igniter.
- **OPV calibration**: automated overshoot measurement after each cycle so the boiler stops at the right moment. Requires 40 samples before accepting a result.
- **Six safety layers**: sensor staleness timeout, OT read failure threshold, boiler fault detection, window detection, enable/disable flag, and cold-start-disabled default.
- **Presets**: Away, Eco, Comfort, Sleep, Activity, Home — available via MQTT or REST API.
- **Multi-zone room temperature**: up to 4 input zones with configurable weights for a single weighted room temperature.
- **CH pressure monitoring**: EMA smoothing plus linear regression drop-rate detection with confirmation hysteresis.
- **Solar gain compensation**: reduces flow setpoint on sunny days based on sun elevation angle.
- **Summer Simmer Index**: uses indoor humidity to compute thermal comfort and inhibit unnecessary heating.
- **Gas consumption estimation**: modulation-based kW and kWh tracking published to MQTT.
- **Thermal drop learning**: fallback mode uses a learned heat-loss coefficient when MQTT connectivity is lost.
- **250+ Home Assistant auto-discovery entities**: climate entity, temperature sensors, PID diagnostics, presets, settings, cycle statistics, pressure health, and more.
- **Four-view WebUI dashboard**: Thermostat, Expert, Diagnostics, and Settings views. Live WebSocket updates.

### BLE Temperature Sensors (ESP32 only)

On OTGW32 (ESP32), SAT can read room temperature from Bluetooth Low Energy sensors using BTHome v2 advertisements. Xiaomi LYWSD03MMC with custom ATC firmware is the reference device. Up to 4 sensors, one per zone, with staleness tracking.

### OTDirect: Native OpenTherm on ESP32

OTGW32 can drive the OpenTherm bus directly, without the PIC, using hardware timer Manchester encoding. Five modes: Gateway, Monitor, Bypass, Master, and Loopback. Includes PI room compensation and flame ratio tracking. Controlled via REST API (`/api/v2/otdirect/*`) and MQTT.

### Dual-Platform Build (ESP8266 and ESP32)

A single codebase now compiles for both platforms. `boards.h` maps GPIO pins per board variant. `platform.h` abstracts serial, filesystem, and timing. `HAS_PIC`, `HAS_DIRECT_OT`, and `HAS_ETH_CAPABLE` feature flags gate platform-specific code. PlatformIO is the primary build system; the existing `build.py` / Arduino IDE path continues to work for ESP8266.

### Ethernet Support (ESP32)

W5500 SPI Ethernet on OTGW32. DHCP automatic, MAC derived from the ESP32 eFuse. Automatic failover between Ethernet and WiFi. All firmware services work identically over both interfaces.

### OLED Display (both platforms)

128x64 SSD1306 I2C OLED. Four pages: Dashboard (room/flow/target/flame), HC1, HC2, and System Status (IP, uptime, RSSI, heap). Button cycles pages on short press. 30-second auto-off.

### Network and WiFi Improvements

- **WiFi signal quality bars** in the Web UI header, live from RSSI.
- **WiFi SSID display and Reset WiFi button** in the Settings page.
- **NTP bogus time guard**: rejects the 0xFFFFFFFF sentinel the ESP8266 SDK emits before the first sync.
- **Beta: AP fallback mode** when saved credentials are unavailable (pre-release builds only).

## Breaking changes

### OT-bus state moved to generic MQTT topics

Three OT-bus presence values previously published under hardware-specific subtrees (`otgw-pic/*` on PIC-based gateways, `otgw-otdirect/*` on OTGW32) are now published under the generic value namespace, regardless of hardware variant. The former `otgw-otdirect/ot_online` topic has been retired; the same concept now lives under `otgw_connected`.

**Removed topics:**

- `OTGW/value/<uniqueId>/otgw-pic/boiler_connected`
- `OTGW/value/<uniqueId>/otgw-pic/thermostat_connected`
- `OTGW/value/<uniqueId>/otgw-pic/otgw_connected`
- `OTGW/value/<uniqueId>/otgw-otdirect/boiler_connected`
- `OTGW/value/<uniqueId>/otgw-otdirect/thermostat_connected`
- `OTGW/value/<uniqueId>/otgw-otdirect/ot_online`

**New canonical topics:**

- `OTGW/value/<uniqueId>/boiler_connected`
- `OTGW/value/<uniqueId>/thermostat_connected`
- `OTGW/value/<uniqueId>/otgw_connected`

Payload semantics are unchanged: `"ON"` / `"OFF"`, retained.

**What you need to do:**

- **Home Assistant users**: nothing. Entity `unique_id`s are stable, discovery auto-republishes on reconnect, and history is preserved. On OTGW32 builds without a PIC, `Boiler connected` and `Thermostat connected` entities now appear for the first time (they were previously gated behind the PIC flag).
- **Custom MQTT consumers** (Node-RED, openHAB, scripts) subscribed to the old topics: update your topic patterns to the new canonical paths. The firmware auto-cleans retained payloads on the deprecated topics at first reconnect, so no manual broker cleanup is required in the typical case. If you want to clean up manually, see the `mosquitto_pub` one-liner in the [MQTT migration guide](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/api/MQTT.md#migration-from-14x--pre-release-200-ot-bus-state-topics).

Rationale: these values describe what the OTGW-firmware observes on the OpenTherm bus, not properties of the PIC coprocessor or the OT-direct driver. Grouping them under the hardware-specific subtrees made them disappear from Home Assistant on OTGW32 builds without a PIC, and forced custom consumers to switch topic prefixes when the underlying hardware changed. See ADR-084 (amends ADR-065) and the [MQTT migration guide](https://github.com/rvdbreemen/OTGW-firmware/blob/main/docs/api/MQTT.md#migration-from-14x--pre-release-200-ot-bus-state-topics) for the full story.

## Bug fixes

- **OTGW Answer Thermostat messages** now published to the correct boiler MQTT source topic.
- **NTP last-sync timestamp** no longer corrupted by bogus SDK 0xFFFFFFFF sentinel.
- **ESP8266 and ESP32 build failures** after the platform merge: all resolved.
- **SAT PROGMEM stack overflow** in `weatherSendStatusJSON`: stacked 556-byte local buffers replaced with static allocation.
- **SAT PROGMEM pointer passed to LittleFS** in `satMigrateFile`: fixed to RAM buffer.
- **SAT PID deadband default** corrected to 0.1°C; manual gains mode restored.
- **SAT continuous mode setpoint clamp bypass** corrected for flame-off transitions.
- **SAT weather HTTP client null pointer** on timeout: guarded before use.
- **SAT diagnostics CSS grid** classes `.sat-grid` and `.sat-row` added.
- **`collectHeaders` variadic guard** restored for ESP8266 Core 3.x compatibility.

## Upgrade notes

- **Breaking changes vs v1.3.5:** see the [Breaking changes](#breaking-changes) section above. Only three MQTT topic paths changed (OT-bus presence values); REST endpoints and `settings.ini` format are unchanged. Home Assistant users are unaffected; custom MQTT consumers should update their topic patterns.
- **Flash both firmware and filesystem** (OLED support and SAT WebUI require the updated filesystem).
- **Hard-refresh the browser** after flashing (Ctrl+F5).
- **SAT is disabled by default.** Enable via `set/{nodeId}/sat/enabled = 1` over MQTT, or via the Settings page.
- **ESP32 / OTGW32 users:** build with PlatformIO (`pio run -e esp32 -t upload && pio run -e esp32 -t uploadfs`). Verify GPIO pin assignments in `boards.h` against your actual hardware.

## Thank you

Special thanks to **George Dellas (SergeantD)** for the SAT algorithm design, testing, and countless hours of review feedback, and to **Alex Wijnholds (Alexwijn)** for the original SAT Python concept and heating curve foundation that made this port possible.

Thank you to everyone who tested betas, filed issues, and pushed the project forward.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.

If you want to support the project: [Buy me a coffee](https://www.buymeacoffee.com/rvdbreemen)

---

Previous release: [v1.3.5](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.3.5)
