---
id: TASK-1901
title: >-
  A replayed thermostat MsgID 16 write must not change the stored SAT target
  (SATtargettemp)
status: To Do
assignee: []
created_date: '2026-11-13T16:22:00Z'
labels:
  - sat
  - otgw
  - simulation
dependencies: []
priority: medium
ordinal: 969000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in d3's read-only analysis for TASK-1895 (workflow wf_a2aa95ff-0f6, trace + independent verifier); line numbers at origin/dev 1c7757419. OTGW-Core.ino:6051-6054 feeds a T Write-Data MsgID 16 to satOnThermostatSetpoint() without a bReplayed check, and with follow-thermostat on that ends in updateSetting("SATtargettemp") (SATcontrol.ino:2283-2293), which is persisted. So an OTGW simulation replay (/simulate/start or debug otgwsim) can overwrite the user's real SAT target. Precedents: TASK-1649 (replayed MsgID 27 must not set the outside temperature), TASK-1895 (replayed T lines no longer write the thermostat bitmaps). Fix: add && !OTdata.bReplayed to that condition.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: a replayed T Write-Data MsgID 16 does not reach satOnThermostatSetpoint() (or leaves SATtargettemp unchanged); red on the current code, green on the fix
- [ ] #2 A real T MsgID 16 write still reaches it (regression check)
<!-- AC:END -->
