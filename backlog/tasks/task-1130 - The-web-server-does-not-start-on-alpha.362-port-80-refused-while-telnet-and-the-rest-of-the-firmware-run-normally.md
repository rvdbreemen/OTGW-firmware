---
id: TASK-1130
title: >-
  The web server does not start on alpha.362: port 80 refused while telnet and
  the rest of the firmware run normally
status: To Do
assignee: []
created_date: '2026-09-05 19:08'
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
