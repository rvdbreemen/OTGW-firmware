---
id: TASK-1202
title: >-
  HA discovery: the Reset Gateway button and the SAT Heating System select
  publish invalid JSON (no cmd_t key), so Home Assistant drops both entities
status: Done
assignee:
  - '@claude'
created_date: '2026-10-03 17:54'
updated_date: '2026-10-04 07:20'
labels:
  - ha-discovery
  - bug
  - mqtt
dependencies: []
priority: high
ordinal: 324000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by a TASK-1201 verifier and confirmed 2026-10-03 on the laptop rig (alpha.404) by three sources:
- Code: MQTTHaDiscovery.cpp writes ctx.mqttSubTopic straight after the name field's comma, with no "cmd_t":" key in front (near :3467 for the select, near :3534 for the button). The other cmd_t writers (for example near :3131) are correct.
- Broker: the retained homeassistant/button/<node>/resetgateway/config fails json.loads at char 153, and .../select/<node>/sat_heating_system/config fails at char 291 (..."name":"Reset Gateway",OTGW/set/...).
- Home Assistant log: 'Unable to parse JSON resetgateway' and 'Unable to parse JSON sat_heating_system'.
The Reset Gateway button and the SAT heating-system select therefore never appear in Home Assistant.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Both discovery payloads parse as JSON and carry cmd_t with the same command topic as before
- [x] #2 A host test validates the JSON of every discovery composer (or at least these two) and fails on the old code
- [x] #3 On the laptop rig after a republish: no 'Unable to parse JSON' warning for either config, and Home Assistant shows the Reset Gateway button and the SAT Heating System select
- [x] #4 Build green for esp32, esp32-classic and esp32-combo; python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-10-04 on dev (alpha.407+e2bd97d):
- AC#1 and AC#2: python test/host/test_ha_discovery_json.py --old-rev 8795bacc0 gives RESULT: PASS. On OLD both composers give invalid JSON on every payload (select char 291, button char 153, the same positions the rig broker showed). The FIX changes only these two payloads and keeps their command topic. All 10326 payloads parse.
- AC#3: alpha.407 flashed to the OTGW32 bench, rig HA in the laptop's Docker. The retained configs are valid JSON with cmd_t OTGW/set/otgw-1020BA21B4F8/resetgateway and .../sat/heating_system. In the last 10 minutes HA logged 0 'parse JSON' messages. HA created both entities, button.opentherm_gateway_otgw_reset_gateway (07:06:01Z) and select.opentherm_gateway_otgw_sat_heating_system (07:05:52Z), right after the flash; they did not exist before. The remaining 'already exists' errors in the log are TASK-1201, whose fix is not on dev yet.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reset Gateway button and SAT Heating System select now publish valid discovery JSON: streamButtonDiscovery and streamSatSelectDiscovery wrote the cmd_t value without its "cmd_t":" key prefix, so Home Assistant dropped both entities. New host harness test/host/test_ha_discovery_json.py compiles the real MQTTHaDiscovery.cpp against a thin shim and parses every composer's payload (10326), failing on the old revision exactly at the two composers. Evidence: AC#1/#2 harness PASS old-vs-fix; AC#3 rig HA created both entities right after the alpha.407 flash with 0 parse errors; AC#4 esp32, esp32-classic, esp32-combo SUCCESS (alpha.407), evaluate.py --quick 71 passed 0 failed.
<!-- SECTION:FINAL_SUMMARY:END -->
