---
id: TASK-1593
title: >-
  OT-Direct: a refused SAT write is reported as delivered (addCommandToQueue
  returns Sent while the OT-Direct command ring refuses it)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T18:33:00Z'
updated_date: '2026-11-03T18:34:00Z'
labels:
  - sat
  - otdirect
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 738000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From TASK-1412.11 (Q-1412.11a, Robert 2026-11-03: yes). On OT-Direct addCommandToQueue() returns CmdQueueResult Sent unconditionally (OTGW-Core.ino ~3856-3860) and handleOTDirectCommand() is void (OTDirect.ino ~2749). When the otCmdEnqueue ring (12 slots, coalesced per MsgID) is full the write is refused but SAT's commit-on-success (TASK-1574) caches it as delivered; recovery only via the 5 s CS resend / 60 s MM repair.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 handleOTDirectCommand reports whether the command was accepted; addCommandToQueue maps a refusal to Dropped on OT-Direct (PIC path unchanged)
- [ ] #2 A host test drives the real OT-Direct handler with a full ring: old code reports Sent (red), fix reports Dropped and SAT does not commit its cache (green); the LL-013 open_decision test_ll013b becomes a passing contract test
- [ ] #3 Every other caller of handleOTDirectCommand still compiles and behaves the same (listed with file:line)
- [ ] #4 esp32-combo build, OTA flash and bench check on marvin-otgw32 (OT-Direct): SAT CS/MM still delivered, no new Dropped in normal traffic
<!-- AC:END -->
