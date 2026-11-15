---
id: TASK-1985
title: >-
  MQTT: the retained <label>/override topic keeps its last value after the
  override ends (HA keeps seeing it)
status: To Do
assignee: []
created_date: '2026-11-15T20:58:00Z'
labels:
  - mqtt
  - bug
dependencies: []
priority: medium
ordinal: 1013000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the TASK-1812 bench (09, .61, beta.328, 2026-11-15): OTGW/value/<id>/TSet/override = 40.00 stays retained on the broker after CS=0 and after a master-mode switch emptied the overrides. Cause (verified in code): publishActiveOverrides() (OTGW-Core.ino, ~:2860) publishes each active entry retained, and skips an entry once OVERRIDE_ACTIVE_TIMEOUT has passed; nothing publishes a clear (empty retained payload or a 'none' value), so the broker keeps the last value. Applies to both boards (the store is fed by the decode path). Fix direction: when an entry goes inactive (timeout, CS=0, reset), publish one empty retained message on its topic and free the slot; check HA discovery for the matching entity behaviour.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host test on the real publishActiveOverrides()/store: an override that ends (timeout and explicit clear) produces exactly one empty retained publish on its topic; red on the current code, green on the fix
- [ ] #2 No clear for an override that never was published; no repeated clears
- [ ] #3 Bench: after CS=40 then CS=0 the broker holds no retained <label>/override (mosquitto_sub -R / retained check)
<!-- AC:END -->
