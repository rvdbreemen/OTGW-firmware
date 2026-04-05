---
id: TASK-33
title: Modulation reliability tracker
status: To Do
assignee: []
created_date: '2026-04-05 20:51'
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
- [ ] #1 Track modulation value changes over time
- [ ] #2 Detect stuck or unreliable modulation reporting
- [ ] #3 Flag unreliable modulation state for control loop
- [ ] #4 MQTT publish: modulation_reliable boolean
<!-- AC:END -->
