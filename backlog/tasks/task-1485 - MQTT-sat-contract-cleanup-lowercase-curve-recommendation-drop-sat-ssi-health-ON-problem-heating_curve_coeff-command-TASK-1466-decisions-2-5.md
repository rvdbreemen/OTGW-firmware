---
id: TASK-1485
title: >-
  MQTT sat/* contract cleanup: lowercase curve recommendation, drop sat/ssi,
  health ON = problem, heating_curve_coeff command (TASK-1466 decisions 2-5)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T20:30:00Z'
updated_date: '2026-10-29T23:58:00Z'
labels:
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 644000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29 (TASK-1466): (2) sat/heating_curve_recommendation all lowercase (SATcycles.ino:316,1356-1360), keep sat/curve_recommendation too; (3) remove sat/ssi (SATcontrol.ino:3663), keep sat/summer_simmer_index, empty the retained topic; (4) pressure_health ON = problem (SATcontrol.ino:3534) and device_health ON at insufficient data (thermo-nova binary_sensor.py:404-406) instead of boiler off (:3646-3648); (5) rename the MQTT command heating_curve to heating_curve_coeff (MQTTstuff.ino:708), no alias. Breaking for HA automations: MQTT.md + CHANGELOG + retire via kRetiredSatEntities where needed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host tests red on HEAD, green after, for each of (2)-(5); MQTT.md and CHANGELOG list every breaking change; retained sat/ssi emptied
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 close-out: merged on origin/dev as c00c5f025 (alpha.542). Batch build 2.0.0-alpha.542+c00c5f0 (esp32-combo from origin/dev c00c5f025, 2026-10-30 00:47, exit 0, fresh artefacts, Discord msg 1565512587494301767); integration check on the merged set: evaluate --quick 100%, 0 failed; every tests/test_sat_*.py plus tests/test_manual_sat_commands.py exit 0. Workflow journal: red OTGW_SRC_REV=a5efbf83d python tests/test_sat_mqtt_contract_1485.py Ran 15, FAILED (failures=12); green Ran 15, OK; an independent verifier reproduced both counts; all 33 tests/test_sat_*.py rc=0; evaluate --quick 79 passed / 0 failed. Fresh rerun this session on c00c5f025: Ran 15, OK. device_health is red at source level only: the wire payload is unchanged (satUpdateBoilerStatus never assigns OFF), the change makes the meaning explicit. Follow-ups outside this AC: REST still accepts heating_curve and reports device_health/flame_health as true = healthy (restAPI.ino ~1000, ~1689), a decision for Robert; docs/sat/firmware-sat-spec.md and sat-comparison.md P7 still describe the old pressure_health/device_health polarity, sat/ssi and upper-case HCR.

alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
MQTT sat/* contract cleanup (TASK-1466 decisions 2-5), merged c00c5f025, alpha.542: sat/heating_curve_recommendation lower case; sat/ssi removed and emptied via kRetiredSatEntities (sat/summer_simmer_index stays); sat/pressure_health ON = problem; sat/device_health ON = insufficient data (thermo-nova binary_sensor.py:237-239, :253-295, :401-406, device/__init__.py:44-49); MQTT command heating_curve renamed heating_curve_coeff, no alias. MQTT.md and CHANGELOG [Unreleased] list every breaking change.

Evidence per AC:
- AC#1: tests/test_sat_mqtt_contract_1485.py compiles the real functions (HCR, satUpdateBoilerStatus, publish segments, kRetiredSatEntities + clearRetiredSatEntities, kSatMqttCmds dispatch). Red on a5efbf83d: Ran 15, FAILED (failures=12); green Ran 15 OK; both reproduced by an independent verifier; rerun OK on c00c5f025 on 2026-10-30. The MQTT.md and CHANGELOG checks are in the same test. device_health is evidenced at source level only (wire payload unchanged). Build 2.0.0-alpha.542+c00c5f0 exit 0; evaluate 100%, 0 failed.
Not verified: a broker capture of the real payloads (publish helpers are stubbed in the test). Open follow-ups: REST naming and polarity (heating_curve, device_health) for Robert; stale rows in firmware-sat-spec.md and sat-comparison.md.
Status: In Progress. alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.
<!-- SECTION:FINAL_SUMMARY:END -->
