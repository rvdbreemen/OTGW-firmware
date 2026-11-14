---
id: TASK-1950
title: >-
  OT-Direct: remove the unused setback settings, FS= flag and setback_active
  fields after TASK-1806
status: To Do
assignee: []
created_date: '2026-11-14T22:21:00Z'
labels:
  - otdirect
  - cleanup
  - needs-maintainer-ok
dependencies: []
priority: low
ordinal: 997000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1806 (lane-1924, d3). Since 1806 OT-Direct no longer applies a setback on thermostat loss, but OTDsetbacktemp, OTDfailsafe (FS=), the REST fields setback_temp / setback_active / otdsetback, the MQTT topic otgw-otdirect/setback_active, the web UI rows (v2.js otd_row_setback, otdsetbacktemp) and the RAM state otSetbackEngaged / state.otd.bSetbackActive / otFailSafeEnabled stay, so the API did not change. Removing them is an API contract change (REST, MQTT, settings.ini migration): needs Robert's decision. Also: the OT-Direct SB= handler stores otSetbackTemp (OTDirect.ino SB= branch), which has no consumer any more; the PIC's SB= is the GPIO away room setpoint (gateway.asm 6.6 :5390-5398).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decided which fields go and how a stored value migrates
- [ ] #2 The removed fields are gone from firmware, REST, MQTT, web UI and the manual EN/NL; a settings.ini with the old keys still loads
<!-- AC:END -->
