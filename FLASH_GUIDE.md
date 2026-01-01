# ESP8266 Flashing Guide

This guide explains how to use the `flash_esp.py` script to flash the OTGW-firmware onto your ESP8266 device (NodeMCU or Wemos D1 mini).

## Prerequisites

### Hardware
- ESP8266 development board (NodeMCU or Wemos D1 mini)
- MicroUSB cable
- Computer with USB port

### Software
- Python 3.6 or higher
- The `flash_esp.py` script will automatically install `esptool` if needed

### USB Drivers

Depending on your ESP8266 board and operating system, you may need to install USB drivers:

#### Windows
- **CP210x** driver for NodeMCU boards: [Download from Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- **CH340** driver for some NodeMCU/Wemos clones: [Download from manufacturer](http://www.wch-ic.com/downloads/CH341SER_EXE.html)

#### macOS
- Most drivers are included in recent macOS versions
- For older versions, install CP210x or CH340 drivers as needed

#### Linux
- Drivers are usually built into the kernel
- Add your user to the `dialout` group for port access:
  ```bash
  sudo usermod -a -G dialout $USER
  ```
  (Log out and back in for this to take effect)

## Quick Start

### Simple Mode - Download Latest Release (Recommended for Users)

The easiest way to flash your ESP8266 is to use the download mode, which automatically fetches the latest release from GitHub:

```bash
python3 flash_esp.py --download
```

This mode will:
1. Check for and install `esptool` if needed
2. Fetch the latest release from GitHub
3. Download the firmware and filesystem files
4. Auto-detect available serial ports
5. Guide you through the flashing process

### Developer Mode - Build and Flash

If you're developing or want to flash your own build:

```bash
python3 flash_esp.py --build
```

This mode will:
1. Check for and install `esptool` if needed
2. Build the firmware using `make binaries`
3. Build the filesystem using `make filesystem`
4. Auto-detect available serial ports
5. Flash the locally built files

### Manual Mode - Use Existing Files

If you already have firmware files downloaded or built:

```bash
python3 flash_esp.py --firmware OTGW-firmware-fw.bin --filesystem OTGW-firmware-fs.bin
```

Or use interactive mode to select files:

```bash
python3 flash_esp.py
```

### First-Time Installation

For a clean first-time installation with the latest release, it's recommended to erase the flash first:

```bash
python3 flash_esp.py --download --erase
```

For developer builds:

```bash
python3 flash_esp.py --build --erase
```

This ensures no old settings or data interfere with the new firmware.

## Command-Line Options

### Basic Usage

```bash
python3 flash_esp.py [OPTIONS]
```

### Available Options

| Option | Description |
|--------|-------------|
| `-d`, `--download` | Download latest release from GitHub and flash (simple mode) |
| `--build` | Build firmware locally and flash (developer mode) |
| `-p PORT`, `--port PORT` | Serial port (e.g., `COM5`, `/dev/ttyUSB0`). Auto-detected if not specified. |
| `-f FILE`, `--firmware FILE` | Path to firmware binary file (`.bin`) - for manual mode |
| `-s FILE`, `--filesystem FILE` | Path to filesystem binary file (`.littlefs.bin`) - for manual mode |
| `-b BAUD`, `--baud BAUD` | Baud rate for flashing (default: 460800) |
| `-e`, `--erase` | Erase flash before flashing (recommended for first install) |
| `--no-interactive` | Disable interactive prompts (for automation) |
| `-h`, `--help` | Show help message and exit |

## Examples

### Download and Flash Latest Release (Simple Mode)

```bash
python3 flash_esp.py --download
```

### Build and Flash Locally (Developer Mode)

```bash
python3 flash_esp.py --build
```

### Download with Specific Port

```bash
python3 flash_esp.py --download --port /dev/ttyUSB0
```

### Build with Lower Baud Rate

```bash
python3 flash_esp.py --build --baud 115200
```

### Manual Mode - Flash Specific Firmware File

```bash
python3 flash_esp.py --firmware build/OTGW-firmware.ino.bin
```

### Manual Mode - Flash Both Firmware and Filesystem

```bash
python3 flash_esp.py \
  --firmware build/OTGW-firmware.ino.bin \
  --filesystem build/OTGW-firmware.ino.littlefs.bin
```

### Manual Mode - Specify Port and Baud Rate

```bash
python3 flash_esp.py \
  --port /dev/ttyUSB0 \
  --baud 115200 \
  --firmware build/OTGW-firmware.ino.bin
```

### Download Mode - Complete Flash with Erase (Recommended for First Install)

```bash
python3 flash_esp.py \
  --download \
  --erase
```

### Build Mode - Complete Flash with Erase

```bash
python3 flash_esp.py \
  --build \
  --erase
```

### Automated/Non-Interactive Mode

For use in scripts or CI/CD:

**Download mode:**
```bash
python3 flash_esp.py \
  --download \
  --no-interactive \
  --port /dev/ttyUSB0
```

**Build mode:**
```bash
python3 flash_esp.py \
  --build \
  --no-interactive \
  --port /dev/ttyUSB0
```

**Manual mode:**
```bash
python3 flash_esp.py \
  --no-interactive \
  --port /dev/ttyUSB0 \
  --firmware OTGW-firmware-fw.bin \
  --filesystem OTGW-firmware-fs.bin
```

## Step-by-Step Flashing Process

### 1. Prepare Your ESP8266

1. **Disconnect the ESP8266 from the OTGW** (if already installed)
2. **Connect the ESP8266 to your computer** via USB cable
3. **Verify the device is recognized**:
   - **Windows**: Check Device Manager for COM port
   - **macOS**: Check for `/dev/tty.usb*` or `/dev/cu.usb*`
   - **Linux**: Check for `/dev/ttyUSB*` or `/dev/ttyACM*`

### 2. Obtain Firmware Files

You have three options:

**Option A: Download Latest Release (Simple Mode - Recommended)**
```bash
python3 flash_esp.py --download
```
The script will automatically download the latest release from GitHub.

**Option B: Build from Source (Developer Mode)**
```bash
python3 flash_esp.py --build
```
The script will automatically build the firmware using `make`.

**Option C: Manual Download**
- Download the latest release from: https://github.com/rvdbreemen/OTGW-firmware/releases
- Extract the `.bin` files
- Use manual mode: `python3 flash_esp.py --firmware <file> --filesystem <file>`

### 3. Run the Flash Script

**Simple Mode (Download Latest Release):**
```bash
python3 flash_esp.py --download
```

**Developer Mode (Build and Flash):**
```bash
python3 flash_esp.py --build
```

**Manual Mode:**
```bash
python3 flash_esp.py --firmware <firmware-file> --filesystem <filesystem-file>
```

Or use interactive file selection:
```bash
python3 flash_esp.py
```

### 4. Wait for Completion

The flashing process typically takes 30-90 seconds. You'll see:
- Connection to the ESP8266
- Chip detection
- Flash progress (percentage)
- Verification
- Completion message

### 5. Install and Configure

1. **Disconnect the ESP8266 from USB**
2. **Reconnect it to the OTGW board**
3. **Power on the OTGW**
4. **Configure WiFi**:
   - If the device can't connect to a saved network, it will start a WiFi AP
   - Connect to the AP: `OTGW-<mac-address>`
   - Configure your WiFi credentials
   - The device will reboot and connect to your network

5. **Access the Web UI**:
   - Navigate to `http://<device-ip>/` or `http://otgw/`
   - Configure MQTT and other settings

## Troubleshooting

### Port Not Detected

**Problem**: "No serial ports detected!"

**Solutions**:
- Verify USB cable is properly connected (some cables are charge-only)
- Install appropriate USB drivers (CP210x or CH340)
- Try a different USB port
- On Linux, check user permissions (dialout group)

### Flash Failed / Timeout

**Problem**: "Flashing failed" or timeout errors

**Solutions**:
1. **Reduce baud rate**:
   ```bash
   python3 flash_esp.py --baud 115200
   ```

2. **Try erasing flash first**:
   ```bash
   python3 flash_esp.py --erase
   ```

3. **Check USB cable quality** - use a short, high-quality cable

4. **Manual boot mode** (rarely needed):
   - Hold down the FLASH/BOOT button on ESP8266
   - Press and release RESET button
   - Release FLASH/BOOT button
   - Try flashing again

### Permission Denied (Linux)

**Problem**: "Permission denied" when accessing `/dev/ttyUSB0`

**Solution**:
```bash
sudo usermod -a -G dialout $USER
```
Then log out and back in.

Or run with sudo (not recommended):
```bash
sudo python3 flash_esp.py
```

### esptool Not Found

**Problem**: "esptool not found" or installation fails

**Solutions**:
1. **Install manually**:
   ```bash
   pip3 install --user esptool
   ```

2. **Use system package manager**:
   - **Ubuntu/Debian**: `sudo apt install esptool`
   - **macOS**: `brew install esptool`

### Wrong Chip Detected

**Problem**: Script detects wrong chip or fails to connect

**Solutions**:
- Ensure you're using an ESP8266 (not ESP32)
- Try different USB port
- Check for counterfeit chips
- Verify the board is in flash mode

## Platform-Specific Notes

### Windows
- COM ports are named `COM1`, `COM5`, etc.
- Use Device Manager to identify the correct port
- Some antivirus software may interfere with flashing

### macOS
- Ports are typically `/dev/tty.usbserial-*` or `/dev/cu.usbserial-*`
- Use `/dev/cu.*` ports for flashing (not `/dev/tty.*`)
- SIP (System Integrity Protection) does not affect USB drivers

### Linux
- Ports are typically `/dev/ttyUSB0` or `/dev/ttyACM0`
- User must be in `dialout` group (or run with sudo)
- ModemManager may interfere - disable if issues occur

## Binary File Locations

The script searches for binary files in these locations:
1. Current directory
2. `build/` directory
3. `releases/` directory

### Expected File Names

**Firmware files** (one of):
- `*.bin`
- `*-fw.bin`
- `*.ino.bin`

**Filesystem files** (one of):
- `*-fs.bin`
- `*.littlefs.bin`

## After Flashing

Once flashing is complete:

1. **First Boot**: The device will create a WiFi AP if no credentials are saved
2. **Connect to AP**: `OTGW-<mac>` (no password, or check documentation)
3. **Configure WiFi**: Use the captive portal or `http://192.168.4.1`
4. **Access Web UI**: `http://<device-ip>/` after connecting to your network
5. **Configure MQTT**: For Home Assistant integration
6. **Test Connection**: Use telnet, OTmonitor, or the Web UI to verify communication with the gateway

## Additional Resources

- **Project Wiki**: https://github.com/rvdbreemen/OTGW-firmware/wiki
- **Issues/Support**: https://github.com/rvdbreemen/OTGW-firmware/issues
- **Discord Community**: https://discord.gg/zjW3ju7vGQ
- **OTmonitor Application**: http://otgw.tclcode.com/
- **esptool Documentation**: https://github.com/espressif/esptool

## Safety Notes

⚠️ **Important**:
- **Never flash PIC firmware over WiFi using OTmonitor** - it can brick the PIC
- Use the built-in Web UI PIC firmware upgrade feature instead
- Always backup your settings before flashing
- Use the erase option (`--erase`) when switching between major versions

## License

This script is part of the OTGW-firmware project and is released under the MIT License.
