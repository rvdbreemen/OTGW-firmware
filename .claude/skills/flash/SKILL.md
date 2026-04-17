---
name: flash
description: Build firmware + filesystem and flash to ESP via USB. Auto-detects serial port. No user input needed.
disable-model-invocation: true
---

# /flash - Build and flash OTGW-firmware

Build firmware and filesystem, then flash both to the connected ESP via USB.
Fully automated, no user interaction required.

## Steps

Run these commands sequentially. Stop and report if any step fails.

```bash
cd D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware
python build.py
python flash_esp.py --build --no-interactive
```

The `build.py` without flags builds both firmware AND filesystem.
The `flash_esp.py --build --no-interactive` flashes both to the ESP without prompts, auto-detecting the serial port.

## After flashing

Report:
- Build result (firmware size, filesystem size)
- Flash result (port used, success/failure)
- The firmware version that was flashed (from version.h)
