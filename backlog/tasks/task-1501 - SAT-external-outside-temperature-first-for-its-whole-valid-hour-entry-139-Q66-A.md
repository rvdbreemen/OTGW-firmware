---
id: TASK-1501
title: >-
  SAT: external outside temperature first for its whole valid hour (entry 139,
  Q66 A)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-30T06:11:00Z'
updated_date: '2026-10-30T13:42:00Z'
labels:
  - sat sergeant-d
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 657000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-30 Q66 A (entry 139): order external sensor (MQTT/REST, valid < 1 h), MsgID 27, weather, MsgID 27 reading 0.0, restored value. The firmware still prefers the external value only for 10 minutes (SAT_STALE_OUTDOOR_MS, SATcontrol.ino:142, :1878, :1900 at 8ded885eb). Found by the coordinator 2026-10-30 after TASK-1482 was closed with a wrong 'matches the implementation' note.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real source selection: an external value 30 min old wins over MsgID 27 and weather (red on 8ded885eb), a 61 min old one does not
- [x] #2 The restored-value and MsgID 27 = 0.0 fallbacks keep their order; docs/sat/firmware-sat-spec.md updated
- [x] #3 build + evaluate green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Second-reader checked 2026-10-30 (C18, C19 confirmed). satResolveOutsideTemp() SATcontrol.ino:1888-1911: change :1900 to extSet && extAge < SAT_OUTSIDE_VALID_MS, delete :1904 and SAT_STALE_OUTDOOR_MS (:142, only user :1900; comment :1878). Validity (satOutsideTempFresh, restart gate, saved ots) unchanged. Host test tests/test_sat_outside_order.py on the real satOutside block (red on HEAD: 30 min external loses to MsgID 27/weather). Bench with simulation off: external 3.0 via POST /api/v2/sat/externaloutdoor, read sat/status outside_temp at 5/11/59/61 min. Docs: firmware-sat-spec.md, sat-comparison.md, MQTT.md/openapi wording. Waiting for Sergeant D's OK (msg 1565656570702987396).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30: Sergeant D entry 146: must be in beta 1 (TASK-1504, target Sun 2026-11-01). Priority raised to high.

Bench expiry (.63 alpha.552, sim off, weather 14.6 valid): external 3.0 pushed 12:40:21 UTC -> 13:30:05 (50 min) outside_temp 3; 13:42:33 (62 min) outside_temp 14.6 (weather). The external_outdoor_valid flag stays true; selection uses the 1 h age (SATcontrol.ino satResolveOutsideTemp).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
alpha.551 (c1a001a32, pushed). An external outside temperature (MQTT/REST) wins for its whole valid hour; order external (<1 h), MsgID 27 (non-zero), weather, MsgID 27 = 0.0, restored value; SAT_STALE_OUTDOOR_MS removed. Evidence: AC1 tests/test_sat_outside_order.py compiles the real satResolveOutsideTemp block: 3 failures on old code (30 min external lost to MsgID 27/weather), 6/6 OK on fix incl. the 61 min case (re-run this session: Ran 6 OK). Bench .63 alpha.552, simulation off, weather forecast enabled as competitor (weather_valid true, 14.4 C): external 3.0 pushed 12:40:21 UTC; at 13:01:21 (21 min) outside_temp 3 (old code switches to the forecast after 10 min). In simulation mode the sim value overrides all sources by design (SATcontrol.ino:1889), so that run was not evidence. AC2: restore/0.0 order kept (test), firmware-sat-spec.md and sat-comparison.md updated in the commit. AC3: evaluate --quick EXIT 0, build exit 0 (+03ba984). Bench 50/62 min expiry check still running at Done time; result appended in notes.
<!-- SECTION:FINAL_SUMMARY:END -->
