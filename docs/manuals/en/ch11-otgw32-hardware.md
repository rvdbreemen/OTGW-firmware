## Chapter 11: The OpenTherm Gateway OTGW32

---

Firmware 2.0 supports exactly two hardware configurations:

1. **The OTGW Classic with an ESP32-S3 module.** The traditional gateway with the PIC co-processor and a LOLIN S3 Mini or S3 Mini Pro in the module socket (chapter 2).
2. **The OpenTherm Gateway OTGW32.** A board with an ESP32-S3 that speaks OpenTherm itself, with its own connections to the boiler and to the thermostat. There is no PIC on this board. This chapter describes it. Chapter 12 compares the two.

**What works on the OTGW32:** everything the gateway is for (OpenTherm, MQTT, Home Assistant, SAT). A few optional extras (Bluetooth sensors, long graph history, bigger buffers) need PSRAM (extra working memory) and are off on this board.

**Also not on the OTGW32:** the bypass relay (hardware version 1.1 has a place for one; it is not fitted as supplied and the firmware does not use it), a ready-made terminal block (the 10 way block J4 is not fitted at the factory: you fit it yourself) and PIC firmware updates (there is no PIC).

> **The OTGW32 is new, and the firmware for it has not run in a real home yet.** Hardware versions 1.0 and 1.1 are very similar; version 1.1 has an optional bypass relay that is not fitted as supplied and is not used by the firmware. Firmware 2.0 supports it, but so far the firmware has only run on a bench unit, without a thermostat and without a real boiler on the bus (a simulated boiler, and master mode without a thermostat). What is not yet field-proven: how it behaves with different boilers and thermostats, long runs, the Ethernet and OLED options on real hardware, and OTmonitor (a PC program for the OpenTherm Gateway) over port 25238. If you run an OTGW32, please send a capture with your report (see Chapter 7).

**Which one do you have?** After the first flash the unit tells you: the web UI has an Advanced, PIC firmware tab on an OTGW Classic and no such tab on an OTGW32, and `hardware_type` in the device info (`/api/v2/device/info`) is `otgw-classic` or `otgw32`. If you are not sure which board you have, ask in the Discord support channel before you flash: a factory install erases the unit.

### 11.1 What it is

- **Processor and memory:** an ESP32-S3 with 4 MB of flash and no PSRAM (extra working memory; the LOLIN S3 Mini module has it). The firmware checks for PSRAM while it runs.
- **OpenTherm:** the ESP32-S3 drives the boiler side and the thermostat side itself. The firmware calls this OT-Direct (the ESP32 does the work that the PIC does on the Classic). There is no PIC, so there is no PIC firmware to update.
- **Options in the firmware:** wired Ethernet (a W5500 chip), a small OLED display and a Dallas temperature sensor input are supported by the firmware. They have not been seen on real OTGW32 hardware in our tests.
- **Connector:** the 10 way terminal block (J4) is not fitted at the factory; you fit it yourself. Terminal 1 is the one nearest the reset button. Terminals 1 and 2 are the OpenTherm boiler lines and terminals 3 and 4 are the OpenTherm thermostat lines. Terminal 5 is labelled DI: it is connected to GPIO 5 through a 330 ohm series resistor, with a 3.3 k pull-up and a 470 pF capacitor to ground. Terminals 6 to 10 carry power, ground and the 1-wire line, but we do not know their order yet. Do not connect anything to terminals 6 to 10 until it is documented.
- **What the unit reports:** `GET /api/v2/device/info` shows `hardware_type` `otgw32` and the hardware mode `OT-Direct`. MQTT publishes the same as `otgw-firmware/hardware_type` and `otgw-firmware/hardware_mode`.

### 11.2 The operating modes

OT-Direct has these modes. Choose one in the web UI (Settings, OT-Direct mode; the web UI asks for a confirmation), over REST (`POST /api/v2/otdirect/mode?mode=...`) or with the command `GW=`.

| Mode | What it does | Command |
|---|---|---|
| **Gateway** (default) | Sits between the thermostat and the boiler like the PIC does: it forwards the thermostat's messages, answers some itself and applies the overrides (SAT, `CS=`, `TT=` and so on). | `GW=1` |
| **Monitor** | Forwards every message unchanged and logs everything. Nothing is overridden. | `GW=0` or `GW=M` |
| **Master** | The unit is the OpenTherm master and polls the boiler itself. A thermostat is optional: if one stays connected, the unit answers it from its cache and passes its settings to the boiler. | `GW=2` or `GW=S` |
| **Loopback** | A test mode with simulated boiler answers. No hardware is needed. | `GW=L` |

A fifth mode, **bypass** (the thermostat wired straight to the boiler through a relay), is listed in some places, but this release has no relay support (hardware version 1.1 has an optional relay, not fitted as supplied, and the firmware does not use it): choosing it falls back to gateway and `GW=P` answers `NG`. Do not plan on bypass.

The mode is stored in the setting `OTDmode` and kept across restarts. In gateway mode the unit listens for the thermostat for 5 seconds at start-up. If it hears none, it runs in master mode for that start only; the next start looks again.

### 11.3 First install and updating

- **First install over USB:** use `flash_otgw.bat` (Windows) or `./flash_otgw.sh` (Linux and macOS) from the release zip. A factory install erases the unit: its WiFi login and its settings are gone. The tool finds the USB port by itself. (The older flash guide is for the 1.x ESP8266 only.)
- **Updating over WiFi:** open `http://<device-ip>/update` and give it the release zip. Settings are kept; save them first anyway.

> **Before you update:** for 1 to 2 minutes the thermostat is not connected to the boiler, and the boiler runs on its own settings. Pick a time when nobody needs a warm house in the next few minutes.
- **Flash layout:** safeboot 960 KB, firmware 2240 KB, web files 768 KB. A unit on an older layout needs the one-time USB migration (`flash_otgw --migrate --host <ip>`).

### 11.4 How the unit finds out it is an OTGW32

The release image is one image for both configurations. At start-up, in the default setting (Board mode: Auto), the firmware looks for a PIC, up to 3 times. If a PIC answers, the unit is an OTGW Classic and that is remembered. If none answers and the board has no PSRAM, it starts OT-Direct. A module with PSRAM and no PIC answering is not treated as an OTGW32: it stays in a degraded state with the PIC link open.

You can set Board mode to OT-Direct yourself. The unit then looks for a PIC once and starts OT-Direct when there is none; this saves up to two probes at every start.

### 11.5 What the web UI shows

The web UI follows what the unit reports (`otcommandinterface`), not the board name:

- The **PIC firmware** tab (Advanced) is hidden.
- The connection map shows **OT-Direct** instead of the PIC link; the command prompt reads `OT ›` instead of `PIC ›`.
- The **OT-Direct overrides** and **OT-Direct status** panels and the OT-Direct mode setting are shown.
- Everything else is the same: Home, Monitor, SAT, Settings.


### 11.6 Optional extras that need PSRAM, and other limits

The core of the gateway does not depend on PSRAM. The list below is what is different on a board with 4 MB of flash and no PSRAM.

- **Graph history** keeps 1 hour on this board. Units with PSRAM keep a longer history (up to 7 days).
- **Bluetooth (BLE) is off on this board** because it needs PSRAM: the setting is visible but refused. A Bluetooth room sensor therefore cannot feed SAT on an OTGW32.
- **REST:** for now, at most 2 requests are answered at the same time. A third gets HTTP 503 `Retry-After: 1`. The limit was not measured for this board class.
- **Diagnostic recorder:** a small buffer of about 2 minutes instead of the large one.
- **MQTT:** a unit without extra memory sends bursts in steps; the limits are not calibrated for it yet (see Known issues in the release notes).
- **Pins:** wired Ethernet (W5500) uses SPI on GPIO 11, 12, 13, 14 and 15, and the 1-Wire input for Dallas sensors is on GPIO 4 (hardware information from an OTGW32 owner, matching the pin table in `docs/hardware/PINOUT.md`). On an OTGW32 the Dallas pin starts at GPIO 4: an installation that still holds the old default (GPIO 10, a pin the board uses itself) is moved to 4 once, and a pin you chose yourself is never changed. S0 energy meter counting stays available and needs an input pin. Pin 5 (DI) of the terminal block is connected to GPIO 5 through a 330 ohm series resistor, with a 3.3 k pull-up and a 470 pF capacitor to ground, and is a general purpose digital input; the 1-wire line (GPIO 4, with a 3.3 k pull-up and a 470 pF capacitor to ground) is intended for DS18x20 temperature sensors. The old S0 default (GPIO 12) is a W5500 pin, so on an OTGW32 the S0 pin starts at GPIO 5, the DI pad; an installation that still holds the old default is moved to 5 once, and a pin you chose yourself is never changed. Counting pulses on the DI pad has not been tested with an S0 meter yet. The S0 counter does not start on a pin the enabled 1-wire sensors use. The GPIO output is not available on an OTGW32: there is no pad for it, so the setting is hidden in the web UI and refused (the `has_output_pin` field of `GET /api/v2/device/info` is false on this board). The ESP32-S3 module itself has unused pins that are not routed to a connector.

### 11.7 What is the same as on the OTGW Classic

- **SAT** is available. It is the only controller built into the firmware on OT-Direct; without SAT the thermostat controls the boiler as usual. In gateway mode it works through the control setpoint and the CH-enable bit of the relayed status message. A capture with a live thermostat that shows the boiler following SAT on an OTGW32 is still open. (chapter 5)
- **MQTT, Home Assistant, the REST API and the web UI** work the same. Discovery is just in time, so an OTGW32 only gets entities for the OpenTherm messages it has seen.
- **Port 25238** (for OTmonitor and the Home Assistant OpenTherm Gateway integration) is emulated on OT-Direct, line by line, and is off by default. It was not tested with OTmonitor on an OTGW32. A PIC firmware upgrade through OTmonitor does not exist on this board.
- **Return temperature:** in gateway mode the unit asks the boiler for it in the slot of a supported thermostat request and answers the thermostat from its cache.

### 11.8 What has been tested, and what has not

Tested on one OTGW32 bench unit: start-up detection (OT-Direct, no PIC), loopback and master mode, SAT with the built-in simulation, MQTT and web UI work, updates over WiFi through safeboot.

Not tested on an OTGW32: a real thermostat on the bus, a real boiler in gateway or monitor mode, Ethernet, the OLED display, OTmonitor over port 25238, the USB first install, and long runs. These are what field reports will show.
