---
id: TASK-1169
title: >-
  Investigate: otgwstream_tx_dropped rises, PIC-to-client bytes lost on port
  25238
status: To Do
assignee: []
created_date: '2026-09-27 21:22'
labels:
  - bug
  - port-25238
dependencies: []
priority: medium
ordinal: 238000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during the TASK-1167 bench validation on .88.68 (build 1.7.6-beta.7+3bc71d6). otgwstream_tx_dropped counts PIC->client bytes the firmware could not hand to a port-25238 client. It went 0 -> 237 during the ADR-097 tests (T2 sent ~52 KB to each of two clients in 180 s), then 237 -> 241 in one step with a SINGLE pyotgw client (heap baseline, t=1246 s) and 241 -> 245 with only OTmonitor connected. So loss also happens with one client and is not caused by the new second writer. ADR-095 requires the bridge to be byte transparent in both directions; a dropped byte can corrupt a line a tool parses (pyotgw logged "Unknown message" warnings while alone). Maintainer agreed to ship beta.7 with this open.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The code path that increments otgwstream_tx_dropped is traced with file:line, and the condition that triggers it (client TCP window, write size, heap gate or other) is identified with evidence
- [ ] #2 The loss is reproduced on the bench with a harness that counts bytes on the serial side against bytes each client received
- [ ] #3 It is established whether 1.7.6-beta.6 and v1.7.4 drop too under the same load (old vs new)
- [ ] #4 A fix, or a recorded decision why the loss is acceptable, with the before/after byte counts
<!-- AC:END -->
