---
id: TASK-1439
title: 'feat-2.0.0: port TASK-1431 — report dropped PIC commands on MQTT and in HA'
status: To Do
assignee: []
created_date: '2026-10-28T22:37:00Z'
labels:
  - enhancement
  - port
dependencies: []
priority: low
ordinal: 600000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
1.x TASK-1431 (commit 28efefe8a on otgw-1.x.x) adds an HA-facing contract that 2.0.0 should match: otgw-firmware/command_dropped (not retained, payload = command text), otgw-firmware/command_retry_exceeded (retained ON/OFF; ON on a drop, OFF on the next confirmed non-PR command, republished on MQTT connect), cmd_dropped in /api/v2/device/info, HA sensor 'Last Dropped Command' + binary sensor 'Command Dropped', and TP= commands appended in the queue instead of replaced. Check the 2.0.0 queue (incl. OTDirect synthesizeResponse paths) and the binary-sensor discovery label sanitizing before porting.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Same topics, payloads and retain flags as 1.x TASK-1431
- [ ] #2 TP= commands are appended, not replaced, in the 2.0.0 command queue
- [ ] #3 Build for the 2.0.0 targets and evaluator green
<!-- AC:END -->
