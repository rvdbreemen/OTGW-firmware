---
id: TASK-1220
title: >-
  W5500 detection: follow the reference driver path instead of a one-shot raw
  SPI probe
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-12 16:09'
updated_date: '2026-10-12 16:50'
labels:
  - network
  - ethernet
  - otgw32
dependencies: []
priority: medium
ordinal: 340000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On the bench OTGW32 (W5500 fitted, no cable) probeW5500() reads VERSIONR 0x00 once, 2 ms after a reset pulse, on both the combo and the fixed esp32 image (boots 53, 56, 57), so bEthernetPresent stays false and loopEthernet() is gated off for the whole boot (no hot-plug). The reference (OT-Thing main.cpp:320-345, 241-269) has no separate probe: it calls Ethernet.init(driver) + Ethernet.begin(mac, 1000) and re-checks linkStatus() every 5 s. Chip verification happens inside ESP-IDF esp_eth_mac_w5500.c w5500_verify_id(), which polls VERSIONR every 10 ms for up to 100 ms because some units read 0 right after reset; the W5500Driver also drives CS HIGH before the first frame, which our probe never does. Replace probeW5500() with the reference path and derive presence from Ethernet.hardwareStatus() (EthernetHardwareFound once the driver install verified the chip; cable not needed). No recorded bench evidence exists that probeW5500() ever returned 0x04.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 probeW5500() removed; initEthernet() runs the reference sequence (SPI begin, W5500Driver 10 MHz phy 0, Ethernet.init, Ethernet.begin with 1 s DHCP timeout or static IP) and sets state.hw.bEthernetPresent from Ethernet.hardwareStatus()==EthernetHardwareFound
- [ ] #2 Old-vs-fix on the bench OTGW32 (W5500 fitted, no cable): old image reports ethernetpresent false, fix image reports ethernetpresent true in /api/v2/device/info, telnet shows the detection line; WiFi stays the active transport
- [x] #3 build.bat --target esp32-combo green (firmware) and evaluate.py --quick shows no new FAIL
- [x] #4 Device restored to its as-found state afterwards (settings intact, MQTT connected, OT-Direct running)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Ethernet.ino: delete probeW5500(); initEthernet() does ethSPI.begin(pins), deriveEthMAC, startEthernet(1000), then bEthernetPresent = Ethernet.hardwareStatus()==EthernetHardwareFound, with a DebugTf line. Switch to Ethernet only when startEthernet returned true. loopEthernet keeps its bEthernetPresent gate: without hardware the reference's linkStatus() stays Unknown forever, so the gate is behaviour-equivalent and avoids touching the driver on boards without a W5500.
2. Bump prerelease, build esp32-combo, evaluate --quick.
3. Bench: app-only flash of fix on OTGW32, capture device/info + telnet boot line; old evidence already in EVIDENCE.md Phase 4/5 (0x00, ethernetpresent false, boots 53/56/57).
4. Commit, push dev if gates green.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented (alpha.419): probeW5500() removed, initEthernet() follows OT-Thing main.cpp setup() and sets bEthernetPresent from Ethernet.hardwareStatus(). Build esp32-combo SUCCESS, evaluate --quick 71/0/0. Bench OTGW32 (W5500 fitted, no cable), app-only flash: old alpha.418 ethernetpresent=false (boot 58), fix alpha.419 ethernetpresent=false (boots 59/61/63/64). The reference-equivalent driver path does not find the chip either, so on this unit the cause is not the one-shot read or the missing CS-HIGH; hardware is the leading suspect. No-chip path verified graceful: boots, OT-Direct + MQTT up, 0/182 settings changed. AC2 open: needs a W5500 that answers (module check by maintainer) to show ethernetpresent=true. IDF error line not observable (UART0 console muted on combo, TASK-972). Evidence: %LOCALAPPDATA%/OTGW-capture/otgw32-validation-20261012/EVIDENCE.md Phase 6.

Pin-level diagnostic (temp telnet key, not committed; EVIDENCE.md Phase 7): with the module LED on, VERSIONR reads 0xFF with the ESP32 MISO pull-up and 0x00 with the pull-down at 100 kHz, 1 MHz and 8 MHz; RTR (default 0x07D0) and PHYCFGR also read nothing; RST released, CS idles high. Nothing drives MISO: no chip answers on the bus, so this bench unit cannot satisfy AC2 whatever the firmware does. Device restored to committed alpha.419+2a2e9b9, 0/182 settings changed. AC2 needs a board whose W5500 answers on SPI.
<!-- SECTION:NOTES:END -->
