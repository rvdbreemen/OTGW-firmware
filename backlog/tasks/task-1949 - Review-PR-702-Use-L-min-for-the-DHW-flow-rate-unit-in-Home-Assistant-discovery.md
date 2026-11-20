---
id: TASK-1949
title: >-
  Review PR #702: Use L/min for the DHW flow rate unit in Home Assistant
  discovery
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T20:51:00Z'
updated_date: '2026-11-20T11:28:00Z'
labels:
  - github
  - pr
  - mqtt
  - homeassistant
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 995000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/pull/702 by justink0k (fork), base dev, opened 2026-11-14T20:41Z. One-line change in MQTTHaDiscovery.cpp:2138 haUnitStr(): HaUnit::l_min 'l/min' -> 'L/min'. Reported HA error: 'The unit of measurement l/min is not valid together with device class volume_flow_rate' for boiler_DHWFlowRate (MsgID 19), so the sensor is not created (firmware 2.0.0-alpha.416, HA 2026.11.1). Checked by comms: HA core const.py:653 LITERS_PER_MINUTE = 'L/min', sensor/const.py:711 VOLUME_FLOW_RATE allows set(UnitOfVolumeFlowRate) only. Other 'l/min' strings (not part of the PR): OTGW-Core.h:392 OT map unit, MQTTstuff.h:127/142 comments, docs/api/MQTT.md:241/744. The author has not built it. The claude-review run is 'action_required' (first-time contributor workflow approval).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reviewed against the HA unit list and every consumer of haUnitStr(HaUnit::l_min) (review comment on the PR)
- [x] #2 Tests and the esp32-combo build green on the PR head
- [x] #3 Bench: HA discovery creates the DHW flow rate sensor with unit L/min (HA log shows no unit error)
- [x] #4 Robert decides on the merge; the author is credited in #alpha-testing
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (comms): Robert merged PR #702 himself at 21:02:55Z (squash 2cc72d819 on origin/dev, author justink0k). Before that, comms posted the ack on the PR (https://github.com/rvdbreemen/OTGW-firmware/pull/702#issuecomment-6884052172) promising a build and bench result. AC4 (Robert decides the merge) is settled by the merge; the credit in #alpha-testing and AC2/AC3 (tests, combo build, HA bench with L/min) remain, now on dev.

95 (2026-11-14 22:4x): Robert merged #702 himself (squash 2cc72d819, no bump); lane-95-1848 rebased onto it, beta.257 bump covers it. Consumers of HaUnit::l_min: only the two MsgID 19 rows (MQTTHaDiscovery.cpp:930-931, both volume_flow_rate); haUnitStr() used at :2555 (sensor configs) and :2913 (degC only). Host test test/host/test_dhw_water_meter.py D2 expected 'l/min' and FAILED on the PR code (got unit=L/min; scratchpad t1949 dhw_mid.txt, 44 other cases pass, 118 mutant checks yes). D2 now expects L/min: discovery-only run 24/24 pass on 2cc72d819 code; with haUnitStr l_min set back to 'l/min' D2 FAIL (rc=1, failed=[D2]); source restored. MQTTstuff.h:142 comment updated; CHANGELOG Fixed line credits justink0k.

2026-11-14 (92): the other 'l/min' strings. docs/api/MQTT.md:249 and :752 -> 'L/min' (wt-comms b18c98996). Robert's 47f0ea37e already fixed test/host/test_dhw_water_discovery.cpp D2 (it expected 'l/min' from the real haUnitStr() and went red after #702). Left for the firmware lane wt-92-webui (needs a bump): OTGW-Core.h:392 OT map unit (log text only: AddLogf in OTGW-Core.ino), v2.js:2580 OTMSG display unit, MQTTstuff.h:127 comment.
AC3 bench by 09, 2026-11-14, .61 beta.257+47f0ea3 against broker 192.168.88.32: payload part PASS. RED on 252: retained homeassistant/sensor/otgw-1020BA21B4F8/boiler_DHWFlowRate/config had unit_of_measurement l/min. On 257 the config is only re-announced after the first MsgID 19 sample (ADR-182); triggered with 2 min GW=L loopback, then GW=S: the config came live and retained with device_class volume_flow_rate, unit_of_measurement L/min, state_class measurement (GREEN). NOT verified: the Home Assistant side (sensor created, HA log without the unit error): no HA instance is attached to this broker on the bench; the ha-ingress rig needs Docker. AC3 left open for the HA part.

2026-11-19 (ba, triage): origin/dev f5bce220a. git grep 'l/min' over src/ docs/api test/: 0 hits (OTGW-Core.h:392, v2.js OTMSG unit, MQTTstuff.h comments all fixed). python test/host/test_dhw_water_meter.py on f5bce220a: RESULT: PASS rc=0 (D2 expects L/min; mutants incl. MR20-22 caught). PR #702 comment by rvdbreemen verifies against HA UnitOfVolumeFlowRate (AC1). 2cc72d819 is in published tags v2.0.0-beta.294/323/339 (esp32-combo builds); .61 runs 2.0.0-beta.401 (AC2). AC3 HA side still open: no HA instance on the bench broker; needs a tester/HA confirmation that sensor.boiler_DHWFlowRate exists with L/min.

2026-11-19 coordinator plan for AC3: 09 checks the HA side on the HA-ingress rig after the beta, bundled with TASK-1780 as one HA bench item.

2026-11-20 09 interim for AC3: retained homeassistant/sensor/otgw-1020BA21B4F8/boiler_DHWFlowRate/config (beta.401+47d08b1, .61) and otgw-AC276ECE45D8 (poc.31+8461936, .63) both carry device_class volume_flow_rate, unit_of_measurement L/min, state_class measurement (OTGW-field-data/bench-otgw32/2026-11-20_task1780-ac2-broker/dhwflow-discovery-retained.txt). HA log check on the HA rig follows after 08:00.

Robert 2026-11-20, interactive in session 95 (otgw-docs), TASK-1740 AC1 waiver round: A: otgw-bench-tester finishes AC3 on the HA-ingress rig (with TASK-1780) before the release; then the known-issue line comes out of the stable notes.

2026-11-20 bench (09, otgw-bench-tester), AC3 evidence: rig HA (setup_rig.py, bench broker 192.168.88.32). Entity sensor.opentherm_gateway_marvin_otgw32_water_flow_rate_in_dhw_circuit (unique_id otgw-1020BA21B4F8-otd_DHWFlowRate, discovery published by .61 401+47d08b1, retained config origin sw 2.0.0-beta.401+47d08b1): unit_of_measurement L/min, device_class volume_flow_rate, state_class measurement, state 0.00. HA error_log: 1 line total (custom integration 'ingress' warning), 0 lines with mqtt/unit_of_measurement. Files: OTGW-field-data/bench-63/2026-11-20_ha-1780-1949/ (ha1780.log, ha_error_log.txt), OTGW-field-data/bench-otgw32/2026-11-20_task1780-ac2-broker/dhwflow-discovery-retained.txt. No HA screenshots (REST/registry + error_log as evidence, coordinator OK). .63 has no DHW flow entity (no retained config for it on the broker).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
PR #702 (HA discovery unit l/min -> L/min for the DHW flow rate). AC1: reviewed against HA's UnitOfVolumeFlowRate list (PR comment by rvdbreemen); every haUnitStr(HaUnit::l_min) consumer checked, git grep 'l/min' over src/, docs/api and test/ 0 hits. AC2: python test/host/test_dhw_water_meter.py RESULT: PASS rc=0; commit 2cc72d819 shipped in beta.294, beta.323 and beta.339 (esp32-combo builds). AC3: bench by 09 on the HA rig 2026-11-20: .61 (beta.401+47d08b1) entity water_flow_rate_in_dhw_circuit created with unit_of_measurement L/min, device_class volume_flow_rate, state_class measurement; HA error_log 0 lines about mqtt/unit_of_measurement (files OTGW-field-data/bench-63/2026-11-20_ha-1780-1949/, notes in c673e878d); retained configs for .61 and .63 also carry L/min (dhwflow-discovery-retained.txt). AC4: merged with Robert's OK, author credited in #alpha-testing (earlier notes).
<!-- SECTION:FINAL_SUMMARY:END -->
