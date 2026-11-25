# Updating the gateway firmware over WiFi

Applies to ESP32-S3 gateways running 2.0.0. The details for developers are in [WIFI_FIRMWARE_UPDATE_REFERENCE.md](WIFI_FIRMWARE_UPDATE_REFERENCE.md).

> **Heating warning.** While the update runs (1 to 2 minutes of the 2 to 3 minutes in total) the gateway does nothing for your heating. On a board that talks to the boiler directly (no PIC chip) the link between thermostat and boiler is interrupted and the boiler runs on its own settings. On a board with the PIC chip the PIC keeps passing the OpenTherm messages; that is how it is built, it is not measured on a bench. Pick a moment when a short interruption does not matter, and do not unplug the gateway while it runs.

## What you need

- The release zip for your board: `OTGW-firmware-<board>-<version>-flash.zip`. Unpack it.
- The gateway's IP address: look for it in your router's list of connected devices. Use the address, not `otgw2.local`, for the update page.
- Stop any capture tool (`capture-mqtt-debug`, `otgw_capture.py`) first. On a busy gateway the upload can drop halfway.
- Optional safety net: on the update page, use **Download a settings backup** before you start.

## 1. Once: move the gateway to the new flash layout (USB)

From firmware 2.0.0-alpha.425, a gateway with 4 MB flash takes firmware updates over WiFi through a small built-in updater called **safeboot**. A gateway flashed with an older firmware still has the old flash layout (how the flash memory is divided between the firmware and the web files), so it needs one USB flash to move to the new one. The tool saves your settings and puts them back, and your WiFi stays.

1. Connect the gateway over USB. It must also be on your network, so the tool can save its settings.
2. In the unpacked zip, run:
   - Windows: `flash_otgw.bat --migrate --host <ip-of-your-gateway>`
   - Linux/macOS: `./flash_otgw.sh --migrate --host <ip-of-your-gateway>`
   - Add `--password <pw>` if you set an admin password.
3. Wait until it says it is done. A copy of your settings stays in the `backup` folder.

`flash_otgw --update` refuses to run on a gateway that still has the old layout and tells you to do this step first. If the gateway cannot be reached over the network, add `--no-backup`: it then migrates without saving the settings, and they return to defaults.

## 2. Every later update (WiFi)

1. Open `http://<ip-of-gateway>/update`.
2. Choose the release zip (or its two files `*.ino.bin` and `*.littlefs.bin`). You can drop them on the page. The page refuses files that do not belong together.
3. Press **Update** and keep the tab open until it says Done. It restarts the gateway into the updater, installs the firmware, waits 35 seconds to see that the new firmware is healthy, installs the web files, and checks the result.
4. When an admin password is set, the browser asks for it again when the updater starts.

If you close the tab or lose the connection in between, open `/update` again and choose the same files: the page sees what is already installed and does the rest. A filesystem update (the web files) keeps your settings and the learned data; the details are in the reference.

## 3. When something goes wrong

| What you see | What to do |
|---|---|
| The upload was interrupted (WiFi drop, power cut) | The gateway waits in the updater and the old firmware is gone. Open `http://<ip-of-gateway>/` and upload the firmware again |
| "Update failed" in the updater | The file is incomplete or not a firmware image. Upload a valid `.ino.bin` |
| "The firmware failed to start and was rolled back" | The new firmware crashed in its first 30 seconds and the gateway returned to the updater. Upload a working firmware |
| The updater cannot join your WiFi (no stored WiFi) | It opens its own WiFi network named `<hostname>-safeboot`. Connect to it and open `http://192.168.4.1/` |
| The update page does not find the updater within a minute (Ethernet or fixed IP address) | The updater always uses WiFi and DHCP, so it may have another address. Find the gateway in your router, upload the `.ino.bin` there, then open `/update` again and choose the same files to finish the web files. Or switch the gateway off and on: it returns to the old firmware |
| The web files step failed or the connection dropped | The new firmware is installed. Press **Try again** on the page, or upload only the `.littlefs.bin` in the second form. Your settings are kept |

The gateway has room for one firmware only, so there is no automatic return to the previous version. While the updater is active the gateway does not run.

## Words used here

- **Gateway**: your OTGW. **Web UI**: the pages you open in a browser at its address.
- **Updater (safeboot)**: the small program that installs the firmware.
- **Web files**: what the web UI is made of (called the filesystem in the reference).
- **Flash layout**: how the flash memory is divided.
- **PIC**: the chip on the Classic board that handles the OpenTherm messages.
