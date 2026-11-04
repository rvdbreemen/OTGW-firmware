---
id: TASK-1514
title: >-
  SAT page: rename the max flow setpoint card to 'Central Heating Settings' and
  add a Max relative modulation slider (0-100 %)
status: Done
assignee: []
created_date: '2026-10-30T15:16:00Z'
updated_date: '2026-10-30T16:34:00Z'
labels:
  - sat
  - webui
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 670000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 153 (msg 1565737668627660861). The card from TASK-1373 is renamed 'Central Heating Settings' and gets a Max relative modulation slider 0-100 % that edits the existing setting satmaxmodulation (settings.sat.iMaxRelModulation, default 100, thermo-nova const.py:148), which already caps SAT's MM= (sent every 5 s since TASK-1370). Same build as the max flow setpoint slider: HA number entity + MQTT set topic, REST, persisted, applied at once; no 'not chosen' state (100 = no cap). UI-only plus HA/MQTT exposure; no change to control behaviour.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT page card titled 'Central Heating Settings' with the max flow setpoint slider and a 0-100 % max relative modulation slider (browser screenshot before/after, webui test)
- [x] #2 Moving the modulation slider changes satmaxmodulation and the next MM= carries it (bench telnet capture)
- [x] #3 HA number entity + MQTT set topic for max relative modulation, docs/api/MQTT.md updated
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented (sonnet lane, integrated on alpha.557 as alpha.558): card 'Central Heating Settings' with satModSlider 0-100 (existing setting satmaxmodulation via REST sat/settings/max_modulation, existing MQTT set/state topic sat/max_modulation), new HA number entity sat_max_modulation (streamSatNumberDiscovery index 1), MQTT.md and CHANGELOG. tests/webui/sat-max-modulation.test.mjs 7/7 (old v2.js: 1 pass 6 fail); tests/test_sat_max_modulation_1514.py 5 OK (source-text checks only); full suite 70/71 (float roundtrip pre-existing); evaluate EXIT 0. Before screenshot alpha.556: card 'Max flow setpoint', one slider.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
alpha.558 (191e2f32a, pushed, zip msg 1565765628441395201). SAT card renamed 'Central Heating Settings' with a Max relative modulation slider 0-100 % on the existing setting satmaxmodulation (caps every MM= SAT sends); new HA number entity sat_max_modulation (streamSatNumberDiscovery index 1) on the existing MQTT topic sat/max_modulation; MQTT.md, CHANGELOG. Evidence: AC1 headless-Edge screenshots on .63: before (alpha.556) card 'Max flow setpoint' with one slider; after (alpha.558) card 'Central Heating Settings' with max flow 70 C and max relative modulation 60 %; tests/webui/sat-max-modulation.test.mjs old 1/7, new 7/7. AC2 bench telnet CmdQueue: after POST satmaxmodulation 60 every 5 s MM=60 (before: MM=100). AC3: discovery code + docs, tests/test_sat_max_modulation_1514.py 5 OK (source-text), full suite 70/71 (pre-existing float roundtrip), evaluate EXIT 0. NOT verified: the HA number discovery payload on a live broker (bench broker 192.168.88.32 offline this session); OT-Direct.
<!-- SECTION:FINAL_SUMMARY:END -->
