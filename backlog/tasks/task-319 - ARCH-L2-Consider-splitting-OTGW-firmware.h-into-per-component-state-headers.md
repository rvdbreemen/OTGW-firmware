---
id: TASK-319
title: '[ARCH-L2] Consider splitting OTGW-firmware.h into per-component state headers'
status: To Do
assignee: []
created_date: '2026-04-18 19:24'
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
- [ ] #1 Design proposal documented as ADR-amendment to ADR-051
- [ ] #2 Per-component state header files proposed or implemented (state_sat.h, state_otdirect.h, state_mqtt.h, ...)
- [ ] #3 OTGWSettings/OTGWState aggregates and the two globals stay in OTGW-firmware.h (ADR-044 preserved)
<!-- AC:END -->
