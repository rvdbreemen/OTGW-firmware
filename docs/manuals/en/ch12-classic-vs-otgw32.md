## Chapter 12: Differences between an OTGW Classic with PIC and an OTGW32

---

Firmware 2.0 supports two hardware configurations: the **OTGW Classic with an ESP32-S3 module** (with a PIC co-processor) and the **OpenTherm Gateway OTGW32** (no PIC, OT-Direct). Most of what you do is the same. This chapter lists what differs, so you know which parts of this manual apply to your unit. Chapter 11 describes the OTGW32 in more detail.

> **The OTGW32 is a new device and still has to prove itself in the field.** Hardware versions 1.0 and 1.1 are very similar; version 1.1 has an optional bypass relay that is not fitted as supplied and is not used by the firmware. The Classic has alpha and beta runs on several units behind it; the OTGW32 has run on a bench unit only, without a thermostat and without a real boiler.

### 12.1 Side by side

| | OTGW Classic with an ESP32-S3 module | OpenTherm Gateway OTGW32 |
|---|---|---|
| **Who speaks OpenTherm** | The PIC co-processor. The ESP32-S3 talks to the PIC over a serial link. | The ESP32-S3 itself (OT-Direct), with its own interface to the boiler and to the thermostat. |
| **PIC firmware and its updates** | Yes. Update it from the web UI (Advanced, PIC firmware). Never over WiFi with OTmonitor. | There is no PIC. The PIC firmware tab is hidden. |
| **Serial bridge, port 25238** | Passes the PIC's serial stream. Off by default. While SAT is on it filters control commands. | Emulated line by line. Off by default. Not tested with OTmonitor on an OTGW32. |
| **OTmonitor** | OTmonitor connects over port 25238. | USB is not an OTmonitor connection. Opening the USB port from a program can reset the unit, depending on how the program sets the USB control lines; this was not tested with OTmonitor. |
| **Operating modes** | The PIC decides (gateway behaviour). You steer it with PIC commands. | Gateway (default), monitor, master, loopback (chapter 11.2). |
| **Flash and memory** | 4 MB flash. The LOLIN S3 Mini and S3 Mini Pro have extra memory (PSRAM). | 4 MB flash, no PSRAM. |
| **Graph history** | Up to 7 days on a module with PSRAM. | 1 hour. |
| **Bluetooth (BLE) room sensor** | Yes, on a module with PSRAM. | Cannot be switched on without PSRAM. |
| **REST requests at the same time** | At most 2 for now. | At most 2 for now; not measured for this board. |
| **Ethernet and OLED** | OLED optional. No Ethernet. | Ethernet (W5500) and OLED are supported by the firmware; not seen on real hardware in our tests. |
| **Start-up detection** | Finds the PIC and remembers that. | Finds no PIC and starts OT-Direct. Up to 3 PIC probes at every start while Board mode is Auto. |
| **Updating over WiFi** | The PIC keeps passing messages (not measured on a bench). | The thermostat to boiler link is interrupted for 1 to 2 minutes; the boiler runs on its own settings. |
| **Free header pins** | As on the Classic board. | The S0, output and Dallas defaults clash with pins the board uses, and no free pin is confirmed (the terminal block has a digital-input pad DI whose use is not confirmed); set the Dallas pin to 4 (chapter 11.6). |
| **Field experience** | Alpha and beta runs on several units. | New device, hardware versions 1.0 and 1.1. |

### 12.2 What is the same

- The **web UI**, the **REST API**, **MQTT** and **Home Assistant** discovery.
- **SAT** (the Smart Autotune Thermostat) works on both. On the OTGW32 it is the only controller built into the firmware; without SAT the thermostat controls the boiler as usual.
- The **settings**, the **update page** (`/update`) and the **safeboot** recovery.
- The **one firmware image** for both: the release zip for `esp32-combo`. The firmware finds out at start-up which one it runs on.

### 12.3 What to read in this manual

- **OTGW Classic:** chapters 1 to 10 as written. Skip chapter 11.
- **OTGW32:** chapters 1 to 10, with these exceptions: everything about the PIC (PIC firmware, PIC commands such as `PR=`, the `otgw-pic/` MQTT topics, the PIC reset button in Home Assistant) does not apply, because there is no PIC. Read chapter 11 for the OT-Direct modes.
- **Reporting a problem:** say which of the two you have. The device info shows it as `hardware_type`: `otgw-classic` or `otgw32`.
