---
id: TASK-1490
title: >-
  HA entities for SAT health, sync, window and setpoint topics (TASK-1486 audit
  sections 1 and 2)
status: To Do
assignee: []
created_date: '2026-10-29T22:35:00Z'
updated_date: '2026-10-31T05:26:00Z'
labels:
  - sat
  - mqtt
  - ha
milestone: 2.0.0
dependencies:
  - TASK-1485
  - TASK-1484
priority: low
ordinal: 649000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1486 audited the 91 published sat/* state topics without a Home Assistant entity (docs/sat/sat-ha-entity-audit.md). Add the 13 section-1 entities to the discovery tables in MQTTHaDiscovery.cpp: 7 SAT_BIN rows (device_health, cycle_health, pressure_health, setpoint_sync, modulation_sync, ch_sync: class problem, diagnostic, on_off payload; window_open: class window, true_false) and 6 sensor rows (requested_setpoint, heating_curve_recommendation, manufacturer, consumption as volume_flow_rate m3/h, max_setpoint read-only sensor, pressure_drop_rate), with class, unit and category per the audit. Plus the section-2 items Robert accepts (recorded in the notes). Ordering: pressure_health, device_health and heating_curve_recommendation ship with or after TASK-1485; consumption after TASK-1484. Update docs/api/MQTT.md SAT entity counts, CHANGELOG and any host test that asserts discovery row counts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host test streams the discovery for each new row with the class, unit, category and payload from the audit; red on HEAD, green after
- [ ] #2 Bench: mosquitto_sub on homeassistant/# shows each new config, and HA shows each entity with a state (gated topics excepted)
- [ ] #3 At least one health and one sync entity read OFF on a healthy bench and ON with the fault forced (simulation)
- [ ] #4 Flash and static-RAM delta measured from a before/after esp32-combo build (estimate ~0.9 KB flash for section 1, ~1.5 KB with all recommended section-2 adds, 0 B static RAM)
- [ ] #5 MQTT.md entity counts and CHANGELOG updated
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31: TASK-1486 decision (Robert): add also binary_sensor problem (diagnostic) for sat/fallback_active, room_temp_lost, ot_link_lost, push_refused; switch for sat/enabled; select for sat/preset. See docs/sat/sat-ha-entity-audit.md section 2.
<!-- SECTION:NOTES:END -->
