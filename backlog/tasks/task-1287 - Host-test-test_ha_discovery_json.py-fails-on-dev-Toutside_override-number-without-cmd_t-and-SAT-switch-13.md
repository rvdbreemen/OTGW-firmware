---
id: TASK-1287
title: >-
  Host test test_ha_discovery_json.py fails on dev: Toutside_override number
  without cmd_t and SAT switch 13
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T02:04:00Z'
updated_date: '2026-10-22T02:30:00Z'
labels:
  - mqtt
  - ha
  - test
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 415000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1276 (2026-10-22): `python test/host/test_ha_discovery_json.py` compiles the real MQTTHaDiscovery.cpp and prints RESULT: FAIL on a clean HEAD worktree as well, so it is pre-existing and not caused by TASK-1276. The failing parts reported: the Toutside_override number entity has no cmd_t, and SAT switch 13 (overshoot_protection, added in TASK-1268) trips the gate. Sensor payloads all pass. Find whether the test or the discovery payload is wrong, fix it, and make sure the host test is run somewhere (it is not part of evaluate.py or the usual test runs, which is why it went unnoticed).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 test/host/test_ha_discovery_json.py prints RESULT: PASS on dev, with the root cause of each failure recorded (test vs payload)
- [x] #2 A broker discovery capture shows the Toutside_override number and SAT switch 13 payloads valid for HA (cmd_t present where HA needs it)
- [x] #3 The host test is part of a documented test run (evaluate.py gate or the pr-checklist) so a regression shows up
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 root cause (main session): only ONE real failure. The host test looped SAT switches 0..13 and expected 13 to be out of range (s <= 12), but streamSatSwitchDiscovery has 16 cases (13 overshoot_protection TASK-1268, 14 follow_thermostat TASK-1259, 15 external_temp TASK-1284), so 13 'unexpectedly' published and 14/15 were never exercised. The 'Toutside_override number without cmd_t' lines were the M1 mutant section (M1 renames cmd_t on purpose; its verdict line 'the gate flags exactly streamNumberDiscovery' says yes), misread by the TASK-1276 agent as a FIX failure. Fix: constexpr SAT_HA_SWITCH_COUNT = 16 in MQTTstuff.h, used by the discovery loop in MQTTstuff.ino and by the host test (0..COUNT, COUNT out of range), so the test follows the firmware. Evidence: before RESULT: FAIL (4 unexpected, switch=13); after RESULT: PASS exit 0, all verdict lines yes; mutant COUNT=15 -> exit 1, unexpected switch=15. Broker capture (alpha.450+b84a0f4 on .88.61, same payloads as this constant-only change): number Toutside_override cmd_t OTGW/set/otgw-1020BA21B4F8/outside; switch sat_overshoot_protection_enable cmd_t .../sat/overshoot_protection, stat_t .../sat/overshoot_protection_enable (retained value true seen on the broker); 16 switch configs. pr-checklist MQTT/HA section now lists the host test (+ test_dhw_water_meter.py on sensor-table changes). Evidence %TEMP%\hadisc-run.txt, hadisc-fix.txt, hadisc-mut.txt, bench-1287\disc.txt.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The HA discovery host test failed on dev because it hard-coded 13 SAT switches while the firmware has 16 (overshoot_protection, follow_thermostat and external_temp were added since), so switch 13 published 'unexpectedly' and 14/15 were never tested. The reported Toutside_override cmd_t failure was not real: those lines come from the M1 mutant section, which renames cmd_t on purpose. Fix: one constant SAT_HA_SWITCH_COUNT (MQTTstuff.h) used by the discovery loop and the host test. AC1: RESULT FAIL before, PASS after (exit 0, all verdict lines yes); mutant COUNT=15 turns it red. AC2: broker capture shows Toutside_override with cmd_t .../set/.../outside and switch 13 with cmd_t and a published retained state topic, 16 switch configs. AC3: pr-checklist MQTT/HA section runs the host test. Gates: evaluate --quick 72/0, build.bat esp32-combo SUCCESS. Not done: wiring the host test into evaluate.py (it compiles with MSVC and takes minutes, so it stays a checklist item).
<!-- SECTION:FINAL_SUMMARY:END -->
