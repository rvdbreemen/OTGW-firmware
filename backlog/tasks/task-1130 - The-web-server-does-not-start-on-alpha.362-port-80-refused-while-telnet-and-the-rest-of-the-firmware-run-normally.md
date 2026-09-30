---
id: TASK-1130
title: >-
  The web server does not start on alpha.362: port 80 refused while telnet and
  the rest of the firmware run normally
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-05 19:08'
updated_date: '2026-09-30 05:42'
labels:
  - bug
  - webserver
dependencies: []
priority: high
ordinal: 278000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observed on the bench ESP32-S3 (MAC ac:27:6e:ce:45:d8) at 2.0.0-alpha.362+ed79ac1, on BOTH the esp32-combo and the esp32-classic build, after a merged-full flash and headless WiFi provisioning. The device joins WiFi, answers ping, and serves telnet on port 23. The firmware is fully alive on telnet: MQTT publishing, OpenTherm frame processing, SAT BLE sensor updates, heap around 48k free and reported HEALTHY. But TCP port 80 is actively REFUSED, so no listener was ever created. Actively refused rules out the AsyncTCP wedge from ADR-139, which hangs rather than refuses, and CONFIG_ASYNC_TCP_STACK_SIZE is already 16384 in platformio.ini. Every REST and web-UI verification is blocked while this holds, including the diagnose screen port in TASK-1128.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The reason the listener is never created is identified from evidence, not inference
- [ ] #2 GET /api/v2/device/info answers on a freshly flashed and provisioned bench board
- [ ] #3 Whatever start-order or gate caused this is covered so it cannot regress silently
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
CAVEAT bij de waarneming: de ESP zat tijdens deze meting LOS van de carrier. Dat verklaart de PIC- en I2C-observaties in TASK-1129, maar het verklaart op zichzelf niet waarom poort 80 wordt geweigerd, want de webserver hoort niet van de carrier af te hangen.

Wel moet dit opnieuw gemeten worden met het bord op de carrier voordat er conclusies aan verbonden worden. Denkbaar is dat een mislukte I2C- of OLED-initialisatie de opstartvolgorde verstoort en de listener nooit wordt aangemaakt; dat zou juist een echte bug zijn, maar het is nu niet onderscheiden van een meetartefact.

Herhaal na terugplaatsen: flash, provisioneer, en test poort 80 op beide builds.

REPRODUCEERT NIET met carrier aangesloten. Na een harde herstart is poort 80 gewoon OPEN en antwoordt GET /api/v2/device/info volledig, met heap rond 51k.

De eerdere weigering trad op terwijl de ESP los van de carrier zat en de firmware in Degraded respectievelijk OT-Direct mode draaide, direct na provisioning. Of de oorzaak de ontbrekende carrier was of die specifieke boot valt uit deze waarnemingen niet te scheiden.

Voorstel: sluiten als niet-reproduceerbaar. Komt het terug, dan is het onderscheidende gegeven dat telnet blijft werken terwijl poort 80 weigert, wat betekent dat de listener nooit is aangemaakt.

2026-09-30 root cause + OLD-code evidence (bench OTGW32 COM4, MAC 10:20:BA:21:B4:F8).

Mechanism (code-level, three sources): lwIP tcp_bind() returns ERR_USE (-8) when any pcb in the listen/bound/active/TIME_WAIT lists already holds local port 80, unless the NEW pcb carries SOF_REUSEADDR (CONFIG_LWIP_SO_REUSE=y in the S3 sdkconfig). ESP32Async/AsyncTCP 3.4.10 AsyncServer::begin() never sets SOF_REUSEADDR; on a bind error _tcp_bind_api closes the pcb, begin() only logs 'bind error: %d' and returns, and AsyncWebServer::begin() is void, so the firmware never learns there is no listener. The WiFiManager config portal (sync WebServer on :80) closes each HTTP connection server-side, leaving pcbs in TIME_WAIT for 2*TCP_MSL = 120 s (CONFIG_LWIP_TCP_MSL=60000). startWebserver() runs seconds later in the same boot -> bind fails -> port 80 refused while telnet :23 (bound fresh) works. Contrast: the ESP8266 core used by 1.x sets pcb->so_options |= SOF_REUSEADDR in WiFiServer::begin(), so 1.x is not affected (dev-only fix).

OLD code on the bench (no-fix image, same alpha.377 tag, build 6935, identity checked against the build log; fix build ended at 6930):
- 07:31-07:34, first boot after provisioning (telnet 'D' dump: uptime 702 s, SSID KeepOut2, OT-Direct): telnet 23 OPEN, GET /api/v2/device/info -> 'actively refused'. Still refused ~10 min after the 120 s TIME_WAIT window: without a retry the old code never recovers.
- 07:34:26 esptool --after hard_reset (no flash, NVS kept, no portal in the boot): port 80 OPEN within 5 s, device/info answers fwversion 2.0.0-alpha.377+ef84b59. Same image, same creds: the defect is specific to the provisioning boot.
Transcript: %LOCALAPPDATA%/OTGW-capture/task1130-nofix-telnet-dump-20260930-073207.txt

Earlier TASK-961 (archived, closed without repro) is the same symptom, reproduced twice then (2026-07-05, 2026-07-06).

FIX (alpha.377): startWebserver() logs when server.state() != LISTEN after begin(); handleWebserverListener() on a 5 s timer in doBackgroundTasks() retries server.begin() until LISTEN and logs 'listening on port 80 after N retries'. Build esp32-combo OK, evaluate.py --quick 68 passed / 0 failed. Fix-run on the bench pending a user provisioning cycle (Claude does not enter WiFi credentials).
<!-- SECTION:NOTES:END -->
