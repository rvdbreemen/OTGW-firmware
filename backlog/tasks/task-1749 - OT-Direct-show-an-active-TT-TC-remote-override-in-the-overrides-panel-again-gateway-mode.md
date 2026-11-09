---
id: TASK-1749
title: >-
  OT-Direct: show an active TT=/TC= remote override in the overrides panel again
  (gateway mode)
status: To Do
assignee: []
created_date: '2026-11-09T09:26:00Z'
labels:
  - otdirect
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 851000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1733 / ADR-223 (found by the 2026-11-09 correctness review, otgw-firmware-80). In gateway mode TT=/TC= no longer set a write override on MsgID 16/100, so /api/v2/otdirect/overrides (getOTDirectOverridesJSON, OTDirect.ino ~1131-1139) and state.otd.iOverrideCount / overrides_active (restAPI.ino ~3910) no longer list an active remote override; the v2 overrides panel and the aRoomSet badge (v2.js ~2092-2123) then show 'No active overrides' while TT= is active. Expose otRemoteOverride (mode, value, accepted, MsgID 9 answered) as its own entry and show it in the panel, with Clear = TT=0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 GET /api/v2/otdirect/overrides lists an active TT=/TC= remote override (mode, value, whether the thermostat read MsgID 9, accepted) in gateway mode; host test
- [ ] #2 The v2 overrides panel shows it with a working Clear (TT=0); webui test red on the current UI, green after
<!-- AC:END -->
