---
id: TASK-1485
title: >-
  MQTT sat/* contract cleanup: lowercase curve recommendation, drop sat/ssi,
  health ON = problem, heating_curve_coeff command (TASK-1466 decisions 2-5)
status: To Do
assignee: []
created_date: '2026-10-29T20:30:00Z'
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
- [ ] #1 Host tests red on HEAD, green after, for each of (2)-(5); MQTT.md and CHANGELOG list every breaking change; retained sat/ssi emptied
<!-- AC:END -->
