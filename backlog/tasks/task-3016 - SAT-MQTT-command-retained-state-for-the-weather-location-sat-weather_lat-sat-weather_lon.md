---
id: TASK-3016
title: >-
  SAT: MQTT command + retained state for the weather location (sat/weather_lat,
  sat/weather_lon)
status: To Do
assignee: []
created_date: '2026-11-21T16:38:00Z'
updated_date: '2026-11-21T16:41:00Z'
labels:
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1133000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing msg 1573739161251356854 (2026-11-21): the weather location is REST-only, so MQTT clients cannot read or fill it; he wants Homey to fill it. Verified at origin/dev: settings.sat.fWeatherLat/Lon (SATtypes.h:566) are exposed in REST settings (restAPI.ino:4272-4273, 4530: satweatherlat/satweatherlon) and not in MQTTstuff.ino (no hit). Tester request: needs Robert's OK. Location is personal data: the retained state topic publishes the user's coordinates to the broker; weigh that (opt-in or command-only) in the design.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decides (command only, or command + retained state)
- [ ] #2 MQTT set command for lat/lon validated to -90..90 / -180..180, persisted like the REST path; state published per the decision
- [ ] #3 Host test on the real MQTT handler: valid value stored, out-of-range rejected (red before, green after)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: released by Robert in #alpha-testing (msg 1573739678606295074): implement as requested (MQTT command + retained state for lat/lon). The privacy note above (coordinates in a retained topic) is for the implementer to mention in the docs, not a blocker.
<!-- SECTION:NOTES:END -->
