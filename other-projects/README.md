# other-projects

This directory contains reference implementations, related tools, and upstream source code
used for cross-referencing, porting, and auditing the OTGW-firmware project.

None of these projects are part of the OTGW-firmware build. They are included for
study and comparison purposes only.

---

## otgw-6.6

**OpenTherm Gateway PIC firmware — version 6.6**

The original firmware that runs on the PIC16F1847 microcontroller inside the NodoShop
OpenTherm Gateway hardware. This is the reference implementation for all PIC-specific
command behaviour (TT=, GW=, BS=, PR=, etc.), OpenTherm message handling, and the
command protocol that the ESP8266 firmware speaks to the PIC over serial.

| | |
|---|---|
| **Author** | Schelte Bron |
| **Source** | https://otgw.tclcode.com/download/otgw-6.6.tgz |
| **Homepage** | https://otgw.tclcode.com/ |
| **Language** | PIC16F1847 Assembly (GPASM / gputils) |
| **Build tool** | GNU Make + Tcl (tclsh) |
| **License** | Custom open-source (see `license.txt`) — permits use/modify/distribute; Schelte Bron retains copyright |

---

## otgwmcu

**NodeMCU/ESP8266 WiFi bridge firmware for the OTGW**

Firmware for the ESP8266 module in the NodoShop WiFi version of the OpenTherm Gateway.
Serves two purposes: (1) recovery tool to reflash the PIC after a failed OTA update, and
(2) serial-to-network proxy that allows OTmonitor to connect over TCP/IP.

| | |
|---|---|
| **Author** | Schelte Bron |
| **Source** | https://otgw.tclcode.com/otgwmcu.html |
| **Homepage** | https://otgw.tclcode.com/ |
| **Language** | Arduino C/C++ (ESP8266 / NodeMCU) |
| **License** | MIT — Copyright (c) 2021 Schelte Bron |

---

## otmonitor-6.6

**OTmonitor — desktop monitoring tool for the OTGW**

A graphical (Tcl/Tk) and command-line tool for interacting with the OpenTherm Gateway.
Supports real-time monitoring of OpenTherm frames, sending gateway commands, reading
sensor values, and flashing PIC firmware. The reference tool for the OTGW ecosystem.

| | |
|---|---|
| **Author** | Schelte Bron |
| **Source** | https://otgw.tclcode.com/ |
| **Homepage** | https://otgw.tclcode.com/ |
| **Language** | Tcl/Tk |
| **Build / run** | tclkit or tclsh; Docker image available (see `Dockerfile`) |
| **License** | GNU LGPL v3 |

---

## OT-Thing-OTGW32

**OT-Thing firmware — OTGW32 hardware variant**

OT-Thing is a compact WiFi ↔ OpenTherm interface that acts as both OT master and slave
without a separate PIC microcontroller. This directory contains the firmware variant
targeting the OTGW32 hardware (ESP32-S3 based), including Home Assistant auto-discovery
via MQTT, a REST API, and direct OpenTherm GPIO control via hardware timer and ISR.

This is the primary reference for the OTGW32 port in this project. The `thermo-nova`
branch of the SAT Python project is ported into this firmware.

| | |
|---|---|
| **Author** | Seegel Systeme |
| **Source** | https://github.com/OTGW32/OT-Thing |
| **Homepage** | https://www.seegel-systeme.de/2025/01/05/ot-thing-das-universelle-wifi-opentherm-interface/ |
| **Community** | https://community.home-assistant.io/t/ot-thing-an-opentherm-wifi-gateway-with-integrated-ot-master-slave/824667 |
| **Language** | C/C++ (ESP32-S3, Arduino / PlatformIO) |
| **License** | Mixed: hardware CC BY-SA 4.0, firmware GPL v3, docs CC BY 4.0 — Copyright (c) 2025 Seegel Systeme |

---

## OT-Thing-Pkunfazir

**OT-Thing firmware — Phunkafizer fork with PCB design**

A fork of the OT-Thing project by Phunkafizer, with its own PCB design files and
hardware variant. Includes schematics and a Home Assistant dashboard. Used as an
additional reference for hardware design and firmware differences.

| | |
|---|---|
| **Author** | Seegel Systeme (original), Phunkafizer (fork) |
| **Source** | https://github.com/Phunkafizer/OT-Thing |
| **Homepage** | https://www.seegel-systeme.de/2025/01/05/ot-thing-das-universelle-wifi-opentherm-interface/ |
| **Language** | C/C++ (ESP32, Arduino / PlatformIO) |
| **License** | Mixed: hardware CC BY-SA 4.0, firmware GPL v3, docs CC BY 4.0 — Copyright (c) 2025 Seegel Systeme |

---

## SAT-releases-thermo-nova

**Smart Autotune Thermostat — Home Assistant custom component (`thermo-nova` branch)**

SAT is a Home Assistant custom component that implements a PID-based self-tuning
thermostat for OpenTherm boilers. It controls flow temperature, modulation, and
heating curves, and integrates with climate entities. The `thermo-nova` branch is the
reference for the SAT feature port into the OTGW-firmware project — the PI controller,
weather-compensated heating curve, flame ratio tracking, and related settings are ported
from here into the C++ firmware.

| | |
|---|---|
| **Author** | @Alexwijn (Alexwijn) |
| **Source** | https://github.com/Alexwijn/SAT |
| **Language** | Python (Home Assistant custom component) |
| **Distribution** | HACS (Home Assistant Community Store) |
| **License** | GNU GPL v3 |

---

## Archive files

| File | Contents |
|------|----------|
| `otgw-6.6.tgz` | Source tarball of `otgw-6.6` (original upstream download) |
| `OT-Thing-master.zip` | Archive of the original OT-Thing master branch |
| `OT-Thing-OTGW32.zip` | Archive of the OT-Thing OTGW32 variant |
