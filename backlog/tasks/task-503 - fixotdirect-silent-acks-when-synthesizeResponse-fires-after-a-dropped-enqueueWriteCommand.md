---
id: TASK-503
title: >-
  fix(otdirect): silent acks when synthesizeResponse fires after a dropped
  enqueueWriteCommand
status: To Do
assignee: []
created_date: '2026-05-01 17:37'
labels:
  - bug
  - otdirect
  - observability
  - follow-up
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - >-
    Found via Georges SAT log 2026-05-01 (build 3e14c2c) — see chat transcript
    with bare 'CS: 62.00' / 'MM: 100' echoes following queue-full drops
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Problem

In `OTDirect.ino:handleOTDirectCommand`, every command path calls `enqueueWriteCommand(...)` to push an OT WRITE_DATA frame, then unconditionally calls `synthesizeResponse(buf, rspBuf)` to feed a PIC-style "XX: value" response back into `processOT()`. The synthesized response runs **regardless of whether enqueueWriteCommand actually queued the frame**.

When the OT command queue is full, `enqueueWriteCommand` returns false, logs `OT-direct: <label> command dropped (queue full)`, and the frame never reaches the bus. But the call site below it still synthesizes the ack — so all downstream consumers (SAT controller, MQTT publisher, WebUI, command-queue clear logic) see a successful acknowledgment for a command that never went out.

## Where

Affects every CS, TC, TT, C2, CC, SW, SH, MM, OT, VS, SC, etc. handler in `handleOTDirectCommand` (HEAD: `OTDirect.ino:2418-2530+`). Pattern shape:

```cpp
enqueueWriteCommand(1, f88, "CS");          // may silently drop
// ...some bookkeeping...
synthesizeResponse(buf, rspBuf);            // ALWAYS fires
```

## Why it matters

Silent failure mode. The SAT controller's `state.sat.fLastCommandedSetpoint` (or equivalent) advances to the dropped value because it sees the synthetic ack. Operators have no idea the boiler never received the setpoint. Particularly painful when diagnosing a stuck or unresponsive boiler — observed in Georges build `3e14c2c` log on 2026-05-01 where every CS=62.0 and MM=100 cycle dropped on queue-full while the synthesized acks looked perfectly healthy.

Note: TASK-494's coalesce-by-MsgID dramatically reduces the queue-full rate for repeated same-MsgID submissions, so on healthy HEAD the bug is rare. But the silent-ack pattern is still wrong on principle and bites whenever the queue does fill (e.g. unique-MsgID burst, or boiler not draining).

## Approach

Have each call site capture the return value from `enqueueWriteCommand` and skip `synthesizeResponse` on false. The `OT-direct: <label> command dropped (queue full)` log already exists inside `enqueueWriteCommand` — sufficient for observers to see the drop directly without an additional warning.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every call site of enqueueWriteCommand in handleOTDirectCommand checks the bool return value before calling synthesizeResponse
- [ ] #2 When enqueueWriteCommand returns false, no synthesized 'XX: value' line is emitted to telnet or processOT()
- [ ] #3 The existing 'OT-direct: %s command dropped (queue full)' log inside enqueueWriteCommand remains the only drop notification (no double-logging)
- [ ] #4 Unit/manual test: with the OT cmd queue artificially full, sending CS=62.0 produces the drop log and NO 'CS: 62.00' bare-line in telnet, and downstream state (state.sat.* or equivalent) does not advance
- [ ] #5 python evaluate.py passes
- [ ] #6 No behavioral change on the healthy path: when enqueueWriteCommand returns true, synthesizeResponse fires exactly as before
<!-- AC:END -->
