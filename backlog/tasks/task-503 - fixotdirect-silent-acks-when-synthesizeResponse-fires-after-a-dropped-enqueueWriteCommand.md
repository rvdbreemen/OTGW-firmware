---
id: TASK-503
title: >-
  fix(otdirect): silent acks when synthesizeResponse fires after a dropped
  enqueueWriteCommand
status: Done
assignee: []
created_date: '2026-05-01 17:37'
updated_date: '2026-05-01 17:58'
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
- [x] #1 Every call site of enqueueWriteCommand in handleOTDirectCommand checks the bool return value before calling synthesizeResponse
- [x] #2 When enqueueWriteCommand returns false, no synthesized 'XX: value' line is emitted to telnet or processOT()
- [x] #3 The existing 'OT-direct: %s command dropped (queue full)' log inside enqueueWriteCommand remains the only drop notification (no double-logging)
- [x] #4 Unit/manual test: with the OT cmd queue artificially full, sending CS=62.0 produces the drop log and NO 'CS: 62.00' bare-line in telnet, and downstream state (state.sat.* or equivalent) does not advance
- [x] #5 python evaluate.py passes
- [x] #6 No behavioral change on the healthy path: when enqueueWriteCommand returns true, synthesizeResponse fires exactly as before
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Capture the bool return value of every `enqueueWriteCommand(...)` call inside `handleOTDirectCommand` (CS, TC, TT, C2, CC, SW, SH, MM, OT, VS, SC, etc.) and gate the subsequent `synthesizeResponse(buf, rspBuf)` on success. The existing `OT-direct: %s command dropped (queue full)` log inside `enqueueWriteCommand` is the only drop notification; no new logging on the dropped path. Single-file change in `OTDirect.ino`. Validate with `python evaluate.py --quick`.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in 9 call sites in handleOTDirectCommand (CS, C2, CC, SW, SH, MM, OT, VS, CL). Two patterns: (a) single-call branches (CC, MM, OT, VS) wrap the helper in `if (enqueueWriteCommand(...))`, (b) mixed clear+write branches (CS, C2, SW, SH, CL) introduce `bool enqueued = true;` default so the clear-path keeps synthesizing, then assign the helper return on the write-path and gate formatting + synthesizeResponse on `enqueued`. SC-time at line 2587 left as-is (no synthesizeResponse there to begin with). TASK-442 `otCSLastCommandMs` / `otC2LastCommandMs` refresh moved INSIDE the success branch since they mirror 'we successfully scheduled a setpoint' and must not advance on dropped frames.

AC 5 (`python evaluate.py` passes): exit code 1, but agent verified via stash-test that the failure is the pre-existing 14 PROGMEM violations baseline, identical pre and post this change. No regression. Health Score 95.5%. Strictly AC 5 unticked; pre-existing baseline issue out of scope for this task — needs a separate cleanup task, see escalation in chat.

Correction on AC 5: rechecked `python evaluate.py --quick` post-commit, exit code is **0**, not 1 as the implementer agent reported. Evaluation summary: 67 checks, 57 passed, 2 warnings, 1 failed (the PROGMEM-flash-string check listing 14 baseline violations), 7 info. Health Score 95.5%. The non-fatal-failed check returns 0 from the overall script, so the AC `python evaluate.py passes` is met as written. The 14 PROGMEM violations remain a pre-existing baseline issue worth a separate cleanup task; none involve the lines TASK-503 touched (verified by stash-test).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented in commit 7631eadb. Gates synthesizeResponse on enqueueWriteCommand return value across 9 call sites in handleOTDirectCommand (CS, C2, CC, SW, SH, MM, OT, VS, CL). Two patterns: `if (enqueueWriteCommand(...)) { ... synthesizeResponse(...); }` for single-call branches; `bool enqueued = true;` defaulting true with conditional gating for mixed clear+write branches so the clear-path keeps synthesizing. SC-time at line 2587 left untouched (no synthesizeResponse there). TASK-442 expiry-timer refresh moved INSIDE the success branch so it doesn't advance on dropped frames. The "OT-direct: %s command dropped (queue full)" log inside enqueueWriteCommand stays as the single drop notification — no double-logging. All 6 ACs met. Pre-existing 14 PROGMEM violations are out of scope (separate cleanup task).
<!-- SECTION:FINAL_SUMMARY:END -->
