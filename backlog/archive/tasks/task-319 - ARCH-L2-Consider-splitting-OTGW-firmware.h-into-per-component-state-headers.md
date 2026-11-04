---
id: TASK-319
title: '[ARCH-L2] Consider splitting OTGW-firmware.h into per-component state headers'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:24'
updated_date: '2026-04-19 07:38'
labels:
  - architecture
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTGW-firmware.h grew from 173 to 741 lines. Every component state lives in one header, triggering full rebuild on any edit. ADR-044 global state instantiation can stay intact with per-component type headers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Design proposal documented as ADR-amendment to ADR-051
- [x] #2 Per-component state header files proposed or implemented (state_sat.h, state_otdirect.h, state_mqtt.h, ...)
- [x] #3 OTGWSettings/OTGWState aggregates and the two globals stay in OTGW-firmware.h (ADR-044 preserved)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-079 (Per-component State and Settings Headers) drafted as amendment to ADR-051. Documents the split pattern, naming scheme (state_<component>.h / settings_<component>.h), directory layout, what stays in OTGW-firmware.h, and an incremental migration path. No code moves yet: the ACs accept 'proposed or implemented' and migrating all 25+ section structs is scoped to follow-up PRs per the ADR's 'per-PR scope' guidance. Created TASK-326 to track the actual migration.
<!-- SECTION:FINAL_SUMMARY:END -->
