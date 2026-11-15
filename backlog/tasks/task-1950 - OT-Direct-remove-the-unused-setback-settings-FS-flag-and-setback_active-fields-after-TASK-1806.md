---
id: TASK-1950
title: >-
  OT-Direct: remove the unused setback settings, FS= flag and setback_active
  fields after TASK-1806
status: To Do
assignee: []
created_date: '2026-11-14T22:21:00Z'
updated_date: '2026-11-15T09:26:00Z'
labels:
  - otdirect
  - cleanup
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
- [x] #1 Robert decided which fields go and how a stored value migrates
- [ ] #2 The removed fields are gone from firmware, REST, MQTT, web UI and the manual EN/NL; a settings.ini with the old keys still loads
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 Robert decided: remove the unused setback settings, FS= and setback_active ('2. OK' in Discord DM 1571454228763779093, answer to the coordinator's list 1571452589231439943, item 1950 'remove the unused OT-Direct setback settings / FS= / setback_active (rec. yes, PIC parity after 1806)'; acknowledged in 1571454958447099976). Migration (the recommendation he accepted, recorded as our reading): a settings.ini with the old keys keeps loading, the removed keys are ignored on load and dropped at the next save; the REST fields and MQTT topic go. Added scope (d3, 2026-11-15): since TASK-1802 bug 9 (commit 103fedacf on lane-1954) OTDsetbacktimeout / iSetbackTimeout is no longer read either (fixed 3 s loss detection), so it joins the removal list: settings key OTDsetbacktimeout, REST otdsetbacktimeout and setback_timeout, v2.js otdsetbacktimeout row, settingsHelp entry. Queue (coordinator): after 1954, 1812 and bug 9.
<!-- SECTION:NOTES:END -->
