---
id: TASK-1196
title: >-
  Bump ESPAsyncWebServer 3.11.0 to 3.11.2 for two multipart-parser security
  fixes
status: To Do
assignee: []
created_date: '2026-10-02 20:07'
labels:
  - security
  - dependency
  - web
dependencies: []
priority: high
ordinal: 318000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
platformio.ini pins ESP32Async/ESPAsyncWebServer @ 3.11.0. The pin comment gives one reason: 3.11.1 was 5 days old at the time. Since then two high-severity advisories against the multipart/form-data parser (src/WebRequest.cpp) were published:

- GHSA-4phx-fcj6-46r4 (2026-06-06), affects <= 3.11.0, fixed in 3.11.1.
  - `_boundaryPosition` is a uint8_t, so a 256-byte boundary wraps it and the parser loops forever.
  - One crafted request from the LAN starves the CPU into a FreeRTOS watchdog reset.
- GHSA-8m8p-vhxc-jmjw (2026-06-28), affects <= 3.11.1, fixed in 3.11.2.
  - A NULL-pointer write when the closing boundary of a file part is followed by an unexpected byte.

The 3.11.1 and 3.11.2 release notes list only these parser fixes, a refactor of the first one, and CI/example changes. 3.12.x is a separate step: it needs AsyncTCP 3.5.0, where abort() runs in the caller's context, and it brings a WebSocket refactor.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 platformio.ini pins ESP32Async/ESPAsyncWebServer @ 3.11.2 and the pin comment names the two advisories
- [ ] #2 WebResponses.cpp and the AsyncTCP pin are unchanged against 3.11.0 (diff of the resolved library), so the response path the storm measured stays the same
- [ ] #3 esp32-combo build (firmware and filesystem) SUCCESS with fresh images, and evaluate.py green
- [ ] #4 On the bench, multipart uploads still work: refresh_storm.py --upload-abort 30 gives complete readbacks and a final 303, and an FSexplorer upload reads back intact
<!-- AC:END -->
