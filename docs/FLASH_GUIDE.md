# ESP8266 Flashing Guide

This guide explains how to use flash_esp.py to flash OTGW-firmware onto your ESP8266 device (NodeMCU or Wemos D1 mini).

## Prerequisites

### Hardware
- ESP8266 development board (NodeMCU or Wemos D1 mini)
- MicroUSB cable
- Computer with USB port

### Software
- Python 3.6 or higher
- The flash_esp.py script will automatically install esptool if needed

### USB Drivers
Depending on your board and OS, install drivers as needed:
- Windows: CP210x or CH340 USB-to-UART driver
- macOS: Drivers are usually included on recent versions
- Linux: Ensure your user is in the dialout group for serial port access

## Quick Start

### Download Latest Release (Default)

Run without arguments to download and flash the latest release:

python3 flash_esp.py

This will:
1. Install esptool if needed
2. Download the latest release firmware and filesystem
3. Auto-detect serial ports
4. Flash the device

### Developer Mode - Build and Flash

Use a local build and flash it:

python3 flash_esp.py --build

### Manual Mode - Use Existing Files

If you already have the images:

python3 flash_esp.py --firmware OTGW-firmware-fw.bin --filesystem OTGW-firmware-fs.bin

## First-Time Installation (Erase)

For a clean install, erase flash before flashing:

python3 flash_esp.py --download --erase

Or for local builds:

python3 flash_esp.py --build --erase

## Common Options

- --port PORT: Serial port (e.g., COM5 or /dev/ttyUSB0)
- --baud BAUD: Flash baud rate (default: 460800)
- --erase: Erase flash before flashing
- --no-interactive: Disable prompts for automation

## Notes

- Never flash the PIC firmware over WiFi using OTmonitor.
- Use a reliable USB cable and avoid hubs if you see flashing errors.
- If auto-install of esptool fails, install it manually via pip.

For full usage details, run:

python3 flash_esp.py --help
