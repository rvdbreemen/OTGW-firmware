---
id: TASK-1537
title: >-
  SAT Immergas quirk sends TP=11:12=<setpoint> instead of thermo-nova's min(max
  modulation, 80)
status: To Do
assignee: []
created_date: '2026-11-01T02:01:00Z'
labels:
  - sat
  - bug
dependencies: []
priority: medium
ordinal: 693000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1412.17 profile work, second-read 2026-11-01. Firmware SATcontrol.ino:6772-6777 sends TP=11:12=<finalSetpoint> with every control update for Immergas. thermo-nova sends TP=11:12={min(value, 80)} together with MM=, where value is the max relative modulation (coordinator/mqtt/opentherm.py:214-219 at 7beaaf5). Entry 21 is the related Sergeant D record (check its wording before the fix). Sending a flow setpoint into the Immergas TSP 11/12 (max modulation) parameter may set a wrong modulation limit on Immergas boilers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Entry 21 and the Immergas quirk origin checked; conflict (if any) put to Sergeant D before the change
- [ ] #2 Host test on the real code: with the Immergas quirk TP=11:12 carries min(max relative modulation, 80), sent with MM= (red on HEAD, green)
<!-- AC:END -->
