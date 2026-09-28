---
id: TASK-1170
title: Non-blocking port-25238 writers with a per-client buffer
status: To Do
assignee: []
created_date: '2026-09-28 04:06'
labels:
  - feature
  - port-25238
dependencies:
  - TASK-1169
priority: medium
ordinal: 239000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1169 (maintainer request 2026-09-28). Today every write to a port-25238 client goes straight to WiFiClient::write() with a 1000 ms timeout (SimpleTelnet_impl.tpp:445). A client that makes no send progress for 1 s (not reading, or no ACKs during a WiFi stall) makes the write return short: the unsent bytes are lost (SimpleTelnet_impl.tpp:222) and the whole firmware loop blocks ~1 s per write, so the PIC serial line is not drained meanwhile. After a few failed writes the client is disconnected. Bench-measured on beta.6 and the beta.7 candidate alike: 127 bytes lost, disconnect after ~9 s, HTTP latency peak 1-3 s.

Goal: writes to port-25238 clients never block the loop, and a briefly slow client loses nothing. Each client gets its own bounded buffer; the loop hands bytes to the buffer and drains it non-blocking as the TCP window allows. Only a client whose buffer stays full is dropped, and that is counted. Byte transparency (ADR-095) and the write floor (ADR-097) stay intact. This changes SimpleTelnet (maintainer approval for the library change is part of the task) and costs RAM, so it needs an ADR with measured heap cost.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR written (Proposed) with the buffer size choice, RAM cost measured on the bench, and the drop policy for a client whose buffer stays full; accepted by the maintainer before implementation
- [ ] #2 A port-25238 client that stops reading for up to the buffer's worth of output loses no bytes, measured with the TASK-1169 harness (t1169.py) old vs new
- [ ] #3 A stalled client no longer blocks the loop: HTTP latency during the stall stays at idle level (old: 1-3 s peak), measured old vs new
- [ ] #4 Byte transparency and the ADR-097 floor tests (T1-T4) still pass on the bench
- [ ] #5 Heap with two streaming clients over 30 minutes stays within the budget stated in the ADR
- [ ] #6 Build (build.bat, fresh bins, success line) and evaluate.py --quick green
<!-- AC:END -->
