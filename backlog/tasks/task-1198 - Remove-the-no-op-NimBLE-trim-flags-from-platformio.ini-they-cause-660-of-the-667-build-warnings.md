---
id: TASK-1198
title: >-
  Remove the no-op NimBLE trim flags from platformio.ini; they cause 660 of the
  667 build warnings
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-03 08:58'
updated_date: '2026-10-03 08:59'
labels:
  - build
  - ble
dependencies: []
priority: medium
ordinal: 320000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on 2026-10-03 while answering the maintainer's question about build warnings.
A full esp32-combo build (scratchpad build-1162-flagoff.log) has 667 warning lines. 660 of them are six CONFIG_BT_NIMBLE_* macros 'redefined' in 110 translation units: MAX_CONNECTIONS, MAX_BONDS, MAX_CCCDS, ATT_PREFERRED_MTU, TRANSPORT_ACL_FROM_LL_COUNT and MSYS1_BLOCK_COUNT.
Cause: platformio.ini passes nine -D flags (TASK-988 and TASK-995, lines 86-91 and 98-100), and the prebuilt framework sdkconfig.h defines the same macros with other values. NimBLE-Arduino 2.5.1 nimconfig.h includes sdkconfig.h first on ESP_PLATFORM, and its #ifndef defaults never fire, the role blocks included (nimconfig.h:180-200).
Proof that the flags do nothing: -E -dM on the real compile commands of NimBLEDevice.cpp and ble_hs.c (compile_commands.json). The effective values are MAX_CONNECTIONS 3, MAX_BONDS 3, MAX_CCCDS 8, ATT_PREFERRED_MTU 256, TRANSPORT_ACL_FROM_LL_COUNT 24 and MSYS_1_BLOCK_COUNT 12, with ROLE_PERIPHERAL, ROLE_CENTRAL and ROLE_BROADCASTER all 1. Those are the framework values, not ours.
The remaining 7 warnings sit in vendored libraries (NetApiHelpers flush(), OneWire #undef, OpenTherm volatile++, espMqttClient close(bool)) and are out of scope.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The nine CONFIG_BT_NIMBLE_* -D flags are gone from platformio.ini. The TASK-988/995 comments are replaced by one comment stating that sdkconfig.h decides these values on ESP_PLATFORM and naming the follow-up task
- [ ] #2 Preprocessor check before and after (-E -dM with the compile_commands.json commands of NimBLEDevice.cpp and ble_hs.c): the effective NimBLE values are identical
- [ ] #3 Full esp32-combo build: 0 'CONFIG_BT_NIMBLE_* redefined' warnings (was 660 of 667). The .elf text/data/bss section sizes are identical to the build before the change
- [ ] #4 python evaluate.py --quick shows no new failures
<!-- AC:END -->
