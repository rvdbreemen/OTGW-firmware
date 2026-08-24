---
id: TASK-1080
title: 'Fix: MsgID 24 (Tr) forwarded to boiler as 0.00°C, breaks Tr write path'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-24 18:10'
updated_date: '2026-08-24 20:32'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/677'
priority: high
ordinal: 182000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
GitHub #677 (RonVervoort): after firmware update, thermostat's real room temp (MsgID 24 Tr write) forwarded to boiler as 0.00 instead of actual value. Bus log shows thermostat sends correct Tr value, gateway relays 0x0000. Gateway then reports Unknown-Data-Id back to thermostat and falsely concludes boiler doesn't implement MsgID 24. HA raw Tr sensor also reads 0.00, consistent with corruption in Tr parse/relay path itself.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root cause of Tr (MsgID 24) value being zeroed in the write-forward path identified
- [ ] #2 Fix forwards actual Tr value from thermostat to boiler, verified via bus log capture
- [x] #3 Gateway no longer reports false Unknown-Data-Id / not-implemented for MsgID 24 when boiler acks correctly
<!-- AC:END -->
