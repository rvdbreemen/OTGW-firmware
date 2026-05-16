# PIC Firmware Guide

*→ Ook beschikbaar in het [Nederlands](PIC_FIRMWARE_NL.md)*

This guide explains the role of the PIC microcontroller firmware in the OpenTherm Gateway,
how to upgrade it, and what the three available firmware images do.

For authoritative and up-to-date information, always refer to
[Schelte Bron's OTGW website](https://otgw.tclcode.com/) — the original author of the
OpenTherm Gateway hardware and PIC firmware.

---

## Two firmwares, one device

The NodoShop OpenTherm Gateway contains **two separate processors**, each running its own
firmware. Understanding what each one does helps you keep your gateway working and up to date.

### The PIC microcontroller — the OpenTherm brain

The gateway is built around a **PIC microcontroller** (PIC16F88 or PIC16F1847). The PIC sits
physically on the OpenTherm bus, the two-wire connection between your thermostat and your
boiler. It is the only part of the device that can speak the OpenTherm protocol:

- It reads every message your thermostat sends to the boiler, and every reply from the boiler.
- It lets the gateway *intercept* those messages so it can, for example, raise or lower the
  boiler's hot-water temperature setpoint independently of what the thermostat asked for.
- It sends commands to the boiler on behalf of the gateway when you request an override.

Without a working PIC firmware:

- No OpenTherm messages can be read at all.
- No setpoints can be overridden.
- Nothing reaches your smart home (no MQTT data, no Home Assistant sensors, no Web UI readings).

The PIC firmware is written and maintained by **Schelte Bron**, the original designer of the
OpenTherm Gateway hardware. The OTGW-firmware downloads the latest PIC firmware from
[otgw.tclcode.com](https://otgw.tclcode.com/) and programs it into the PIC for you.

### The ESP8266 — the home-automation integration layer

Alongside the PIC sits an **ESP8266 Wi-Fi module** running OTGW-firmware. While the PIC
handles the raw OpenTherm bus signal, the ESP8266 fully understands the protocol — it
decodes all 80+ message IDs defined in the OpenTherm specification, controls the heater on
your behalf, and connects everything to your smart home through MQTT, Home Assistant, and a
REST API.

For the complete feature overview and ESP8266 flash instructions, see the
**[ESP8266 Firmware Guide](FLASH_GUIDE_EN.md)**.

---

## The three PIC firmware images

Schelte Bron provides three firmware images for the PIC. The OTGW-firmware stores them in
the LittleFS filesystem so they can be flashed directly from the Web UI.

### 1. `gateway.hex` — Standard gateway firmware

This is the normal operating firmware. It implements the full OpenTherm Gateway feature set:

- Intercepts all OpenTherm messages between thermostat and boiler.
- Allows the ESP8266 to override setpoints, read sensor data, and inject commands.
- Supports all OTGW serial commands (`TT`, `SW`, `PR`, `GW`, etc.).
- Enables Home Assistant integration, MQTT publishing, and REST API responses.

**This is the firmware that should be running during normal use.**
It is automatically selected for the update check whenever the device is running in
standard gateway mode.

Source details: [Gateway firmware details](https://otgw.tclcode.com/firmware.html)

### 2. `interface.hex` — Interface firmware

The interface firmware turns the OTGW hardware into a **serial-to-OpenTherm interface** for
experimentation. It reports OpenTherm traffic to the serial connection, but it does **not**
forward thermostat/boiler messages on its own. You must send an 8-hex-character OpenTherm
frame (followed by carriage return), and the firmware transmits it to thermostat or boiler
based on message direction.

Use cases:
- OpenTherm protocol experiments and custom message manipulation tooling.
- Installations where you want manual message control instead of full gateway behaviour.

> **Note:** With the interface firmware loaded, the gateway-specific override commands
> (`TT`, `SW`, etc.) no longer work. Switch back to `gateway.hex` to restore full gateway
> functionality.

For more details, see:
- [Interface firmware details](https://otgw.tclcode.com/interface.html)
- [Firmware overview](https://otgw.tclcode.com/firmware.html)

### 3. `diagnose.hex` — Diagnostic firmware

The diagnostic firmware temporarily replaces normal gateway firmware to help diagnose
gateway hardware and OpenTherm wiring issues. It provides six tests with detailed
timing/electrical information, while thermostat↔boiler communication usually keeps running.

Normal thermostat–boiler communication continues while most tests run. The one exception
is Test #4 (delay symmetry), which requires the master and slave interfaces to be looped
together.

**Available tests:**

| # | Name | Description |
|---|------|-------------|
| 1 | LED test | Pulls LED outputs low one by one to verify LED wiring. Works without thermostat/boiler connected. Send `<CR>` to end. |
| 2 | Bit timing — thermostat | Reports high/low durations (µs) for each thermostat message. Each value should fall in a half-bit (400–650 µs) or full-bit (900–1150 µs) range. Requires thermostat connected. Send `<CR>` to end. |
| 3 | Bit timing — boiler | Same as test #2 but for the boiler side. If no thermostat is connected, a test message is sent every second. Send `<CR>` to end. |
| 4 | Delay symmetry | Measures opto-coupler propagation delay in both directions (low-to-high and high-to-low). Requires a loopback between master and slave interfaces. Test ends automatically once all four measurements are complete. |
| 5 | Voltage levels | Performs A/D measurements on both interfaces and the reference voltage. Reports the logical high and low voltage levels. At the end, prompts for a new reference voltage setting (`VR` command). Useful when messages are not being decoded correctly. |
| 6 | Idle times | Measures the idle time (ms) between OpenTherm messages. |

After running the desired tests, flash `gateway.hex` back to restore normal operation.

Source details: [Diagnostic firmware details](https://otgw.tclcode.com/diagnose.html)

---

## How to upgrade the PIC firmware

### Via the Web UI (recommended)

The OTGW-firmware Web UI provides a built-in PIC firmware upgrade feature. This is the
recommended method because the upgrade routine (originally from Schelte Bron's NodeMCU
firmware) handles all the low-level PIC programming protocol steps automatically.

1. Open the Web UI in your browser (e.g. `http://<device-ip>/`).
2. Navigate to **Settings → PIC Firmware** (or the **Update** tab, depending on your version).
3. The current PIC firmware type and version are shown.
4. Click **Check for update** to compare the installed version against the latest available on
   [otgw.tclcode.com](https://otgw.tclcode.com/).
5. If an update is available (or if you want to switch firmware type), select the firmware
   image: **gateway**, **interface**, or **diagnose**.
6. Click **Flash** (or **Upgrade**). The firmware downloads the `.hex` file from
   `otgw.tclcode.com` over HTTP and programs it into the PIC directly from the ESP8266.
7. Progress is displayed in the UI. Do not power off the device during programming.
8. After completion, the PIC reboots automatically and the new version is shown.

> **Security note:** The `.hex` file is downloaded over plain HTTP from `otgw.tclcode.com`.
> Make sure the device is on a trusted local network.

> **Important:** Never flash the PIC firmware using the **OTmonitor** application over
> Wi-Fi. This can corrupt the PIC and leave it in an unrecoverable state. Always use the
> built-in Web UI upgrade feature.

### Manual upload via the filesystem explorer *(advanced users only)*

> **⚠️ Advanced users only.** This method is intended for developers or users who have a
> custom or patched `.hex` file. If you are not sure whether you need this, use the
> Web UI upgrade described above instead.

1. Open the filesystem explorer at `http://<device-ip>/fs`.
2. Upload your `.hex` file (e.g. `gateway.hex`) to the root of the filesystem.
3. Trigger the upgrade via the Web UI as described above.

---

## Which PIC processor do I have?

The Web UI displays the detected processor type alongside the firmware version. There are
two hardware variants:

| Processor | Used in firmware versions |
|-----------|--------------------------|
| PIC16F88 | Older gateway.hex versions (< 5.x for gateway) |
| PIC16F1847 | Newer versions (introduced in gateway 5.x / 6.x) |

The `OTGW_ERROR_DEVICE` error during an upgrade means you tried to flash a `.hex` file
built for the wrong PIC variant. The OTGW-firmware detects the correct variant automatically
and selects the right image from `otgw.tclcode.com`.

---

## Further reading

- [Schelte Bron's OTGW project](https://otgw.tclcode.com/) — hardware design, PIC firmware
  downloads, command reference, and support forum.
- [OTGW firmware commands](https://otgw.tclcode.com/firmware.html) — full list of serial
  commands supported by the PIC firmware.
- [OTGW support forum](https://otgw.tclcode.com/support/forum) — community support for
  hardware and PIC firmware issues.
- [ESP8266 Firmware Guide](FLASH_GUIDE_EN.md) — full ESP8266 feature overview and flash
  instructions.
