## Chapter 10: Appendix

---

### A. OpenTherm Message ID Quick Reference

The OpenTherm protocol defines message IDs (0-127) for all data exchanged between a thermostat (master) and a boiler (slave). Each message ID carries a 16-bit data word whose interpretation depends on the data type. The firmware decodes all standard message IDs and publishes them to MQTT and the WebSocket stream.

Direction: M→S = master (thermostat) to slave (boiler). S→M = slave to master. Both = appears in both directions.

Data types:
- **f8.8**: Fixed-point float, 8-bit integer + 8-bit fraction. Range: -128.0 to +127.996. Resolution: 1/256 ≈ 0.004°C.
- **u8/u8**: Two unsigned 8-bit values packed in one 16-bit word (high byte / low byte).
- **s8/s8**: Two signed 8-bit values.
- **u16**: Unsigned 16-bit integer.
- **s16**: Signed 16-bit integer.
- **flag8/flag8**: Two 8-bit flag fields (master flags / slave flags). Individual bits are decoded separately.
- **flag8/u8**: High byte = flag field, low byte = unsigned integer.
- **u8**: Single unsigned 8-bit value in the low byte.

#### Most Commonly Used Message IDs

| ID | Label | Data Type | Direction | Description |
|----|-------|-----------|-----------|-------------|
| 0 | Status | flag8/flag8 | Both | Master status bits (CH enable, DHW enable, cooling enable, OTC active, CH2 enable, summer/winter mode, DHW blocking) and slave status bits (fault, CH mode, DHW mode, flame, cooling, CH2 mode, diagnostic) |
| 1 | TSet | f8.8 | M→S | Control setpoint: CH water temperature requested by thermostat (°C) |
| 2 | MasterConfig | flag8/u8 | M→S | Master configuration flags and member ID code |
| 3 | SlaveConfig | flag8/u8 | S→M | Slave configuration flags (DHW present, modulating, cooling, DHW config, pump control) and member ID code |
| 5 | ASFflags | flag8/u8 | S→M | Application-specific fault flags and OEM fault code |
| 6 | RBPflags | flag8/flag8 | Both | Remote boiler parameter transfer-enable and read/write flags |
| 7 | CoolingControl | f8.8 | M→S | Cooling control signal (0-100%) |
| 9 | TrOverride | f8.8 | Both | Remote override room setpoint (°C) — used by gateway |
| 14 | MaxRelModLevelSetting | f8.8 | M→S | Maximum relative modulation level setting (%) |
| 16 | TrSet | f8.8 | M→S | Room setpoint (°C) — requested room temperature |
| 17 | RelModLevel | f8.8 | S→M | Relative modulation level (%) |
| 18 | CHPressure | f8.8 | S→M | CH water system pressure (bar) |
| 19 | DHWFlowRate | f8.8 | S→M | DHW flow rate (l/min) |
| 24 | Tr | f8.8 | M→S | Room temperature (°C) — measured by thermostat |
| 25 | Tboiler | f8.8 | S→M | Boiler flow water temperature (°C) |
| 26 | Tdhw | f8.8 | S→M | DHW temperature (°C) |
| 27 | Toutside | f8.8 | Both | Outside temperature (°C) — sent by thermostat or overridden |
| 28 | Tret | f8.8 | S→M | Return water temperature (°C) |
| 31 | TstorageTank | f8.8 | S→M | Solar storage tank temperature (°C) |
| 33 | TExhaust | s16 | S→M | Boiler exhaust temperature (°C) |
| 35 | HeatExchanger | f8.8 | S→M | Boiler heat exchanger outlet temperature (°C) |
| 48 | TdhwSetUBTdhwSetLB | s8/s8 | S→M | DHW setpoint upper and lower bounds (°C) |
| 49 | MaxTSetUBMaxTSetLB | s8/s8 | S→M | Max CH setpoint upper and lower bounds (°C) |
| 56 | TdhwSet | f8.8 | Both | DHW setpoint (°C) — requested / confirmed |
| 57 | MaxTSet | f8.8 | Both | Max CH water setpoint (°C) |
| 70 | MasterMemberIDCode | flag8/u8 | M→S | Master (thermostat) configuration and member ID |
| 100 | RemoteOverrideFunction | flag8 | S→M | Remote override function flags |
| 115 | OEMDiagnosticCode | u16 | S→M | OEM/manufacturer-specific diagnostic code |
| 116 | BurnerStarts | u16 | Both | Burner start count |
| 117 | CHPumpStarts | u16 | Both | CH pump start count |
| 120 | BurnerOperationHours | u16 | Both | Burner operating hours |
| 121 | CHPumpOperationHours | u16 | Both | CH pump operating hours |

The full list of all 128 message IDs, including solar storage (IDs 101-103), ventilation and heat recovery (IDs 70-78), RF sensors (IDs 98-99), and vendor-specific IDs, is in the OpenTherm Protocol Specification v4.2: `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`. The project-specific message ID mapping is in `docs/opentherm specification/OpenTherm-specifications.md`.

---

### B. Glossary

| Term | Definition |
|------|-----------|
| **ADR** | Architecture Decision Record. A short document capturing a significant design choice, its rationale, and its consequences. All ADRs live in `docs/adr/`. Once Accepted, an ADR is immutable. |
| **AceTime** | Arduino timezone library used by the firmware for NTP time management and zone-aware timestamps. |
| **ArduinoJson** | A popular Arduino JSON parsing library. Explicitly excluded from this project (ADR-042) due to RAM and flash cost. JSON is handled via manual `snprintf_P` construction and `parseJsonKVLine()` parsing. |
| **BLE** | Bluetooth Low Energy. On ESP32 hardware (OTGW32), the firmware can receive room temperature and humidity from a BTHome-compatible BLE sensor (e.g., a Xiaomi sensor). |
| **boards.h** | Board-specific pin mapping header. Defines GPIO assignments per hardware variant (e.g., `BOARD_NODOSHOP_ESP32`). Located at `src/OTGW-firmware/boards.h`. |
| **BTHome** | An open BLE advertisement protocol for home sensor devices. Used by the SAT BLE integration for passive scanning of room temperature sensors. |
| **C4 model** | A four-level architecture documentation model: Context, Container, Component, Code. The firmware's architecture is documented using C4 in `docs/c4/`. |
| **CH** | Central Heating. Refers to the radiator or underfloor heating circuit, as opposed to DHW. |
| **CMSG_SIZE** | 512-byte general-purpose scratch buffer constant. Most REST API response buffers and single MQTT message buffers use this size. |
| **DallasTemperature** | Arduino library for DS18B20 1-Wire temperature sensors. Used by `sensorStuff.ino`. |
| **DHW** | Domestic Hot Water. The circuit supplying hot tap water, as distinct from the central heating circuit. |
| **DUE()** | A timer macro from `safeTimers.h`. Returns true when the declared timer's interval has elapsed, and false otherwise. Handles `millis()` rollover safely. |
| **ESP8266** | The Espressif Wi-Fi microcontroller used in the NodoShop OTGW board. Runs at 80 or 160 MHz, approximately 40 KB usable heap. |
| **ESP32** | The more powerful Espressif microcontroller used in the NodoShop OTGW32 board. Runs at 240 MHz, significantly more RAM and flash, supports BLE, Ethernet, and OTDirect. |
| **f8.8** | OpenTherm fixed-point data format: an 8-bit signed integer plus an 8-bit fractional part (1/256 per LSB). Used for all temperature values in the protocol. |
| **Frame Bridge** | The abstraction layer (ADR-065) that provides a uniform OpenTherm frame interface regardless of whether the underlying transport is PIC UART (ESP8266) or OTDirect GPIO (ESP32). |
| **HA** | Home Assistant. The primary target home automation platform for this firmware. |
| **HEAP_CRITICAL** | Heap level below ~3 KB. Emergency mode: MQTT and WebSocket sends are blocked. |
| **HEAP_HEALTHY** | Heap level above ~8 KB. Normal operation. |
| **HEAP_LOW** | Heap level 5–8 KB. MQTT publish rate reduced. |
| **HEAP_WARNING** | Heap level 3–5 KB. WebSocket sends blocked; MQTT aggressively throttled. |
| **JIT discovery** | Just-in-time Home Assistant discovery. Rather than publishing all 200+ HA discovery payloads at once on connect, the firmware publishes a discovery payload for a given message ID the first time it receives data for that ID. This spreads the load over the first few minutes of operation. |
| **LittleFS** | The flash filesystem used by the firmware for storing settings, web assets, discovery configs, sensor labels, SAT state, and crash logs. Mounted on the ESP8266/ESP32 SPI flash. |
| **LLMNR** | Link-Local Multicast Name Resolution. Used for Windows name resolution of `otgw.local` without a DNS server. |
| **mDNS** | Multicast DNS. Allows the device to be reached as `otgw.local` (or custom hostname + `.local`) on the local network without a DNS server. |
| **MQTT** | Message Queuing Telemetry Transport. The lightweight publish-subscribe protocol used for integration with Home Assistant and other automation platforms. The firmware supports MQTT 3.1.1 via the PubSubClient library. |
| **NTP** | Network Time Protocol. Used by the firmware for accurate wall-clock time (boiler clock sync, SAT calculations, log timestamps). Default server: `pool.ntp.org`. |
| **NodeMCU** | An ESP8266 development board. Functionally equivalent to Wemos D1 mini for this firmware. |
| **OT** | OpenTherm. The communication protocol used between room thermostats and central heating boilers. |
| **OTGW** | OpenTherm Gateway. The NodoShop hardware device that sits in-line on the OpenTherm bus and allows monitoring and override of messages. Refers to both the hardware and to this firmware project. |
| **OTGW32** | The NodoShop OpenTherm Gateway board based on ESP32 with direct GPIO OpenTherm interface (OTDirect), W5500 Ethernet, BLE, and OLED. |
| **OTDirect** | The ESP32-native direct GPIO OpenTherm bus interface. Replaces the PIC co-processor on OTGW32 boards. Implemented in `OTDirect.ino` and guarded by `#if HAS_DIRECT_OT`. Supports gateway, monitor, bypass, master, and loopback operating modes (ADR-064). |
| **OPV** | Overshoot Protection Value. SAT calibration parameter (ADR-076) that limits flow temperature overshoot during burner ignition by capping the initial setpoint boost. |
| **PIC** | The PIC16F88 or PIC16F1847 microcontroller co-processor on the NodoShop OTGW board. Handles the OpenTherm electrical bus and communicates with the ESP over UART at 9600 baud. |
| **PROGMEM** | Program Memory. On ESP8266, string literals declared with `PROGMEM` (or wrapped in `F()` / `PSTR()`) are stored in flash memory instead of RAM, conserving the scarce ~40 KB RAM budget. |
| **PubSubClient** | The Arduino MQTT client library used by the firmware. Pinned to version 2.8.0 in `platformio.ini`. |
| **platform.h** | The unified platform abstraction header (ADR-061). Provides `#define` guards and typedefs that isolate ESP8266-specific and ESP32-specific code behind a single interface. |
| **PlatformIO** | Cross-platform embedded development toolchain and IDE plugin. Used for building the ESP32 target and optionally ESP8266. Config: `platformio.ini`. |
| **SAT** | Smart Autotune Thermostat. The embedded heating controller in the firmware that replaces the room thermostat's role. Combines a weather-compensated heating curve with a PID v3 control loop and automatic gain tuning. |
| **SLINE_SIZE** | 1200-byte MQTT autoconfig line buffer. Used exclusively in `MQTTstuff.ino` for `mqttha.cfg` processing. Larger than `CMSG_SIZE` to accommodate HA discovery JSON lines up to ~900 bytes. |
| **SNTP** | Simple Network Time Protocol. UDP-based time synchronization, subset of NTP used for embedded devices. Port 123. |
| **SSD1306** | OLED display controller used on OTGW32 hardware. Driven by the `SSD1306Ascii` library. Displays device status at boot. |
| **W5500** | SPI Ethernet chip used on OTGW32 hardware for wired Ethernet connectivity. Controlled by the `EthernetESP32` library. |
| **WifiManager** | Arduino library that provides the captive portal for initial Wi-Fi configuration. When no credentials are saved, the device creates a temporary access point for setup. Pinned to version 2.0.17. |
| **yield()** | Arduino/ESP8266 cooperative scheduling call. Allows the Wi-Fi/TCP stack to process pending events. Calling `yield()` from within a background task handler can cause re-entrancy. The firmware carefully avoids `yield()` in `feedWatchDog()` for this reason. |

---

### C. Default Settings

These are the factory defaults. All settings are configurable via the web UI Settings page or `POST /api/v2/settings`.

#### Device Settings

| Setting key | Default | Description |
|-------------|---------|-------------|
| `hostname` | `OTGW` | mDNS hostname (device accessible at `otgw.local`) |
| `httppassword` | (empty) | HTTP Basic Auth password. Empty = no authentication. |
| `ledblink` | `true` | LED heartbeat blink enabled |
| `darktheme` | `false` | Web UI dark theme |

#### Network Settings

| Setting key | Default | Description |
|-------------|---------|-------------|
| `ntptimezone` | `Europe/Amsterdam` | IANA timezone for clock display |
| `ntphost` | `pool.ntp.org` | NTP server hostname |

#### MQTT Settings

| Setting key | Default | Description |
|-------------|---------|-------------|
| `mqttenable` | `false` | MQTT client enabled |
| `mqttbroker` | (empty) | MQTT broker hostname or IP |
| `mqttport` | `1883` | MQTT broker TCP port |
| `mqttuser` | (empty) | MQTT username |
| `mqttpassword` | (empty) | MQTT password |
| `mqtttoptopic` | `OTGW` | Top-level MQTT namespace |
| `mqttuniqueid` | `otgw-{MAC}` | Unique device ID embedded in topic paths |
| `mqtthaprefix` | `homeassistant` | HA auto-discovery prefix |
| `mqtthadiscovery` | `false` | Home Assistant auto-discovery enabled |
| `mqttotmessage` | `false` | Publish raw OT frame strings to `otmessage` topic |
| `mqttinterval` | `0` | Force-publish interval in ms (0 = on-change only) |
| `mqttseparatesources` | `false` | Source-separated subtopics enabled |

#### GPIO / Sensor Settings

| Setting key | Default | Description |
|-------------|---------|-------------|
| `gpiosensorsenable` | `false` | Dallas DS18B20 sensor scanning enabled |
| `gpiosensorspin` | `14` (D5) | 1-Wire bus GPIO pin |
| `gpiosensorsinterval` | `30` | Sensor scan interval in seconds |
| `s0enable` | `false` | S0 pulse counter enabled |
| `s0pin` | `12` (D6) | S0 input GPIO pin |
| `s0pulsesperkwh` | `1000` | Pulses per kWh (meter calibration) |

#### SAT Settings

| Setting key | Default | Description |
|-------------|---------|-------------|
| `satenable` | `false` | SAT thermostat enabled |
| `sattarget` | `20.0` | Target room temperature (°C) |
| `satheatingcurvecoeff` | `1.5` | Heating curve slope coefficient |
| `satboilercapacity` | `24.0` | Boiler nominal capacity (kW) |
| `satsumthreshold` | `18.0` | Summer simmer outdoor temperature threshold (°C) |
| `satdeadband` | `0.25` | PID deadband (°C) |
| `satcontrolinterval` | `30` | Control loop interval (seconds) |
| `satautotune` | `false` | Automatic PID gain tuning enabled |
| `satheatingsystem` | `1` | 0=auto, 1=radiators, 2=heat pump, 3=underfloor |

---

### D. Port Reference

| Port | Transport | Direction | Service |
|------|-----------|-----------|---------|
| 80 | TCP | Inbound | HTTP: web UI, REST API v2, file serving, OTA upload |
| 81 | TCP | Inbound | WebSocket: live OpenTherm log stream and graphs |
| 23 | TCP | Inbound | Telnet: debug console output (read-only) |
| 25238 | TCP | Inbound | TCP serial bridge: raw OTGW PIC serial stream |
| 1883 | TCP | Outbound | MQTT: connection to broker |
| 123 | UDP | Outbound | SNTP: NTP time synchronization |
| 5353 | UDP | Both | mDNS: `{hostname}.local` name resolution |
| 5355 | UDP | Both | LLMNR: Windows LAN name resolution |

All inbound services are unencrypted (HTTP, not HTTPS; WS, not WSS). The device is designed for trusted local network use only. For remote access, use a VPN or a reverse proxy that handles TLS termination.

---

### E. Useful Links

#### Hardware

- NodoShop OpenTherm Gateway (ESP8266): [https://www.nodo-shop.nl/](https://www.nodo-shop.nl/)
- NodoShop OpenTherm Gateway 32 (ESP32): see the NodoShop product page for the OTGW32 board

#### Firmware

- GitHub repository: [https://github.com/rvdbreemen/OTGW-firmware](https://github.com/rvdbreemen/OTGW-firmware)
- Releases and changelogs: [https://github.com/rvdbreemen/OTGW-firmware/releases](https://github.com/rvdbreemen/OTGW-firmware/releases)
- Issue tracker: [https://github.com/rvdbreemen/OTGW-firmware/issues](https://github.com/rvdbreemen/OTGW-firmware/issues)

#### Home Assistant Integration

- Home Assistant OTGW integration (HACS): [https://github.com/mvdwetering/hass-otgw](https://github.com/mvdwetering/hass-otgw)
- Home Assistant MQTT integration documentation: [https://www.home-assistant.io/integrations/mqtt/](https://www.home-assistant.io/integrations/mqtt/)
- Home Assistant MQTT discovery: [https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery)

#### OpenTherm Protocol

- OpenTherm Protocol Specification v4.2: included in this repository at `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`
- OpenTherm Association: [https://www.opentherm.eu/](https://www.opentherm.eu/)
- PIC firmware source (reference): `other-projects/otgw-6.6/` in this repository
- OTmonitor reference client (Tcl/Tk): `other-projects/otmonitor-6.6/` in this repository

#### Community

- Tweakers forum thread (Dutch, primary community): search for "OTGW-firmware" on [https://tweakers.net/](https://tweakers.net/)
- Discord server: community link available in the GitHub repository README

#### Libraries Used

- PubSubClient (MQTT): [https://github.com/knolleary/pubsubclient](https://github.com/knolleary/pubsubclient)
- WiFiManager: [https://github.com/tzapu/WiFiManager](https://github.com/tzapu/WiFiManager)
- AceTime: [https://github.com/bxparks/AceTime](https://github.com/bxparks/AceTime)
- TelnetStream: [https://github.com/jandrassy/TelnetStream](https://github.com/jandrassy/TelnetStream)
- WebSockets (Links2004): [https://github.com/Links2004/arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets)
- DallasTemperature: [https://github.com/milesburton/Arduino-Temperature-Control-Library](https://github.com/milesburton/Arduino-Temperature-Control-Library)
- EthernetESP32: [https://github.com/JAndrassy/EthernetESP32](https://github.com/JAndrassy/EthernetESP32)
- OTGWSerial (Schelte Bron): included in `libraries/OTGWSerial/`

---

### F. Changelog Summary

The full changelog is in `CHANGELOG.md` at the repository root.

#### v2.0.0 (current)

Version 2.0.0 is a major release. The headline changes are:

**Dual platform support.** A single codebase now targets both ESP8266 (NodoShop OTGW) and ESP32 (NodoShop OTGW32). A unified platform abstraction layer (`platform.h`) isolates hardware differences. The ESP32 build includes OTDirect, W5500 Ethernet, BLE, and OLED features that are absent on ESP8266.

**OTDirect (OTGW32).** On ESP32, the PIC co-processor is replaced by a direct GPIO OpenTherm bus interface using interrupt-driven Manchester coding. OTDirect supports gateway, monitor, bypass, master, and loopback operating modes. It provides full OT-Thing protocol parity including the command scheduler, stored response overrides, and response modifiers.

**SAT (Smart Autotune Thermostat).** The embedded PID thermostat is now a major feature. Version 2.0.0 adds PID v3 with deadband and anti-cycling, a heating curve advisor (INCREASE/DECREASE/HOLD), multi-zone room temperature averaging, Summer Simmer Index, solar gain compensation, DHW setpoint control, boiler gas consumption estimation, and BLE room temperature sensor support (ESP32).

**Settings/State architecture (ADR-051).** Over 62 global variables have been replaced by the `OTGWSettings settings` and `OTGWState state` structs. This eliminates naming collisions and makes the persistent-versus-transient boundary explicit.

**W5500 Ethernet (OTGW32).** Hardware Ethernet with automatic WiFi/Ethernet failover. When an Ethernet cable is connected, WiFi is disabled. The `isNetworkUp()` / `getActiveIP()` abstraction provides a consistent network interface to all subsystems.

**AP fallback mode (beta only).** When no saved WiFi is available, the device opens a fallback access point rather than going fully offline. Guarded by `_VERSION_PRERELEASE`.

**MQTT routing fix.** OTGW "Answer Thermostat" messages (A-prefix frames) are now correctly routed to the boiler source topic rather than the thermostat source topic.

**Async bitmap-driven MQTT discovery drip publisher.** HA auto-discovery payloads are now published asynchronously in small batches using a bitmap scheduler, avoiding the memory spikes caused by publishing all discovery messages at once on MQTT connect.

**Nightly restart.** A configurable scheduled restart can be enabled to reclaim heap memory over long uptimes. The restart hour and enable flag are exposed via the REST API and settings whitelist.

**16 new ADRs (061-076).** Covering the unified platform abstraction, OTGW32 hardware, OTDirect operating modes, SAT PID v3, heating curve algorithm, SAT MQTT topics, BLE platform compatibility, and WiFi reconnect timeout tuning. See section M for the full reference.

**Full CHANGELOG:** see `CHANGELOG.md` in the repository root.

#### v1.3.5

WiFi reconnection regression fix: the 5-second state machine timeout was too short for ESP8266 association (5-10 seconds on some routers). Timeout increased to 30 seconds, restoring reliable reconnection behavior.

#### v1.3.4

S0 pulse counter fixes and MQTT uptime publishing.

#### v1.3.0

Settings and state architecture migration (ADR-051), non-blocking WiFi reconnect (ADR-047), webhook state machine improvements (ADR-048).

#### v1.2.0

SAT initial integration, OTA update improvements, ETag-based browser caching for web assets.

#### v1.0.0

First stable release. Static buffer allocation (ADR-004), heap monitoring system, PROGMEM audit, WebSocket OT log streaming.

---

### G. Build Artifact Reference

When building, the following output files are produced:

| File | Description |
|------|-------------|
| `build/OTGW-firmware.ino.bin` | ESP8266 firmware binary for OTA or direct flash |
| `build/OTGW-firmware.ino.elf` | ELF debug file for crash address symbolication |
| `build/OTGW-firmware.ino.map` | Linker map (symbol addresses and section sizes) |
| `build/OTGW-firmware.ino.littlefs.bin` | LittleFS filesystem image (web assets, configs) |
| `.pio/build/esp32/firmware.bin` | ESP32 firmware binary (PlatformIO build) |
| `.pio/build/esp32/firmware.elf` | ESP32 ELF debug file |

#### Crash Address Symbolication

If the device crashes and prints a backtrace (ESP8266 exception dump), you can decode the stack addresses using the `.elf` file:

```bash
# Install xtensa-lx106 toolchain (included with ESP8266 Arduino core)
xtensa-lx106-elf-addr2line -fe build/OTGW-firmware.ino.elf 0x40215abc
```

For ESP32:
```bash
xtensa-esp32s3-elf-addr2line -fe .pio/build/esp32/firmware.elf 0x400d1abc
```

The ELF file is included in GitHub release artifacts specifically to support crash debugging in the field.

---

### H. PlatformIO Environment Summary

| Environment | Target | Chip | Framework | Flash | Filesystem |
|-------------|--------|------|-----------|-------|------------|
| `esp8266` | Wemos D1 mini / NodoShop OTGW | ESP8266 | Arduino Core 3.1.2 (espressif8266@4.2.1) | 4 MB (DIO) | 2 MB LittleFS |
| `esp32` | NodoShop OTGW32 | ESP32 | Arduino-ESP32 v3.3.5 (pioarduino fork) | ~4 MB (custom partition) | 768 KB LittleFS |

ESP32 partition layout (`partitions_otgw_esp32.csv`): two OTA application partitions of 1.5 MB each, plus 768 KB LittleFS. Safe OTA rollback is supported if an update fails to boot.

---

### I. Frequently Asked Questions

**The device connects to WiFi but MQTT is not working.**

Check that `mqttenable` is `true` and `mqttbroker` is set to the correct IP address or hostname of your MQTT broker. Verify with the Telnet debug console (port 23) that the MQTT state machine reaches `MQTT_STATE_IS_CONNECTED`. Common causes: wrong broker address, firewall blocking port 1883, incorrect MQTT credentials.

**Home Assistant entities appear but have no state.**

The firmware uses JIT discovery: entity payloads are published when the first OT message for each ID arrives. Wait a few minutes after connection for all entities to populate. If entities remain unavailable after 5 minutes, check that `mqtthadiscovery` is enabled and call `POST /api/v2/otgw/discovery` to force republishing.

**The device is stuck in AP mode (access point visible but not connecting to my network).**

The WifiManager captive portal is active. Connect to the `OTGW-{MAC}` access point and enter your WiFi credentials. If your network credentials changed, use the "Reset WiFi" button on the Settings page to clear saved credentials and re-run the setup portal.

**The Telnet debug output shows "Exception(2)" or "Exception(3)" crashes.**

Exception 2 is an invalid fetch (IRAM/DRAM address error). Exception 3 is a load/store alignment error. Both are most commonly caused by PROGMEM pointer misuse: passing a flash-resident pointer to a function that expects a RAM pointer. Check any recent `strcmp`, `sprintf`, or string literal usage added near the crash site. Use `strcmp_P` with `PSTR()`, never `strcmp` with `PROGMEM` strings directly.

**Sensors are not publishing to MQTT.**

Confirm `gpiosensorsenable` is `true` and `gpiosensorspin` matches the GPIO number where the 1-Wire bus is connected. The firmware scans for sensors at boot and on each interval. Check Telnet output for sensor scan results. Dallas sensors require a 4.7 kΩ pull-up resistor on the data line.

**The SAT setpoint is much higher than expected.**

Check the `satheatingcurvecoeff` setting. A coefficient that is too high causes the heating curve to demand excessive flow temperature. Start with `1.5` for radiator systems and `0.8` for underfloor heating, then adjust based on the `sat/curve_recommendation` MQTT topic.

**After a firmware update, the web UI shows a "version mismatch" warning.**

This means the firmware version and the LittleFS (web assets) version do not match. Flash the filesystem image separately: `pio run -e esp8266 -t uploadfs` or use the web UI Files page to upload the new `OTGW-firmware.ino.littlefs.bin`.

**The REST API returns 503 for every request.**

Free heap is below 4 KB. The firmware's heap gate blocks new HTTP connections in this state to prevent a crash. Reboot the device. If the problem recurs within minutes of boot, it usually indicates a memory leak introduced in a recent firmware update or a pathological MQTT publish loop. Connect the Telnet debug console and watch for heap stats.

---

### J. SAT Boiler Status and Cycle Class Reference

#### Boiler Status Codes (`sat/boiler_status`)

| Code | Name | Description |
|------|------|-------------|
| 0 | Off | Boiler fully off, no demand |
| 1 | Idle | Boiler on but not firing; CH demand pending |
| 2 | Preheating | Pre-heat phase before ignition |
| 3 | At Setpoint | Flow temperature at target; flame stable |
| 4 | Modulating Up | Modulation increasing toward setpoint |
| 5 | Modulating Down | Modulation decreasing toward setpoint |
| 6 | Ignition Surge | Initial ignition overshoot in progress |
| 7 | Stalled Ignition | Ignition attempted but flame not established |
| 8 | Anti-Cycling | Burner blocked to prevent short cycling |
| 9 | Pump Starting | CH pump starting before burner |
| 10 | Waiting Flame | Burner running; awaiting flame signal |
| 11 | Overshoot Cooling | Flow temperature above setpoint; cooling down |
| 12 | Post-Cycle | Post-purge phase after flame off |
| 13 | Heating | Normal steady heating operation |
| 14 | Cooling | Cooling mode active (heat pump systems) |

#### Cycle Class Values (`sat/cycle_class`)

| Value | Description |
|-------|-------------|
| `none` | No complete cycle yet observed |
| `good` | Cycle reached setpoint with acceptable overshoot |
| `overshoot` | Flow temperature exceeded setpoint + overshoot margin |
| `underheat` | Cycle ended before reaching setpoint; demand not met |
| `short` | Cycle duration too short (below minimum on-time); short cycling |
| `uncertain` | Insufficient data to classify |

#### Flame Status Values (`sat/flame_status`)

| Value | Description |
|-------|-------------|
| `insufficient_data` | Too few cycles to assess flame behaviour |
| `healthy` | Normal flame cycling pattern |
| `idle_ok` | No demand; flame correctly off |
| `stuck_on` | Flame on when no CH/DHW demand; boiler fault suspected |
| `stuck_off` | CH demand present but flame not lighting |
| `pwm_short` | PWM cycle too short; boiler not modulating as expected |
| `short_cycling` | Excessive on/off cycling; check heating curve coefficient |

---

### K. OpenTherm PIC Command Reference

These are the commands sent by the firmware to the PIC co-processor over UART (or to OTDirect on ESP32). Use them via `POST /api/v2/otgw/command` or the `command` MQTT topic.

#### Temperature Setpoint Commands

| Command | Description | Example |
|---------|-------------|---------|
| `TT=xx.xx` | Temporary room temperature setpoint override (°C). Thermostat still sends requests; gateway substitutes this value. | `TT=21.00` |
| `TC=xx.xx` | Constant room setpoint. Does not reset when thermostat cycles. | `TC=21.00` |
| `CS=xx.xx` | Direct CH control setpoint. Bypasses the thermostat setpoint entirely. | `CS=55.00` |
| `SH=xx` | Max CH water setpoint (°C). Limits the boiler's max flow temperature. | `SH=75` |
| `SW=xx` | Max DHW setpoint (°C). | `SW=60` |

#### Hot Water Commands

| Command | Description |
|---------|-------------|
| `HW=1` | Force hot water on |
| `HW=0` | Force hot water off |
| `HW=P` | One-time DHW push (comfort tap) |
| `HW=A` | Return to automatic (thermostat-controlled) DHW |

#### Gateway Mode Commands

| Command | Description |
|---------|-------------|
| `GW=1` | Gateway mode: intercept and substitute messages |
| `GW=0` | Monitor mode: transparent pass-through |
| `GW=S` | Standalone master mode (OTGW32 only) |
| `GW=M` | Monitor mode with full frame logging (OTGW32 extension) |
| `GW=L` | Loopback test mode (OTGW32 only) |
| `GW=R` | Full gateway reset (OTGW32 only) |

#### Override and Schedule Commands

| Command | Description |
|---------|-------------|
| `SR=ID:HHHH` | Set stored response for message ID (4-hex data word) |
| `CR=ID` | Clear stored response override |
| `RM=ID:HHHH` | Set response modifier mask |
| `CM=ID` | Clear response modifier |
| `AA=ID` | Re-enable a disabled schedule entry (MsgID 1-127) |
| `DA=ID` | Disable a schedule entry |
| `UI=ID` | Mark message ID as unknown (gateway responds UNKNOWN_DATA_ID) |
| `KI=ID` | Mark message ID as known again |
| `PM=ID` | Priority message: poll this ID on the next cycle |

#### System Commands

| Command | Description |
|---------|-------------|
| `PS=1` | Enable Print Summary mode (suppress frame-by-frame, emit CSV) |
| `PS=0` | Disable Print Summary mode |
| `SC=HH:MM/D` | Synchronize boiler clock. D=day of week 1-7 |
| `PR=A` | Query PIC banner (returns firmware identifier) |
| `PR=M` | Query current gateway mode |
| `PR=O` | Query current setpoint override |
| `RS=HBS` | Reset burner start counter (replace HBS with counter code) |
| `MM=xx` | Set maximum modulation level (%) |
| `OT=xx.xx` | Override outside temperature reported to boiler |

---

### L. Wiring and GPIO Pin Reference

#### ESP8266 (NodoShop OTGW) Pin Assignments

The NodoShop OTGW board uses a fixed pin assignment. Do not attempt to remap PIC serial or OTGW-critical pins.

| Function | GPIO | Arduino name | Notes |
|----------|------|-------------|-------|
| PIC UART TX | GPIO1 | TX | Serial to PIC (reserved, never write debug here) |
| PIC UART RX | GPIO3 | RX | Serial from PIC |
| PIC reset | GPIO5 | D1 | Active-low reset for PIC firmware upgrade |
| LED 1 (status) | GPIO2 | D4 | Active-low; used for heartbeat and activity |
| LED 2 (activity) | GPIO16 | D0 | Active-low; used during PIC upgrade |
| I2C SDA | GPIO4 | D2 | Shared I2C bus (OLED, sensors) |
| I2C SCL | GPIO5 | D1 | Shared I2C bus |
| Button | GPIO0 | D3 | Boot mode / factory reset |
| 1-Wire bus | GPIO14 | D5 | Default for Dallas DS18B20 sensors (configurable) |
| S0 pulse input | GPIO12 | D6 | Default for S0 energy meter (configurable) |
| GPIO relay | GPIO13 | D7 | Optional output relay (configurable) |

GPIO pin assignments for Dallas sensors, S0 counter, and the output relay are configurable via settings. Check for GPIO conflicts if using non-default pins (the firmware detects and warns about conflicts between sensors, S0, and output functions).

#### ESP32 (OTGW32) Pin Assignments

On OTGW32 hardware, pin assignments are defined in `boards.h` under the `BOARD_NODOSHOP_ESP32` guard. The OTGW32 uses OTDirect GPIO pins for the OpenTherm bus instead of the PIC UART. Refer to the OTGW32 hardware documentation for the exact pinout.

Key differences from ESP8266:
- OTDirect uses two GPIO pins for OpenTherm (one read, one write), driven by ISR.
- W5500 Ethernet uses SPI pins.
- OLED display uses I2C.
- BLE uses the integrated ESP32 radio; no additional hardware required.
- The PIC UART pins are absent (no PIC co-processor).

---

### M. Architecture Decision Record (ADR) Reference

All ADRs live in `docs/adr/`. An ADR captures a significant architectural choice, its context, alternatives considered, and consequences. Once Accepted, an ADR is immutable; changes require a new ADR that supersedes the old one.

The full ADR index with topic grouping and dependency diagrams is in `docs/adr/README.md`.

#### ADRs added since v1.3.5

These ADRs were introduced as part of the v2.0.0 development cycle (ESP32/OTGW32 dual-platform support, SAT thermostat, and related work):

| ADR | Title | Status |
|-----|-------|--------|
| 061 | Unified ESP8266/ESP32 Platform Abstraction | Accepted |
| 062 | SAT Smart Autotune Thermostat Integration | Accepted |
| 063 | OTGW32 Hardware Support | Accepted |
| 064 | OTDirect Operating Mode Architecture | Accepted |
| 065 | Frame Bridge Pattern | Accepted |
| 066 | Thermostat Auto-Detection Master Mode | Accepted |
| 067 | SSD1306Ascii OLED Library | Accepted |
| 068 | OTDirect Schedule Tuning Constants | Accepted |
| 069 | SAT PID v3 Implementation | Accepted |
| 070 | SAT Memory Allocation Strategy | Accepted |
| 071 | SAT Heating Curve Algorithm | Accepted |
| 072 | SAT Platform Compatibility Layer | Accepted |
| 073 | SAT MQTT Topic Structure | Accepted |
| 074 | ADR Audit SAT Integration Phase | Accepted |
| 075 | WiFi Reconnect Timeout Tuning | Accepted |
| 076 | SAT OPV Calibration | Accepted |

#### ADR status changes since v1.3.5

| ADR | Old Status | New Status |
|-----|-----------|------------|
| 001 | Accepted | Superseded by ADR-061 |
| 047 | Superseded by ADR-061 | Superseded by ADR-075 |

#### Complete ADR catalog (all 76 ADRs)

| ADR | Title | Status |
|-----|-------|--------|
| 001 | ESP8266 Platform Selection | Superseded by ADR-061 |
| 002 | Modular .ino File Architecture | Accepted |
| 003 | HTTP-Only Network Architecture (No HTTPS) | Accepted |
| 004 | Static Buffer Allocation Strategy | Superseded by ADR-053 |
| 005 | WebSocket for Real-Time Streaming | Accepted |
| 006 | MQTT Integration Pattern | Accepted |
| 007 | Timer-Based Task Scheduling | Accepted |
| 008 | LittleFS for Configuration Persistence | Accepted |
| 009 | PROGMEM Usage for String Literals | Accepted |
| 010 | Multiple Concurrent Network Services | Accepted |
| 011 | External Hardware Watchdog for Reliability | Accepted |
| 012 | PIC Firmware Upgrade via Web UI | Accepted |
| 013 | Arduino Framework Over ESP-IDF | Accepted |
| 014 | Dual Build System (Makefile + Python Script) | Accepted |
| 015 | NTP and AceTime for Time Management | Accepted |
| 016 | OpenTherm Command Queue with Deduplication | Accepted |
| 017 | WiFiManager for Initial Configuration | Accepted |
| 018 | ArduinoJson for Data Interchange | Superseded by ADR-042 |
| 019 | REST API Versioning Strategy | Accepted |
| 020 | Dallas DS18B20 Temperature Sensor Integration | Accepted |
| 021 | S0 Pulse Counter Hardware Interrupt Architecture | Accepted |
| 022 | GPIO Output Control (Bit-Flag Triggered Relays) | Accepted |
| 023 | File System Explorer HTTP Architecture | Accepted |
| 024 | Debug Telnet Command Console | Accepted |
| 025 | Safari WebSocket Connection Management | Accepted |
| 026 | Conditional JavaScript Cache-Busting | Accepted |
| 027 | Version Mismatch Warning System in Web UI | Accepted |
| 028 | File Streaming Over Loading for Memory Safety | Accepted |
| 029 | Simple XHR-Based OTA Flash (KISS Principle) | Accepted |
| 030 | Heap Memory Monitoring and Emergency Recovery | Accepted |
| 031 | Two-Microcontroller Coordination Architecture | Accepted |
| 032 | No Authentication Pattern (Local Network Security Model) | Accepted |
| 033 | Dallas Sensor Custom Labels and Graph Visualization | Accepted |
| 034 | Non-Blocking Modal Dialogs for User Input | Accepted |
| 035 | RESTful API Compliance Strategy | Accepted |
| 036 | Boot Sequence Initialization Ordering | Accepted |
| 037 | Gateway Mode Detection via PR=M Polling | Accepted |
| 038 | OpenTherm Message Data Flow Pipeline | Accepted |
| 039 | Real-Time OTGraph Charting Architecture | Accepted |
| 040 | MQTT Source-Specific Topics for OpenTherm Values | Accepted |
| 041 | JIT Home Assistant Discovery | Accepted |
| 042 | Streaming JSON I/O (No ArduinoJson) | Accepted |
| 043 | Reset-Pattern WiFi Recovery Trigger | Accepted |
| 044 | Global State Header Definition Pattern | Accepted |
| 045 | PS=1 Print Summary Parsing | Superseded by ADR-046 |
| 046 | PS=1 Summary Translation with Shared Publish Helpers | Accepted |
| 047 | Non-Blocking WiFi Reconnect State Machine | Superseded by ADR-075 |
| 048 | Non-Blocking Webhook State Machine with Retry | Accepted |
| 049 | String Class Prohibition in Protocol Paths | Accepted |
| 050 | Centralized API Route Dispatch Table | Accepted |
| 051 | Dual Encapsulating Structs (Settings + State) | Accepted |
| 052 | MQTT Publish Eligibility and Reconnect Refresh Contract | Accepted |
| 053 | Large Feature Buffer Static Allocation | Accepted |
| 054 | Optional HTTP Basic Authentication for Settings | Superseded by ADR-056 |
| 055 | Webhook Outbound HTTP Integration | Superseded by ADR-057 |
| 056 | Protected Admin Endpoint Security and Secret-Handling Contract | Accepted |
| 057 | Webhook Delivery, Retry, and Protected Test Endpoint Policy | Accepted |
| 058 | Non-Blocking PIC Command Response | Accepted |
| 059 | Ser2net Queue Awareness | Accepted |
| 060 | PIC Availability Guard Pattern | Accepted |
| 061 | Unified ESP8266/ESP32 Platform Abstraction | Accepted |
| 062 | SAT Smart Autotune Thermostat Integration | Accepted |
| 063 | OTGW32 Hardware Support | Accepted |
| 064 | OTDirect Operating Mode Architecture | Accepted |
| 065 | Frame Bridge Pattern | Accepted |
| 066 | Thermostat Auto-Detection Master Mode | Accepted |
| 067 | SSD1306Ascii OLED Library | Accepted |
| 068 | OTDirect Schedule Tuning Constants | Accepted |
| 069 | SAT PID v3 Implementation | Accepted |
| 070 | SAT Memory Allocation Strategy | Accepted |
| 071 | SAT Heating Curve Algorithm | Accepted |
| 072 | SAT Platform Compatibility Layer | Accepted |
| 073 | SAT MQTT Topic Structure | Accepted |
| 074 | ADR Audit SAT Integration Phase | Accepted |
| 075 | WiFi Reconnect Timeout Tuning | Accepted |
| 076 | SAT OPV Calibration | Accepted |

#### Supersession chain summary

- ADR-001 (ESP8266 Platform) -> ADR-061 (Unified Platform Abstraction)
- ADR-004 (Static Buffers) -> ADR-053 (Large Feature Buffer Static Allocation)
- ADR-018 (ArduinoJson) -> ADR-042 (Streaming JSON, No ArduinoJson)
- ADR-045 (PS=1 Parsing) -> ADR-046 (PS=1 Summary Translation)
- ADR-047 (WiFi Reconnect) -> ADR-075 (WiFi Reconnect Timeout Tuning)
- ADR-054 (HTTP Basic Auth) -> ADR-056 (Protected Admin Security Contract)
- ADR-055 (Webhook HTTP) -> ADR-057 (Webhook Delivery + Test Endpoint Policy)
