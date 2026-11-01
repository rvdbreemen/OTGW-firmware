## Chapter 2: Hardware Setup and Installation

### Supported Hardware

#### OTGW Classic with LOLIN S3 Mini or S3 Mini Pro (NodoShop)

The 2.x firmware runs on the NodoShop OTGW Classic with a LOLIN S3 Mini or S3 Mini Pro (ESP32-S3) in the Wemos D1 mini socket. The S3 Mini fits the Wemos D1 mini socket: each Classic signal lands on the S3 GPIO at the matching hole. The PIC stays in place. You flash one image, `esp32-combo`, which detects the PIC at boot and runs in PIC gateway mode.

Characteristics:

- PIC16F88 or PIC16F1847 co-processor for the OpenTherm bus
- ESP32-S3 (LOLIN S3 Mini or S3 Mini Pro) in the Wemos D1 mini socket
- 4 MB SPI flash with a safeboot partition layout: one firmware slot plus an updater for firmware updates over WiFi (see Partition Table below)
- I2C header for the external watchdog and an optional SSD1306 OLED display
- 1-Wire GPIO for Dallas DS18B20 temperature sensors
- Bluetooth LE radio for BLE temperature sensors
- The on-board RGB pixel of the LOLIN module shows the WiFi status
- No Ethernet and no OT-Direct hardware

The Wemos D1 mini (ESP8266) runs the 1.x firmware. Going back to 1.x means putting the Wemos D1 mini back: a 1.x zip does not run on an S3.

#### Other Boards

Other ESP32-S3 boards may compile and run the firmware but are not tested or supported. GPIO pin assignments for Dallas sensors, OLED, and the S0 counter may need adjustment in the settings.

### Hardware Capability Flags (boards.h)

The `boards.h` header defines compile-time capability flags that control which code is included in the firmware binary. Understanding these flags helps when reading build logs or contributing to the firmware.

| Flag | Compiled into esp32-combo | Meaning |
|---|---|---|
| `HAS_PIC` | 1 | PIC co-processor support. Includes OTGWSerial, PIC detection, PIC firmware upgrade, and I2C watchdog code. |
| `HAS_DIRECT_OT` | 1 | OT-Direct mode, used only on hardware without a PIC. Includes OTDirect.ino and the opentherm_library. |
| `HAS_ETH_CAPABLE` | 1 | Ethernet support, used only with Ethernet hardware. Includes Ethernet.ino and the EthernetESP32 library. |

The `esp32-combo` image compiles in both the PIC path and the OT-Direct path and picks one at boot (`HAS_RUNTIME_HW_DETECT`). On the OTGW Classic it detects the PIC and runs in PIC gateway mode. `build.bat` (Windows) or `./build.sh` (Linux/macOS) builds this image.

Application code guards platform-specific blocks with `#if HAS_PIC`, `#if HAS_DIRECT_OT`, and `#if HAS_ETH_CAPABLE`. User-facing runtime checks use the helper functions `isPICEnabled()` and `isOTDirectEnabled()`, which add a runtime detection layer on top of the compile-time flag.

### GPIO Pin Maps

The following table lists the GPIO assignments defined in `boards.h` for the OTGW Classic. The S3 Mini and S3 Mini Pro sit in the Wemos D1 mini socket, so each signal is listed by its socket hole. The firmware detects at boot whether a Mini or a Mini Pro is fitted. Application code uses the `PIN_*` constants and never references hole labels or raw GPIO numbers directly.

#### OTGW Classic with LOLIN S3 Mini / S3 Mini Pro

| Function | Socket hole | S3 Mini GPIO | S3 Mini Pro GPIO | Notes |
|---|---|---|---|---|
| PIC UART (ESP TX to PIC RX) | TX | 43 | 43 | Serial link to the PIC |
| PIC UART (ESP RX from PIC TX) | RX | 44 | 44 | Serial link to the PIC |
| I2C SCL | D1 | 36 | 11 | Watchdog + OLED |
| I2C SDA | D2 | 35 | 12 | Watchdog + OLED |
| Button | D3 | 18 | 13 | Config/reset, active LOW |
| LED1 | D4 | 16 | 14 | Active LOW |
| PIC reset | D5 | 12 | 40 | Resets the PIC co-processor |
| LED2 | D0 | 4 | 41 | Active LOW |
| RGB status pixel | (on the module) | 47 | 8 | WiFi status |

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

The ESP32-S3 firmware uses a custom partition layout for 4 MB flash (ADR-191). The firmware does not fit twice, so instead of two firmware slots there is one firmware slot plus a small updater (safeboot) that installs a new firmware over WiFi:

| Partition | Offset | Size | Purpose |
|---|---|---|---|
| nvs | 0x9000 | 20 KB | Non-volatile storage (WiFi credentials) |
| otadata | 0xE000 | 8 KB | OTA state |
| safeboot | 0x10000 | 960 KB | Updater for firmware updates over WiFi |
| app0 | 0x100000 | 2240 KB | Application firmware |
| spiffs | 0x330000 | 768 KB | LittleFS filesystem (web UI, settings, PIC firmware) |
| coredump | 0x3F0000 | 64 KB | Core dump storage |

> **Note**: Units on the earlier layout (an early 2.0.0 alpha) need one USB migration (`flash_otgw --migrate --host <ip>`); after that, firmware updates work over WiFi. See [WiFi firmware updates](../../guides/WIFI_FIRMWARE_UPDATE.md).

### Optional Hardware

#### Ethernet

The OTGW Classic has no Ethernet. The Ethernet settings in the web UI apply only to Ethernet hardware.

#### OLED Display (SSD1306)

A 128x64 SSD1306 I2C OLED display can be connected to the I2C header of the OTGW Classic. The display shows four pages of information cycled by a button press:

1. System status (device name, uptime, firmware version)
2. Temperatures (room, boiler flow, boiler return, outside)
3. Network (IP address, SSID or Ethernet status, signal quality)
4. Boiler status (flame, modulation, CH and DHW setpoints)

The display turns off automatically after a configurable idle timeout. Configure the I2C address, pin assignments, and timeout in Settings under the Hardware section.

#### Dallas DS18B20 Temperature Sensors

Up to 16 DS18B20 sensors can be connected on a single 1-Wire bus to a configurable GPIO pin (setting range 0 to 16, default GPIO 10). Each sensor is identified by its 64-bit address and can be given a custom label via the web interface. All sensors are published to MQTT and appear in Home Assistant auto-discovery.

> **Note for users upgrading from versions before v1.0.0**: The default Dallas GPIO pin changed from GPIO 13 (D7) to GPIO 10 (SD3) in v1.0.0. If your sensors stopped working after an upgrade, check the Dallas GPIO setting in Settings and change it back to 13 if needed.

Wiring a DS18B20 sensor:

- Connect VDD to 3.3 V (or use parasitic power mode with VDD tied to GND).
- Connect GND to ground.
- Connect DATA to the configured GPIO pin.
- Add a 4.7 kOhm pull-up resistor between VDD and DATA.

#### S0 Pulse Counter

The S0 pulse counter reads electricity pulses from an energy meter that has an S0 output (a standard impulse interface common on DIN-rail energy meters). Connect the S0 output to a configurable GPIO pin. Configure the pulses-per-kWh ratio in Settings. The firmware reports cumulative kWh and instantaneous power in watts to MQTT and Home Assistant.

#### BLE Temperature Sensors

Up to four Xiaomi LYWSD03MMC Bluetooth LE sensors are supported on the ESP32-S3; the LOLIN S3 Mini and S3 Mini Pro both have the BLE radio. The firmware reads them passively using the BTHome v2 protocol (these sensors broadcast their readings as advertisements; no pairing is required). Room temperature and humidity from these sensors feed into the SAT multi-zone averaging and are published to Home Assistant as individual sensor entities.

### Flashing the Firmware

#### First-Time Flashing with flash_otgw (Recommended)

The first flash of a new S3 Mini goes over USB. The release zip (`OTGW-firmware-esp32-combo-<version>-flash.zip`) contains the flash scripts `flash_otgw.bat` (Windows) and `flash_otgw.sh` (Linux/macOS) and a merged-full image that combines the bootloader, partition table, firmware, and filesystem in a single file flashed at address 0x0.

The scripts need no Python: on the first run they download Espressif's standalone esptool to `./tools/esptool/`. By default they do a factory reset: they erase the flash and write the merged-full image. WiFi credentials and settings are removed.

**Prerequisites:**

- A USB cable between your computer and the USB port of the S3 Mini
- The S3 Mini uses the built-in ESP32-S3 USB-Serial/JTAG interface (Espressif VID 0x303A, PID 0x1001), typically no extra driver needed. A fresh board that still runs its factory firmware can show up as PID 0x4001 until the first flash; `flash_otgw` finds both without `--port`
- Linux: the script escalates with `sudo` when your user is not in the `dialout` group. To avoid that, add your user to the group (`sudo usermod -aG dialout $USER`, then log out and back in)

**Steps:**

1. Unpack the release zip and connect the USB cable to the S3 Mini.

2. Open a terminal in the unpacked folder and run:

```bash
flash_otgw.bat          # Windows
./flash_otgw.sh         # Linux/macOS
```

3. The script picks the merged-full image from the folder and auto-detects the serial port by USB VID/PID. If auto-detection fails, or more than one S3 is connected, specify the port explicitly:

```bash
flash_otgw.bat --port COM5              # Windows
./flash_otgw.sh --port /dev/ttyACM0     # Linux
./flash_otgw.sh --port /dev/cu.usbmodem-XXXX  # macOS
```

4. After flashing completes, the device reboots automatically.

Later updates go over WiFi (see Upgrading from a Previous Version below). A unit on the old partition layout (an early 2.0.0 alpha) needs one `--migrate` run: `flash_otgw.bat --migrate --host <ip>` or `./flash_otgw.sh --migrate --host <ip>`. It saves `settings.ini` over HTTP, writes the new layout with WiFi kept, and restores the settings. Run the script with `--help` for all options.

#### Building from Source

On the 2.x line the only build is `esp32-combo`. Build it with the wrapper scripts, which set up their own Python environment and the PlatformIO toolchain:

```bash
# Clone the repository
git clone --recursive https://github.com/rvdbreemen/OTGW-firmware.git
cd OTGW-firmware

build.bat                # Windows: firmware + filesystem, esp32-combo
./build.sh               # Linux/macOS: firmware + filesystem, esp32-combo
./build.sh --firmware    # Firmware only (skip filesystem)
./build.sh --filesystem  # Filesystem only
./build.sh --clean       # Clean build artifacts
```

The build writes its output to `build/`, including the release-style zip with the merged-full image and the flash scripts. Flash it with `flash_otgw` as described above. Run `./build.sh --help` for the full, current list of flags. Chapter 8 covers the build in more detail.

#### Manual Flashing with Existing Binaries

If you already have a merged-full image, pass it explicitly:

```bash
flash_otgw.bat --bin OTGW-firmware-esp32-combo-<version>-merged-full.bin
./flash_otgw.sh --bin OTGW-firmware-esp32-combo-<version>-merged-full.bin
```

### First-Time Setup: Connecting to Wi-Fi

On the first boot after flashing, the device has no saved Wi-Fi credentials. It broadcasts a temporary Wi-Fi access point so you can configure it.

**AP name format**: `<hostname>-<MAC suffix>`

The default hostname on 2.0.0 is `OTGW2`, so the AP will typically be named something like `OTGW2-A4:CF:12:21:B4:F8`.

**Steps:**

1. On your phone or laptop, open the Wi-Fi settings and connect to the `otgw-XXXXXX` network.

2. A captive portal opens automatically in your browser. If it does not open automatically, navigate to `http://192.168.4.1/`.

3. Select your home Wi-Fi network from the list, enter the password, and tap Save.

4. The device connects to your home network, obtains a DHCP address, and announces itself as `otgw2.local` via mDNS.

5. Navigate to `http://otgw2.local/` in your browser to open the web interface. On Windows, the device also responds to `http://otgw2/` via LLMNR.

> **AP Fallback Mode (beta)**: If the configured Wi-Fi network becomes unavailable after initial setup, the device opens a fallback AP named `OTGW-<MAC>` with password `otgw1234` and IP `192.168.4.1`, keeping the web UI accessible.

### Filesystem Upload (LittleFS)

The web assets (HTML, CSS, JavaScript, OLED icons, PIC firmware files, and the Home Assistant discovery configuration) are stored in a LittleFS partition on the flash. This partition must be flashed alongside the firmware binary.

When using `flash_otgw`, the filesystem is flashed automatically as part of the merged-full image.

> **After a firmware update via OTA**: The web assets in LittleFS must match the firmware. The **Update** page installs both in one action (choose the release zip, press Update). You can also update only the filesystem on the same page, or upload a new filesystem image manually via the File Manager.

### Upgrading from a Previous Version

#### OTA Upgrade (Recommended)

Open `http://<ip>/update`, choose the release zip (or the `.ino.bin` and `.littlefs.bin` files) and press **Update**. The page hands over to the safeboot updater, installs the firmware, waits for the restart and then installs the web files; your settings and sensor names are kept. Keep the tab open for 2 to 3 minutes. A unit on the earlier layout needs one USB migration first (`flash_otgw --migrate --host <ip>`). Details: [WiFi firmware updates](../../guides/WIFI_FIRMWARE_UPDATE.md).

#### Manual OTA Upload

If you have a firmware binary file and want to upload it directly:

1. Navigate to the **Update** page in the web interface.
2. Use the file upload area to select your firmware `.bin` file.
3. The device flashes the binary and reboots.

#### Notes When Upgrading from v1.x

- Moving from 1.x to 2.x means replacing the Wemos D1 mini (ESP8266) with a LOLIN S3 Mini or S3 Mini Pro and flashing it over USB (see Flashing the Firmware). Going back to 1.x means putting the Wemos D1 mini back; a 1.x zip does not run on an S3.
- No settings migration is required. Settings files from v1.3.x load without modification in v2.0.0.
- MQTT topics and the REST API are unchanged from v1.x.
- If you are upgrading from before v1.2.0, be aware that REST API v0 and v1 endpoints were removed in v1.2.0. Only `/api/v2/` endpoints are available. See [docs/BREAKING_CHANGES.md](../../BREAKING_CHANGES.md).
- After upgrade, delete any orphaned Home Assistant entities and allow MQTT auto-discovery to recreate them to pick up any new or renamed entities.

#### Important Warnings

- **Never flash PIC firmware over Wi-Fi using OTmonitor.** This can brick the PIC microcontroller. Use the built-in PIC firmware upgrade feature in the web UI instead.
- **In OT-Direct mode** (only on hardware without a PIC), the PIC UART is bypassed. Do not attempt PIC firmware upgrades while OT-Direct is the active OpenTherm path.

---
