---
id: TASK-33
title: Modulation reliability tracker
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:51'
updated_date: '2026-04-05 23:25'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Track whether the boiler modulation reporting via OT is reliable. Some boilers report incorrect or stuck modulation values. The tracker observes modulation changes over time and flags unreliable reporting so the control loop can adapt (e.g. skip modulation-based decisions).

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/device/modulation.py
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Track modulation value changes over time
- [x] #2 Detect stuck or unreliable modulation reporting
- [x] #3 Flag unreliable modulation state for control loop
- [x] #4 MQTT publish: modulation_reliable boolean
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Modulation reliability tracker.\n\nChanges:\n- 10-minute observation window counting modulation value changes\n- Needs >= 3 changes per window to be "reliable"\n- Flags unreliable modulation for control loop decisions\n- MQTT: sat/modulation_reliable, REST: modulation_reliable\n\nFiles: OTGW-firmware.h, SATcontrol.ino
<!-- SECTION:FINAL_SUMMARY:END -->
