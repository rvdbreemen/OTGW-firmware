---
id: TASK-1170
title: Non-blocking buffered writers for port 25238 clients and MQTT
status: To Do
assignee: []
created_date: '2026-09-28 04:06'
updated_date: '2026-09-28 19:57'
labels:
  - feature
  - port-25238
  - wontfix
dependencies:
  - TASK-1164
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
- [ ] #7 MQTT publishes no longer block the loop on a stalled broker: longest REST gap during a mid-payload stall at idle level, measured old (8.3 s) vs new with the stall broker harness
- [ ] #8 Measured whether a multi-second loop stall can overflow the PIC serial receive buffer (buffer size from code with file:line, PIC output rate on the bench), old vs new
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-28 scope extended (maintainer, TASK-1164 decision): the same blocking-write problem exists on the MQTT path. With a broker that stops reading mid-payload, one WiFiClient::write() blocks up to 5 s and the 5 s MQTT_WRITE_STALL_BUDGET_MS is only checked between writes, so the loop is held up to 8.3 s (measured) before the link is dropped. One design for both writers.

WON'T DO. Maintainer decided on 2026-09-28 not to build this: v1.7.6-beta.7 works well for users. The measured behaviour stays as documented in TASK-1164 and TASK-1169: a stalled port-25238 client loses output and is dropped after ~9 s, and a broker that stops reading mid-payload holds the loop up to ~8 s before a clean reconnect. Related earlier decision: TASK-1149 (TX ring for SimpleTelnet), also won't do.
<!-- SECTION:NOTES:END -->
