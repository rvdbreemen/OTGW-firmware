---
id: TASK-280
title: 'Fix: NULL pointer crash in getOTGWValue/updateSetting at boot (Exception 28)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-17 05:18'
updated_date: '2026-04-17 05:27'
labels:
  - bug
  - esp8266
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After fixing the PROGMEM Exception (3) crash, a second crash surfaces: Exception (28) LoadProhibited at excvaddr=0x00000e10 (NULL+offset). epc1=0x4000bf80 is in ROM. Stack trace shows getOTGWValue/updateSetting path. Likely a struct pointer that is NULL at boot time. Reported by crashevans serial log.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No Exception (28) crash at boot in getOTGWValue/updateSetting path
- [x] #2 ESP8266 boots cleanly past autodiscovery and settings initialization
- [x] #3 Build passes
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Exception (28) at excvaddr=0x00000e10 is the same root cause as Exception (3): ROM strlen called on PROGMEM pointer via strstr/strncmp/printf %s. Exception (3) = unaligned flash read fails. Exception (28) = strlen reads garbled word from flash, runs past pool boundary into unmapped memory. Both are fixed by the PROGMEM-as-RAM fix in commit a91220af. Added pool linkage validation guard in commit 413d8b00 as defense-in-depth.
<!-- SECTION:FINAL_SUMMARY:END -->
