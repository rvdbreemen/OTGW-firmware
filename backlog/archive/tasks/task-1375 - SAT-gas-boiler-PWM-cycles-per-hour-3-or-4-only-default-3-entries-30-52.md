---
id: TASK-1375
title: 'SAT: gas boiler PWM cycles per hour 3 or 4 only, default 3 (entries 30, 52)'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T07:21:00Z'
updated_date: '2026-10-30T13:03:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - TASK-1412
ordinal: 510000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 52 (2026-10-25, answer to Q16): default 3 cycles per hour for gas boilers, 2 for heat pumps. Entry 30: gas boilers use 3 or 4. Firmware today: iCyclesPerHour default 3 (SATtypes.h:548), accepted 2..6 (settingStuff.ino:1107). Change: for gas boilers accept 3 or 4 only (clamp stored values outside into range on load), UI offers 3/4; heat pump values are TASK-1366. Hybrids: open (entry 52).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Setting accepts only 3 or 4 for gas boilers; other values clamp to the nearest; default stays 3
- [x] #2 v2 UI offers 3 and 4 for gas boilers
- [x] #3 Host test shows HEAD accepts 2/5/6 and the fix rejects them
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Second-reader checked 2026-10-30 (C25-C27 confirmed). Setter settingStuff.ino:1131 -> (atoi>=4)?4:3 (covers file load, REST, MQTT; mark settings dirty when a loaded value snapped); getter SATcontrol.ino:350-353 -> (>=4)?4:3 (fallback 3); REST metadata restAPI.ino:3983 addInt 3..4; v2.js ENUM_OPTS satcyclesperhour [[3,'3 per hour (default)'],[4,'4 per hour']]; settingsHelp.h; docs MQTT.md, openapi.yaml, firmware-sat-spec.md. Heat pumps untouched. Host test: extend tests/test_sat_hp_pwm.py with the real setter (2,5,6,0,garbage,9 -> 3,4,4,3,3,4; red on HEAD) and getter period. Bench: old-vs-new stored 6 -> 4 after OTA app-only; POST 2 -> 3; MQTT 5 -> 4; screenshots of the 3/4 choice. Waiting for Sergeant D's OK (msg 1565656570702987396).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28: Q16 'please expand' (entry 68): default settled (entry 52), gas range re-asked. Q37 A (entry 82): hybrids keep gas timing, so the range covers hybrids; align the out-of-range fallback (SATcontrol.ino:354 returns 4 for radiators) with default 3.

2026-10-28 Q16 A (entry 99, msg 1564910294709571667): gas boilers 3 or 4 only, default 3, radiators and underfloor; stored 2->3, 5/6->4; hybrids follow (entry 82). Decision complete, task unblocked.

2026-10-30: Sergeant D entry 146: must be in beta 1 (TASK-1504, target Sun 2026-11-01). Priority raised to high.

Robert 2026-10-30: the TASK-1412.3 dependency is met for this task by its own host tests that are red on HEAD and green on the fix (recorded in TASK-1412.3 too); TASK-1412.3 stays open for the broader low-load regression set.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
alpha.552 (03ba9847c, pushed). Gas boilers and hybrids: cycles per hour 3 or 4 only, default 3; stored/new values snap (<=2 -> 3, >=5 -> 4) on every input path; getter fallback 3; REST metadata 3..4; v2 UI 3/4 choice. Heat pumps unchanged. Evidence: AC1+AC3 host test tests/test_sat_hp_pwm.py on the real setter/getter, 12 sub-test failures on old code, 27/27 OK on fix (re-run this session in wt-1501: Ran 27 OK). Bench .63 Classic+S3 Mini alpha.552 OTA (settings 174 keys unchanged): POST 2/5/6/4/1/3 read back 3/4/4/4/3/3, metadata min 3 max 4 (alpha.550: 2..6). AC2: device serves v2.js with satcyclesperhour options [[3,'3 per hour (default)'],[4,'4 per hour']] (grep of served asset; no browser screenshot taken). Gates: evaluate --quick EXIT 0 (86 checks, 0 FAIL); full tests 64/67, the 3 others pre-existing on dev or a stale build artefact (test_sat_mqtt_state OK after removing the generated .ino.cpp). Build esp32-combo exit 0, artefacts 13:36-13:37 +03ba984. Zip posted msg 1565712393663811664.
<!-- SECTION:FINAL_SUMMARY:END -->
