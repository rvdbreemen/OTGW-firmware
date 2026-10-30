---
id: TASK-1506
title: >-
  SAT gas consumption: split into CH and DHW consumption sensors (entry 144,
  Q69)
status: To Do
assignee: []
created_date: '2026-10-30T06:45:00Z'
labels:
  - sat sergeant-d mqtt
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 662000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-30 06:42 on Q69: 'you can have a split gas consumption sensors. CH gas consumption and DHW gas consumption' (entry 144, reading to be confirmed by him). Follow-up of TASK-1484. Today: one sat/consumption gated on boiler CH mode + flame (SATcontrol.ino:4059-4071 at 5544e8a63); thermo-nova has a single sensor (sensor.py:219-259). Proposal: keep sat/consumption for CH (CH mode bit + flame), add sat/consumption_dhw (DHW-active bit + flame), same formula min + modulation/100 * (max - min), both only when min and max > 0; HA discovery for the new topic; docs/api/MQTT.md. Do not implement before his confirmation is recorded.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's confirmation of the entry 144 reading recorded in sergeant-d-corrections.md
- [ ] #2 Host test on the real publish path: flame + CH mode -> CH value, DHW 0; flame + DHW active -> DHW value, CH 0; no flame -> both 0 (red on HEAD for the DHW topic)
- [ ] #3 HA discovery entity for sat/consumption_dhw; docs/api/MQTT.md and firmware-sat-spec.md updated
- [ ] #4 build + evaluate green
<!-- AC:END -->
