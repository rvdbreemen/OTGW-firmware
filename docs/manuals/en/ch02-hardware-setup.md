## Chapter 2: Hardware Setup and Installation

### Supported Hardware

#### ESP8266 OTGW (NodoShop)

The NodoShop OpenTherm Gateway with an ESP8266 module is the primary and longest-supported hardware target. Two board revisions exist:

| NodoShop OTGW version | ESP8266 module |
|---|---|
| 1.x through 2.0 | NodeMCU ESP8266 |
| 2.3 and later | Wemos D1 mini ESP8266 |

Both revisions use the same firmware binary. The NodeMCU and Wemos D1 mini modules are pin-compatible from the firmware's perspective.

Characteristics:

- ESP8266 at 160 MHz (configured in `platformio.ini`), approximately 80 KB usable RAM
- PIC16F88 or PIC16F1847 co-processor for the OpenTherm bus
- 4 MB SPI flash, `eagle.flash.4m2m.ld` linker layout: 1 MB firmware, 2 MB LittleFS, 1 MB reserved for OTA
- Micro-USB connector for power and flashing
- I2C pins for optional OLED display and external watchdog
- 1-Wire GPIO for Dallas DS18B20 temperature sensors

The PlatformIO environment is `esp8266` (board `d1_mini`, platform `espressif8266@4.2.1`, Arduino Core 3.1.2). The same binary runs on both NodeMCU and D1 mini variants.

#### ESP32-S3 OTGW32 (NodoShop)

The OTGW32 is a separate board from NodoShop built around an ESP32-S3 module. It includes onboard OpenTherm bus circuitry that allows the ESP32-S3 to communicate directly on the OpenTherm bus without a PIC co-processor. The PIC firmware chip is optional: when present, it provides the classic serial bridge path; when absent, OTDirect GPIO mode takes over.

Characteristics:

- ESP32-S3 at 240 MHz, approximately 300 KB usable RAM
- No PIC co-processor: OpenTherm is handled natively via OTDirect on GPIO
- OTDirect master/slave GPIO pins for direct OpenTherm bus communication
- Step-up converter (18V) for OT bus power, controlled via GPIO
- Bypass relay output (GPIO 47)
- W5500 SPI Ethernet header for wired networking
- I2C header for SSD1306 OLED display
- Bluetooth LE radio for BLE temperature sensors (Xiaomi LYWSD03MMC)
- 4 MB SPI flash with a single-app partition layout (no dual-OTA in 4 MB, see Partition Table below)
- USB-C connector
- Two buttons: boot button (GPIO 0) and config button (GPIO 9)
- Three LEDs: OT red (GPIO 2), status (GPIO 8), and OT green (GPIO 48)

#### ESP32 (generic, experimental)

Standard (non-S3) ESP32 modules are selected by defining `BOARD_NODOSHOP_ESP32` and compiling with the ESP32 toolchain, but the pin map in `boards.h` targets the OTGW32 (ESP32-S3) board. On a plain ESP32, the OTDirect GPIO pins and the W5500 SPI assignments may not match the module breakout. Treat this as a developer target.

#### Other Boards

Other ESP8266 or ESP32 boards may compile and run the firmware but are not tested or supported. GPIO pin assignments for Dallas sensors, OLED, and the S0 counter may need adjustment in the settings.

### Hardware Capability Flags (boards.h)

The `boards.h` header defines compile-time capability flags that control which code is included in each firmware binary. Understanding these flags helps when reading build logs or contributing to the firmware.

| Flag | ESP8266 OTGW | ESP32-S3 OTGW32 | Meaning |
|---|---|---|---|
| `HAS_PIC` | 1 | 0 | PIC co-processor present. Includes OTGWSerial, PIC detection, PIC firmware upgrade, and I2C watchdog code. |
| `HAS_DIRECT_OT` | 0 | 1 | Direct GPIO OpenTherm bus interface present. Includes OTDirect.ino, the opentherm_library, step-up converter control, and GPIO OT pins. |
| `HAS_ETH_CAPABLE` | 0 | 1 | W5500 SPI Ethernet hardware present. Includes Ethernet.ino and the EthernetESP32 library. |

These flags are mutually exclusive between the two build targets: a single firmware binary is not possible because the GPIO pin assignments for OT-direct and I2C conflict at the electrical level. The `python build.py` command (or `pio run`) builds both binaries from the same source tree.

Application code guards platform-specific blocks with `#if HAS_PIC`, `#if HAS_DIRECT_OT`, and `#if HAS_ETH_CAPABLE`. User-facing runtime checks use the helper functions `isPICEnabled()` and `isOTDirectEnabled()`, which add a runtime detection layer on top of the compile-time flag.

### GPIO Pin Maps

The following tables list the GPIO assignments defined in `boards.h` for each platform. Application code uses the `PIN_*` constants and never references Dx aliases or raw GPIO numbers directly.

#### ESP8266 (Wemos D1 mini)

| Function | GPIO | Dx alias | Notes |
|---|---|---|---|
| I2C SCL | 5 | D1 | Watchdog + OLED |
| I2C SDA | 4 | D2 | Watchdog + OLED |
| Button | 0 | D3 | Config/reset, active LOW with pull-up |
| LED1 | 2 | D4 | Active LOW, also onboard LED |
| PIC reset | 14 | D5 | Resets the PIC co-processor |
| LED2 | 16 | D0 | Active LOW |

#### ESP32-S3 (OTGW32)

| Function | GPIO | Notes |
|---|---|---|
| Boot button | 0 | Active LOW with pull-up |
| Config button | 9 | Active LOW |
| OT red LED | 2 | Active LOW |
| Status LED | 8 | Active LOW |
| OT green LED | 48 | Active HIGH |
| I2C SCL | 17 | OLED display |
| I2C SDA | 18 | OLED display |
| OT master input | 21 | From boiler |
| OT master output | 1 | To boiler |
| OT slave input | 6 | From thermostat |
| OT slave output | 7 | To thermostat |
| Step-up enable | 10 | HIGH = enable 18V OT bus power |
| 1-Wire (Dallas) | 4 | DS18B20 sensors |
| Bypass relay | 47 | Relay output |
| SPI CS (W5500) | 14 | Ethernet chip select |
| SPI INT (W5500) | 15 | Ethernet interrupt |
| SPI RST (W5500) | 16 | Ethernet reset |
| SPI SCK | 12 | SPI clock |
| SPI MISO | 13 | SPI data in |
| SPI MOSI | 11 | SPI data out |

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

### ESP32-S3 Partition Table

The OTGW32 uses a custom partition layout for 4 MB flash. Because the full ESP32-S3 build (Wi-Fi + Ethernet + OTDirect) exceeds 2 MB, a dual-OTA layout does not fit. The 4 MB flash is laid out as follows:

| Partition | Offset | Size | Purpose |
|---|---|---|---|
| nvs | 0x9000 | 20 KB | Non-volatile storage |
| otadata | 0xE000 | 8 KB | OTA state |
| app0 | 0x10000 | 2.875 MB | Application firmware |
| spiffs | 0x2F0000 | 1 MB | LittleFS filesystem (web assets, config, PIC firmware) |
| coredump | 0x3F0000 | 64 KB | Core dump storage |

> **Note**: With only one app partition, OTA updates on 4 MB flash are not possible. To enable OTA on ESP32, an 8 MB flash module with a custom partition table is required. For routine updates, reflash via USB using `flash_esp.py`.

### Optional Hardware

#### Ethernet W5500 (OTGW32 / ESP32-S3 Only)

The W5500 SPI Ethernet module connects to the dedicated SPI header on the OTGW32 board. When a network cable is plugged in and the firmware detects a valid link, Ethernet is used automatically and Wi-Fi is switched off. Unplugging the cable causes automatic failover to Wi-Fi within approximately five seconds. No configuration change is required.

#### OLED Display (SSD1306)

A 128x64 SSD1306 I2C OLED display can be connected to both ESP8266 and ESP32 boards. The display shows four pages of information cycled by a button press:

1. System status (device name, uptime, firmware version)
2. Temperatures (room, boiler flow, boiler return, outside)
3. Network (IP address, SSID or Ethernet status, signal quality)
4. Boiler status (flame, modulation, CH and DHW setpoints)

The display turns off automatically after a configurable idle timeout. Configure the I2C address, pin assignments, and timeout in Settings under the Hardware section.

#### Dallas DS18B20 Temperature Sensors

Up to 16 DS18B20 sensors can be connected on a single 1-Wire bus to a configurable GPIO pin (default GPIO 10 / SD3 on ESP8266, GPIO 4 on ESP32-S3). Each sensor is identified by its 64-bit address and can be given a custom label via the web interface. All sensors are published to MQTT and appear in Home Assistant auto-discovery.

> **Note for users upgrading from versions before v1.0.0**: The default Dallas GPIO pin changed from GPIO 13 (D7) to GPIO 10 (SD3) in v1.0.0. If your sensors stopped working after an upgrade, check the Dallas GPIO setting in Settings and change it back to 13 if needed.

Wiring a DS18B20 sensor:

- Connect VDD to 3.3 V (or use parasitic power mode with VDD tied to GND).
- Connect GND to ground.
- Connect DATA to the configured GPIO pin.
- Add a 4.7 kOhm pull-up resistor between VDD and DATA.

#### S0 Pulse Counter

The S0 pulse counter reads electricity pulses from an energy meter that has an S0 output (a standard impulse interface common on DIN-rail energy meters). Connect the S0 output to a configurable GPIO pin. Configure the pulses-per-kWh ratio in Settings. The firmware reports cumulative kWh and instantaneous power in watts to MQTT and Home Assistant.

#### BLE Temperature Sensors (ESP32-S3 / OTGW32 Only)

Up to four Xiaomi LYWSD03MMC Bluetooth LE sensors are supported on the ESP32-S3. The firmware reads them passively using the BTHome v2 protocol (these sensors broadcast their readings as advertisements; no pairing is required). Room temperature and humidity from these sensors feed into the SAT multi-zone averaging and are published to Home Assistant as individual sensor entities.

### Flashing the Firmware

#### First-Time Flashing with flash_esp.py (Recommended)

The `flash_esp.py` script handles everything automatically: it can download the latest release from GitHub, auto-detect your board and serial port by USB VID/PID, and flash the firmware. For ESP32-S3, the script uses a merged binary that contains the bootloader, partition table, firmware, and filesystem in a single image.

Under the hood the script calls `python -m esptool` (esptool.py v5 or newer) using the v5 subcommand names: `erase-flash` and `write-flash` with hyphens. The underscored v4 forms (`erase_flash`, `write_flash`) are no longer accepted by esptool v5. The script installs esptool via `pip` if it is missing, so you normally do not need to manage this yourself.

**Prerequisites:**

- Python 3.6 or higher
- esptool v5 (installed automatically by `flash_esp.py` on first run)
- A USB cable connected between your computer and the ESP module (micro-USB for ESP8266, USB-C for ESP32-S3/OTGW32)
- The correct USB-to-UART driver installed for your board:
  - ESP8266: CP210x driver (NodeMCU) or CH340 driver (Wemos D1 mini clones)
  - ESP32-S3 OTGW32: uses the built-in ESP32-S3 USB-Serial/JTAG interface (Espressif VID 0x303A, PID 0x1001), typically no extra driver needed
  - Linux: add your user to the `dialout` group (`sudo usermod -aG dialout $USER`, then log out and back in)

**Steps:**

1. Connect the USB cable to the ESP module (not to the OTGW board, but to the ESP module's own USB port, or to the OTGW board's USB connector if it has one).

2. Open a terminal and run:

```bash
python3 flash_esp.py
```

Without additional arguments, the script enters interactive mode: it asks you to select a board (ESP8266 or ESP32), then offers to flash existing build artifacts, rebuild from source, or download the latest GitHub release. Follow the prompts.

3. For non-interactive use, specify the board and a mode:

```bash
# Download latest release and flash (ESP8266)
python3 flash_esp.py --board esp8266 --download

# Download latest release and flash (ESP32)
python3 flash_esp.py --board esp32 --download

# Build from source and flash
python3 flash_esp.py --board esp32 --build
```

4. The script auto-detects the serial port by matching USB VID/PID to known board configurations. If auto-detection fails, specify the port explicitly:

```bash
python3 flash_esp.py --port COM5          # Windows
python3 flash_esp.py --port /dev/ttyUSB0  # Linux
python3 flash_esp.py --port /dev/cu.usbserial-0001  # macOS
```

5. For a completely clean first-time install, use the `--erase` flag to wipe the flash first. Note: for ESP32-S3, the script always erases flash before writing to clear the otadata partition and prevent boot loops.

```bash
python3 flash_esp.py --board esp8266 --erase --download
```

6. After flashing completes, the device reboots automatically.

**ESP32-S3 merged binary**: The flash tool uses the `merged-full` image for ESP32-S3. A merged-full binary combines the bootloader, partition table, firmware, and filesystem into a single file flashed at address 0x0. The helper scripts erase flash first, so the result is a factory reset with fresh firmware and filesystem.

#### Flashing with PlatformIO (Build from Source)

PlatformIO is the primary build system. The repository contains a `platformio.ini` with two environments: `esp8266` (Wemos D1 mini, ESP8266) and `esp32` (OTGW32, ESP32-S3).

```bash
# Install PlatformIO (as VS Code extension or via pip)
pip install platformio

# Clone the repository
git clone https://github.com/rvdbreemen/OTGW-firmware.git
cd OTGW-firmware

# Build and flash ESP8266
pio run -e esp8266 --target upload
pio run -e esp8266 --target uploadfs

# Build and flash ESP32-S3 / OTGW32
pio run -e esp32 --target upload
pio run -e esp32 --target uploadfs
```

The `upload` target flashes the firmware binary. The `uploadfs` target flashes the LittleFS filesystem image containing web assets, the Home Assistant discovery configuration, and PIC firmware files.

Both targets require the device to be connected via USB.

Alternatively, use the `build.py` helper script. It wraps PlatformIO by default and can optionally use arduino-cli as a backend; invoke esptool v5 under the hood for any flashing it performs:

```bash
python build.py                      # Full build, both platforms, PlatformIO backend
python build.py --target esp8266     # ESP8266 only
python build.py --target esp32       # ESP32-S3 / OTGW32 only
python build.py --firmware           # Firmware only (skip filesystem)
python build.py --filesystem         # Filesystem only
python build.py --merged             # Also produce a merged binary (ESP32)
python build.py --merged --compress  # Merged and gzip-compressed
python build.py --clean              # Clean build artifacts
python build.py --distclean          # Also drop cached cores and libraries
python build.py --arduino-cli        # Use the legacy arduino-cli backend
```

Run `python build.py --help` for the full, current list of flags.

#### Manual Flashing with Existing Binaries

If you already have firmware and filesystem binary files:

```bash
python3 flash_esp.py --board esp8266 --firmware OTGW-firmware-esp8266.ino.bin --filesystem OTGW-firmware-esp8266.littlefs.bin
python3 flash_esp.py --board esp32 --firmware OTGW-firmware-esp32.ino.bin --filesystem OTGW-firmware-esp32.littlefs.bin
```

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

> **ESP32-S3 with Ethernet**: If you have a W5500 Ethernet module and a network cable connected, the device obtains an IP address via DHCP on the wired interface at boot and does not open an AP. No Wi-Fi setup is needed.

> **AP Fallback Mode (beta, ESP32-S3 only)**: If the configured Wi-Fi network becomes unavailable after initial setup, the device opens a fallback AP named `OTGW-<MAC>` with password `otgw123` and IP `192.168.4.1`, keeping the web UI accessible.

### Filesystem Upload (LittleFS)

The web assets (HTML, CSS, JavaScript, OLED icons, PIC firmware files, and the Home Assistant discovery configuration) are stored in a LittleFS partition on the flash. This partition must be flashed alongside the firmware binary.

When using `flash_esp.py`, the filesystem is flashed automatically (either as part of a merged binary on ESP32-S3 or as a separate image on ESP8266). When using PlatformIO, run `pio run -e <env> --target uploadfs` after the firmware upload.

> **After a firmware update via OTA**: The web assets in LittleFS are updated separately. Use the web UI's Update page to update both firmware and filesystem, or upload a new filesystem image manually via the File Manager.

### Upgrading from a Previous Version

#### OTA Upgrade (Recommended for ESP8266)

From v1.3.0 onwards, the web interface provides a one-click OTA updater:

1. Open the web interface and navigate to the **Update** tab (or the update icon in the header).
2. The page fetches the list of available GitHub releases and shows Installed, Update, and Rollback badges next to each version.
3. Click **Update** next to the version you want to install.
4. The firmware downloads the new binary from GitHub, flashes it to the inactive OTA partition, and reboots.
5. The web interface polls the device until it responds, then confirms success.
6. Update the filesystem image using the same page to ensure web assets match the new firmware version.

> **ESP32-S3 on 4 MB flash**: OTA is not available with the standard 4 MB partition layout (only one app partition fits). Update via USB using `flash_esp.py` instead.

#### Manual OTA Upload

If you have a firmware binary file and want to upload it directly:

1. Navigate to the **Update** page in the web interface.
2. Use the file upload area to select your firmware `.bin` file.
3. The device flashes the binary and reboots.

#### Notes When Upgrading from v1.x

- No settings migration is required. Settings files from v1.3.x load without modification in v2.0.0.
- MQTT topics and the REST API are unchanged from v1.x.
- If you are upgrading from before v1.2.0, be aware that REST API v0 and v1 endpoints were removed in v1.2.0. Only `/api/v2/` endpoints are available. See [docs/BREAKING_CHANGES.md](../docs/BREAKING_CHANGES.md).
- After upgrade, delete any orphaned Home Assistant entities and allow MQTT auto-discovery to recreate them to pick up any new or renamed entities.

#### Important Warnings

- **Never flash PIC firmware over Wi-Fi using OTmonitor.** This can brick the PIC microcontroller. Use the built-in PIC firmware upgrade feature in the web UI instead.
- **When OTDirect is active (ESP32-S3 only)**, the PIC UART is bypassed. Do not attempt PIC firmware upgrades while OTDirect is the active OpenTherm path.

---
