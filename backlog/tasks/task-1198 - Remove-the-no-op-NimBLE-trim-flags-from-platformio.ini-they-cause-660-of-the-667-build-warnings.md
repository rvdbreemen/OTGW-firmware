---
id: TASK-1198
title: >-
  Remove the no-op NimBLE trim flags from platformio.ini; they cause 660 of the
  667 build warnings
status: Done
assignee:
  - '@claude'
created_date: '2026-10-03 08:58'
updated_date: '2026-10-03 09:11'
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
- [x] #1 The nine CONFIG_BT_NIMBLE_* -D flags are gone from platformio.ini. The TASK-988/995 comments are replaced by one comment stating that sdkconfig.h decides these values on ESP_PLATFORM and naming the follow-up task
- [x] #2 Preprocessor check before and after (-E -dM with the compile_commands.json commands of NimBLEDevice.cpp and ble_hs.c): the effective NimBLE values are identical
- [x] #3 Full esp32-combo build: 0 'CONFIG_BT_NIMBLE_* redefined' warnings (was 660 of 667). The .elf text/data/bss section sizes are identical to the build before the change
- [x] #4 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed the nine CONFIG_BT_NIMBLE_* -D flags from platformio.ini (TASK-988/995: roles, connections, bonds, CCCDs, ATT MTU, ACL and MSYS pools). They never took effect: on ESP_PLATFORM, NimBLE-Arduino 2.5.1 nimconfig.h includes the framework sdkconfig.h first, so its values win. Their only output was 660 of the 667 warnings of a full build. The comment in platformio.ini now states why, and dated correction notes went into docs/evidence/task994_ble_dram_hypotheses.md and docs/research/2026-07-03-ble-memory-implementation-proposal.md. Commit d637044c6, no prerelease bump: firmware behaviour is unchanged, as AC#2 and AC#3 show.

Evidence per AC:
- AC#1: platformio.ini diff in d637044c6; no -DCONFIG_BT_NIMBLE flag is left in the compile database.
- AC#2: -E -dM with the compile_commands.json commands of NimBLEDevice.cpp and ble_hs.c gives MAX_CONNECTIONS 3, MAX_BONDS 3, MAX_CCCDS 8, ATT_PREFERRED_MTU 256, TRANSPORT_ACL_FROM_LL_COUNT 24, MSYS_1_BLOCK_COUNT 12 and roles PERIPHERAL/CENTRAL/BROADCASTER/OBSERVER all 1, identical before and after.
- AC#3: full rebuild via build.bat --target esp32-combo (362 TUs; firmware SUCCESS 4:56, filesystem SUCCESS 0:20). Warning lines went from 667 (full build log of 2026-10-02) to 7, and NimBLE 'redefined' from 660 to 0. The .elf sizes are identical before (alpha.404+1d9ae71) and after (alpha.404+d637044): text 1522573, data 529620, bss 3579765, and xtensa size -A shows no section that differs. The .ino.bin is 2028992 B in both.
- AC#4: python evaluate.py --quick: 71 passed, 0 warnings, 0 failed.

The 7 remaining warnings are in vendored libraries (NetApiHelpers flush() deprecated x2, OneWire #undef extra tokens x2, OpenTherm volatile++, espMqttClient AsyncClient::close(bool)) and stay as they are. Making the observer-only trims work is TASK-1199.
<!-- SECTION:FINAL_SUMMARY:END -->
