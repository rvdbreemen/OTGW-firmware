---
id: TASK-1855
title: >-
  OT-Direct: a write command (CS=, C2=, MM=, SW=, SH=, CC=/CL=, VS=) puts its
  WRITE on the bus twice
status: To Do
assignee: []
created_date: '2026-11-12T02:35:00Z'
labels:
  - otdirect
dependencies: []
priority: low
ordinal: 933000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
09's bench of beta.197 on .61 (loopback): VS=15 and CL=0 each put their WRITE frame on the socket twice. Cause from the code (OTDirect.ino): enqueueWriteCommand() enqueues the one-shot frame and updateWriteCache() sets valueSet on the matching W_ENTRY; a write entry that was never sent has lastSentMs 0, so scheduleMasterRequest() finds it due at once and sends the same WRITE again. Existing pattern for every write command with a write entry (1, 7, 8, 14, 16, 24, 27, 56, 57; 71 since TASK-1831). Harmless (same value) but one extra bus frame per command; the PIC sends one.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A write command puts exactly one WRITE frame on the bus; the scheduler refresh follows after its interval (e.g. set lastSentMs when the one-shot frame is enqueued, or let the schedule send the first frame)
- [ ] #2 Host test on the real enqueueWriteCommand/updateWriteCache/scheduler: red on the current code, green on the fix
- [ ] #3 Bench: VS=15 and CS=50 each give one R frame in a capture
<!-- AC:END -->
