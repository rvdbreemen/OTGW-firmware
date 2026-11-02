---
id: TASK-1573
title: 'PIC command queue: make the buffer visible in /api/v2/device/info (rule 6)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T21:11:00Z'
updated_date: '2026-11-02T21:51:00Z'
labels:
  - pic
  - command-queue
  - rest-api
milestone: 2.0.0
dependencies:
  - TASK-1569
priority: medium
ordinal: 721000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-02, queue design rule 6. Expose the PIC command queue state so drips, waits and drops can be seen without telnet: depth (cmdQueueSize), the oldest waiting command and its age, the command in flight (TASK-1571) and the PM=/TP= slot holder (rule 3), and counters: sent, retried, rejected (PIC error code), dropped (retry exceeded; today state.pic.iCmdDropped). Hand-rolled JSON (ADR-146), read under the queue lock with a bounded timeout from async_tcp (TASK-1569, ADR-like OTStateLock rule TASK-879).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 /api/v2/device/info carries a cmdqueue object: depth, oldest_cmd, oldest_age_ms, inflight, prio_slot, sent, retried, rejected, dropped (host test on the real JSON writer, or a REST test on the bench)
- [ ] #2 Values move as expected on the bench: a burst of 5 REST commands raises depth and sent, an unknown command raises rejected; docs/api updated
- [ ] #3 Read under the queue lock with a bounded timeout; no ArduinoJson (evaluate gate)
<!-- AC:END -->
