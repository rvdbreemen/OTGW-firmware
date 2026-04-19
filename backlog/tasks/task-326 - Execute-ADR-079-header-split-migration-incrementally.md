---
id: TASK-326
title: Execute ADR-079 header split migration incrementally
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-19 07:38'
updated_date: '2026-04-19 17:10'
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
- [ ] #3 Continue section-by-section until OTGW-firmware.h contains only aggregates + globals + cross-cutting enums
- [ ] #4 Each PR builds cleanly on both ESP8266 and ESP32; OTGWSettings/OTGWState struct sizes remain identical
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SAT extraction completed and merged 2026-04-19: originally split into state_sat.h + settings_sat.h, then merged into a single SATtypes.h per user request. The merged form is cleaner because settings defaults reference runtime enums (SAT_HSYS_AUTO, SAT_MFR_AUTO) -- keeping them together removes an artificial cross-file dependency. ADR-079 updated to reflect the new <Component>types.h naming convention. OTGW-firmware.h shrank 1102 -> 742 lines (-33%). Both platforms build clean (ESP8266 0.76 MB, ESP32-S3 1.79 MB, unchanged).

OTDirect extracted 2026-04-19: OTDirectRequestOrigin + OTDirectMode enums, OTDirectSection state struct, OTDirectSettingsSection settings struct all moved to OTDirecttypes.h (same bundling pattern as SATtypes.h). OTGW-firmware.h: 742 -> 694 lines. Both platforms build clean (ESP8266 0.76 MB, ESP32-S3 1.79 MB, unchanged).
<!-- SECTION:NOTES:END -->
