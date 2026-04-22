---
id: TASK-384
title: 'Fix: v1.3.5 bootloop on fresh flash to Wemos D1'
status: To Do
assignee: []
created_date: '2026-04-22 20:53'
updated_date: '2026-04-22 20:53'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/554'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
GitHub #554 (ArnoudPJ, 2026-04-22): A fresh Wemos D1 mini could not be flashed with 1.3.5 directly; it went into a bootloop. Workaround was to flash 1.2 first, connect to WiFi, then OTA-upgrade to latest. Maintainer asked if 1.4.1 direct-flash would also bootloop (still waiting for reporter answer). Root cause unclear without flash method + serial-during-boot logs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reporter confirms whether 1.4.1 direct-flash also bootloops on a fresh Wemos D1
- [ ] #2 Serial output or telnet log during bootloop captured
- [ ] #3 Root cause identified (partition mismatch, LittleFS init, PROGMEM alignment, or other)
- [ ] #4 Fix verified by reporter or on a fresh Wemos D1 in the lab
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Waiting for: (1) reporter answer on whether 1.4.1 direct-flash also bootloops; (2) serial output during bootloop. Maintainer already asked the question in the issue thread on 2026-04-22T16:15Z.
<!-- SECTION:NOTES:END -->
