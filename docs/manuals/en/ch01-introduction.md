## Chapter 1: Introduction

### What Is OTGW-firmware?

OTGW-firmware turns the NodoShop OpenTherm Gateway into a fully networked smart heating controller. It runs on the ESP8266 or ESP32 Wi-Fi microcontroller built into the OTGW device, monitors the OpenTherm communication bus between your room thermostat and your boiler, and connects your entire heating system to your home automation platform via MQTT, a browser-based web interface, and a REST API.

The firmware ships as a single unified codebase that targets both the ESP8266-based OTGW (NodeMCU or Wemos D1 mini variants) and the newer ESP32-based OTGW32 board. Both platforms are fully supported and build from the same source tree via PlatformIO.

### The OpenTherm Gateway Hardware

OpenTherm is the communication protocol used by most modern central heating systems in Europe. A room thermostat and a boiler talk to each other over a two-wire OpenTherm bus, exchanging temperature requests, setpoints, modulation levels, fault codes, and dozens of other measurements.

The NodoShop OpenTherm Gateway (OTGW) is a small hardware device that sits physically in-line on that two-wire bus, between the thermostat and the boiler. Inside the device is a PIC microcontroller (PIC16F88 or PIC16F1847) that implements the OpenTherm electrical interface. Alongside it sits an ESP8266 or ESP32 Wi-Fi module. The PIC decodes every OpenTherm frame on the bus and forwards it to the ESP over a serial link. The ESP firmware then makes all of that data available on your home network.

On its own, the PIC does its job quietly. OTGW-firmware is the software that makes the entire system useful: it decodes, displays, stores, publishes, and acts on every message the thermostat and boiler exchange.

On the OTGW32 board (ESP32), the hardware includes onboard OpenTherm circuitry that allows the ESP32 to communicate directly on the OpenTherm bus via GPIO, without requiring the PIC co-processor at all. This is the OTDirect mode.

### Key Features

#### Both Platforms (ESP8266 and ESP32)

- Full OpenTherm bus monitoring: all 80+ standard message IDs decoded in real time, covering heating, hot water, modulation, fault codes, solar, ventilation, and more.
- MQTT integration with 250+ Home Assistant auto-discovery entities: sensors, binary sensors, and a climate entity, all created automatically without any manual YAML configuration.
- PROGMEM-based MQTT discovery: auto-discovery payloads are stored in flash (PROGMEM) and published asynchronously using a bitmap-driven drip publisher, eliminating large RAM staging buffers and reducing heap pressure.
- Browser-based web interface with live OpenTherm log, real-time temperature graphs, settings panel, dark and light themes, and OTA firmware updates.
- REST API v2 with consistent JSON responses and proper HTTP status codes.
- SAT (Smart Autotune Thermostat): an embedded, weather-compensated PID heating controller that runs entirely on the device and can replace the room thermostat's control role.
- Weather integration via Open-Meteo: free weather API provides outdoor temperature, solar elevation, and other atmospheric data for SAT weather compensation when no local outdoor sensor is available.
- DS18B20 Dallas temperature sensors on the 1-Wire bus, with custom labels, up to 16 sensors.
- S0 pulse-counting energy meter.
- SSD1306 OLED display (128x64, I2C, four information pages).
- OTA firmware and filesystem updates via the web interface.
- TCP serial bridge on port 25238, compatible with OTmonitor and the Home Assistant built-in OpenTherm Gateway integration.
- Telnet debug log on port 23 (SimpleTelnet, multi-client capable).
- Webhook callbacks triggered on OpenTherm status bit changes.
- Triple-reset Wi-Fi credential recovery.
- Optional HTTP Basic Auth for settings and maintenance endpoints.
- Nightly scheduled restart: configurable automatic reboot (time and day selection) to recover heap fragmentation on long-running devices.

#### ESP32 / OTGW32 Only

- OTDirect: direct GPIO OpenTherm bus communication with five operating modes (no PIC required).
- W5500 Ethernet with automatic cable-detect failover between wired Ethernet and Wi-Fi.
- BLE room temperature and humidity sensors: up to four Xiaomi LYWSD03MMC sensors read passively via BTHome v2 protocol.
- AP fallback mode (beta): the web UI stays reachable even when the home network is unavailable.

### What Is New in v2.0.0

Version 2.0.0 is a major platform release. It ships full ESP32 and OTGW32 support alongside the established ESP8266 path, all in a single unified codebase. There are no breaking changes to MQTT topics or the REST API from v1.x. Settings files from v1.3.x load without modification.

#### Highlights

- **OTGW32 / ESP32 platform**: Native ESP32 support. The OTGW32 board combines an ESP32 with onboard OpenTherm circuitry. The PIC gateway chip is optional. Build with `pio run -e esp32`.
- **OTDirect**: ESP32-only direct GPIO OpenTherm master/slave with five operating modes: thermostat, boiler, gateway, monitor, and a combined master+slave pair, all without a PIC co-processor.
- **Ethernet (W5500)**: Wired Ethernet via W5500 SPI on the OTGW32 board. The firmware auto-detects cable presence and switches transparently between Ethernet and Wi-Fi in both directions.
- **BLE temperature sensors**: On ESP32, up to four Xiaomi LYWSD03MMC sensors are read passively over Bluetooth LE. Room temperature and humidity feed directly into SAT and Home Assistant.
- **SAT (Smart Autotune Thermostat)**: A full embedded heating controller with weather-compensated PID control, multi-zone support, Summer Simmer Index, solar gain compensation, pressure monitoring, OPV calibration, and six configurable presets.
- **Weather integration (Open-Meteo)**: Free weather API provides outdoor temperature and solar data for SAT when no local sensor is available.
- **OLED display**: 128x64 SSD1306 I2C display on both platforms. Four information pages cover status, temperatures, network, and boiler. A button cycles pages; the display turns off automatically after a configurable timeout.
- **250+ Home Assistant entities**: Full climate entity, all SAT entities, pressure monitoring, BLE sensor entities, and OLED status, all via MQTT auto-discovery.
- **Streaming MQTT HA discovery**: Auto-discovery payloads are now compiled into flash (PROGMEM) and published via an asynchronous, bitmap-driven drip publisher that streams each entity directly to the broker. This replaces the previous LittleFS- and RAM-staged approach, eliminating large staging buffers and improving heap stability on ESP8266. The streaming pipeline also covers SAT switches and select entities (TASK-284), runtime-discovered Dallas sensors, and the climate/number entities.
- **Nightly restart scheduling**: Configurable automatic reboot at a chosen time and day to recover from heap fragmentation on long-running ESP8266 devices.
- **Platform abstraction (boards.h)**: A new `boards.h` header defines pin maps, feature flags (`HAS_PIC`, `HAS_DIRECT_OT`, `HAS_ETH_CAPABLE`, `HAS_OLED_CAPABLE`), and hardware capabilities per board variant. This cleanly separates platform-specific configuration from application logic.
- **SimpleTelnet**: The telnet debug log now uses SimpleTelnet, a unified multi-client telnet library that replaces the separate TelnetStream and ESPTelnet libraries used in v1.x.
- **Toolchain refresh**: ESP8266 builds now use Arduino core 3.1.2 (up from 2.7.4 in v1.3.5), and time handling has moved to AceTime 4.x. These updates bring current lwIP, updated Wi-Fi stack, and a smaller, faster time library.
- **Wi-Fi resilience**: AP fallback mode keeps the web UI reachable when the home network is unavailable. A Wi-Fi signal quality indicator appears in the web UI header. Triple-reset credential recovery wipes saved credentials without a reflash.
- **PlatformIO build**: Unified `platformio.ini` with `esp8266` and `esp32` environments. Arduino IDE still works for ESP8266.

### Platform Comparison

| Feature | ESP8266 (NodeMCU / Wemos D1 mini) | ESP32 / OTGW32 |
|---|---|---|
| OpenTherm via PIC co-processor | Yes (required) | Yes (optional) |
| OpenTherm via direct GPIO (OTDirect) | No | Yes |
| Wi-Fi | Yes | Yes |
| Ethernet (W5500) | No | Yes |
| BLE sensors | No | Yes (up to 4) |
| OLED display | Yes | Yes |
| SAT thermostat | Yes | Yes |
| Dallas sensors (DS18B20) | Yes | Yes |
| S0 pulse counter | Yes | Yes |

### How to Use This Manual

This manual is structured for two audiences.

**End users** who want to install, configure, and use the firmware should read:

- Chapter 2: Hardware Setup and Installation
- Chapter 3: The Web Interface
- Chapter 4: MQTT and Home Assistant Integration
- Chapter 5: SAT Smart Thermostat
- Chapter 6: Additional Sensors and Hardware

**Developers and advanced users** who want to build from source, understand the architecture, or extend the firmware should additionally read:

- Chapter 7: REST API Reference
- Chapter 8: Building from Source
- Chapter 9: Architecture Overview
- Chapter 10: Contributing

If you are upgrading from v1.x, start with Chapter 2 (the upgrade section) and the [CHANGELOG](../CHANGELOG.md) to check for any changes that affect your setup.

### Prerequisites

Before you begin, you need:

- A NodoShop OTGW device (ESP8266 NodeMCU or Wemos D1 mini variant) or a NodoShop OTGW32 (ESP32). Other ESP8266/ESP32 boards may work but are not officially supported.
- A USB cable (micro-USB or USB-C, depending on the ESP module) for initial flashing.
- A computer with a USB port and Python 3.6 or higher installed (for the flash script), or PlatformIO installed (for building from source).
- A home Wi-Fi network with a 2.4 GHz access point (the ESP8266 does not support 5 GHz).
- For MQTT and Home Assistant integration: a running MQTT broker (such as Mosquitto) on your local network.
- For full Home Assistant auto-discovery: Home Assistant with the MQTT integration enabled.

The firmware does not require an internet connection for normal operation. An internet connection is used optionally for: NTP time synchronization, one-click OTA updates from GitHub, and the Open-Meteo weather API used by SAT for outdoor temperature and solar data.

---
