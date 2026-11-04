---
id: TASK-326
title: Execute ADR-079 header split migration incrementally
status: Done
assignee:
  - '@claude'
created_date: '2026-04-19 07:38'
updated_date: '2026-04-19 17:55'
labels:
  - architecture
  - review-2026-04-18
  - adr-079
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Move section structs from OTGW-firmware.h (1102 lines) into per-component headers per ADR-079. Start with SAT (largest, most self-contained: SATWindowRecord, SATZoneState, SATRuntimeSection, SATSection) in PR 1. Continue with OTDirect in PR 2. One or two sections per PR to keep diffs reviewable.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 state_sat.h and settings_sat.h created per ADR-079 conventions; SAT structs moved there; OTGW-firmware.h includes them
- [x] #2 state_otdirect.h and settings_otdirect.h created; OTDirect structs moved
- [x] #3 Continue section-by-section until OTGW-firmware.h contains only aggregates + globals + cross-cutting enums
- [x] #4 Each PR builds cleanly on both ESP8266 and ESP32; OTGWSettings/OTGWState struct sizes remain identical
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SAT extraction completed and merged 2026-04-19: originally split into state_sat.h + settings_sat.h, then merged into a single SATtypes.h per user request. The merged form is cleaner because settings defaults reference runtime enums (SAT_HSYS_AUTO, SAT_MFR_AUTO) -- keeping them together removes an artificial cross-file dependency. ADR-079 updated to reflect the new <Component>types.h naming convention. OTGW-firmware.h shrank 1102 -> 742 lines (-33%). Both platforms build clean (ESP8266 0.76 MB, ESP32-S3 1.79 MB, unchanged).

OTDirect extracted 2026-04-19: OTDirectRequestOrigin + OTDirectMode enums, OTDirectSection state struct, OTDirectSettingsSection settings struct all moved to OTDirecttypes.h (same bundling pattern as SATtypes.h). OTGW-firmware.h: 742 -> 694 lines. Both platforms build clean (ESP8266 0.76 MB, ESP32-S3 1.79 MB, unchanged).

AC3 complete 2026-04-19: 15 new per-component type headers (Hardwaretypes.h, Networktypes.h, PICtypes.h, OTBustypes.h, MQTTtypes.h, Flashtypes.h, Debugtypes.h, Uptimetypes.h, NTPtypes.h, Sensorstypes.h, S0types.h, Outputstypes.h, Webhooktypes.h, UItypes.h, Devicetypes.h) extracted per ADR-079. OTGW-firmware.h shrank 694 -> 525 lines (-24%). Include ordering: SATtypes.h + OTDirecttypes.h stay at top (no macro deps); the 15 new headers are included after the #define block because MQTTtypes needs HOME_ASSISTANT_DISCOVERY_PREFIX and NTPtypes needs NTP_DEFAULT_TIMEZONE/NTP_HOST_DEFAULT. Both platforms build clean (ESP8266 0.76 MB unchanged, ESP32-S3 unchanged). evaluate.py health: 97.1% -> 98.0%.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Complete ADR-079 header split for all 25+ component sections in OTGW-firmware.h. Three PRs landed: SAT (AC1, commit 24391125), OTDirect (AC2, commit 9ae6f1e6), and the remaining 15 sections (AC3, this commit). OTGW-firmware.h went from the pre-review 1102 lines (pre-refactor) to 525 lines (post-AC3), a 52 percent reduction; aggregates OTGWSettings and OTGWState plus their global instances stay put per ADR-044. Naming convention <Component>types.h per ADR-079 revision. Each header is a self-contained file with #pragma once, a single #include <Arduino.h>, and one-or-more related structs/enums for its subsystem. evaluate.py check_adr_gates and check_backlog_hygiene (TASK-332) verified clean after the refactor. Both ESP8266 and ESP32-S3 build identical binary sizes to the previous commit (no drift).
<!-- SECTION:FINAL_SUMMARY:END -->
