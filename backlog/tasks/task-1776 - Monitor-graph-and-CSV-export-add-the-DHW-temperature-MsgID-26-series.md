---
id: TASK-1776
title: 'Monitor graph and CSV export: add the DHW temperature (MsgID 26) series'
status: To Do
assignee: []
created_date: '2026-11-10T09:41:00Z'
labels:
  - needs-maintainer-ok
  - web-ui
  - feature-request
milestone: 2.0.0
dependencies: []
ordinal: 873000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tester request (fegyosz_47807, #alpha-testing 2026-11-10 08:45:41 UTC, https://discord.com/channels/812969634638725140/1514720723980259460/1569619039276245013): "I am missing the DHW temperature from the graph and from the CSV export. My boyler has a small tank in it for DHW." Today the 24 h graph ring and its CSV export hold flow, return, control setpoint, room setpoint, room, outside, modulation and the flame/DHW/CH bits, but no DHW temperature (MsgID 26). The ring layout is fixed by ADR-193 (14-byte slots), so adding a series changes the slot layout and probably needs an amending ADR. Robert decides whether and how.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decided whether the DHW temperature is added (and whether an amending ADR to ADR-193 is needed)
- [ ] #2 If added: the graph shows a DHW temperature series and the CSV export has a DHW temperature column, shown as a dash when the boiler does not report MsgID 26
<!-- AC:END -->
