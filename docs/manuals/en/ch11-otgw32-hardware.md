## Chapter 11: The OpenTherm Gateway OTGW32

---

Firmware 2.0 supports exactly two hardware configurations:

1. **The OTGW Classic with an ESP32-S3 module.** The traditional gateway with the PIC co-processor and a LOLIN S3 Mini or S3 Mini Pro in the module socket (chapter 2).
2. **The OpenTherm Gateway OTGW32.** A board with an ESP32-S3 that speaks OpenTherm itself, with its own interface to the boiler and to the thermostat. There is no PIC on this board. This chapter describes it. Chapter 12 compares the two.

> **The OTGW32 is a new device and it still has to prove itself in the field.** Hardware versions 1.0 and 1.1 are very similar; version 1.1 has an optional bypass relay that is not fitted as supplied and is not used by the firmware. Firmware 2.0 supports it, but so far the firmware has only run on a bench unit, without a thermostat and without a real boiler on the bus (a simulated boiler, and master mode without a thermostat). What is not yet field-proven: the variety of boilers and thermostats, long runs, the Ethernet and OLED options on real hardware, and OTmonitor over port 25238. If you run an OTGW32, please send a capture with your report (see Chapter 7).

### 11.1 What it is

- **Processor and memory:** an ESP32-S3 with 4 MB of flash and no PSRAM (the extra memory that the LOLIN S3 Mini module has). The firmware checks for PSRAM while it runs.
- **OpenTherm:** the ESP32-S3 drives the boiler side and the thermostat side itself. The firmware calls this OT-Direct. There is no PIC, so there is no PIC firmware to update.
- **Options in the firmware:** wired Ethernet (a W5500 chip), a small OLED display and a Dallas temperature sensor input are supported by the firmware. They have not been seen on real OTGW32 hardware in our tests.
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
- **Heating warning:** on a unit that talks to the boiler directly, the link between thermostat and boiler is interrupted while the update runs, 1 to 2 minutes. The boiler then runs on its own settings. Pick a quiet moment.
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


### 11.6 Limits that come from 4 MB flash and no PSRAM

- **Graph history** keeps 1 hour. Units with PSRAM keep a longer history (up to 7 days).
- **Bluetooth (BLE) cannot be switched on** without PSRAM: the setting is visible but refused. A Bluetooth room sensor therefore cannot feed SAT on an OTGW32.
- **REST:** for now, at most 2 requests are answered at the same time. A third gets HTTP 503 `Retry-After: 1`. The limit was not measured for this board class.
- **Diagnostic recorder:** a small buffer of about 2 minutes instead of the large one.
- **MQTT:** a unit without extra memory sends bursts in steps; the limits are not calibrated for it yet (see Known issues in the release notes).
- **Pins:** wired Ethernet (W5500) uses SPI on GPIO 11, 12, 13, 14 and 15, and the 1-Wire input for Dallas sensors is on GPIO 4 (hardware information from an OTGW32 owner, matching the pin table in `docs/hardware/PINOUT.md`). The firmware defaults for the Dallas sensor (GPIO 10), the S0 counter (GPIO 12) and the GPIO output (GPIO 16) are pins the board uses itself, so the firmware refuses the Dallas default and does not start sensors until you choose a pin: set the Dallas pin to 4. Which header pins are free for the S0 counter and the output is not documented yet.

### 11.7 What is the same as on the OTGW Classic

- **SAT** is available. It is the only controller built into the firmware on OT-Direct; without SAT the thermostat controls the boiler as usual. In gateway mode it works through the control setpoint and the CH-enable bit of the relayed status message. A capture with a live thermostat that shows the boiler following SAT on an OTGW32 is still open. (chapter 5)
- **MQTT, Home Assistant, the REST API and the web UI** work the same. Discovery is just in time, so an OTGW32 only gets entities for the OpenTherm messages it has seen.
- **Port 25238** (for OTmonitor and the Home Assistant OpenTherm Gateway integration) is emulated on OT-Direct, line by line, and is off by default. It was not tested with OTmonitor on an OTGW32. A PIC firmware upgrade through OTmonitor does not exist on this board.
- **Return temperature:** in gateway mode the unit asks the boiler for it in the slot of a supported thermostat request and answers the thermostat from its cache.

### 11.8 What has been tested, and what has not

Tested on one OTGW32 bench unit: start-up detection (OT-Direct, no PIC), loopback and master mode, SAT with the built-in simulation, MQTT and web UI work, updates over WiFi through safeboot, and memory and MQTT measurements without PSRAM.

Not tested on an OTGW32: a real thermostat on the bus, a real boiler in gateway or monitor mode, Ethernet, the OLED display, OTmonitor over port 25238, the USB first install, and long runs. These are what field reports will show.
