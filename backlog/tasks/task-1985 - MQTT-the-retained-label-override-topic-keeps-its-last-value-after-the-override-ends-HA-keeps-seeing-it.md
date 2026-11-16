---
id: TASK-1985
title: >-
  MQTT: the retained <label>/override topic keeps its last value after the
  override ends (HA keeps seeing it)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T20:58:00Z'
updated_date: '2026-11-16T00:07:00Z'
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
- [x] #1 A host test on the real publishActiveOverrides()/store: an override that ends (timeout and explicit clear) produces exactly one empty retained publish on its topic; red on the current code, green on the fix
- [x] #2 No clear for an override that never was published; no repeated clears
- [ ] #3 Bench: after CS=40 then CS=0 the broker holds no retained <label>/override (mosquitto_sub -R / retained check)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Within ADR-118 (store active while refreshed within OVERRIDE_ACTIVE_TIMEOUT; publication on the periodic MQTT path): when publishActiveOverrides() meets a stale entry it publishes one empty retained message on its <label>/override topic (removes it from the broker) and frees the slot; a never-published entry is freed without a clear; a failed clear is retried next round. A published flag in OTOverrideEntry_t (in the padding after kind, size unchanged). No ADR change: ADR-118 names no behaviour for expiry; this closes a gap, it does not change the decision. Timing consequence: the clear follows the ADR-118 staleness, so up to OVERRIDE_ACTIVE_TIMEOUT (10 min) + the 60 s publish timer after the last override frame, not at once after CS=0.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 d3: implemented. OTGW-Core.ino recordOTOverride() resets published when a slot takes a new id; publishActiveOverrides() (markers publishActiveOverrides) clears stale entries. tests/test_mqtt_override_retained_clear.py on the real functions + header struct/defines: 4 cases (ends -> one clear and it is the last publish; never published -> no publish; failed clear -> sent again; active -> never cleared). Old code (origin a2bfce174): 2 FAIL (ends, failed-clear), the other 2 pass as expected. Also green: test/host test_override_modlevel PASS, test_dhw_water_meter PASS (both use the store), test_mqtt_* 5 files, evaluate 87/0. Open edge (not fixed, noted): a retained override topic left from before a reboot is never cleared (the store is RAM); and a stale published slot that another id reclaims within the 60 s publish window keeps the old topic.

2026-11-16 gates before commit: test_mqtt_override_retained_clear 4/4 (old 2 FAIL), test_override_modlevel + test_dhw_water_meter PASS, test_mqtt_* 5 files, evaluate 87/0, run_tests --changed vs origin 239/241 (the 2 others re-run alone and green: test/host/test_override_reply.py 4 OK twice; test_sat_lowload_contract 31 passed alone + ll018 10 passed after the B3e harness stub). AC3 (bench) waits for 09. Coordinator 2026-11-15: keep the ADR-118 timeout, no instant clear; the reboot edge is TASK-1991.
<!-- SECTION:NOTES:END -->
