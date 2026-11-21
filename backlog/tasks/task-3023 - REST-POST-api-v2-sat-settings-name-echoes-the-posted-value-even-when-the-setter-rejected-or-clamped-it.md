---
id: TASK-3023
title: >-
  REST POST /api/v2/sat/settings/<name> echoes the posted value even when the
  setter rejected or clamped it
status: To Do
assignee: []
created_date: '2026-11-21T19:21:00Z'
labels:
  - rest
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1137000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the TASK-3011 bench (otgw-bench-tester, 21 Nov): POST room_source=foo replied 200 {status:ok,setting:room_source,value:foo} while the setting kept 'thermostat'. Generic, not 3011-specific: handleSAT() (src/OTGW-firmware/restAPI.ino ~1773-1775) builds the reply from the request value after any updateSetting() call, for every SAT setting (also clamped ones such as cycles_per_hour, valve_offset). A client (web UI, HA, scripts) then believes a value was stored that was not.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The reply carries the value the setting holds after updateSetting() (read back), or a 4xx when the value was rejected; the choice is recorded
- [ ] #2 Host or REST contract test: a rejected value (room_source=foo) and a clamped value (cycles_per_hour out of range) reply with the stored value, red on the old code
- [ ] #3 Web UI callers checked for reliance on the echoed value (otgw-webui)
<!-- AC:END -->
