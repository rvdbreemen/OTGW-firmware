## Chapter 12: Differences between an OTGW Classic with PIC and an OTGW32

---

Firmware 2.0 supports two hardware configurations: the **OTGW Classic with an ESP32-S3 module** (with a PIC co-processor) and the **OpenTherm Gateway OTGW32** (no PIC, OT-Direct). Most of what you do is the same. This chapter lists what differs, so you know which parts of this manual apply to your unit. Chapter 11 describes the OTGW32 in more detail.

> **The OTGW32 is new, and the firmware for it has not run in a real home yet.** Version 1.0 of the OTGW32 is the first production run. The firmware does not use a bypass relay. The Classic has alpha and beta runs on several units behind it; the OTGW32 has run on a bench unit only, without a thermostat and without a real boiler.

**Which one do you have?** After the first flash the unit tells you: the web UI has an Advanced, PIC firmware tab on an OTGW Classic and no such tab on an OTGW32, and `hardware_type` in the device info (`/api/v2/device/info`) is `otgw-classic` or `otgw32`. If you are not sure which board you have, ask in the Discord support channel before you flash: a factory install erases the unit.

### 12.1 Side by side

| | OTGW Classic with an ESP32-S3 module | OpenTherm Gateway OTGW32 |
|---|---|---|
| **Who speaks OpenTherm** | The PIC co-processor. The ESP32-S3 talks to the PIC over a serial link. | The ESP32-S3 itself (OT-Direct), with its own connections to the boiler and to the thermostat. |
| **PIC firmware and its updates** | Yes. Update it from the web UI (Advanced, PIC firmware). Never over WiFi with OTmonitor. | There is no PIC. The PIC firmware tab is hidden. |
| **Serial bridge, port 25238** | Passes the PIC's serial stream. Off by default. While SAT is on it filters control commands. | Emulated line by line. Off by default. Not tested with OTmonitor on an OTGW32. |
| **OTmonitor** | OTmonitor (a PC program for the OpenTherm Gateway) connects over port 25238. | USB is not an OTmonitor connection. Opening the USB port from a program can reset the unit, depending on how the program sets the USB control lines; this was not tested with OTmonitor. |
| **Operating modes** | The PIC decides (gateway behaviour). You steer it with PIC commands. | Gateway (default), monitor, master, loopback (chapter 11.2). |
| **Graph history** | Up to 7 days on a module with PSRAM. | 1 hour. |
| **Bluetooth (BLE) room sensor** | Yes, on a module with PSRAM. | Off on this board (needs PSRAM). |
| **Ethernet and OLED** | OLED optional. No Ethernet. | Ethernet (W5500) and OLED are supported by the firmware; not seen on real hardware in our tests. |
| **Updating over WiFi** | The PIC keeps passing messages (not measured on a bench). | The thermostat to boiler link is interrupted for 1 to 2 minutes; the boiler runs on its own settings. |
| **Field experience** | Alpha and beta runs on several units. | New device, version 1.0 is the first production run. |

### 12.1b For the curious

Technical rows: you do not need these to choose or to use your unit.

| | OTGW Classic with an ESP32-S3 module | OpenTherm Gateway OTGW32 |
|---|---|---|
| **Flash and memory** | 4 MB flash. The LOLIN S3 Mini and S3 Mini Pro have extra memory (PSRAM). | 4 MB flash, no PSRAM. |
| **REST requests at the same time** | At most 2 for now. | At most 2 for now; not measured for this board. |
| **Start-up detection** | Finds the PIC and remembers that. | Finds no PIC and starts OT-Direct. Up to 3 PIC probes at every start while Board mode is Auto. |
| **Free header pins** | As on the Classic board. | The S0, output and Dallas defaults clash with pins the board uses. S0 counting needs an input pin: terminal 5 (DI) on the terminal block is connected to GPIO 5, the 1-wire line is GPIO 4, and the S0 counter has not been tested on an OTGW32; you choose the pin in the settings. The GPIO output has no pad on this board. Set the Dallas pin to 4 (chapter 11.6). |

### 12.2 What is the same

- The **web UI**, the **REST API**, **MQTT** and **Home Assistant** discovery.
- **SAT** (the Smart Autotune Thermostat) works on both. On the OTGW32 it is the only controller built into the firmware; without SAT the thermostat controls the boiler as usual.
- The **settings**, the **update page** (`/update`) and the **safeboot** recovery.
- Both use the same release zip: download the file whose name contains `esp32-combo`. The firmware finds out by itself which of the two it is on.

### 12.3 What to read in this manual

- **OTGW Classic:** chapters 1 to 10 as written. Skip chapter 11.
- **OTGW32:** chapters 1 to 10, with these exceptions: everything about the PIC (PIC firmware, PIC commands such as `PR=`, the `otgw-pic/` MQTT topics, the PIC reset button in Home Assistant) does not apply, because there is no PIC. Read chapter 11 for the OT-Direct modes.
- **Reporting a problem:** say which of the two you have. The device info shows it as `hardware_type`: `otgw-classic` or `otgw32`.
