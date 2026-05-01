# OTGW-firmware 2.0.0 Manual

This manual covers the full OTGW-firmware v2.0.0 product for both end users (Chapters 1-7) and developers (Chapters 8-10). Individual chapters are also available as separate files in `docs/manuals/en/` (English) and `docs/manuals/nl/` (Dutch).

---

## Chapter 1: Introduction

### What Is OTGW-firmware?

OTGW-firmware turns the NodoShop OpenTherm Gateway into a fully networked smart heating controller. It runs on the ESP8266 or ESP32 Wi-Fi microcontroller built into the OTGW device, monitors the OpenTherm communication bus between your room thermostat and your boiler, and connects your entire heating system to your home automation platform via MQTT, a browser-based web interface, and a REST API.

The firmware ships as a single unified codebase that targets both the ESP8266-based OTGW (NodeMCU or Wemos D1 mini variants) and the newer ESP32-based OTGW32 board. Both platforms are fully supported and build from the same source tree.

### The OpenTherm Gateway Hardware

OpenTherm is the communication protocol used by most modern central heating systems in Europe. A room thermostat and a boiler talk to each other over a two-wire OpenTherm bus, exchanging temperature requests, setpoints, modulation levels, fault codes, and dozens of other measurements.

The NodoShop OpenTherm Gateway (OTGW) is a small hardware device that sits physically in-line on that two-wire bus, between the thermostat and the boiler. Inside the device is a PIC microcontroller (PIC16F88 or PIC16F1847) that implements the OpenTherm electrical interface. Alongside it sits an ESP8266 or ESP32 Wi-Fi module. The PIC decodes every OpenTherm frame on the bus and forwards it to the ESP over a serial link. The ESP firmware then makes all of that data available on your home network.

On its own, the PIC does its job quietly. OTGW-firmware is the software that makes the entire system useful: it decodes, displays, stores, publishes, and acts on every message the thermostat and boiler exchange.

On the OTGW32 board (ESP32), the hardware includes onboard OpenTherm circuitry that allows the ESP32 to communicate directly on the OpenTherm bus via GPIO, without requiring the PIC co-processor at all. This is the OTDirect mode.

### Key Features

#### Both Platforms (ESP8266 and ESP32)

- Full OpenTherm bus monitoring: all 80+ standard message IDs decoded in real time, covering heating, hot water, modulation, fault codes, solar, ventilation, and more.
- MQTT integration with 250+ Home Assistant auto-discovery entities: sensors, binary sensors, and a climate entity, all created automatically without any manual YAML configuration.
- Browser-based web interface with live OpenTherm log, real-time temperature graphs, settings panel, dark and light themes, and OTA firmware updates.
- REST API v2 with consistent JSON responses and proper HTTP status codes.
- SAT (Smart Autotune Thermostat): an embedded, weather-compensated PID heating controller that runs entirely on the device and can replace the room thermostat's control role.
- DS18B20 Dallas temperature sensors on the 1-Wire bus, with custom labels, up to 16 sensors.
- S0 pulse-counting energy meter.
- SSD1306 OLED display (128x64, I2C, four information pages).
- OTA firmware and filesystem updates via the web interface.
- TCP serial bridge on port 25238, compatible with OTmonitor and the Home Assistant built-in OpenTherm Gateway integration.
- Telnet debug log on port 23.
- Webhook callbacks triggered on OpenTherm status bit changes.
- Triple-reset Wi-Fi credential recovery.
- Optional HTTP Basic Auth for settings and maintenance endpoints.

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
- **OLED display**: 128x64 SSD1306 I2C display on both platforms. Four information pages cover status, temperatures, network, and boiler. A button cycles pages; the display turns off automatically after a configurable timeout.
- **SAT enhancements**: Summer Simmer Index, improved solar gain compensation, expanded multi-zone support, and BLE sensor integration.
- **250+ Home Assistant entities**: Full climate entity, all SAT entities, pressure monitoring, BLE sensor entities, and OLED status, all via MQTT auto-discovery.
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
- A USB cable (micro-USB) for initial flashing.
- A computer with a USB port and Python 3.6 or higher installed (for the flash script), or PlatformIO installed (for building from source).
- A home Wi-Fi network with a 2.4 GHz access point (the ESP8266 does not support 5 GHz).
- For MQTT and Home Assistant integration: a running MQTT broker (such as Mosquitto) on your local network.
- For full Home Assistant auto-discovery: Home Assistant with the MQTT integration enabled.

The firmware does not require an internet connection for normal operation. An internet connection is used optionally for: NTP time synchronization, one-click OTA updates from GitHub, and the Open-Meteo weather API used by SAT.

---

## Chapter 2: Hardware Setup and Installation

### Supported Hardware

#### ESP8266 OTGW (NodoShop)

The NodoShop OpenTherm Gateway with an ESP8266 module is the primary and longest-supported hardware target. Two board revisions exist:

| NodoShop OTGW version | ESP8266 module |
|---|---|
| 1.x through 2.0 | NodeMCU ESP8266 |
| 2.3 and later | Wemos D1 mini ESP8266 |

Both revisions use the same firmware binary. The NodeMCU and Wemos D1 mini modules are pin-compatible from the firmware's perspective.

#### ESP32 OTGW32 (NodoShop)

The OTGW32 is a separate board from NodoShop built around an ESP32. It includes onboard OpenTherm bus circuitry that allows the ESP32 to communicate directly on the OpenTherm bus without a PIC co-processor. The PIC firmware chip is optional: when present, it provides the classic serial bridge path; when absent, OTDirect GPIO mode takes over.

The OTGW32 also has a W5500 Ethernet SPI header, I2C header for an OLED display, and headers for BLE sensor configuration.

#### Other Boards

Other ESP8266 or ESP32 boards may compile and run the firmware but are not tested or supported. GPIO pin assignments for Dallas sensors, OLED, and the S0 counter may need adjustment in the settings.

### Hardware Capability Flags (boards.h)

The `boards.h` header defines compile-time capability flags that control which code is included in each firmware binary. Understanding these flags helps when reading build logs or contributing to the firmware.

| Flag | ESP8266 OTGW | ESP32 OTGW32 | Meaning |
|---|---|---|---|
| `HAS_PIC` | 1 | 0 | PIC co-processor present. Includes OTGWSerial, PIC detection, PIC firmware upgrade, and I2C watchdog code. |
| `HAS_DIRECT_OT` | 0 | 1 | Direct GPIO OpenTherm bus interface present. Includes OTDirect.ino, the opentherm_library, step-up converter control, and GPIO OT pins. |
| `HAS_ETH_CAPABLE` | 0 | 1 | W5500 SPI Ethernet hardware present. Includes Ethernet.ino and the EthernetESP32 library. |

These flags are mutually exclusive between the two build targets: a single firmware binary is not possible because the GPIO pin assignments for OT-direct and I2C conflict at the electrical level. The `python build.py` command (or `pio run`) builds both binaries from the same source tree.

Application code guards platform-specific blocks with `#if HAS_PIC`, `#if HAS_DIRECT_OT`, and `#if HAS_ETH_CAPABLE`. User-facing runtime checks use the helper functions `isPICEnabled()` and `isOTDirectEnabled()`, which add a runtime detection layer on top of the compile-time flag.

### Physical Connections

#### Connecting the OTGW to Your Heating System

The OTGW hardware must be wired in-line between your room thermostat and your boiler on the OpenTherm bus. This is a two-wire connection and is not polarity-sensitive.

```
[Room Thermostat] --- (OpenTherm 2-wire) --- [OTGW] --- (OpenTherm 2-wire) --- [Boiler]
```

The OTGW PCB has clearly labelled terminal blocks for the thermostat side (TT) and boiler side (CV) connections. Refer to the NodoShop OTGW hardware documentation and your boiler's wiring diagram for the exact terminal locations on each device.

> **Important**: Only connect one end of the OpenTherm bus to the OTGW at a time during installation. Follow your boiler manufacturer's instructions for any work on the boiler terminals.

#### Power

The OTGW derives operating power from the OpenTherm bus itself when wired to a boiler. No separate power supply is required once the device is in circuit. During initial flashing via USB, power is supplied through the USB cable.

### Optional Hardware

#### Ethernet W5500 (OTGW32 / ESP32 Only)

The W5500 SPI Ethernet module connects to the dedicated SPI header on the OTGW32 board. When a network cable is plugged in and the firmware detects a valid link, Ethernet is used automatically and Wi-Fi is switched off. Unplugging the cable causes automatic failover to Wi-Fi within approximately five seconds. No configuration change is required.

#### OLED Display (SSD1306)

A 128x64 SSD1306 I2C OLED display can be connected to both ESP8266 and ESP32 boards. The display shows four pages of information cycled by a button press:

1. System status (device name, uptime, firmware version)
2. Temperatures (room, boiler flow, boiler return, outside)
3. Network (IP address, SSID or Ethernet status, signal quality)
4. Boiler status (flame, modulation, CH and DHW setpoints)

The display turns off automatically after a configurable idle timeout. Configure the I2C address, pin assignments, and timeout in Settings under the Hardware section.

#### Dallas DS18B20 Temperature Sensors

Up to 16 DS18B20 sensors can be connected on a single 1-Wire bus to a configurable GPIO pin (default GPIO 10 / SD3 on ESP8266). Each sensor is identified by its 64-bit address and can be given a custom label via the web interface. All sensors are published to MQTT and appear in Home Assistant auto-discovery.

> **Note for users upgrading from versions before v1.0.0**: The default Dallas GPIO pin changed from GPIO 13 (D7) to GPIO 10 (SD3) in v1.0.0. If your sensors stopped working after an upgrade, check the Dallas GPIO setting in Settings and change it back to 13 if needed.

Wiring a DS18B20 sensor:

- Connect VDD to 3.3 V (or use parasitic power mode with VDD tied to GND).
- Connect GND to ground.
- Connect DATA to the configured GPIO pin.
- Add a 4.7 kOhm pull-up resistor between VDD and DATA.

#### S0 Pulse Counter

The S0 pulse counter reads electricity pulses from an energy meter that has an S0 output (a standard impulse interface common on DIN-rail energy meters). Connect the S0 output to a configurable GPIO pin. Configure the pulses-per-kWh ratio in Settings. The firmware reports cumulative kWh and instantaneous power in watts to MQTT and Home Assistant.

#### BLE Temperature Sensors (ESP32 / OTGW32 Only)

Up to four Xiaomi LYWSD03MMC Bluetooth LE sensors are supported on the ESP32. The firmware reads them passively using the BTHome v2 protocol (these sensors broadcast their readings as advertisements; no pairing is required). Room temperature and humidity from these sensors feed into the SAT multi-zone averaging and are published to Home Assistant as individual sensor entities.

**Trust model and MAC filtering.** BLE advertisements are unauthenticated broadcasts: any device in radio range can publish a BTHome v2 or ATC/pvvx packet that the firmware will parse and accept. The firmware applies the following filter:

- If a sensor MAC is configured (Settings → SAT → BLE sensors), the firmware accepts only advertisements from that exact MAC.
- If the configured-MAC list is empty, **the firmware accepts any parseable BTHome v2 / ATC sensor in range** and assigns it to a free slot until all four slots are filled. Once a slot is taken, only advertisements from that slot's MAC continue to update it (slot stickiness).
- The firmware never writes to or queries BLE devices — reception only. There is no risk of leaking secrets or commands to a hostile sensor.

In practice this means: in a typical home with one or two BTHome devices, default-allow is convenient and harmless. In a dense apartment building, or any environment where you do not control all nearby BLE devices, configure the MAC of each sensor explicitly so you do not pick up your neighbour's thermometer. Spoofing a configured MAC is theoretically possible but requires a hostile device with line-of-sight; the impact is bounded to feeding bogus temperature/humidity values into SAT.

### Flashing the Firmware

#### First-Time Flashing with flash_esp.py (Recommended)

The `flash_esp.py` script handles everything automatically: it downloads the latest release from GitHub, auto-detects your serial port, and flashes both the firmware binary and the LittleFS filesystem image.

**Prerequisites:**

- Python 3.6 or higher
- A micro-USB cable connected between your computer and the ESP8266/ESP32 module
- The correct USB-to-UART driver installed for your board:
  - Windows: CP210x driver (NodeMCU) or CH340 driver (some Wemos D1 mini clones)
  - macOS: usually included in recent versions
  - Linux: add your user to the `dialout` group (`sudo usermod -aG dialout $USER`, then log out and back in)

**Steps:**

1. Connect the USB cable to the ESP module (not to the OTGW board, but to the ESP module's own USB port, or to the OTGW board's USB connector if it has one).

2. Open a terminal and run:

```bash
python3 flash_esp.py
```

This downloads the latest release, auto-detects the serial port, and flashes both the firmware and the filesystem. For a completely clean first-time install, use the `--erase` flag to wipe the flash first:

```bash
python3 flash_esp.py --erase
```

3. If the script cannot auto-detect your port, specify it explicitly:

```bash
python3 flash_esp.py --port COM5          # Windows
python3 flash_esp.py --port /dev/ttyUSB0  # Linux
python3 flash_esp.py --port /dev/cu.usbserial-0001  # macOS
```

4. After flashing completes, the device reboots automatically.

#### Flashing with PlatformIO (Build from Source)

If you have PlatformIO installed and want to build the firmware yourself:

```bash
# Flash ESP8266
pio run -e esp8266 --target upload
pio run -e esp8266 --target uploadfs

# Flash ESP32 / OTGW32
pio run -e esp32 --target upload
pio run -e esp32 --target uploadfs
```

The `upload` target flashes the firmware binary. The `uploadfs` target flashes the LittleFS filesystem image containing web assets, the Home Assistant discovery configuration, and PIC firmware files.

Both targets require the device to be connected via USB.

#### Manual Flashing with Existing Binaries

If you already have firmware and filesystem binary files:

```bash
python3 flash_esp.py --firmware OTGW-firmware-fw.bin --filesystem OTGW-firmware-fs.bin
```

### Release artefact verification

Every GitHub release for OTGW-firmware ships a `SHA256SUMS.txt` file alongside the binaries (`firmware-esp32.bin`, `firmware-nodemcuv2.bin`, `littlefs-esp32.bin`, etc.). The file lists the SHA-256 digest of each artefact in the canonical `sha256sum` format: 64 hex characters, two spaces, filename.

Verifying a download takes a few seconds and protects against three real failure modes:

- A download mirror or CDN serving a tampered binary
- Silent transit corruption (rare, but possible on flaky links)
- Accidentally flashing a stale binary that you forgot you had on disk

**Linux and macOS:**

Place `SHA256SUMS.txt` in the same directory as the downloaded artefacts and run:

```bash
sha256sum -c SHA256SUMS.txt
```

Each artefact prints `OK` on a successful match. Any mismatch prints `FAILED` and a non-zero exit code: do not flash that file.

**Windows (PowerShell):**

PowerShell's built-in `Get-FileHash` does the same job. Run it once per artefact and compare the printed digest against the matching line in `SHA256SUMS.txt`:

```powershell
Get-FileHash -Algorithm SHA256 firmware-esp32.bin
```

If the hashes match, the file is intact. If they do not match, redownload from the GitHub release page rather than from a mirror or cache.

### First-Time Setup: Connecting to Wi-Fi

On the first boot after flashing, the device has no saved Wi-Fi credentials. It broadcasts a temporary Wi-Fi access point so you can configure it.

**AP name format**: `<hostname>-<MAC suffix>`

The default hostname is `otgw`, so the AP will typically be named something like `otgw-A1B2C3`.

**Steps:**

1. On your phone or laptop, open the Wi-Fi settings and connect to the `otgw-XXXXXX` network.

2. A captive portal opens automatically in your browser. If it does not open automatically, navigate to `http://192.168.4.1/`.

3. Select your home Wi-Fi network from the list, enter the password, and tap Save.

4. The device connects to your home network, obtains a DHCP address, and announces itself as `otgw.local` via mDNS.

5. Navigate to `http://otgw.local/` in your browser to open the web interface. On Windows, the device also responds to `http://otgw/` via LLMNR.

> **ESP32 with Ethernet**: If you have a W5500 Ethernet module and a network cable connected, the device obtains an IP address via DHCP on the wired interface at boot and does not open an AP. No Wi-Fi setup is needed.

> **AP Fallback Mode (beta, ESP32 only)**: If the configured Wi-Fi network becomes unavailable after initial setup, the device opens a fallback AP named `OTGW-<MAC>` with password `otgw123` and IP `192.168.4.1`, keeping the web UI accessible.

### Filesystem Upload (LittleFS)

The web assets (HTML, CSS, JavaScript, OLED icons, PIC firmware files, and the Home Assistant discovery configuration) are stored in a LittleFS partition on the flash. This partition must be flashed alongside the firmware binary.

When using `flash_esp.py`, the filesystem is flashed automatically. When using PlatformIO, run `pio run -e <env> --target uploadfs` after the firmware upload.

> **After a firmware update via OTA**: The web assets in LittleFS are updated separately. Use the web UI's Update page to update both firmware and filesystem, or upload a new filesystem image manually via the File Manager.

### Upgrading from a Previous Version

#### OTA Upgrade (Recommended)

From v1.3.0 onwards, the web interface provides a one-click OTA updater:

1. Open the web interface and navigate to the **Update** tab (or the update icon in the header).
2. The page fetches the list of available GitHub releases and shows Installed, Update, and Rollback badges next to each version.
3. Click **Update** next to the version you want to install.
4. The firmware downloads the new binary from GitHub, flashes it to the inactive OTA partition, and reboots.
5. The web interface polls the device until it responds, then confirms success.
6. Update the filesystem image using the same page to ensure web assets match the new firmware version.

#### Manual OTA Upload

If you have a firmware binary file and want to upload it directly:

1. Navigate to the **Update** page in the web interface.
2. Use the file upload area to select your `OTGW-firmware-fw.bin` file.
3. The device flashes the binary and reboots.

#### Notes When Upgrading from v1.x

- No settings migration is required. Settings files from v1.3.x load without modification in v2.0.0.
- MQTT topics and the REST API are unchanged from v1.x.
- If you are upgrading from before v1.2.0, be aware that REST API v0 and v1 endpoints were removed in v1.2.0. Only `/api/v2/` endpoints are available. See [docs/BREAKING_CHANGES.md](../docs/BREAKING_CHANGES.md).
- After upgrade, delete any orphaned Home Assistant entities and allow MQTT auto-discovery to recreate them to pick up any new or renamed entities.

#### Important Warnings

- **Never flash PIC firmware over Wi-Fi using OTmonitor.** This can brick the PIC microcontroller. Use the built-in PIC firmware upgrade feature in the web UI instead.
- **When OTDirect is active (ESP32 only)**, the PIC UART is bypassed. Do not attempt PIC firmware upgrades while OTDirect is the active OpenTherm path.

---

## Chapter 3: The Web Interface

### Overview

The OTGW web interface is a single-page application (SPA) served directly by the device itself, with no cloud service or external server required. Open it in any modern browser (Chrome, Firefox, or Safari) by navigating to `http://otgw.local/` or to the device's IP address: `http://<device-ip>/`.

The interface provides:

- A live dashboard with real-time OpenTherm data and temperature graphs.
- A full settings panel for all device configuration.
- A real-time OpenTherm message log with search and export.
- SAT thermostat controls and diagnostics.
- OTA firmware updates.
- A file manager for LittleFS.

The interface adapts to desktop and mobile screens and supports both light and dark themes.

### Navigation

The interface is organized into four main tabs, accessible from the navigation bar at the top of the page:

| Tab | Purpose |
|---|---|
| **Home** | Live OpenTherm log, real-time temperature graphs, boiler status |
| **SAT** | Smart thermostat configuration, heating curve, PID diagnostics |
| **Settings** | All device configuration: network, MQTT, NTP, hardware, security |
| **Advanced** | File manager, PIC firmware upgrade, device info, debug tools |

A header bar across the top shows the device name, firmware version, network status indicator, and theme toggle button.

### Dashboard (Home Tab)

The Home tab is the primary monitoring view. It has three main areas.

#### OpenTherm Log Viewer

The log viewer displays every OpenTherm message received from the bus in real time, streamed from the device over a WebSocket connection. Each line shows:

- A direction indicator (`>` for messages sent to the boiler, `<` for responses received)
- The OpenTherm message ID and decoded label (for example, `CH water temperature`)
- The decoded value with units
- A timestamp (toggled with the clock icon)

The log viewer includes:

- **Search/filter**: type a message ID number or label keyword to filter the displayed lines.
- **Auto-scroll**: the log follows new messages automatically. Click anywhere in the log to pause auto-scroll; click the scroll-to-bottom button to resume.
- **Export**: download the current log buffer as a text file using the export button.
- **Command bar**: send one-shot PIC commands (`TT=20.5`, `GW=R`, etc.) directly from the log view and see the response in-line.

The browser persists the log buffer to `localStorage` between sessions, so a page refresh does not lose recent data. The buffer is cleared automatically after a firmware or filesystem flash.

> **Simulation mode**: When the firmware is running in simulation mode (no physical OTGW connected), a `SIMULATION` badge appears in the log header. This mode is useful for testing dashboards and automations without hardware.

#### Real-Time Temperature Graph

Below the log viewer, an ECharts temperature graph shows a rolling 24-hour view of:

- Boiler flow temperature (CH water temperature)
- Return temperature
- Room temperature (from the thermostat or SAT)
- Outside temperature (if available)
- Modulation level (percentage)
- Flame status
- DHW (hot water) temperature

Dallas DS18B20 sensors are automatically discovered and added to the graph with dynamically assigned colors. Sensor labels (configured on the Settings page) appear in the graph legend.

The graph synchronizes its color scheme with the active light or dark theme.

#### Boiler Status

A status row at the top of the Home tab shows key boiler readings at a glance: flame on/off, CH setpoint, current flow temperature, room temperature, modulation percentage, and DHW status. These values update in real time as new OpenTherm messages arrive.

### Network Status Indicator

The header bar shows the current network status using a visual indicator:

- **Wi-Fi signal bars**: shows signal quality from 0 to 4 bars based on RSSI, using a quadratic mapping that reflects actual usability rather than raw dBm values.
- **AP badge**: displayed when the device is operating in AP or AP fallback mode.
- **Ethernet indicator**: displayed on OTGW32 when a wired Ethernet link is active.

Hovering over the indicator shows the exact SSID and signal quality percentage (Wi-Fi) or IP address (Ethernet).

### Settings Page

The Settings tab contains all device configuration, organized into sections. After changing any settings, click **Save** at the bottom of the section to apply and persist the changes.

#### Wi-Fi

| Setting | Description |
|---|---|
| Hostname | mDNS and DHCP hostname for the device (default: `otgw`) |
| Connected SSID | Displays the currently connected Wi-Fi network (read-only) |
| Reset WiFi | Clears saved credentials and reopens the captive portal on next boot |

To change the Wi-Fi network, click **Reset WiFi**, then reconnect to the `otgw-XXXXXX` AP and enter the new credentials.

#### MQTT

| Setting | Description |
|---|---|
| MQTT Broker | IP address or hostname of your MQTT broker |
| MQTT Port | Broker port (default: `1883`) |
| MQTT User | Broker username (leave blank if not required) |
| MQTT Password | Broker password |
| MQTT Top Topic | Prefix for all published topics (default: `OTGW`) |
| MQTT Unique ID | Device identifier used in topic paths and HA discovery (default: `otgw`) |
| HA Discovery | Enable/disable Home Assistant MQTT auto-discovery payloads |
| HA Prefix | Discovery topic prefix (default: `homeassistant`) |
| Publish Interval | Heartbeat interval in seconds (0 = publish every message as received) |
| Separate Sources | Publish per-source sub-topics (`/thermostat`, `/boiler`, `/gateway`) |

#### NTP and Time

| Setting | Description |
|---|---|
| NTP Server | Hostname or IP of the NTP server (default: `pool.ntp.org`) |
| Timezone | POSIX timezone string for local time display and boiler clock sync |

The firmware sends a clock synchronization command to the boiler PIC when NTP time is acquired.

#### Device

| Setting | Description |
|---|---|
| Hostname | Device hostname (also shown in the Wi-Fi section) |
| LED Pin | GPIO pin for the status LED |
| Dallas GPIO | GPIO pin for the 1-Wire DS18B20 bus (default: GPIO 10) |
| S0 Pin | GPIO pin for the S0 pulse counter input |
| S0 Pulses/kWh | Pulses per kWh from your energy meter |
| OLED Address | I2C address of the SSD1306 OLED display (typically `0x3C` or `0x3D`) |
| OLED Timeout | Display off timeout in seconds (0 = always on) |

#### Security

| Setting | Description |
|---|---|
| Endpoint Password | HTTP Basic Auth password for settings, file management, and OTA endpoints (leave blank to disable) |

When an endpoint password is set, the following actions require authentication:

- Reading and changing device settings
- File upload, download, and delete
- Reboot and Wi-Fi reset
- OTA firmware and filesystem updates
- Webhook test

Live monitoring, sensor values, and the WebSocket stream remain accessible without authentication.

#### Webhook

| Setting | Description |
|---|---|
| Enable Webhook | Enable outbound HTTP callback on OpenTherm status bit change |
| Trigger Bit | OpenTherm status bit to monitor (for example, flame on/off) |
| URL On | HTTP POST URL to call when the bit goes high |
| URL Off | HTTP POST URL to call when the bit goes low |
| Payload | POST body template |
| Content-Type | Content-Type header for the POST request |

A **Test** button sends a test webhook immediately so you can verify your endpoint is reachable.

> **Security note**: Webhook URLs are restricted to local network addresses to prevent server-side request forgery. Do not attempt to configure public internet URLs.

### Real-Time OpenTherm Log

The OpenTherm log on the Home tab is powered by a WebSocket connection to the device (port 80, path `/ws`). The firmware streams every decoded OpenTherm frame as it arrives. The browser maintains a circular buffer of recent messages and persists it to `localStorage`.

**Heap backpressure**: On the ESP8266, available RAM is limited. The firmware monitors free heap and automatically reduces WebSocket streaming frequency when memory is low:

- Above 8 KB free: immediate streaming (HEALTHY).
- 5-8 KB free: 50 ms throttle between frames (LOW).
- 3-5 KB free: 200 ms throttle (WARNING).
- Below 3 KB free: WebSocket streaming paused (CRITICAL).

This prevents the device from crashing under heavy load. You may notice the log slowing down or pausing briefly during peak activity; this is normal and expected behavior.

### OTA Firmware Update via the Web Interface

The **Update** section (accessible from the Advanced tab or the update icon in the header) provides two ways to update the firmware.

#### One-Click GitHub Release Update

1. The page fetches the list of available releases from GitHub and displays them with version badges: **Installed** (current version), **Update** (newer available), or **Rollback** (older version).
2. Click **Update** next to the target release.
3. The firmware binary is downloaded directly from GitHub to the device and flashed to the inactive OTA partition.
4. The device reboots automatically. The web interface polls `/api/v2/health` until the device responds, confirming a successful update.
5. Update the filesystem (LittleFS) using the same page to ensure web assets match the new firmware version.

#### Manual Binary Upload

1. Download or build a firmware binary (`.bin` file).
2. Use the file upload area on the Update page to select the file.
3. The device flashes the binary and reboots.

> **PIC firmware upgrade**: The Update page also supports upgrading the PIC microcontroller firmware. Select the correct PIC type (PIC16F88 or PIC16F1847), upload the `.hex` file, and follow the on-screen instructions. Never use OTmonitor to flash PIC firmware over Wi-Fi.

### File Manager (FSexplorer)

The **FSexplorer** is accessible from the Advanced tab. It provides a browser-based view of the LittleFS filesystem on the device, with the ability to:

- Browse all files stored in LittleFS.
- Upload new files (drag and drop or file select).
- Download existing files.
- Delete or rename files.

This is useful for manually backing up or restoring `settings.ini`, `dallas_labels.ini`, the SAT PID state file, or custom web assets. The file listing is sorted and filtered in the browser; hidden files (those with a `.` prefix) are not displayed.

> **File size limits**: Files larger than 10 KB should be streamed rather than loaded into RAM. The firmware always streams files rather than buffering them, so there is no hard upload size limit beyond the available LittleFS partition space.

### Light and Dark Theme

A sun/moon toggle button in the header switches between light and dark themes. The preference is saved in browser `localStorage` and persists across sessions and page refreshes. The real-time temperature graph automatically synchronizes its color scheme with the active theme.

### Browser Debug Console

For developers and advanced troubleshooting, the firmware exposes a diagnostic toolkit in the browser JavaScript console. Open the browser developer tools and type `otgwDebug` to see the available functions:

```
otgwDebug.status()     - current device status
otgwDebug.info()       - device information
otgwDebug.settings()   - current settings (sanitized)
otgwDebug.wsStatus()   - WebSocket connection state
otgwDebug.wsReconnect()- force WebSocket reconnection
otgwDebug.logs()       - recent log buffer
otgwDebug.api()        - run an API call
otgwDebug.health()     - health endpoint
otgwDebug.sendCmd()    - send a PIC command
otgwDebug.exportLogs() - download log buffer
otgwDebug.exportData() - download all buffered data
```

These tools are always available and do not require any configuration to enable.
## Chapter 4: Home Assistant Integration

### Overview

OTGW-firmware is designed for tight Home Assistant integration. Once you have an MQTT broker running and the firmware connected to it, Home Assistant automatically discovers all OpenTherm sensors, the boiler climate entity, and binary status sensors, with no YAML required. This chapter covers how that works, what gets discovered, and how to send commands from Home Assistant to the gateway.

---

### 4.1 Prerequisites

Before Home Assistant can see the gateway, you need:

- A running MQTT broker (Mosquitto is the most common choice, covered below).
- The MQTT integration enabled in Home Assistant (Settings > Devices and Services > MQTT).
- The MQTT integration configured to use the same broker the firmware connects to.
- MQTT discovery enabled in Home Assistant (it is on by default).

The firmware does not require any YAML in Home Assistant when auto-discovery is used.

---

### 4.2 MQTT Broker Setup

#### Using the Mosquitto Add-on (Recommended)

The easiest path is the official Mosquitto broker add-on for Home Assistant OS:

1. In Home Assistant, go to Settings > Add-ons > Add-on Store.
2. Find "Mosquitto broker" and install it.
3. After installation, configure a username and password in the add-on configuration.
4. Start the add-on and enable "Start on boot."

The Mosquitto add-on automatically registers a broker connection with the HA MQTT integration.

#### Using an External Broker

If you already run Mosquitto (or another MQTT broker) separately:

1. Ensure the broker is reachable from both the ESP device and the Home Assistant host.
2. In Home Assistant, go to Settings > Devices and Services > Add Integration > MQTT.
3. Enter the broker address, port (default 1883), and credentials.

#### Broker Configuration Tips

- Use a dedicated MQTT user for the gateway with a strong password.
- Enable persistence in Mosquitto (`persistence true`) so retained messages survive broker restarts.
- Default port is 1883 (plain TCP). The firmware uses plain MQTT only, no TLS.
- If you use a firewall, open port 1883 between the ESP device and the broker host.

A minimal Mosquitto configuration for a local install:

```
listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
persistence true
persistence_location /var/lib/mosquitto/
```

---

### 4.3 Configuring the Firmware for MQTT

Open the OTGW-firmware web UI and go to the Settings page. The MQTT section has the following fields:

| Setting | Default | Description |
|---|---|---|
| MQTT Broker | (empty) | IP address or hostname of the broker |
| MQTT Port | 1883 | Broker port |
| MQTT User | (empty) | Username (leave empty if anonymous) |
| MQTT Password | (empty) | Password |
| Top Topic | `OTGW` | Root namespace for all published topics |
| Unique ID | `otgw-{MAC}` | Node identifier, included in every topic path |
| HA Discovery Prefix | `homeassistant` | Must match the discovery prefix in HA MQTT integration |

After saving, the firmware connects to the broker within a few seconds. The MQTT status indicator on the web UI main page turns green when the connection is established.

---

### 4.4 MQTT Topic Structure

All topics follow a two-namespace convention.

#### Published topics (gateway to broker)

```
{TopTopic}/value/{UniqueId}/{topic-name}
```

Example with defaults:

```
OTGW/value/otgw-AABBCCDDEEFF/boilertemperature
OTGW/value/otgw-AABBCCDDEEFF/flamestatus
OTGW/value/otgw-AABBCCDDEEFF/sat/setpoint
```

Published topics carry `retain = true` by default, so Home Assistant receives the last known value immediately on startup.

#### Command topics (HA to gateway)

```
{TopTopic}/set/{UniqueId}/{command-name}
```

Example:

```
OTGW/set/otgw-AABBCCDDEEFF/setpoint
OTGW/set/otgw-AABBCCDDEEFF/hotwater
OTGW/set/otgw-AABBCCDDEEFF/sat/target
```

#### Connection status

The firmware publishes a birth/last-will message at the root of the publish namespace:

```
OTGW/value/otgw-AABBCCDDEEFF   →  "online"  (retained, published on connect)
OTGW/value/otgw-AABBCCDDEEFF   →  "offline" (retained, last will on disconnect)
```

---

### 4.5 MQTT Publish Gate (Interval Filtering)

By default, the firmware publishes an OpenTherm value every time it is received from the bus. For busy installations this can mean dozens of messages per second. The publish interval setting lets you reduce this traffic.

#### How it works

The `MQTT Interval` setting (Settings page, field `iInterval`) controls when a value is published:

| `iInterval` value | Publish behavior |
|---|---|
| `0` (default) | Publish on every received message, no filtering |
| `> 0` (seconds) | Publish only when: the value is seen for the first time (`firstSeen`), the value has changed since the last publish (`valueChanged`), or the configured interval has elapsed since the last publish (`intervalElapsed`) |

When the interval is set to, say, 60 seconds, a stable boiler temperature will be published at most once per minute. A value that changes mid-interval is still published immediately.

The interval gate applies to OpenTherm data messages only. SAT topics, connection status, version info, and other non-OT topics are always published when they change, regardless of the interval setting.

#### Debugging the gate

If you connect to the firmware telnet debug port (port 23) and press `g`, the MQTT gate debug flag toggles. With it enabled, the firmware logs every gate decision to the debug console:

```
MQTT gate id=25 src=S slot=25 prev=0x0000 curr=0x0510 first=true changed=true interval=false last=0 now=42 => publish [tracked update]
MQTT gate id=25 src=S slot=25 prev=0x0510 curr=0x0510 first=false changed=false interval=false last=42 now=71 => skip [suppressed by interval]
```

This is useful when diagnosing why a sensor is or is not appearing in Home Assistant.

---

### 4.6 Home Assistant Auto-Discovery

When the firmware connects to the MQTT broker, it automatically publishes discovery configuration messages to Home Assistant. You do not need to write any YAML.

#### How it works

Home Assistant listens on `homeassistant/#` for discovery messages. The firmware publishes a JSON configuration payload for each entity at:

```
homeassistant/{component}/{node_id}/{entity_id}/config
```

The firmware uses Just-In-Time (JIT) discovery: rather than flooding the broker with 200+ discovery messages at startup, it publishes a discovery config the first time it sees a particular OpenTherm message ID.

If you restart Home Assistant, the firmware detects this by monitoring `homeassistant/status`. When HA comes back online, the firmware re-publishes all discovery configurations automatically.

#### Triggering a full rediscovery manually

```bash
curl -X POST http://otgw.local/api/v2/otgw/discovery
```

---

### 4.7 Auto-Discovered Entities

#### OpenTherm Sensors

| Sensor | Topic suffix | Unit |
|---|---|---|
| Boiler water temperature | `boilertemperature` | °C |
| Return water temperature | `returnwatertemperature` | °C |
| Room temperature | `roomtemperature` | °C |
| Room setpoint | `roomsetpoint` | °C |
| Control setpoint | `controlsetpoint` | °C |
| Relative modulation level | `relmodlvl` | % |
| DHW temperature | `dhwtemperature` | °C |
| DHW setpoint | `dhwsetpoint` | °C |
| Max CH water setpoint | `maxchwatersetpoint` | °C |
| Outside temperature | `outsidetemperature` | °C |
| CH water pressure | `chwaterpressure` | bar |

#### Binary Sensors

| Entity | Description |
|---|---|
| Flame status | Whether the boiler flame is on |
| Central heating active | CH mode active on boiler |
| DHW mode | Hot water mode active |
| Fault indicator | Boiler fault flag |
| CH enable | Thermostat requesting central heating |
| DHW enable | Thermostat requesting hot water |

#### Climate Entity

The firmware publishes a climate entity showing the current room temperature, the active setpoint, and allows setting a new target temperature from the HA dashboard. If SAT is enabled, a dedicated SAT climate entity (`sat_climate`) appears with full mode and preset support.

#### SAT Sensor Entities

When SAT is enabled, the firmware auto-discovers additional diagnostic entities. These are published under the `sat/` topic prefix.

**Core SAT topics (always published when SAT is active):**

| Topic suffix | Description | Unit |
|---|---|---|
| `sat/setpoint` | Final flow temperature sent to boiler | °C |
| `sat/target` | Target room temperature | °C |
| `sat/mode` | Control mode: `off`, `continuous`, or `pwm` | - |
| `sat/heating_curve` | Heating curve base value | °C |
| `sat/pid_output` | PID corrected output | °C |
| `sat/error` | PID error (target minus room temperature) | °C |
| `sat/room_temp` | Room temperature used by PID | °C |
| `sat/outside_temp` | Outdoor temperature used by heating curve | °C |
| `sat/boiler_status` | Current boiler status (text label) | - |
| `sat/active` | Whether SAT is actively controlling | bool |
| `sat/safety_tripped` | Whether a safety layer has tripped | bool |
| `sat/simulation` | Simulation mode on/off | - |

**SAT command topics (gateway subscribes to these):**

| Topic suffix | Accepted values | Description |
|---|---|---|
| `sat/target` | Float, e.g. `21.0` | Set target room temperature |
| `sat/control_mode` | `off`, `continuous`, `pwm`, `auto` | Set control mode |
| `sat/enabled` | `true` / `false` | Enable or disable SAT |
| `sat/indoor_temp` | Float, e.g. `20.5` | Push indoor temperature from external sensor |
| `sat/outdoor_temp` | Float, e.g. `5.2` | Push outdoor temperature from external source |

---

### 4.8 Sending Commands from Home Assistant

#### Setting the Boiler CH Setpoint

```yaml
action:
  - service: mqtt.publish
    data:
      topic: "OTGW/set/otgw-AABBCCDDEEFF/setpoint"
      payload: "20.5"
```

#### Controlling Hot Water (DHW)

```
Topic:   OTGW/set/otgw-AABBCCDDEEFF/hotwater
Payload: 1      (force on)
         0      (force off)
         P      (push: one-shot hot water cycle)
         A      (automatic)
```

#### Sending Raw OTGW Commands

```yaml
action:
  - service: mqtt.publish
    data:
      topic: "OTGW/set/otgw-AABBCCDDEEFF/command"
      payload: "TT=21.5"
```

---

### 4.9 Example Home Assistant Automations

#### Turn down heating when everyone leaves

```yaml
automation:
  - alias: "Away: reduce heating setpoint"
    trigger:
      - platform: state
        entity_id: group.family
        to: "not_home"
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/setpoint"
          payload: "16.0"
```

#### Push outdoor temperature to the gateway for weather compensation

```yaml
automation:
  - alias: "Push outdoor temp to OTGW"
    trigger:
      - platform: state
        entity_id: sensor.openweathermap_temperature
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/outside"
          payload: "{{ states('sensor.openweathermap_temperature') }}"
```

---

### 4.10 Manual Home Assistant Configuration (Without Auto-Discovery)

Example manual sensor configuration:

```yaml
mqtt:
  sensor:
    - name: "Boiler Temperature"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/boilertemperature"
      unit_of_measurement: "°C"
      device_class: temperature
      state_class: measurement

    - name: "Room Temperature"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/roomtemperature"
      unit_of_measurement: "°C"
      device_class: temperature
      state_class: measurement

  binary_sensor:
    - name: "Boiler Flame"
      state_topic: "OTGW/value/otgw-AABBCCDDEEFF/flamestatus"
      payload_on: "ON"
      payload_off: "OFF"
      device_class: heat

  climate:
    - name: "Heating Setpoint"
      modes:
        - "heat"
        - "off"
      current_temperature_topic: "OTGW/value/otgw-AABBCCDDEEFF/roomtemperature"
      temperature_state_topic: "OTGW/value/otgw-AABBCCDDEEFF/roomsetpoint"
      temperature_command_topic: "OTGW/set/otgw-AABBCCDDEEFF/setpoint"
      min_temp: 5
      max_temp: 30
      temp_step: 0.5
```

---

## Chapter 5: SAT - Smart Autotune Thermostat

### Overview

SAT (Smart Autotune Thermostat) transforms the OpenTherm Gateway from a transparent protocol bridge into an active, self-tuning heating controller. Instead of forwarding your wall thermostat's on/off requests to the boiler, SAT takes over: it reads the current room temperature, calculates the exact water temperature the boiler needs to produce, and injects that setpoint directly into the OpenTherm stream.

The result is weather-compensated heating that keeps room temperatures stable within a fraction of a degree, reduces gas consumption, and dramatically cuts the number of boiler ignition cycles. SAT is entirely self-contained: it runs on the ESP microcontroller with no dependency on Home Assistant, cloud services, or internet access.

---

### 5.1 What SAT Does

With SAT, every control loop cycle (default 30 seconds):

1. Reads room temperature (from the OT bus, or an external sensor pushed via MQTT).
2. Reads outdoor temperature (from the OT bus, an external MQTT push, or a weather API call).
3. Calculates the heating curve value: the base flow temperature needed given current outdoor conditions.
4. Applies a PID correction on top of the heating curve.
5. Clamps the result within safety limits.
6. Sends a `CS=` command to the PIC gateway to inject the setpoint into the OpenTherm stream.
7. Publishes all state to MQTT and the web UI dashboard.

---

### 5.2 Prerequisites and Compatibility

- An OpenTherm-compatible boiler.
- A room temperature source: OT bus (MsgID 24), external MQTT push, or BLE sensor (ESP32 only).
- An outdoor temperature source: OT bus (MsgID 27), external MQTT push, or weather API fetch.
- You do not need to remove your wall thermostat. SAT intercepts the setpoint; your thermostat's away mode and schedule still work.

---

### 5.3 Enabling SAT

#### Via the Web UI

1. Open Settings and find the SAT section.
2. Set "SAT Enabled" to on.
3. Choose your heating system type: radiator or underfloor.
4. Set the target room temperature and heating curve coefficient.
5. Save settings.

#### Via MQTT

```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/enabled" -m "true"
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/target" -m "21.0"
```

---

### 5.4 Configuration Parameters

| Setting | Default | Range | Description |
|---|---|---|---|
| `satenabled` | false | bool | Master on/off switch |
| `satsystem` | 0 | 0-1 | 0 = radiator, 1 = underfloor |
| `sattargettemp` | 20.0 | 5-30 °C | Desired room temperature |
| `satcoefficient` | 1.5 | 0.1-5.0 | Heating curve steepness |
| `satdeadband` | 0.25 | 0.05-2.0 °C | PID deadband width |
| `satinterval` | 30 | 10-300 s | Control loop interval |
| `satexternaltemp` | false | bool | Use external indoor sensor via MQTT |
| `satpwmautoswitch` | true | bool | Automatically switch between continuous and PWM modes |

#### Choosing the heating curve coefficient

| Insulation level | Building type | Suggested coefficient |
|---|---|---|
| Excellent | Modern passive house, triple glazing | 0.5 - 0.8 |
| Good | Post-2000 build, cavity wall insulation | 0.8 - 1.2 |
| Average | 1970s-2000s, partial insulation | 1.2 - 1.8 |
| Poor | Pre-1970, single glazing, no cavity insulation | 1.8 - 2.5 |

Start in the middle of the appropriate range. If the room consistently fails to reach the target, increase the coefficient by 0.2-0.3. If the room overshoots, decrease it.

---

### 5.5 How the Heating Curve Works

```
setpoint = base_offset + (coefficient / 4) * curve_value

curve_value = 4 * (T_target - 20)
            + 0.03 * (T_outside - 20)^2
            - 0.4  * (T_outside - 20)
```

Where `base_offset` is 27.2°C for radiators and 20.0°C for underfloor.

Worked examples for a radiator system, coefficient 1.5:

| Outside temp | Target | Flow setpoint |
|---|---|---|
| +15°C | 20°C | ~28°C |
| +5°C | 20°C | ~32°C |
| -5°C | 20°C | ~38°C |
| -10°C | 21°C | ~43°C |

---

### 5.6 Control Modes: Continuous and PWM

**Continuous mode (default)**: SAT sends the PID output directly as a flow temperature setpoint. The boiler modulates to reach and hold it. Preferred for modern condensing boilers.

**PWM mode**: For boilers that cannot modulate below 30-40%, SAT cycles the flame on and off at a calculated duty cycle. Within each control interval, the boiler runs at full setpoint for `duty * interval` seconds, then idles.

**Auto-switch**: When `satpwmautoswitch` is enabled (default), SAT monitors boiler behavior and switches modes automatically.

---

### 5.7 Safety System

SAT implements six independent safety layers. Any single layer tripping sends `CS=0`, releasing the setpoint override and returning control to the wall thermostat immediately.

1. **Boot safety**: `CS=0` is sent before the first control cycle.
2. **Disable safety**: When SAT is turned off, `CS=0` is sent immediately.
3. **Stale sensor fallback**: External temps expire (5 min indoor, 10 min outdoor); SAT falls back to OT bus values.
4. **Hard temperature ceiling**: Underfloor capped at 50°C, radiators at 80°C. Minimum is 10°C.
5. **Consecutive sensor failure counter**: 10 invalid room temperature readings trips the safety.
6. **PIC communication check**: 5 consecutive failed `CS=` commands trips the safety.

---

### 5.8 Simulation Mode

Simulation mode lets you run the SAT control loop without sending real commands to the PIC. Useful for testing configuration without affecting the actual heating system.

```bash
mosquitto_pub -h your-broker -t "OTGW/set/otgw-AABBCCDDEEFF/sat/simulation" -m "ON"
```

---

### 5.9 Home Assistant Integration

When MQTT is enabled, SAT entities are automatically discovered by Home Assistant.

**Climate entity**: The `sat_climate` entity shows current room temperature, target temperature, and mode. Available modes are `off`, `heat` (continuous), and `pwm`. Setting the target temperature persists the value to ESP flash storage.

**Key sensor entities**:

| Entity | Topic suffix | Description |
|---|---|---|
| `sensor.otgw_sat_setpoint` | `sat/setpoint` | Final flow temperature sent to boiler (°C) |
| `sensor.otgw_sat_heating_curve` | `sat/heating_curve` | Heating curve base value (°C) |
| `sensor.otgw_sat_pid_output` | `sat/pid_output` | PID corrected output (°C) |
| `sensor.otgw_sat_error` | `sat/error` | PID error: target minus room temperature (°C) |
| `sensor.otgw_sat_mode` | `sat/mode` | Control mode: `off`, `continuous`, or `pwm` |
| `sensor.otgw_sat_room_temp` | `sat/room_temp` | Room temperature used by PID (°C) |
| `sensor.otgw_sat_outside_temp` | `sat/outside_temp` | Outdoor temperature used by heating curve (°C) |
| `sensor.otgw_sat_boiler_status` | `sat/boiler_status` | Current boiler status (text label) |
| `sensor.otgw_sat_pwm_duty` | `sat/pwm_duty` | PWM duty cycle (0-1) |
| `sensor.otgw_sat_power` | `sat/power` | Estimated boiler power output (W) |
| `sensor.otgw_sat_energy_total` | `sat/energy_total` | Accumulated energy estimate (kWh) |
| `binary_sensor.otgw_sat_safety_tripped` | `sat/safety_tripped` | Whether a safety layer has tripped |
| `binary_sensor.otgw_sat_modulation_reliable` | `sat/modulation_reliable` | Whether boiler modulation feedback is reliable |
| `binary_sensor.otgw_sat_setpoint_mismatch` | `sat/setpoint_mismatch` | Setpoint mismatch between SAT and boiler |
| `binary_sensor.otgw_sat_thermal_model_valid` | `sat/thermal_model_valid` | Whether the thermal model has enough data |
| `binary_sensor.otgw_sat_solar_gain` | `sat/solar_gain` | Solar gain compensation active |
| `binary_sensor.otgw_sat_auto_tune_active` | `sat/auto_tune_active` | Auto-tune in progress |

**Using an external sensor from Home Assistant**:

```yaml
automation:
  - alias: "Push room temperature to SAT"
    trigger:
      - platform: state
        entity_id: sensor.living_room_temperature
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/indoor_temp"
          payload: "{{ states('sensor.living_room_temperature') }}"
```

**Using an external outdoor temperature source**:

```yaml
automation:
  - alias: "Push outdoor temperature to SAT"
    trigger:
      - platform: state
        entity_id: sensor.outdoor_temperature
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/outdoor_temp"
          payload: "{{ states('sensor.outdoor_temperature') }}"
```

External temperature values expire automatically (5 minutes for indoor, 10 minutes for outdoor). If the push stops, SAT falls back to OpenTherm bus values without alarming.

---

### 5.10 Known Limitations

- Not all thermostats report room temperature on OT MsgID 24. If yours does not, use an external sensor pushed via MQTT.
- Outdoor temperature (MsgID 27) is rarely exchanged on the bus. An external push or weather API call is typically needed.
- BLE temperature sensor scanning requires an ESP32 build.
- SAT controls the flow temperature setpoint, but the wall thermostat still controls whether heating is enabled or disabled.

---

### 5.11 Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Room never reaches target temperature | Coefficient too low | Increase by 0.2-0.3 |
| Room consistently overshoots target | Coefficient too high | Decrease by 0.2-0.3 |
| Frequent short boiler cycles | Boiler minimum modulation too high | Enable PWM auto-switch |
| Safety tripped on startup | PIC not ready at SAT init | Normal behavior. Re-enable SAT. |
| "External temp stale" in logs | MQTT push stopped | Check HA automation and broker connectivity |
## Chapter 6: Network Configuration

This chapter covers everything the firmware does on the network: from first-time WiFi setup through Ethernet failover, hostname resolution, time synchronization, and OTA firmware updates.

### 6.1 WiFi Setup

On first boot the firmware has no saved WiFi credentials. It opens a captive portal so you can enter them.

#### 6.1.1 Initial Configuration via Captive Portal

When no credentials are stored, the device starts a WiFi access point named after its hostname and MAC address, for example `otgw-AABBCCDD`. Connect to that SSID from a phone or laptop and a configuration page opens automatically (or navigate to `192.168.4.1` manually). Enter your SSID and password and click Save. The device reboots, connects to your network, and the AP disappears.

#### 6.1.2 Reconnection Behavior

The firmware uses a two-tier reconnection strategy:

1. The ESP SDK's built-in auto-reconnect handles short blips (typically under 30 seconds).
2. If the connection stays down longer, the application-level `loopWifi()` state machine retries non-blocking with a 30-second window per attempt. In production builds it retries up to 10 times before rebooting. In beta builds it enters AP fallback mode after 2 failed retries (see section 6.1.3).

After reconnecting, the firmware re-applies the configured hostname, forces a DHCP re-announce so the router learns the correct hostname, and restarts Telnet, MQTT, and WebSocket services automatically.

#### 6.1.3 AP Fallback Mode

Available in beta builds (v2.0.0-beta and later, all platforms). When WiFi retries are exhausted at runtime, the device switches into AP fallback mode instead of rebooting indefinitely. In production releases the device reboots after 10 failed retries instead.

At boot, if credentials are stored but WiFi is unreachable, beta builds also skip the WiFiManager config portal and go straight to AP fallback.

- **SSID**: `OTGW-<last 3 bytes of MAC address in uppercase hex>`, for example `OTGW-AABBCC`
- **IP address**: `192.168.4.1`
- **Password**: `otgw123`
- **MQTT**: disabled in fallback mode
- **OTA update**: available from fallback mode so you can flash a corrected firmware without serial access

In AP fallback mode the device keeps attempting to reconnect to the configured WiFi network every 5 minutes. When WiFi comes back, services (MQTT, Telnet, WebSocket) restart automatically and the AP is torn down.

**How to recover from fallback:**
1. Connect to the `OTGW-XXXXXX` SSID using password `otgw123`.
2. Open `http://192.168.4.1` in your browser.
3. Correct your WiFi credentials in Settings or use Reset WiFi.
4. Reboot the device.

#### 6.1.4 Resetting WiFi Credentials

| Method | How |
|---|---|
| Web UI | Settings page, click "Reset WiFi" button |
| Triple-reset detection | Power-cycle the device 3 times within 10 seconds |
| Telnet debug console | Send the `resetwifi` command on port 23 |

---

### 6.2 Ethernet (ESP32 + W5500)

Wired Ethernet is available on ESP32 boards with a W5500 SPI Ethernet controller. Not available on ESP8266.

The W5500 uses these default GPIO assignments:

| W5500 Signal | ESP32 GPIO |
|---|---|
| CS (chip select) | GPIO 5 |
| INT (interrupt) | GPIO 34 |
| RST (reset) | GPIO 15 |
| SCK (SPI clock) | GPIO 18 |
| MISO | GPIO 19 |
| MOSI | GPIO 23 |

**Priority over WiFi**: Ethernet always takes priority. When the W5500 is present and a cable is plugged in, WiFi is disabled. If the cable is unplugged at runtime, the firmware detects this (checked every 5 seconds) and switches back to WiFi automatically.

---

### 6.3 Static IP vs DHCP

By default the device uses DHCP. If your router supports DHCP reservations, assigning a fixed IP by MAC address is the recommended approach.

For a true static IP configured in firmware:

1. Open Settings in the web interface.
2. Set the Static IP, Subnet Mask, Gateway, and DNS fields.
3. Save and reboot.

Leave all fields empty to revert to DHCP.

---

### 6.4 mDNS and Hostname

The default hostname is `otgw`. After connecting to your network the device is reachable as `otgw.local` from any mDNS-capable client (macOS, iOS, Linux with Avahi, Windows 10+ with Bonjour).

The firmware also registers an LLMNR responder on ESP8266, which allows Windows machines to resolve `otgw` (without the `.local` suffix).

Set the hostname in Settings. The new hostname propagates to the DHCP request, mDNS advertisement, MQTT client ID, and web UI title.

---

### 6.5 NTP Time Synchronization

| Setting | Default | Description |
|---|---|---|
| NTP server | `pool.ntp.org` | Any NTP server hostname or IP address |
| Timezone | `Europe/Amsterdam` | IANA timezone database name |
| NTP enabled | `true` | Can be disabled if no internet access |

The firmware re-checks the time every 30 minutes. After each successful NTP sync, it sends clock commands to the boiler PIC (`SC=HH:MM/D`, `SR=21:MM,DD`, `SR=22:YH,YL`).

---

### 6.6 OTA Firmware Updates

The OTA update endpoint is at `/update`. Authentication uses HTTP Basic Auth when a password is configured.

**Steps:**
1. Open `http://otgw.local/update` in your browser.
2. Select the `.bin` firmware file.
3. Click Upload.
4. The device flashes the firmware, verifies the image, and reboots.

OTA is available from AP fallback mode as well, so you can recover a device stuck on a bad firmware image without serial access.

---

### 6.7 Port Usage

| Port | Protocol | Service | Notes |
|---|---|---|---|
| 80 | TCP / HTTP | Web interface and REST API | All web UI routes, file serving, OTA update |
| 81 | TCP / WebSocket | Live OpenTherm log stream | Used by the web UI for real-time data |
| 23 | TCP / Telnet | Debug console | Plain-text debug log; served by SimpleTelnet library |
| 25238 | TCP | Serial bridge (ser2net) | Raw OTGW PIC serial over TCP; served by SimpleTelnet library; OTmonitor compatible |
| 123 | UDP (outbound) | NTP (SNTP) | Outbound only |
| 5353 | UDP | mDNS | Local network name resolution |
| 5355 | UDP | LLMNR | Windows name resolution (ESP8266 only) |
| 1883 | TCP (outbound) | MQTT | Outbound to your MQTT broker |

---

### 6.8 Firewall and Router Considerations

The firmware is designed for a local, trusted network. No inbound ports need to be forwarded through your router.

mDNS (`otgw.local`) only works within a single Layer 2 broadcast domain. If Home Assistant is on a different VLAN, use the device's static IP or a DNS entry instead of `.local`.

---

### 6.9 Using Behind a Reverse Proxy

The REST API works correctly behind an HTTPS-terminating reverse proxy (Nginx, Caddy, etc.). The WebSocket live log (port 81) requires a plain `ws://` connection and does not support `wss://`. A reverse proxy that upgrades WebSocket connections to WSS will break the live log in the web interface.

---

## Chapter 7: Troubleshooting

This chapter covers the most common problems, their root causes, and step-by-step remedies. When in doubt, start with the Telnet debug log (section 7.9) and work forward from there.

### 7.1 OTGW Appears Offline After Boot

**Symptom:** The web UI shows the OTGW as offline. MQTT publishes nothing. The live log is empty.

| Check | What to look for |
|---|---|
| PIC serial connection | A loose or broken UART wire between ESP and PIC is the most common hardware cause |
| PIC powered | The OTGW PIC needs power from the OpenTherm bus or an external supply |
| PIC firmware mode | If in diagnostic or interface mode from a previous OTmonitor session, use the web UI to reset the PIC |

**Steps:**
1. Connect to the Telnet debug console (see section 7.9).
2. Look for `OTGW online` or `OTGW offline` messages.
3. If you see `OTGW offline` repeating, the PIC is not responding on serial.
4. Power-cycle the entire OTGW board (not just the ESP).

---

### 7.2 MQTT Not Connecting

**Symptom:** MQTT status shows disconnected.

Common MQTT return codes:

| Code | Meaning |
|---|---|
| -1 | Connection timeout (broker unreachable, wrong IP, firewall) |
| -2 | Connection refused (wrong port, broker not running) |
| 1 | Connection refused: unacceptable protocol version |
| 4 | Connection refused: bad username or password |
| 5 | Connection refused: not authorized |

**Diagnosis:**
1. Open the Telnet debug console (port 23).
2. Search for `MQTT` in the output for error codes.
3. Verify the broker is reachable: `mosquitto_sub -h <broker-ip> -t "test" -v` from another machine.

---

### 7.3 Home Assistant Entities Not Appearing

**Checklist:**

1. MQTT integration enabled in HA: Settings, Devices and Services.
2. Discovery prefix matches: default is `homeassistant` in both HA and firmware.
3. Discovery enabled in firmware: check Settings in the web UI.
4. Wait 30-60 seconds after MQTT connect for discovery to run.
5. Check HA MQTT logs in Settings, System, Logs, filter for `mqtt`.
6. If you run multiple OTGW devices, each must have a unique `settingMQTTuniqueid`.

---

### 7.4 WiFi Keeps Dropping

| Cause | Diagnosis | Fix |
|---|---|---|
| Weak signal | Check RSSI in web UI header. Below -75 dBm is marginal. | Move router or add an AP closer. |
| IP address conflict | Another device has the same IP. | Use a DHCP reservation on the router. |
| AP fallback loop | Check Telnet log for `entering AP fallback mode`. | Fix SSID/password or triple-reset. |

---

### 7.5 OTA Update Failed

| Symptom detail | Cause | Fix |
|---|---|---|
| Browser shows "Authentication required" | HTTP password is set | Use username `admin` and your configured password |
| Device reboots to old firmware | Binary failed MD5 check | Confirm you are uploading the correct `.bin` for your board |
| "Not enough space" error | Filesystem too full | Free space via the File System Explorer at `/fsexplorer` |

---

### 7.6 Web UI Not Loading

1. Confirm the device is on the network: ping `<ip>` or try the IP directly.
2. mDNS resolution failure: on Windows without Bonjour, try the IP address.
3. Filesystem not mounted: flash the filesystem image via `uploadfs` or upload files via `/fsexplorer`.
4. Device in AP fallback mode: connect to the fallback SSID (`OTGW-XXXXXX`) and browse to `http://192.168.4.1`.

---

### 7.7 Serial Overrun and OTGW Online-Offline Flapping

**Root cause in versions before 2.0.0:** The `publishToSourceTopic()` function allocated approximately 1,600 bytes of local buffers on the cooperative stack. The ESP8266 CONT stack is only 4 KB. The fix in v2.0.0 converts those buffers to static or pre-allocated scratch memory.

If you see this on v2.0.0: update to the latest build. If the problem persists, connect to the Telnet debug console and look for `Exception` or `Stack smashing` messages.

---

### 7.8 LED Patterns

The firmware does not implement a dedicated LED status pattern on ESP8266 NodeMCU or Wemos D1 Mini boards.

The physical OTGW board itself has indicator LEDs driven by the PIC:
- **Green LED**: PIC is running and communicating.
- **Yellow LED**: Boiler communication active.
- **Red LED**: Error or fault condition.

---

### 7.9 Telnet Debug Output

The firmware streams a real-time debug log over Telnet on port 23.

#### Connecting

```
telnet otgw.local 23
```

Or use PuTTY: connection type "Other", protocol "Telnet", host `otgw.local`, port 23.

#### Reading Common Error Patterns

| Pattern in log | Meaning |
|---|---|
| `WiFi disconnected` / `WiFi reconnected` in short loops | Intermittent signal or IP conflict |
| `MQTT connect failed, rc=-2` | Broker not reachable; check IP and port |
| `MQTT connect failed, rc=4` | Wrong username or password |
| `OTGW offline` without `OTGW online` ever appearing | PIC not responding on serial |
| `Serial overrun` | Main loop blocked too long |
| `Exception (2)` | Illegal memory access; note the stack trace and report as a bug |
| `Exception (3)` | Load/store alignment error; PROGMEM pointer mismatch |
| `NTP: bogus initial time ignored` | Normal on first boot; SDK quirk guard working correctly |

---

### 7.10 Factory Reset

**Method 1: Web UI**
1. Open `http://otgw.local/fsexplorer`.
2. Delete the file `/settings.json`.
3. Reboot the device.

**Method 2: Triple-reset (WiFi credentials only)**
Power-cycle the device 3 times within 10 seconds.

**Method 3: Telnet debug console**
Connect on port 23 and type `resetwifi` to clear WiFi credentials only.

---

### 7.11 Where to Get Help

1. **GitHub Issues:** `https://github.com/rvdbreemen/OTGW-firmware/issues`
   Include: firmware version, board type, Telnet debug log, and settings (redact passwords).

2. **Discord:** Check the GitHub repository for the invite link.

3. **Tweakers.net forum:** The Dutch home automation community has a long-running OTGW discussion thread.
## Chapter 8: Developer Guide

This chapter is the primary reference for contributors and developers working with the OTGW-firmware source code. It covers the project layout, build system, architecture model, key coding patterns, and the step-by-step workflows for extending the firmware with new REST endpoints, MQTT topics, and settings. Read this chapter before writing a single line of code.

---

### Project Layout

The repository root contains the firmware source, build tooling, documentation, tests, and reference projects. The primary source tree is under `src/OTGW-firmware/`.

#### Source Files

All firmware code lives in `src/OTGW-firmware/` as Arduino `.ino` files. Arduino compiles the main sketch plus all `.ino` files in the same directory as a single translation unit. Each `.ino` file owns one logical domain:

| File | Domain |
|------|--------|
| `OTGW-firmware.ino` | Main entry point: `setup()`, `loop()`, boot sequence, LED management |
| `OTGW-Core.ino` | OpenTherm protocol decoding, PIC serial I/O, command queue, MQTT throttle |
| `OTGW-Core.h` | OpenTherm data structures: `OTdataStruct`, `OTLibMessageID`, frame type enums |
| `OTGW-firmware.h` | Shared typedefs, all struct definitions (`OTGWSettings`, `OTGWState`), forward declarations, constants |
| `MQTTstuff.ino` | MQTT client state machine, HA auto-discovery, command subscriptions, `sendMQTTData()` |
| `networkStuff.ino` | WiFi, mDNS, LLMNR, NTP, OTA, network platform abstraction |
| `restAPI.ino` | REST API v2 dispatch table, all resource handlers, HTTP Basic Auth, CSRF, file serving |
| `FSexplorer.ino` | LittleFS browser: file listing, upload/delete endpoints |
| `jsonStuff.ino` | Low-level JSON formatting helpers: `sendJsonMapEntry()`, `sendJsonKVLine()`, streaming map builders |
| `settingStuff.ino` | Settings load/save (LittleFS JSON), `updateSetting()` validation, side-effect dispatch |
| `webSocketStuff.ino` | WebSocket server on port 81, OT log streaming, heap backpressure gate |
| `helperStuff.ino` | Heap health monitor, `canSendWebSocket()`, `canPublishMQTT()`, LittleFS status helpers |
| `OTDirect.ino` | ESP32-only: direct GPIO OpenTherm bus via ISR, OTDirect operating modes, frame bridge |
| `SAT.ino` | Smart Autotune Thermostat: master enable/disable, SAT settings bridge |
| `SATcontrol.ino` | SAT control loop, heating curve, boiler state machine, setpoint injection |
| `SATpid.ino` | PID v3 implementation: proportional, integral, derivative with deadband |
| `SATcycles.ino` | Cycle classification, overshoot detection, anti-cycling |
| `SATweather.ino` | Open-Meteo weather fetch, outdoor temperature for SAT heating curve |
| `SATble.ino` | ESP32 BLE room temperature sensor integration (BTHome protocol) |
| `sensorStuff.ino` | Dallas DS18B20 temperature sensors, S0 pulse counter, OLED display |
| `boards.h` | Board-specific pin maps and feature flags (`HAS_PIC`, `HAS_DIRECT_OT`, `HAS_ETH_CAPABLE`) |
| `safeTimers.h` | Timer macros: `DECLARE_TIMER_SEC()`, `DUE()`, `RESTART_TIMER()` |
| `platform.h`, `platform_esp8266.h`, `platform_esp32.h` | Platform abstraction layer |

#### Web Assets

The browser SPA lives in `src/OTGW-firmware/data/`. These files are flashed to LittleFS:

| File | Purpose |
|------|---------|
| `index.html` | Single-page application shell (~11 KB — always stream, never load into RAM) |
| `index.js` | Main SPA JavaScript: live OT log, WebSocket client, settings forms |
| `graph.js` | ECharts-based real-time temperature graphs |
| `index.css` | Stylesheet with dark/light theme support |

#### Build Infrastructure

| File / Directory | Purpose |
|-----------------|---------|
| `build.py` | Primary build script: invokes PlatformIO internally, packages firmware and LittleFS artifacts |
| `platformio.ini` | PlatformIO project: `esp8266` and `esp32` environments |
| `evaluate.py` | Static code quality checker: PROGMEM usage, unsafe patterns, String class audit |
| `scripts/` | PlatformIO pre-build scripts: version injection, library patching |
| `libraries/` | Vendored Arduino libraries used by both build systems |
| `docs/` | C4 architecture docs, ADRs, API references, feature docs |
| `other-projects/` | Read-only reference codebases (PIC firmware, OTmonitor, OT-Thing, SAT) |
| `backlog/` | Project task management (Backlog.md) |

---

### Build System

#### build.py (release builds)

The primary build script for producing release artifacts. It invokes PlatformIO internally, handles version embedding, filesystem packaging, and artifact collection.

```bash
python build.py              # Build firmware + LittleFS filesystem image
python build.py --firmware   # Build firmware only (skip filesystem)
python build.py --clean      # Clean build artifacts before building
python build.py --upload     # Build and upload to connected device
```

The script reads the Git tag to embed the version string and places output in the `build/` directory.

#### PlatformIO

PlatformIO is the preferred build system and is used for both ESP8266 and ESP32. Two environments are defined in `platformio.ini`:

| Environment | Target | Board | Core | CPU |
|-------------|--------|-------|------|-----|
| `esp8266` | Wemos D1 mini / NodeMCU (NodoShop OTGW) | `d1_mini` | Arduino Core 3.1.2 (espressif8266) | 160 MHz |
| `esp32` | NodoShop OTGW32 | `esp32dev` | pioarduino espressif32 fork | 240 MHz |

The ESP8266 LittleFS partition size is 2 072 576 bytes (approximately 2 MB). This is configured in `platformio.ini` via the board options.

```bash
pio run                          # Build all environments
pio run -e esp8266               # Build ESP8266 only
pio run -e esp32                 # Build ESP32 only
pio run -e esp8266 -t upload     # Build and upload ESP8266
pio run -e esp8266 -t uploadfs   # Upload LittleFS filesystem (ESP8266)
pio run -e esp32 -t upload       # Build and upload ESP32
pio run -e esp32 -t uploadfs     # Upload LittleFS filesystem (ESP32)
```

PlatformIO's pre-build scripts in `scripts/` handle version injection (`platformio_version.py`) and library compatibility patching (`patch_pio_libs.py`).

##### ESP8266-specific PlatformIO notes

The ESP8266 build excludes `OTDirect.ino` (ESP32-only) via `build_src_filter`. PlatformIO's ctags scanner processes all `.ino` files regardless of the filter, so ESP32-specific types (`OpenThermResponseStatus`, `OTDirectMode`, `OTDirectRequestOrigin`) are stubbed out as `-D` flags to prevent ctags-generated forward declaration errors. These stubs have no runtime effect.

The `OpenTherm Library` is explicitly excluded in `lib_ignore` because PlatformIO's LDF scanner ignores `#if` guards and would otherwise compile the ESP32-only OT library for ESP8266.

##### ESP32-specific PlatformIO notes

PlatformIO's ctags scanner processes all `.ino` files regardless of `#if` guards and generates forward declarations for every function it finds. On ESP32 (`HAS_PIC=0`), `OTGWSerial.h` is never included, so `OTGWFirmware` (an enum type used as a parameter in `fwreportinfo()`) is unknown at ctags forward-declaration time. The flag `-DOTGWFirmware=int` stubs it out so the generated forward declaration compiles. The actual `fwreportinfo()` body is inside a `#if HAS_PIC` block and is never compiled on ESP32. This follows the same pattern as the OTDirect type stubs for ESP8266.

#### Arduino IDE

Arduino IDE is supported for ESP8266 only. Install the ESP8266 Arduino core, then open `src/OTGW-firmware/OTGW-firmware.ino`. Required libraries are in `libraries/` — add that directory to your Arduino sketchbook library path. The IDE does not support the ESP32 target or `OTDirect.ino`.

#### evaluate.py

Run the code quality checker after making changes to catch common ESP8266-specific mistakes:

```bash
python evaluate.py           # Full analysis (all files)
python evaluate.py --quick   # Fast pass (changed files only)
```

The checker flags:
- `Serial.print()` calls (must never appear after init)
- String literal not wrapped in `F()` or `PSTR()`
- `String` class usage in non-setup code
- `strcpy`/`sprintf` without bounds (should use `strlcpy`/`snprintf_P`)
- `strncmp_P`/`strstr_P` on binary data (use `memcmp_P`)

---

### Board Hardware Variants

The firmware supports two official Nodoshop hardware variants, defined in `boards.h`. The board is selected at build time by a `BOARD_*` preprocessor flag set in `platformio.ini`. When no explicit flag is set, the board is auto-detected from the MCU type (`ESP8266` or `ESP32`).

**These flags are board-level, not MCU-level.** The presence of a PIC, Ethernet hardware, or a direct OpenTherm GPIO interface is a property of the specific Nodoshop PCB, not of the MCU family. Replacing an ESP8266 with an ESP32 on the OTGW WiFi board is not an official Nodoshop configuration and is not modelled.

#### Feature Flags

Three compile-time flags in `boards.h` describe the hardware capabilities of each board:

| Flag | Type | OTGW WiFi (ESP8266) | OTGW32 (ESP32) | Meaning |
|------|------|---------------------|----------------|---------|
| `HAS_PIC` | `0` / `1` | `1` | `0` | Board has a PIC co-processor that handles the OpenTherm electrical bus over UART |
| `HAS_DIRECT_OT` | `0` / `1` | `0` | `1` | Board drives the OpenTherm bus directly from GPIO via the OTDirect ISR |
| `HAS_ETH_CAPABLE` | `0` / `1` | `0` | `1` | Board has a W5500 SPI Ethernet module |

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

#### W5500 Runtime Detection

`HAS_ETH_CAPABLE=1` means the board design includes a W5500 module header, but the module may not be physically installed on every unit. At boot, `probeW5500()` in `Ethernet.ino` reads the W5500 VERSION register via SPI:

- If the register returns the expected value, `state.hw.bEthernetPresent` is set to `true` and Ethernet is initialised.
- If the read fails or returns an unexpected value, `state.hw.bEthernetPresent` is set to `false` and the firmware runs in WiFi-only mode.

No user configuration is required. `loopEthernet()` guards itself with `if (!state.hw.bEthernetPresent) return;` so it is a no-op when no hardware is present.

---

### C4 Architecture Overview

The firmware uses a four-level C4 model documented in `docs/c4/`. Read the relevant level before touching code.

#### Level 1: Context (`c4-context.md`)

The OTGW-firmware sits between the OpenTherm heating bus and the home automation network. External actors are: Room Thermostat, Boiler (via PIC co-processor or OTDirect GPIO on ESP32), MQTT Broker, Home Assistant, NTP Server, Web Browser, OTmonitor TCP client, Dallas/S0/BLE sensors.

#### Level 2: Container (`c4-container.md`)

Two deployment targets share one codebase:
- **ESP8266 (OTGW)**: NodoShop board with PIC16F co-processor. PIC handles the OpenTherm electrical bus; ESP8266 communicates with it over UART at 9600 baud.
- **ESP32 (OTGW32)**: NodoShop board with direct GPIO OpenTherm interface (`OTDirect`), W5500 Ethernet, BLE, and OLED. The PIC is absent; the ESP32 drives the OT bus directly via interrupt-driven Manchester coding.

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

The ESP8266 Arduino framework is single-threaded and cooperative. The main `loop()` calls `doBackgroundTasks()`, which in turn calls all subsystem handlers. There is no RTOS; `delay()` blocks everything and must never be used. Long operations must be broken into state machines checked on each `loop()` iteration.

`feedWatchDog()` services the hardware watchdog. It does not call `yield()` (that line is intentionally commented out to prevent re-entrancy of `doBackgroundTasks()`). The background task re-entrancy that does exist comes from MQTT autoconfig's file-reading loop calling `handleOTGW()` mid-iteration.

#### Static Buffers (ADR-004 / ADR-053)

The ESP8266 has approximately 40 KB of usable heap. Dynamic allocation fragments the heap and leads to crashes after days of operation. All buffers are statically allocated:

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

#### Platform Abstraction Layer (ADR-061)

All ESP8266/ESP32 SDK differences are isolated in three files: `platform.h`, `platform_esp8266.h`, and `platform_esp32.h`. Application code includes only `platform.h` and calls the unified functions. Never call platform-specific SDK functions directly in `.ino` files.

Key functions provided by the abstraction layer:

| Function | Description |
|----------|-------------|
| `platformSetHostname(hostname)` | Set the WiFi station hostname for DHCP |
| `platformGetHostname()` | Read back the current hostname |
| `platformRestartDHCP()` | Force a DHCP re-announce (e.g., after hostname change) |
| `platformFreeHeap()` | Free heap in bytes |
| `platformMaxFreeBlock()` | Largest contiguous free block (fragmentation indicator) |
| `platformRestart()` | Platform-safe reboot |
| `platformCoreVersion()` | Arduino core version string |
| `platformChipId()` | Unique chip identifier |
| `platformGetMacAddress(mac)` | Fill a 6-byte buffer with the MAC address |

On ESP8266, `MDNS_NEEDS_UPDATE` is defined as `1`, which means the main loop must call `MDNS.update()` on every iteration. On ESP32 this is not needed (`MDNS_NEEDS_UPDATE` is not defined). The main loop guards this with `#if defined(MDNS_NEEDS_UPDATE)`.

The `PlatformDir` class in `platform.h` provides a unified directory iteration interface over LittleFS (ESP8266 uses `Dir`; ESP32 uses `File`-based iteration). Use `PlatformDir` instead of calling filesystem APIs directly.

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

#### Settings / State Separation (ADR-051)

All persistent configuration lives in `OTGWSettings settings` (serialised to `/settings.ini` on LittleFS). All transient runtime state lives in `OTGWState state`. Both structs are defined in `OTGW-firmware.h` and use two-level named sub-sections with Hungarian prefixes:

```cpp
settings.mqtt.sBroker          // persistent: MQTT broker hostname
settings.sat.fHeatingCurveCoeff // persistent: SAT heating curve coefficient
state.otgw.bOnline             // transient: PIC serial link alive
state.pic.sFwversion           // transient: PIC firmware version string
```

Never persist transient state. Never access `state` fields from `writeSettings()`.

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
- Never use `Serial.print()` for debug — use `DebugTln()` or `DebugTf()`.
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

Example — publish a new float value:

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

The firmware subscribes to `{TopTopic}/set/{UniqueId}/#` on MQTT connect. Incoming messages are dispatched in `mqttCallback()` in `MQTTstuff.ino`. The dispatch table is `cmdtable[]`, an array of `MQTT_set_cmd_t` structs:

```cpp
static const MQTT_set_cmd_t cmdtable[] PROGMEM = {
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

HA discovery payloads are generated from `mqttha.cfg` (stored in LittleFS). This is a template file processed by `doAutoConfigure()` / `doAutoConfigureMsgid()`. If your new topic needs a HA entity, add an entry to `mqttha.cfg` following the existing template format. The file is too large to document fully here — examine existing entries for the pattern.

---

### Adding a New Setting

Adding a setting requires changes in four places: the struct definition, the serializer, the deserializer/validator, and (optionally) the web UI.

#### Step 1: Add to the Settings Struct

Open `OTGW-firmware.h`. Find the relevant sub-section struct (e.g., `MQTTSection`, `SATSection`, `DeviceSection`). Add the new field with its type and default value:

```cpp
struct MySection {
  bool   bMyNewFlag   = false;     // b prefix for bool
  char   sMyString[32] = "";       // s prefix for char array
  int    iMyInteger   = 0;         // i prefix for int
  float  fMyFloat     = 1.0f;      // f prefix for float
};
```

If adding to an existing section, add to the appropriate struct. If creating a new section, add it to `OTGWSettings`:

```cpp
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

The `settingsDirty` flag triggers a deferred LittleFS write (typically after a 2-second debounce timer).

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
- Guard `#ifdef ESP8266` / `#ifdef ESP32` blocks where platform-specific hardware is involved, including debug macro sections that use platform-specific I/O.
- Use `enum class` for internal discriminators, not string tokens.

---

### Debug Tools

#### Telnet Debug Console (Port 23)

The firmware streams diagnostic output to Telnet port 23 via the SimpleTelnet library (`SimpleTelnet<1> debugTelnet`, declared extern in `OTGW-firmware.h` and defined in `networkStuff.ino`). SimpleTelnet replaces the older TelnetStream and ESPTelnet libraries and provides a unified API on both ESP8266 and ESP32. Connect with any Telnet client:

```bash
telnet otgw.local 23
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
| `state.debug.bOTDirect` | `OTDDebugTln()`, `OTDDebugTf()` | OTDirect GPIO frame handling (ESP32) |

#### Browser Console Debug

When `window.otgwDebug = true` is set in the browser console before page load, the JavaScript SPA logs verbose diagnostics. The SAT subsystem has a separate flag: `window.SAT_DEBUG = true`.

#### evaluate.py

After making changes, always run:

```bash
python evaluate.py
```

This checks for the patterns most likely to cause crashes on ESP8266. Address all warnings before submitting a pull request.

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

All commands to the OTGW PIC must go through `addCommandToQueue()`. Direct UART writes are forbidden because the PIC requires response matching and command ordering.

```cpp
// Signature:
void addCommandToQueue(const char* cmd, int len,
                       const bool urgent = false,
                       const int16_t responseTimeout = 1000);

// Examples:
addCommandToQueue(PSTR("TT=21.50"), 8);            // temporary temperature setpoint
addCommandToQueue(PSTR("CS=55.00"), 8, true);      // urgent: control setpoint, high priority
addCommandToQueue(PSTR("GW=1"), 4);                // gateway mode on
```

The queue is processed in the OTGW-Core background task. Responses from the PIC are matched against outstanding commands and published to MQTT. On OTGW32 (OTDirect), the queue is handled internally by `loopOTDirect()`.

Never call `addCommandToQueue()` from an ISR or from within the PIC response handler.

---

### OpenTherm Message Processing Flow

Understanding this pipeline is essential before modifying any OT message handling:

```
Boiler/Thermostat
       |
  OpenTherm bus (2-wire)
       |
  PIC co-processor (ESP8266) or OTDirect GPIO ISR (ESP32)
       |
  UART 9600 baud ASCII line (e.g., "BA000001\r")   [ESP8266]
  or decoded OT frame struct                        [ESP32]
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

ADRs live in `docs/adr/`. There are 76+ ADRs covering every significant design decision. ADRs are immutable once Accepted.

#### When to Read ADRs

Before: modifying architecture, changing API contracts, adding dependencies, touching build tooling.

Not required for: pure bug fixes, minor features within existing patterns.

#### Key ADRs to Know

| ADR | Topic | Impact |
|-----|-------|--------|
| ADR-004 | Static buffer allocation | No `String` in hot paths; all buffers `char[]` |
| ADR-009 | PROGMEM string literals | All literals in flash via `F()` / `PSTR()` |
| ADR-014 | Dual build system | `build.py` for Arduino CLI, PlatformIO for ESP32 |
| ADR-016 | OpenTherm command queue | Always use `addCommandToQueue()` |
| ADR-019 | REST API versioning | `/api/v2/` is current; v0/v1 return 410 Gone |
| ADR-028 | File streaming | Files >2 KB must use `streamFile()`, never load |
| ADR-040 | MQTT source-specific topics | Source separation topic structure |
| ADR-042 | Streaming JSON | No ArduinoJson; manual `snprintf_P` construction |
| ADR-050 | Centralized API dispatch | Single `kV2Routes[]` table; no per-route router changes |
| ADR-051 | Dual encapsulating structs | `OTGWSettings` / `OTGWState` architecture |
| ADR-053 | Large feature buffer allocation | Static allocation for large per-feature buffers |
| ADR-061 | ESP8266/ESP32 platform abstraction | Use `platform.h` abstractions, not direct SDK calls |

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
8. Build for both targets and confirm no compile errors: `pio run`.
9. Test on real hardware if possible; use the simulation mode in `state.debug.bOTGWSimulation` if hardware is unavailable.
10. Write a final summary in the backlog task.
11. Open a pull request against the `dev` branch, not `main`.
12. Include the backlog task ID in the PR description.

Pull requests to `main` are not accepted directly. All merges to `main` go through `dev` and are coordinated by the maintainer.
## Chapter 9: API Reference

This chapter documents all programmatic interfaces exposed by the OTGW-firmware: the REST API, the WebSocket stream, the MQTT integration, and the Telnet debug console. These are the interfaces used by Home Assistant, Node-RED flows, automation scripts, and custom integrations.

---

### REST API Overview

#### Base URL and Versioning

The current REST API version is v2. All v2 endpoints are under:

```
http://{device-ip}/api/v2/
```

Older versions return HTTP 410 Gone. Never use `/api/v0/` or `/api/v1/` in new code.

The device hostname defaults to `otgw.local` (mDNS / LLMNR). Alternatively, use the IP address assigned by your DHCP server.

#### Transport

- HTTP only, no HTTPS (ADR-003). The device operates on a trusted local network. For remote access, place it behind a reverse proxy or VPN.
- Content-Type for all responses: `application/json`
- All JSON responses are UTF-8

#### Authentication

Mutation endpoints (POST, PUT, DELETE) require HTTP Basic Authentication when an HTTP password is configured in Settings. The username is the device hostname (default `OTGW`). Authentication is checked via the `Authorization: Basic` header.

GET endpoints for sensitive data (settings, PIC settings) also require authentication.

Unauthenticated GET endpoints (health, sensor data, OT values) are intentionally public.

#### Error Response Format

All errors use a consistent JSON envelope:

```json
{
  "error": {
    "status": 404,
    "message": "Resource not found"
  }
}
```

HTTP status codes follow RFC 7231. Common error codes:

| Code | Meaning |
|------|---------|
| 400 | Bad Request: malformed JSON or missing required field |
| 401 | Unauthorized: authentication required |
| 403 | Forbidden: CSRF check failed |
| 404 | Not Found: unknown endpoint or resource |
| 405 | Method Not Allowed |
| 410 | Gone: deprecated API version (v0, v1) |
| 500 | Internal Server Error |
| 503 | Service Unavailable: low heap, PIC unavailable, etc. |

---

### REST API Endpoints

#### Health

##### GET /api/v2/health

Returns system health metrics. No authentication required.

**Request:** None

**Response:**

```json
{
  "uptime": 86400,
  "heap_free": 18432,
  "heap_health": "healthy",
  "mqtt_connected": true,
  "pic_available": true,
  "otgw_online": true,
  "wifi_rssi": -58,
  "firmware_version": "2.0.0",
  "timestamp": 1712345678
}
```

| Field | Type | Description |
|-------|------|-------------|
| `uptime` | integer | Seconds since last boot |
| `heap_free` | integer | Free heap in bytes |
| `heap_health` | string | `healthy`, `low`, `warning`, `critical` |
| `mqtt_connected` | boolean | MQTT broker connection status |
| `pic_available` | boolean | PIC co-processor detected and responding |
| `otgw_online` | boolean | OpenTherm serial link active |
| `wifi_rssi` | integer | WiFi signal strength in dBm (absent when on Ethernet) |
| `firmware_version` | string | Installed firmware version |
| `timestamp` | integer | Current Unix timestamp (requires NTP sync) |

This endpoint is safe to poll frequently. Typical use: OTA update completion detection, watchdog integration, Home Assistant availability sensor.

---

#### Settings

##### GET /api/v2/settings

Returns all device settings. Authentication required.

**Request:** None

**Response:** A flat JSON object containing all configurable settings fields. Example subset:

```json
{
  "hostname": "otgw",
  "httppassword": "****",
  "mqttenable": true,
  "mqttbroker": "192.168.1.10",
  "mqttport": 1883,
  "mqttuser": "otgw",
  "mqtttoptopic": "OTGW",
  "mqttuniqueid": "otgw-AABBCCDDEEFF",
  "mqtthaprefix": "homeassistant",
  "mqtthadiscovery": true,
  "ntptimezone": "Europe/Amsterdam",
  "ntphost": "pool.ntp.org",
  "ssid": "MyHomeNetwork",
  "satenable": false
}
```

The `ssid` field is read-only and returns the currently connected WiFi SSID. On Ethernet, `ssid` is not present; check `/api/v2/device/info` for `"ssid": "Wired"`. The `httppassword` field returns a masked placeholder, never the actual password.

##### POST /api/v2/settings

Update one or more settings. Authentication required.

**Request body:** JSON object with any subset of setting key-value pairs.

```json
{
  "mqttbroker": "192.168.1.20",
  "mqttport": 1883,
  "mqtthadiscovery": true
}
```

**Response:**

```json
{
  "status": "ok",
  "saved": true
}
```

Settings are validated server-side. Invalid values are silently ignored or constrained to safe ranges. Side effects (MQTT reconnect, NTP resync, mDNS restart) are applied within 2 seconds via a deferred-write timer.

---

#### Device Information

##### GET /api/v2/device/info

Returns hardware and platform information. No authentication required.

**Response:**

```json
{
  "hostname": "otgw",
  "firmware_version": "2.0.0",
  "platform": "ESP8266",
  "board": "NODOSHOP_ESP8266",
  "cpu_freq_mhz": 160,
  "flash_size": 4194304,
  "chip_id": "AABBCC",
  "ssid": "MyHomeNetwork",
  "ip": "192.168.1.50",
  "mac": "AA:BB:CC:DD:EE:FF",
  "reboot_count": 7,
  "reboot_reason": "Software/System restart"
}
```

On Ethernet (ESP32): `"ssid": "Wired"` and `"ip"` reflects the Ethernet address.

##### GET /api/v2/device/time

Returns current device time.

**Response:**

```json
{
  "epoch": 1712345678,
  "iso": "2026-04-06T12:34:38+02:00",
  "ntp_synced": true,
  "ntp_last_sync": 1712344678
}
```

##### GET /api/v2/device/crashlog

Returns the most recent crash log summary from LittleFS. Returns empty object if no crash log exists. No authentication required.

---

#### OpenTherm Gateway

##### GET /api/v2/otgw/status

Returns current OpenTherm state: all message ID values received from the boiler and thermostat. This is the same data available over MQTT but in a single request-response snapshot.

**Response:**

```json
{
  "Tr": 20.5,
  "Tboiler": 62.3,
  "TSet": 55.0,
  "RelModLevel": 45.2,
  "CHPressure": 1.4,
  "flamestatus": true,
  "chmodus": true,
  "dhwmode": false,
  "faultindicator": false
}
```

The exact fields depend on which OpenTherm message IDs your boiler and thermostat exchange. Fields are absent when the corresponding message ID has not yet been received.

##### GET /api/v2/otgw/messages/{id}

Returns the current value for a specific OpenTherm message ID.

**Parameters:** `{id}` is the numeric message ID (0-127).

**Response:**

```json
{
  "id": 25,
  "label": "Tboiler",
  "value": "62.31",
  "type": "f8.8",
  "last_update": 1712345600
}
```

Alias: `GET /api/v2/otgw/id/{id}`

##### GET /api/v2/otgw/label/{label}

Returns the current value for a specific OpenTherm label string.

**Parameters:** `{label}` is the label name (case-sensitive), e.g., `Tboiler`, `Tr`, `RelModLevel`.

##### GET /api/v2/otgw/otmonitor

Returns all current OpenTherm values in a flat key-value format compatible with Telegraf and OTmonitor. Alias: `GET /api/v2/otgw/telegraf`.

##### POST /api/v2/otgw/command

Send a raw command to the OTGW PIC (or OTDirect on ESP32). Authentication required.

**Request body (JSON):**

```json
{
  "command": "TT=21.50"
}
```

Alternatively, the command string can be sent as a plain text body without JSON wrapping.

**Response:**

```json
{
  "status": "queued",
  "command": "TT=21.50"
}
```

Common PIC command codes:

| Command | Description |
|---------|-------------|
| `TT=xx.xx` | Temporary room setpoint override |
| `TC=xx.xx` | Constant room setpoint override |
| `CS=xx.xx` | Control setpoint (CH flow temperature) |
| `SH=xx` | Max CH water setpoint |
| `HW=1` / `HW=0` | Hot water on / off |
| `GW=1` / `GW=0` | Gateway mode / monitor mode |
| `PS=1` / `PS=0` | Print Summary mode on / off |
| `PR=A` | Query PIC banner string |

Commands are validated and queued. Responses are processed asynchronously and published to MQTT. The REST endpoint returns `"queued"` immediately without waiting for the PIC response.

##### POST /api/v2/otgw/commands

Alias for `/api/v2/otgw/command`. Preferred spelling per ADR-035.

##### POST /api/v2/otgw/discovery

Force a full Home Assistant MQTT auto-discovery republish. Useful after a HA restart when discovery messages were missed. Authentication required.

**Request:** No body required.

**Response:**

```json
{
  "status": "started"
}
```

Discovery runs asynchronously in the background. All ~200 entity configurations are republished to the `homeassistant/` MQTT prefix. This is equivalent to clicking "Rediscover" in the web UI.

Alias: `POST /api/v2/otgw/autoconfigure`

---

#### Sensors

##### GET /api/v2/sensors

Returns all Dallas DS18B20 sensor addresses and their labels. No authentication required.

**Response:**

```json
{
  "sensors": [
    {
      "address": "28FF64D1841703F1",
      "label": "Attic",
      "temperature": 18.3,
      "last_seen": 1712345650
    },
    {
      "address": "28FF9A3B71120502",
      "label": "",
      "temperature": 22.1,
      "last_seen": 1712345651
    }
  ]
}
```

##### GET /api/v2/sensors/{address}

Returns data for a single sensor by its 1-Wire address.

**Parameters:** `{address}` is the 16-character hex address string.

##### GET /api/v2/sensors/labels

Returns all sensor address-to-label mappings.

##### POST /api/v2/sensors/labels

Update sensor labels. Authentication required.

**Request body:**

```json
{
  "28FF64D1841703F1": "Attic",
  "28FF9A3B71120502": "Living Room"
}
```

Labels are persisted to `/dallas_labels.ini` on LittleFS and republished to MQTT.

---

#### Flash and Firmware

##### GET /api/v2/flash/status

Returns ESP flash memory usage and OTA partition status.

**Response:**

```json
{
  "sketch_size": 524288,
  "sketch_md5": "abc123...",
  "free_sketch_space": 1048576,
  "ota_running_partition": "app0",
  "ota_update_partition": "app1"
}
```

##### GET /api/v2/firmware/files

Returns available firmware files on GitHub (fetches the releases list). Used by the web UI OTA update page.

---

#### PIC Firmware

##### GET /api/v2/pic/flash-status

Returns PIC firmware flash operation status.

##### GET /api/v2/pic/update-check

Checks whether a newer PIC firmware is available.

##### GET /api/v2/pic/settings

Returns current PIC settings (queried via PR= commands). Only available when PIC is detected (`isPICEnabled()`).

---

#### SAT (Smart Autotune Thermostat)

The SAT endpoints require authentication for all mutation operations.

##### GET /api/v2/sat

Returns full SAT status. Add `?detail=full` for extended diagnostics including pressure metrics, cycle diagnostics, PID error statistics, and sync health.

**Response (abbreviated):**

```json
{
  "enabled": true,
  "active": true,
  "mode": "continuous",
  "setpoint": 43.1,
  "target": 21.0,
  "room_temp": 20.5,
  "outside_temp": 8.0,
  "heating_curve": 42.3,
  "pid_output": 43.1,
  "pid_p": 0.82,
  "pid_i": 0.03,
  "pid_d": -0.04,
  "boiler_status": 3,
  "flame_status": "healthy",
  "cycle_class": "good",
  "safety_tripped": false,
  "pressure": 1.5,
  "pressure_alarm": false
}
```

##### POST /api/v2/sat/target

Set the target room temperature.

**Request body:** Plain numeric string or JSON `{"value": "21.0"}`.

```
21.5
```

Valid range: 5.0 to 30.0 °C.

##### POST /api/v2/sat/externaltemp

Push an indoor temperature reading to the SAT PID controller. Used when the room temperature sensor is an external source (Node-RED, HA automation, BLE sensor).

**Request body:** Plain numeric string, e.g., `"20.8"`.

##### POST /api/v2/sat/externaloutdoor

Push an outdoor temperature reading. Overrides the OT MsgID 27 value and the Open-Meteo weather source.

**Request body:** Plain numeric string, e.g., `"7.5"`.

##### POST /api/v2/sat/humidity

Push indoor humidity reading (0-100%). Used for Summer Simmer Index calculation.

##### POST /api/v2/sat/preset

Apply a named comfort preset.

**Request body:** One of: `comfort`, `eco`, `away`, `sleep`, `activity`, `home`.

##### POST /api/v2/sat/window

Set window-open state. When open, SAT suppresses heating.

**Request body:** `"1"` (open) or `"0"` (closed).

##### POST /api/v2/sat/flush

Flush SAT short-lived data: resets the PID integral and cycle window. Useful after system disturbances (burner technician visit, manual boiler test).

##### POST /api/v2/sat/reset_integral

Reset only the PID integral accumulator.

##### POST /api/v2/sat/settings/{name}

Update a named SAT setting. Authentication required.

**Parameters:** `{name}` is the setting name, e.g., `heating_curve_coeff`, `boiler_capacity`, `deadband`.

**Request body:** New value as plain string.

---

#### OTDirect (ESP32 / OTGW32 only)

These endpoints are only available on OTGW32 hardware (`HAS_DIRECT_OT`).

##### GET /api/v2/otdirect/status

Returns OTDirect operating mode, schedule counts, thermostat connection status, and bypass relay state.

##### POST /api/v2/otdirect/mode

Set OTDirect operating mode.

**Query parameter:** `mode=gateway|monitor|bypass|master|loopback`

| Mode | Description |
|------|-------------|
| `gateway` | Full gateway: scheduler + thermostat forwarding + overrides (default) |
| `monitor` | Transparent pass-through: all frames forwarded unmodified |
| `bypass` | Thermostat wired directly to boiler via relay; OTDirect inactive |
| `master` | Standalone OT master: scheduler only, no thermostat expected |
| `loopback` | Internal test: simulated boiler responses, no hardware needed |

##### GET /api/v2/otdirect/overrides

Returns all active stored response overrides.

##### POST /api/v2/otdirect/overrides

Manage per-message-ID overrides.

**Query parameters:**
- `action=sr&msgid=X&value=HHHH` — set stored response for message ID X to 4-hex-digit value
- `action=cr&msgid=X` — clear stored response for message ID X
- `action=rm&msgid=X&value=HHHH` — set response modifier (mask applied to boiler response)
- `action=cm&msgid=X` — clear response modifier
- `action=ui&msgid=X` — mark message ID as unknown (gateway returns UNKNOWN_DATA_ID)
- `action=ki&msgid=X` — mark message ID as known again

---

#### Webhook

##### POST /api/v2/webhook/test

Trigger a test webhook delivery. Sends the configured webhook URL an HTTP POST with a test payload. No authentication required.

**Query parameter:** `state=on|off|1|0` — payload state to simulate.

---

#### Simulation

##### GET /api/v2/simulate

Returns current simulation mode status.

##### POST /api/v2/simulate/start

Enable OpenTherm message simulation mode. In this mode, fake OT frames are injected into the processing pipeline without hardware, allowing full MQTT and HA integration testing on a desk.

##### POST /api/v2/simulate/stop

Disable simulation mode.

---

#### Filesystem

##### GET /api/v2/filesystem/files

Returns a listing of all files in LittleFS with sizes.

##### GET /api/v2/filesystem/hash-check

Returns the filesystem content hash (used for cache-busting ETag on `index.html`).

---

### WebSocket API

The firmware runs a WebSocket server that streams live OpenTherm log frames to connected browsers. This is the data source for the live OT log viewer and the real-time temperature graphs in the web UI.

#### Connection

```
ws://{device-ip}:81/
```

The WebSocket server listens on **port 81** (separate from the HTTP server on port 80). No authentication is required. Maximum 3 simultaneous connections.

From JavaScript:

```javascript
const ws = new WebSocket("ws://otgw.local:81/");
ws.onopen  = () => console.log("connected");
ws.onclose = () => setTimeout(reconnect, 3000);  // auto-reconnect
ws.onmessage = (event) => handleFrame(event.data);
```

#### Message Format

Each WebSocket message is a JSON object on a single line. The `type` field discriminates the message kind.

**OpenTherm log frame:**

```json
{
  "type": "otlog",
  "frame": "B4001FF04",
  "msgid": 0,
  "msgtype": "READ_ACK",
  "source": "boiler",
  "decoded": {
    "flamestatus": true,
    "chmodus": true,
    "dhwmode": false,
    "faultindicator": false
  },
  "timestamp": 1712345678
}
```

| Field | Description |
|-------|-------------|
| `type` | Always `"otlog"` for OT frames |
| `frame` | Raw 9-character OT frame string as received from PIC |
| `msgid` | OpenTherm message ID (0-127) |
| `msgtype` | Frame type: `READ_DATA`, `WRITE_DATA`, `READ_ACK`, `WRITE_ACK`, `DATA_INVALID`, `UNKNOWN_DATA_ID` |
| `source` | Frame origin: `thermostat`, `boiler`, `gateway`, `answer` |
| `decoded` | Key-value pairs of decoded fields for this message ID (structure varies by ID) |
| `timestamp` | Unix timestamp at frame receipt |

**Keepalive ping:**

The server sends a ping frame every 15 seconds. The WebSocket library handles ping/pong automatically. If two consecutive pings go unanswered, the server closes the connection.

**Flash progress event:**

During OTA firmware updates, progress events are broadcast:

```json
{
  "type": "flash",
  "progress": 42,
  "total": 100,
  "phase": "writing"
}
```

**SAT status event:**

When SAT is active, state change events are broadcast on significant transitions:

```json
{
  "type": "sat",
  "event": "flame_on",
  "setpoint": 43.1,
  "mode": "continuous"
}
```

#### Reconnection Behavior

The browser SPA implements automatic reconnection with a 3-second delay after disconnection. If the device reboots, the browser detects the close event and reconnects when the device comes back online. There is no message history; the stream is live-only.

#### Heap Backpressure

When free heap falls below the warning threshold, the server stops sending WebSocket messages (`canSendWebSocket()` returns false). This protects against heap exhaustion under heavy MQTT + HTTP + WebSocket concurrent load. Messages dropped during this period are lost; the stream resumes when heap recovers.

---

### MQTT API

The MQTT interface is the primary integration method for Home Assistant and other home automation platforms.

#### Broker Configuration

Configure in Settings (web UI or REST API):

| Setting | Default | Description |
|---------|---------|-------------|
| `mqttbroker` | (empty) | Broker hostname or IP |
| `mqttport` | 1883 | Broker TCP port |
| `mqttuser` | (empty) | Username (optional) |
| `mqttpassword` | (empty) | Password (optional) |
| `mqttenable` | false | Enable MQTT client |
| `mqtttoptopic` | `OTGW` | Top-level topic namespace |
| `mqttuniqueid` | `otgw-{MAC}` | Unique device ID in topics |

#### Topic Namespace

All topics follow this structure:

```
{TopTopic}/value/{UniqueId}/{subtopic}      ← published by firmware
{TopTopic}/set/{UniqueId}/{command}         ← subscribed by firmware
```

Default example (with `TopTopic=OTGW`, `UniqueId=otgw-AABBCCDDEEFF`):

```
OTGW/value/otgw-AABBCCDDEEFF/boilertemperature    ← "62.31"
OTGW/set/otgw-AABBCCDDEEFF/setpoint               ← "21.5"
```

#### Connection Lifecycle

On connect:
1. Publishes `"online"` to `{TopTopic}/value/{UniqueId}` (retained). This is the birth message.
2. Configures Last Will to publish `"offline"` to the same topic on disconnect (retained).
3. Subscribes to `{TopTopic}/set/{UniqueId}/#` for incoming commands.
4. Subscribes to `homeassistant/status` for Home Assistant lifecycle detection.
5. Clears discovery state and triggers JIT (just-in-time) republishing of HA discovery payloads.
6. Publishes firmware version, PIC status, and device info.

#### Published Topics

All topics under the publish namespace are retained by default unless noted otherwise.

##### Firmware and Device Information

Published at startup, on reconnect, and every 5 minutes.

| Topic suffix | Value | Description |
|-------------|-------|-------------|
| `otgw-firmware/version` | `"2.0.0"` | Firmware version string |
| `otgw-firmware/reboot_count` | `"7"` | Total reboots since first flash |
| `otgw-firmware/reboot_reason` | `"Software/System restart"` | Last reboot cause |
| `otgw-firmware/uptime` | `"86400"` | Uptime in seconds (not retained) |

##### PIC Gateway Status

Published at startup, on reconnect, and every 5 minutes.

| Topic suffix | Value | Description |
|-------------|-------|-------------|
| `otgw-pic/version` | `"6.6"` | PIC firmware version |
| `otgw-pic/picavailable` | `"ON"` / `"OFF"` | PIC detected and responding |
| `otgw-pic/gateway_mode` | `"ON"` / `"OFF"` | Gateway mode active |
| `otgw-pic/boiler_connected` | `"ON"` / `"OFF"` | Boiler OT traffic seen |
| `otgw-pic/thermostat_connected` | `"ON"` / `"OFF"` | Thermostat OT traffic seen |
| `otgw-pic/otgw_connected` | `"ON"` / `"OFF"` | Serial link to PIC alive |

##### OpenTherm Status Flags (Message ID 0)

Published when flag values change.

| Topic suffix | Value | Description |
|-------------|-------|-------------|
| `chenable` | `"ON"` / `"OFF"` | Central heating enable (thermostat to boiler) |
| `dhwenable` | `"ON"` / `"OFF"` | DHW (hot water) enable |
| `flamestatus` | `"ON"` / `"OFF"` | Burner flame active |
| `chmodus` | `"ON"` / `"OFF"` | Central heating mode active (boiler to thermostat) |
| `dhwmode` | `"ON"` / `"OFF"` | DHW mode active |
| `faultindicator` | `"ON"` / `"OFF"` | Boiler fault indicator |
| `diagnosticindicator` | `"ON"` / `"OFF"` | Diagnostic indicator |
| `status_master` | text | Full master status word as text |
| `status_slave` | text | Full slave status word as text |

##### OpenTherm Numeric Values

Published when the corresponding message ID is received from the boiler or thermostat. Values are decimal strings. All temperatures in degrees Celsius, pressure in bar, modulation in percent.

| Topic suffix | Unit | OpenTherm Message ID |
|-------------|------|---------------------|
| `controlsetpoint` | °C | 1 |
| `roomsetpoint` | °C | 16 |
| `roomtemperature` | °C | 24 |
| `boilertemperature` | °C | 25 |
| `dhwtemperature` | °C | 26 |
| `outsidetemperature` | °C | 27 |
| `returnwatertemperature` | °C | 28 |
| `dhwsetpoint` | °C | 56 |
| `maxchwatersetpoint` | °C | 57 |
| `chwaterpressure` | bar | 18 |
| `relmodlvl` | % | 17 |
| `maxrelmodlvl` | % | 14 |
| `dhw_flowrate` | l/min | 13 |
| `exhaust_temperature` | °C | 33 |
| `boiler_fan_speed` | rpm | 35 |
| `electrical_current_burner_flame` | µA | 36 |

The exact set of published topics depends on which message IDs your boiler and thermostat exchange. Any message ID with a defined label generates a corresponding topic.

##### Source-Separated Topics (Optional)

When `mqttseparatesources` is enabled:

```
{TopTopic}/value/{UniqueId}/{label}/thermostat   ← thermostat-side value (T-prefix frame)
{TopTopic}/value/{UniqueId}/{label}/boiler       ← boiler-side value (B-prefix frame)
{TopTopic}/value/{UniqueId}/{label}/gateway      ← gateway-injected value (A-prefix frame)
```

This allows Home Assistant automations to distinguish between what the thermostat requested and what the boiler confirmed.

##### S0 Pulse Counter

Published when S0 counting is enabled.

| Topic suffix | Unit | Description |
|-------------|------|-------------|
| `s0pulsecount` | pulses | Pulse count in current interval |
| `s0pulsecounttot` | pulses | Total pulse count since boot |
| `s0powerkw` | kW | Calculated instantaneous power |

##### Dallas Temperature Sensors

Published when sensors are enabled. The topic suffix is the 1-Wire address (e.g., `28FF64D1841703F1`) or the assigned label if `gpiosensorslegacyformat` is off.

| Topic suffix | Unit | Description |
|-------------|------|-------------|
| `{address}` | °C | Temperature from DS18B20 sensor |

##### SAT Topics

Published every SAT control loop interval (default 30 seconds) when SAT is enabled.

| Topic suffix | Retained | Description |
|-------------|---------|-------------|
| `sat/mode` | yes | `"off"`, `"continuous"`, `"pwm"` |
| `sat/active` | yes | `"true"` when SAT is actively controlling |
| `sat/setpoint` | yes | Final flow temperature setpoint sent to boiler (°C) |
| `sat/target` | yes | Target room temperature (°C) |
| `sat/heating_curve` | yes | Heating curve calculated value (°C) |
| `sat/pid_output` | yes | PID controller output = curve + P + I + D |
| `sat/error` | no | PID error = target - room temperature (°C) |
| `sat/pid_p` | no | Proportional term |
| `sat/pid_i` | no | Integral term |
| `sat/pid_d` | no | Derivative term |
| `sat/room_temp` | no | Room temperature used by PID (°C) |
| `sat/outside_temp` | no | Outside temperature used by heating curve (°C) |
| `sat/boiler_status` | no | Boiler state code (0=Off ... 14=Cooling) |
| `sat/flame_status` | no | `"healthy"`, `"stuck_on"`, `"short_cycling"`, etc. |
| `sat/cycle_class` | no | `"good"`, `"overshoot"`, `"underheat"`, `"short"` |
| `sat/safety_tripped` | no | `"true"` when safety shutdown is active |
| `sat/pressure` | no | Smoothed system water pressure (bar) |
| `sat/pressure_alarm` | no | `"true"` when pressure alarm is active |
| `sat/power` | no | Estimated boiler power (kW) |
| `sat/energy_total` | yes | Cumulative energy usage (kWh, for HA energy dashboard) |
| `sat/curve_recommendation` | no | `"increase"`, `"decrease"`, `"hold"`, `"insufficient"` |
| `sat/preset_comfort` | yes | Comfort preset temperature (°C) |
| `sat/preset_eco` | yes | Eco preset temperature (°C) |
| `sat/preset_away` | yes | Away preset temperature (°C) |
| `sat/preset_sleep` | yes | Sleep preset temperature (°C) |
| `sat/thermal_coeff` | yes | Learned thermal drop coefficient |
| `sat/window_open` | no | Window-open detection state |
| `sat/solar_gain` | no | Solar gain compensation active |
| `sat/summer_active` | no | Summer simmer suppression active |

Additional settings echo topics, BLE sensor topics, and multi-zone topics are documented in `backlog/docs/doc-1 - sat-mqtt-topics.md`.

##### Raw OpenTherm Message (Optional)

When `mqttotmessage` is enabled:

| Topic suffix | Value | Description |
|-------------|-------|-------------|
| `otmessage` | `"T80000200"` | Raw OpenTherm frame string as received from PIC |

#### Subscribed Topics (Commands)

The firmware subscribes to `{TopTopic}/set/{UniqueId}/#`.

##### Direct Control Commands

Publish a plain text payload to these topics:

| Topic suffix | Payload | OTGW command | Description |
|-------------|---------|-------------|-------------|
| `setpoint` | `"21.5"` | `TT=21.50` | Temporary room temperature setpoint |
| `constant` | `"21.5"` | `TC=21.50` | Constant room temperature setpoint (persists over thermostat cycles) |
| `outside` | `"8.0"` | `OT=8.0` | Outside temperature override |
| `hotwater` | `"1"` / `"0"` / `"P"` | `HW=1` / `HW=0` / `HW=P` | Hot water on / off / one-time push |
| `gatewaymode` | `"1"` / `"0"` | `GW=1` / `GW=0` | Gateway mode / monitor mode |
| `maxchsetpt` | `"80"` | `SH=80` | Max CH water setpoint (°C) |
| `maxdhwsetpt` | `"60"` | `SW=60` | Max DHW setpoint (°C) |
| `maxmodulation` | `"100"` | `MM=100` | Max relative modulation level (%) |
| `ctrlsetpt` | `"55.0"` | `CS=55.0` | Direct control setpoint (bypasses thermostat) |
| `chenable` | `"1"` / `"0"` | `CH=1` / `CH=0` | Central heating enable bit |
| `summermode` | `"1"` / `"0"` | `SM=1` / `SM=0` | Summer mode (persisted to flash) |
| `command` | `"TT=21.50"` | (raw) | Raw OTGW command passthrough |

##### SAT Command Topics

| Topic suffix | Payload | Description |
|-------------|---------|-------------|
| `sat/target` | `"21.0"` | Set target room temperature |
| `sat/enable` | `"on"` / `"off"` | Enable / disable SAT |
| `sat/mode` | `"continuous"` / `"pwm"` / `"off"` | Set control mode |
| `sat/preset` | `"comfort"` / `"eco"` / `"away"` / `"sleep"` | Apply comfort preset |
| `sat/externaltemp` | `"20.8"` | Push external room temperature |
| `sat/externaloutdoor` | `"7.5"` | Push external outdoor temperature |
| `sat/humidity` | `"58"` | Push indoor humidity (%) |
| `sat/window` | `"1"` / `"0"` | Set window open / closed state |

#### Home Assistant Auto-Discovery

When `mqtthadiscovery` is enabled, the firmware publishes MQTT discovery payloads to the `homeassistant/` prefix (configurable via `mqtthaprefix`). Home Assistant processes these payloads and creates entities automatically.

Discovery is triggered:
- On MQTT connect (just-in-time, per message ID as data is received).
- When `POST /api/v2/otgw/discovery` is called.
- When Home Assistant sends `"online"` to `homeassistant/status`.

Discovery payloads include device metadata, entity names, unit of measurement, device class, and state topics. The firmware creates approximately 200+ entities covering all sensors, binary sensors, and a climate entity for SAT.

---

### Telnet Debug Console

The firmware streams diagnostic output to a Telnet server on port 23.

#### Connecting

```bash
telnet otgw.local 23
# or by IP
telnet 192.168.1.50 23
```

On Windows, use PuTTY (Telnet mode, port 23) or install the optional Windows Telnet client.

No authentication is required. The debug output is read-only; the Telnet connection does not accept commands.

#### Output Format

The stream is plain ASCII, one line per event. Output lines include:

- OpenTherm message decoding: message ID, type, values.
- MQTT state machine transitions: connecting, connected, disconnected, retrying.
- Settings load/save events.
- Sensor readings (Dallas, S0) on each measurement cycle.
- SAT control loop output: setpoint calculation, PID terms, boiler state.
- Heap statistics: free heap, heap health level.
- Network events: WiFi connect/disconnect, IP address.
- Error messages and warnings.

Example output:

```
[OTGW] handleOTGW: B4001FF04 msgid=0 (Status flags)
[OTGW]   flamestatus=ON chmodus=ON
[MQTT] sendMQTTData: OTGW/value/otgw-AABBCC/flamestatus = ON
[SAT]  Control loop: Tr=20.5 Tboiler=62.3 setpoint=43.1 error=0.50
[HEAP] Free: 19234 bytes (healthy)
```

The verbosity of each category is controlled by boolean flags in `state.debug`:

| Flag | Category | Default |
|------|---------|---------|
| `bOTmsg` | OpenTherm message trace | true |
| `bMQTT` | MQTT publish/receive trace | false |
| `bRestAPI` | REST API request trace | false |
| `bSensors` | Dallas sensor scan trace | false |
| `bSAT` | SAT control loop trace | true |
| `bOTDirect` | OTDirect frame handling | true |

Debug flags can be toggled at runtime via the REST API settings endpoint or via web UI settings. High-verbosity flags (`bMQTT`, `bRestAPI`) should be disabled in normal operation to avoid flooding the Telnet stream and consuming heap.

#### TCP Serial Bridge (Port 25238)

A separate TCP socket on port 25238 exposes the raw OTGW serial stream. This is for tools like OTmonitor that need raw protocol access:

```bash
nc otgw.local 25238
# or
telnet otgw.local 25238
```

The output is the unprocessed ASCII stream from the PIC: lines like `T80000200`, `B4001FF04`, `R4001FF04`. You can also send raw PIC commands by typing them followed by a newline. This port is separate from the debug Telnet console.

---

### Port Reference

| Port | Protocol | Service |
|------|----------|---------|
| 80 | TCP (HTTP) | Web UI, REST API v2, file serving |
| 81 | TCP (WebSocket) | Live OT log stream, real-time graphs |
| 23 | TCP (Telnet) | Debug console output (read-only) |
| 25238 | TCP (raw) | TCP serial bridge to OTGW PIC |
| 1883 | TCP (MQTT) | MQTT broker connection (outbound) |
| 123 | UDP (SNTP) | NTP time synchronization (outbound) |
| 5353 | UDP (mDNS) | `otgw.local` name resolution |
| 5355 | UDP (LLMNR) | Windows name resolution |
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
| **OTDirect** | The ESP32-native direct GPIO OpenTherm bus interface. Replaces the PIC co-processor on OTGW32 boards. Implemented in `OTDirect.ino` and guarded by `#if HAS_DIRECT_OT`. |
| **PIC** | The PIC16F88 or PIC16F1847 microcontroller co-processor on the NodoShop OTGW board. Handles the OpenTherm electrical bus and communicates with the ESP over UART at 9600 baud. |
| **PROGMEM** | Program Memory. On ESP8266, string literals declared with `PROGMEM` (or wrapped in `F()` / `PSTR()`) are stored in flash memory instead of RAM, conserving the scarce ~40 KB RAM budget. |
| **PubSubClient** | The Arduino MQTT client library used by the firmware. Pinned to version 2.8.0 in `platformio.ini`. |
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
