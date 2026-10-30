---
id: TASK-1496
title: REST and docs still follow the pre-TASK-1485 SAT contract
status: To Do
assignee: []
created_date: '2026-10-30T00:10:00Z'
labels:
  - sat
  - mqtt
  - rest
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 654000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1485 close-out (sat/* MQTT contract cleanup, c00c5f025). Checked in the code at origin/dev f406d6890; the line numbers in the original brief were swapped (command at ~1689, health at ~1000-1056).

REST differences with the new MQTT contract:
1. Command name: restAPI.ino:1689-1690 (handleSAT, POST /api/v2/sat/settings/{name}) still maps heating_curve -> SATcoefficient. MQTT renamed it to sat/heating_curve_coeff without an alias (MQTTstuff.ino:708, Robert's decision, docs/api/MQTT.md:1089). docs/api/openapi.yaml:4007 and :4022 still give heating_curve as the example; firmware-sat-spec.md:3766 lists heating_curve as the command name.
2. Health booleans in satSendHealthJSON (?detail=full): computed restAPI.ino:999-1007, emitted :1054-1056. All three are true = healthy, while MQTT is ON = problem. The meaning differs too, so inverting polarity alone does not match MQTT:
   - flame_health: REST = !bSafetyTripped (:999); MQTT = flame status STUCK_ON/STUCK_OFF/PWM_SHORT/SHORT_CYCLING, not published while INSUFFICIENT_DATA (SATcontrol.ino:3826-3835).
   - device_health: REST = status not OFF and not STALLED_IGNITION (:1000-1001); MQTT = ON while the boiler status is not evaluated yet (!bBoilerStatusKnown, SATcontrol.ino:3987-3991).
   - cycle_health: REST true = class not OVERSHOOT/UNDERHEAT/UNDERHEAT_PWM/SHORT (:1003-1006); MQTT ON = problem (SATcontrol.ino:3992ff). Not in the original brief.
   REST heating_curve_recommendation (:1051) reads the same sHeatCurveRec, now lower case (SATcycles.ino:1356-1360), so REST already follows MQTT there. restAPI.ino has no ssi / summer_simmer field (grep).
3. Docs still describe the old contract: docs/sat/firmware-sat-spec.md:1411 and :3357 (upper-case INCREASE/DECREASE/HOLD), :2420 and :3084 and :3364 (device_health ON when status OFF), :2423 and :3370 and :3403 (polarity notes), :3353 (sat/ssi), :3362 (pressure_health ON = healthy), :3414 (sat/ssi, polarity remark), :3766 (command name heating_curve); docs/sat/sat-comparison.md P7 (:842, pressure_health ON = healthy), row 48 (:639) and row 10 (:601) (device_health ON when status OFF).

A REST rename or a polarity/meaning change of the health JSON is a breaking API change: it must be named in docs/api (openapi.yaml, and the changelog/release notes). Decide per item (rename with or without alias, invert or rename the fields, align the meaning).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Per item (heating_curve command, flame_health, device_health, cycle_health) a decision is recorded in the task notes: follow the MQTT contract (name, polarity and meaning) or keep REST as is, with the reason; a REST rename or polarity change is listed as a breaking API change in docs/api/openapi.yaml and the changelog
- [ ] #2 A host test on the real restAPI.ino code (handleSAT settings dispatch and satSendHealthJSON) is red on HEAD and green on the fix for every item decided as 'follow MQTT'
- [ ] #3 docs/sat/firmware-sat-spec.md (lines listed in the description) and docs/sat/sat-comparison.md (P7, rows 10 and 48) describe the post-TASK-1485 contract: lower-case recommendation, no sat/ssi, pressure_health/device_health ON = problem, sat/heating_curve_coeff, and the REST outcome
<!-- AC:END -->
