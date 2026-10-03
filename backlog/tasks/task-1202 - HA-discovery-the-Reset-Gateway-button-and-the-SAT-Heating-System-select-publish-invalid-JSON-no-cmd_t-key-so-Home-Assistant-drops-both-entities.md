---
id: TASK-1202
title: >-
  HA discovery: the Reset Gateway button and the SAT Heating System select
  publish invalid JSON (no cmd_t key), so Home Assistant drops both entities
status: To Do
assignee: []
created_date: '2026-10-03 17:54'
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
- [ ] #1 Both discovery payloads parse as JSON and carry cmd_t with the same command topic as before
- [ ] #2 A host test validates the JSON of every discovery composer (or at least these two) and fails on the old code
- [ ] #3 On the laptop rig after a republish: no 'Unable to parse JSON' warning for either config, and Home Assistant shows the Reset Gateway button and the SAT Heating System select
- [ ] #4 Build green for esp32, esp32-classic and esp32-combo; python evaluate.py --quick shows no new failures
<!-- AC:END -->
