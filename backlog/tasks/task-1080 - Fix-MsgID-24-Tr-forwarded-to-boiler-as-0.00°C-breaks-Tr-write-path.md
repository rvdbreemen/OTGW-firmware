---
id: TASK-1080
title: 'Fix: MsgID 24 (Tr) forwarded to boiler as 0.00°C, breaks Tr write path'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-24 18:10'
updated_date: '2026-08-24 20:33'
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
- [x] #2 Gateway no longer reports false Unknown-Data-Id / not-implemented for MsgID 24 when boiler acks correctly
- [ ] #3 The zeroed R-frame and the 0.00 canonical Tr are documented as NOT ESP-side defects: frame relay is the PIC's job, and canonical carrying the boiler-side worldview is ADR-069 by design
- [ ] #4 A previously persisted false unsupported bit self-heals on live traffic without the user deleting /ot-boiler.json
- [ ] #5 RonVervoort confirms 24W no longer appears in retained otgw-firmware/boiler/unsupported_msgids
<!-- AC:END -->
