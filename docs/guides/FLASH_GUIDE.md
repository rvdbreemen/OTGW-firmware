# ESP8266 Firmware Guide — Language / Taal

This guide is available in English and Dutch. Choose your language below.

| Language | Link |
|---|---|
| 🇬🇧 English | [FLASH_GUIDE_EN.md](FLASH_GUIDE_EN.md) |
| 🇳🇱 Nederlands | [FLASH_GUIDE_NL.md](FLASH_GUIDE_NL.md) |

---

> *You were redirected here from a link to `FLASH_GUIDE.md`. The content has been
> split into language-specific files. Please update your bookmarks.*

## Flashing tools at a glance

| Tool | Requires Python? | Best for |
|---|---|---|
| `flash_otgw.sh` / `flash_otgw.bat` | **No** | End users — run from terminal or double-click the `.bat` |
| `flash_esp.py` | Yes (Python 3.6+) | Developers, OTA download, advanced options |

Both tools are included in each release download and in the repository root.

---

## Prerequisites

### Hardware
- ESP8266 development board (NodeMCU or Wemos D1 mini)
- Micro USB cable (data cable, not charge-only)
- Computer with USB port

### Software
- **Simple method**: no extra software — `flash_otgw.sh`/`flash_otgw.bat` downloads esptool automatically on first run
- **Advanced method**: Python 3.6 or higher — `flash_esp.py` installs esptool via pip if needed

### USB Drivers
Depending on your board and OS, install drivers as needed:
- Windows: CP210x or CH340 USB-to-UART driver (check Device Manager if the port is missing)
- macOS: Drivers are usually included on recent versions
- Linux: Ensure your user is in the `dialout` group for serial port access (`sudo usermod -aG dialout $USER`)

---

## Fresh Installation (First-Time Flash)

> **Important**: A fresh install requires BOTH the firmware binary (`OTGW-firmware-<version>.ino.bin`) and the filesystem binary (`OTGW-firmware-<version>.ino.littlefs.bin`). Flashing only the firmware will cause the device to spend several minutes reformatting an empty filesystem on first boot — or, in the worst case, result in a bootloop.

### Simple method (no Python required)

1. From the GitHub release page, download `flash_otgw.sh` (Linux/macOS) or `flash_otgw.bat` (Windows) **and** both binary files:
   - `OTGW-firmware-*.ino.bin` (firmware)
   - `OTGW-firmware*.littlefs.bin` (filesystem)
2. Place all three files in the same directory.
3. Run the script:

**Linux / macOS:**
```bash
chmod +x flash_otgw.sh
./flash_otgw.sh
```

**Windows** (run from Command Prompt or PowerShell):
```bat
flash_otgw.bat
```

The script downloads esptool on first run, auto-detects your serial port, erases flash, and writes both binaries — no prompts, no extra software needed.

If multiple serial ports are present, the first one is used. Pass `--port` to pick a specific one:
```bash
./flash_otgw.sh --port /dev/ttyUSB1
```
```bat
flash_otgw.bat --port COM5
```

### Advanced method (Python)

```bash
# Download the latest release and erase flash for a clean start
python3 flash_esp.py --download --erase
```

This does everything in one step:
1. Downloads the latest `OTGW-firmware-<version>.ino.bin` (firmware) and `OTGW-firmware-<version>.ino.littlefs.bin` (filesystem) from GitHub
2. Erases the entire flash chip before writing (removes any stale data from older versions)
3. Flashes firmware at `0x0` and filesystem at `0x200000` in a single esptool operation

### Why `--erase` matters for a fresh install

Older firmware versions (v1.3.x and below) used a different filesystem partition layout (1 MB LittleFS at `0x300000`). If you skip `--erase`, remnants of the old filesystem at the old address may remain. The new firmware will find no valid filesystem at the new address (`0x200000`) and will either:
- Spend 5–10 minutes silently reformatting the flash (device appears unresponsive — not a bootloop)
- Boot repeatedly into an error state that looks like a bootloop

Using `--erase` eliminates this class of issue entirely.

---

## Upgrading via USB (Recommended for Major Version Changes)

When upgrading from **v1.3.x or earlier** to **v1.4.x or later**, the LittleFS partition size changed from 1 MB to 2 MB. A firmware-only upgrade via USB will trigger a filesystem reformat on first boot and **wipe all your settings**.

### Upgrade procedure (USB, preserving settings where possible)

For upgrading v1.4.x → v1.5.x (same partition layout, no reformat needed):

```bash
python3 flash_esp.py --download
```

Both firmware and filesystem are written in a single operation. No erase is needed; settings stored in the filesystem are preserved.

> **Note**: `flash_otgw.sh` and `flash_otgw.bat` always erase the entire flash before writing. They are not suitable for settings-preserving upgrades. Use `flash_esp.py --download` (without `--erase`) when you want to keep your settings.

For upgrading v1.3.x or earlier → v1.4.x+ (partition layout change, settings will be lost):

```bash
# Back up settings from the Web UI first (Settings → Export), then:
python3 flash_esp.py --download --erase
# or the no-Python scripts, which also erase:
./flash_otgw.sh      # Linux/macOS
flash_otgw.bat       # Windows
```

After the flash, re-import your settings via the Web UI.

---

## Upgrading via Web UI OTA

> **WARNING**: When upgrading from v1.3.x or earlier to v1.4.x via the Web UI OTA page, you **must** flash the filesystem binary first, then the firmware binary. Flashing in the wrong order causes the new firmware to boot against the old 1 MB filesystem layout. The device will spend 5–10 minutes silently reformatting and will then lose all settings.

**Correct OTA order for v1.3.x → v1.4.x upgrades:**

1. Export your settings via the Web UI (Settings page → Export)
2. On the Web UI Update page, upload `OTGW-firmware-*.littlefs.bin` first and wait for the reboot
3. Upload `OTGW-firmware-*.ino.bin` second and wait for the reboot
4. Hard-refresh the browser (Ctrl+F5)
5. Re-import your settings

**This order requirement does NOT apply to v1.4.x → v1.5.x upgrades** (the partition layout is identical).

---

## Quick Start (Standard)

### Preferred: no-Python scripts

Download `flash_otgw.sh` (Linux/macOS) or `flash_otgw.bat` (Windows) and both binary files from the GitHub release page. Place all three in the same directory and run:

```bash
chmod +x flash_otgw.sh
./flash_otgw.sh          # Linux/macOS
```

```bat
flash_otgw.bat           :: Windows — Command Prompt or PowerShell
```

The script downloads esptool on first run (no Python needed), erases flash, and writes both images in one step.

### Download latest release and flash (Python)

```bash
python3 flash_esp.py
```

Or explicitly:

```bash
python3 flash_esp.py --download
```

### Developer mode — build and flash from source

```bash
python3 flash_esp.py --build
```

### Manual mode — use existing binary files

```bash
python3 flash_esp.py --firmware OTGW-firmware-1.5.0.ino.bin --filesystem OTGW-firmware-1.5.0.ino.littlefs.bin
```

---

## Common Options

| Option | Description |
|---|---|
| `--port PORT` | Serial port (e.g., `COM5` or `/dev/ttyUSB0`) |
| `--baud BAUD` | Flash baud rate (default: 460800; try 115200 on unstable connections) |
| `--erase` | Erase entire flash before writing. **Use for first installs and cross-generation upgrades.** |
| `--download` | Download latest release from GitHub and flash |
| `--build` | Build firmware locally and flash (requires arduino-cli) |
| `--no-interactive` / `-y` | Skip all prompts (for automation) |

---

## Troubleshooting Bootloops

A bootloop (device resets repeatedly and never reaches the Web UI) after flashing is almost always caused by one of the following:

### 1. Firmware flashed without a matching filesystem

The firmware cannot find a valid filesystem and resets.

**Fix:** Erase the flash and write both firmware and filesystem in one step.

No-Python scripts (erase is always included):
```bash
./flash_otgw.sh      # Linux/macOS
flash_otgw.bat       # Windows
```

Python:
```bash
python3 flash_esp.py --download --erase
```

### 2. Upgrading from v1.3.x without erasing (stale filesystem at wrong offset)

v1.4.x moved the filesystem partition from `0x300000` (1 MB) to `0x200000` (2 MB). If the old filesystem data is still present, the new firmware may behave unexpectedly.

**Fix:**

No-Python scripts:
```bash
./flash_otgw.sh      # Linux/macOS
flash_otgw.bat       # Windows
```

Python:
```bash
python3 flash_esp.py --download --erase
```

### 3. Flash incomplete or interrupted

**Fix:** Retry with a lower baud rate.

No-Python scripts:
```bash
./flash_otgw.sh --baud 115200
flash_otgw.bat --baud 115200
```

Python:
```bash
python3 flash_esp.py --download --erase --baud 115200
```

### 4. Diagnosing with the serial monitor

If the device is in a bootloop, connect via USB and open a serial terminal at **74880 baud** to capture the ESP8266 ROM bootloader messages. Then switch to **115200 baud** once the firmware banner starts printing (if it does). The first 20–30 lines almost always identify the crash reason (e.g., `Exception 3`, `Fatal exception 28`, `LittleFS mount failed`).

Tools: Arduino IDE Serial Monitor, PuTTY, `screen /dev/ttyUSB0 74880`, or any terminal at the correct baud.

### 5. Hard recovery (device completely unresponsive)

If the Web UI and serial output are both unavailable, specify the port and a conservative baud rate explicitly.

No-Python scripts:
```bash
./flash_otgw.sh --port /dev/ttyUSB0 --baud 115200
flash_otgw.bat --port COM3 --baud 115200
```

Python:
```bash
python3 flash_esp.py --download --erase --baud 115200 --port <your-port>
```

If no port appears, check Device Manager (Windows) or `ls /dev/tty*` (Linux/macOS) for the USB-serial adapter. Common driver packages: CP210x (Silicon Labs) or CH340 (WCH).

---

## After Flashing

After a fresh flash (whether via the simple scripts or `flash_esp.py`), the device opens a WiFi access point named `OTGW-<MAC-address>`. Connect to it and browse to `http://192.168.4.1` to configure your WiFi network and other settings. On subsequent boots the device connects to your configured network.

---

## Notes

- **Never flash the PIC firmware over WiFi using OTmonitor** — this can brick the PIC microcontroller.
- Use a reliable, direct USB cable (avoid hubs) to minimise flash errors.
- If auto-install of esptool fails, install it manually: `pip install esptool`
- On Linux, add yourself to the `dialout` group and log out/in before flashing. On first run, `flash_otgw.sh` auto-escalates with `sudo` if serial port access is denied.

For full usage details of the Python tool:

```bash
python3 flash_esp.py --help
```
