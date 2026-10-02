---
id: TASK-1130
title: >-
  The web server does not start on alpha.362: port 80 refused while telnet and
  the rest of the firmware run normally
status: Done
assignee:
  - '@claude'
created_date: '2026-09-05 19:08'
updated_date: '2026-10-02 05:09'
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
- [x] #1 The reason the listener is never created is identified from evidence, not inference
- [x] #2 GET /api/v2/device/info answers on a freshly flashed and provisioned bench board
- [x] #3 Whatever start-order or gate caused this is covered so it cannot regress silently
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

2026-10-02 FIX run on the bench, OTGW32 COM4, MAC 10:20:BA:21:B4:F8.

Setup:
- Flashed app-only with flash_otgw.bat --update --app: OTGW-firmware-esp32-combo-2.0.0-alpha.397+0fdb1e5.ino.bin. One write at 0x10000, hash verified.
- The image on the chip was confirmed by reading the app partition: version string 2.0.0-alpha.397+0fdb1e5. The old image was 2.0.0-alpha.377+ef84b59, the no-fix image.
- The board was in its config portal (no stored credentials), so this boot is a real provisioning boot.
- Provisioned by Claude at the maintainer's explicit request: bin/provision-wifi-ap.py, /wifisave answered 200.

Timeline:
- Board on the LAN 06:50:07, at 192.168.88.61 on KeepOut2, RSSI -74 dBm.
- Port 80 had no listener until 06:51:37.9. Telnet then logged 'handleWebserverListener(): HTTP Server: listening on port 80 after 21 retries'.
- Port 80 opened at 06:51:38.6.
- GET /api/v2/device/info answered HTTP 200 with fwversion 2.0.0-alpha.397+0fdb1e5.

Same boot as the portal:
- The telnet header said 'Boot: Software reset', 'Up 00:00'.
- state.uptime.iSeconds counts only from loop() (doTaskEvery1s, SKIP_MISSED_TICKS), so the blocking startConfigPortal() time in setup() is not in it.
- 'Software reset' is therefore the previous reset, the 240 s portal-timeout restart (networkStuff.ino:226-230). The provisioning ran inside this boot and setup() continued into startWebserver().

Recovery time:
- 21 retries at 5 s is about 105 s of missing listener. The listener came back about 120 s after the portal's HTTP traffic, which is 2*TCP_MSL, the TIME_WAIT lifetime.
- Caveat: provision-wifi-ap.py does not timestamp its /wifisave POST. The 120 s match therefore rests on the retry count plus the LAN-appearance time; it is not an exact delta.

Probe artifact:
- During the outage the 1 s probe reported 'timeout', not 'refused'.
- Control measurement on the device: closed port 8089 gives 'timeout' at 1.0 s but 'refused' after 2.58 s with a 5 s budget. Windows retries the SYN after an RST.
- So 'timeout' here means no listener, the same state as the OLD run's 'actively refused'.

Old-vs-fix on the same board and the same kind of boot:
- OLD (alpha.377+ef84b59, 2026-09-30): refused for more than 10 min, never recovered without a reset.
- FIX (alpha.397): listener back after 21 retries, about 105 s, with no reset.

Evidence files in %LOCALAPPDATA%/OTGW-capture:
- task1130-fix-provisioning-boot-timeline-20261002-0649.txt
- task1130-fix-provisioning-boot-telnet-20261002-0649.txt
- task1130-fix-provisioning-boot-devinfo-20261002-0651.json

AC#3: no test covers the retry yet; a host harness on the real handleWebserverListener() and its doBackgroundTasks() wiring follows.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Port 80 refused on the first boot after WiFiManager provisioning, while telnet and the rest of the firmware ran.

Cause:
- The config portal (synchronous WebServer on :80) closes each HTTP connection itself, so those connections sit in TIME_WAIT for 2*TCP_MSL = 120 s.
- startWebserver() runs seconds later in the same boot. AsyncTCP binds without SOF_REUSEADDR, so lwIP tcp_bind() returns ERR_USE.
- AsyncServer::begin() only logs that error, and AsyncWebServer::begin() is void, so no listener existed until the next reboot and nothing said so.
- 1.x is not affected: the ESP8266 core's WiFiServer::begin() sets SOF_REUSEADDR.

Fix (a26b19134, alpha.377 and later):
- startWebserver() logs when server.state() is not LISTEN after begin().
- handleWebserverListener(), fired every 5 s from doBackgroundTasks(), re-binds until LISTEN and logs 'listening on port 80 after N retries'.

Evidence per AC:

AC#1, cause from evidence:
- OLD bench, 2026-09-30 (alpha.377+ef84b59, no fix): port 80 refused for more than 10 min on the provisioning boot; a plain reset of the same image opened it within 5 s.
- FIX bench, 2026-10-02 (OTGW32, alpha.397+0fdb1e5, provisioning boot):
  - the firmware itself logged 'HTTP Server: listening on port 80 after 21 retries' at 06:51:37.9, about 105 s of missing listener;
  - that matches the 120 s TIME_WAIT from the portal's HTTP traffic.
- The provisioning ran in the same boot: state.uptime.iSeconds counts only from loop(), and 'Boot: Software reset' is the previous portal-timeout restart.
- Probe artifact: the 1 s probe's 'timeout' means 'no listener' (bug-884). Control on closed port 8089: refused only after 2.58 s.

AC#2: on that freshly flashed and provisioned board, GET /api/v2/device/info answered HTTP 200 with fwversion 2.0.0-alpha.397+0fdb1e5 at 06:51:38. Captures are in %LOCALAPPDATA%/OTGW-capture/task1130-fix-provisioning-boot-*.

AC#3, covered so it cannot regress silently:
- Runtime: a failed bind is now logged and retried; it is no longer silent.
- Host harness test/host/test_webserver_listener_retry.py (+ .cpp), on code extracted from the revision under test (the startWebserver() tail, handleWebserverListener(), the doBackgroundTasks() timer lines, safeTimers.h). Only the lwIP bind and AsyncServer::begin() are modelled.
  - Run with --old-rev a26b19134^: FIX passes L1-L5 and S1-S2. L1 reports 'after 21 retries'; L5 listens at 120.1 s for a bind possible from 120.0 s.
  - OLD fails exactly L1, L3, L4, L5, S1 and S2, and passes L2.
  - Mutants MW1-MW5 (retry call removed, early return removed, counter not reset, startWebserver check removed, 600 s period) each fail exactly their listed cases.
  - RESULT PASS, exit 0.
- Static gate check_webserver_listener_retry in evaluate.py (category Coding). It runs with --quick, which the dev push policy requires before every push.
  - On the real sources it fails at a26b19134^ (all five properties false) and passes at HEAD.
  - Five unit tests in tests/test_evaluate.py; the whole file runs 76 tests OK.
  - evaluate.py --quick: 78 checks, 0 failed, exit 0. Full run: 96 checks, 82 passed, the 4 known warnings, 0 failed, exit 0.

Build: src/ is unchanged since 0fdb1e594, the alpha.397 build that ran on the bench (build.bat --target all SUCCESS for esp32, esp32-classic and esp32-combo). This commit touches only tests, evaluate.py and records, so there is no version bump.
<!-- SECTION:FINAL_SUMMARY:END -->
