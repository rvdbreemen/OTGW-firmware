---
id: TASK-1949
title: >-
  Review PR #702: Use L/min for the DHW flow rate unit in Home Assistant
  discovery
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T20:51:00Z'
updated_date: '2026-11-14T22:10:00Z'
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
- [ ] #1 Reviewed against the HA unit list and every consumer of haUnitStr(HaUnit::l_min) (review comment on the PR)
- [ ] #2 Tests and the esp32-combo build green on the PR head
- [ ] #3 Bench: HA discovery creates the DHW flow rate sensor with unit L/min (HA log shows no unit error)
- [x] #4 Robert decides on the merge; the author is credited in #alpha-testing
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (comms): Robert merged PR #702 himself at 21:02:55Z (squash 2cc72d819 on origin/dev, author justink0k). Before that, comms posted the ack on the PR (https://github.com/rvdbreemen/OTGW-firmware/pull/702#issuecomment-6884052172) promising a build and bench result. AC4 (Robert decides the merge) is settled by the merge; the credit in #alpha-testing and AC2/AC3 (tests, combo build, HA bench with L/min) remain, now on dev.

95 (2026-11-14 22:4x): Robert merged #702 himself (squash 2cc72d819, no bump); lane-95-1848 rebased onto it, beta.257 bump covers it. Consumers of HaUnit::l_min: only the two MsgID 19 rows (MQTTHaDiscovery.cpp:930-931, both volume_flow_rate); haUnitStr() used at :2555 (sensor configs) and :2913 (degC only). Host test test/host/test_dhw_water_meter.py D2 expected 'l/min' and FAILED on the PR code (got unit=L/min; scratchpad t1949 dhw_mid.txt, 44 other cases pass, 118 mutant checks yes). D2 now expects L/min: discovery-only run 24/24 pass on 2cc72d819 code; with haUnitStr l_min set back to 'l/min' D2 FAIL (rc=1, failed=[D2]); source restored. MQTTstuff.h:142 comment updated; CHANGELOG Fixed line credits justink0k.

2026-11-14 (92): the other 'l/min' strings. docs/api/MQTT.md:249 and :752 -> 'L/min' (wt-comms b18c98996). Robert's 47f0ea37e already fixed test/host/test_dhw_water_discovery.cpp D2 (it expected 'l/min' from the real haUnitStr() and went red after #702). Left for the firmware lane wt-92-webui (needs a bump): OTGW-Core.h:392 OT map unit (log text only: AddLogf in OTGW-Core.ino), v2.js:2580 OTMSG display unit, MQTTstuff.h:127 comment.
<!-- SECTION:NOTES:END -->
