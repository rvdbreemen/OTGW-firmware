---
id: TASK-1199
title: >-
  Research: make the NimBLE observer-only trims effective on Arduino-ESP32 3.x
  (NimBLE-Arduino 2.5.1)
status: To Do
assignee: []
created_date: '2026-10-03 08:58'
labels:
  - ble
  - research
dependencies: []
priority: medium
ordinal: 321000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-988 and TASK-995 meant to trim NimBLE to a passive-scan observer: one connection, no bonds or CCCDs, MTU 23, 2 ACL buffers, 6 MSYS1 blocks, and the peripheral, broadcaster and central roles off. The goal was to free internal DRAM, about 16 KB by TASK-995's own estimate, for the static-file serve gate with BLE on (TASK-978).
The trims never took effect: the framework sdkconfig.h wins (see the cleanup task, preprocessor proof). TASK-995 was closed Done with its ACs 'trims applied, saving measured' unchecked.
The prebuilt BT controller (libbt) is compiled with the sdkconfig values. Any override has to keep the host and the controller consistent.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Where the host-side CONFIG_BT_NIMBLE_* values come from for NimBLE-Arduino 2.x on Arduino-ESP32 3.x is documented, from upstream docs or issues plus the nimconfig.h code. So is which of them the prebuilt controller depends on
- [ ] #2 Options compared, each with a named controller/host mismatch risk: an sdkconfig wrapper via #include_next, a custom framework sdkconfig rebuild, a library or upstream option, or no change
- [ ] #3 If an option is chosen, the S3 bench with BLE on and no PSRAM shows internal free heap and maxfreeblock before and after in the same scenario, and a BLE scan still reports an advertiser
<!-- AC:END -->
