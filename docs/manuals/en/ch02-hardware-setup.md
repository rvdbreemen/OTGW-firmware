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

