---
id: TASK-1466
title: >-
  SAT MQTT topic hygiene: decisions needed (dead consumption block, duplicate
  topics, health polarity, heating_curve naming)
status: To Do
assignee: []
created_date: '2026-10-29T07:23:00Z'
updated_date: '2026-10-29T07:58:00Z'
labels:
  - sat
  - mqtt
  - needs-decision
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 626000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. Contract decisions for Robert (ask interactively, one per question, with a recommendation): (1) sat/consumption is dead: minCons/maxCons hard-coded 0 (SATcontrol.ino:3632-3648): remove or wire up? (2) sat/heating_curve_recommendation mixes 'insufficient' with INCREASE/DECREASE/HOLD (SATcycles.ino:316, 1356-1360) and overlaps sat/curve_recommendation (SATcontrol.ino:3475 vs :3488): one case convention, keep both? (3) sat/ssi and sat/summer_simmer_index carry the same value (:3587-3588): one topic? (4) *_health polarity mixed: pressure_health ON=healthy (:3457), flame_health/cycle_health/*_sync ON=problem, device_health ON when the boiler is off (:3570): one convention? (5) sat/heating_curve is the coefficient as command (MQTTstuff.ino:705) but the computed curve as state (SATcontrol.ino:3303): rename? (6) Many sat/* topics have no HA entity (pressure, humidity, window_open, *_sync): intended? Any rename/removal is an MQTT contract change for HA users: retire entities, note in MQTT.md and CHANGELOG.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each decision asked of Robert and recorded in the notes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 (Taken session, TASK-1459 work): the MQTT preset and window handlers set settings.sat.fTargetTemp in RAM only (not persisted), probably deliberate (a temporary target while a preset/window is active), unconfirmed. Add as decision 7: is a preset/window target meant to survive a reboot?
<!-- SECTION:NOTES:END -->
