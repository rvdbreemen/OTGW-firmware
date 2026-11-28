---
id: TASK-3163
title: 'SAT: read-only implied coefficient from the advisor method'
status: To Do
assignee: []
created_date: '2026-11-28T19:51:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1206000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-28 #dev-sat-mqtt 1576323915670757437 'Go' (register entry 548), after the beta. Keep our self-tune as the actor. Add a read-only implied heating-curve coefficient value on MQTT and REST, from our own hourly mean signal: 4 x mean / shape (the advisor method of Alexwijn SAT 5.1.0, heating_curve.py:95-97), shown only when the curve shape is large enough (>= 4, as the advisor does), next to the tuner's reason. No new storage, no behaviour change. Source: OTGW-field-data/_comms/curve_advisor_compare_2026-11-28.md (option 2). Not in scope: scaling our 1.0 C band (option 3, needs Sergeant D first, replay first), a 1:1 port of the advisor.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test of the pure tuner block: the implied coefficient equals 4 x mean / shape for recorded hourly means, is absent when shape < 4; red on the code without it, green with it
- [ ] #2 MQTT topic, REST field and HA discovery contract checked: one new read-only value, no change to any existing topic or setting, no new behaviour of the tuner (tuner decisions identical on the indigo_light replay)
- [ ] #3 Docs updated: manuals EN and NL, MQTT.md, openapi.yaml, the self-tune section of the SAT docs
<!-- AC:END -->
