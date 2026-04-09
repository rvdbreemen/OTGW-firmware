---
id: TASK-163
title: 'OTGW32-Audit-6D: Command queue overflow behavior'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:22'
updated_date: '2026-04-08 22:33'
labels:
  - audit
  - otgw32
  - phase-6
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify the behavior of the 8-entry command ring buffer when it overflows. When otCmdEnqueue() returns false (queue full), the caller must handle this gracefully. Verify no silent data loss occurs without logging, and assess whether queue size 8 is sufficient for worst-case burst scenarios (e.g. MH+MW+M2 all at once, plus SAT-driven CS/MM commands).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 otCmdEnqueue() returns false when queue is full
- [ ] #2 All callers of otCmdEnqueue() handle false return (log or discard gracefully)
- [x] #3 Queue size 8 is assessed against worst-case burst (justify or propose audit-fix)
- [ ] #4 No silent data loss — dropped commands are at minimum logged via DebugTln
- [x] #5 Ring buffer head/tail arithmetic is correct with no off-by-one
- [x] #6 Any issue results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit Findings

### Ring buffer correctness (AC1, AC5)
- `otCmdQueueFull()` returns true when `(head+1) % SIZE == tail`.
- This wastes one slot (7 usable of 8) — standard ring-buffer idiom, correct.
- `otCmdEnqueue()` returns false on full, true on success — PASS.
- Head/tail arithmetic uses modulo correctly — no off-by-one — PASS.

### Callers of otCmdEnqueue() and return-value handling

Via enqueueWriteCommand() helper (AC2 - PASS):
- CS, TC, TT, C2, CC, SW, SH, MM, OT, VS, CL, SC — all use enqueueWriteCommand().
- enqueueWriteCommand() (line 1266) logs "OT-direct: <label> command dropped (queue full)" on false return.

Direct calls that IGNORE return value (AC2 - FAIL):
- Line 1745: HW=P (DHW push) enqueues MsgID 99 — return value discarded.
- Line 1856: MH= enqueues MsgID 99 — return value discarded.
- Line 1869: MW= enqueues MsgID 99 — return value discarded.
- Line 1882: M2= enqueues MsgID 99 — return value discarded.
- Line 1892: RR= enqueues MsgID 4 — return value discarded.
- Line 2144: PM= one-shot read enqueue — return value discarded.
- Line 2293: RS= counter reset — return value discarded.
- Line 2343: TP= thermostat param — return value discarded.
- Silent data loss on 8 call sites when queue is full.

### Worst-case burst analysis (AC3)
- Scenario A: MH + MW + M2 in rapid succession = 3 MsgID-99 frames queued.
- Scenario B: CS + MM + SW + SH from MQTT automation = 4 frames (via enqueueWriteCommand).
- Combined A+B = 7 frames. With 1 wasted slot that fills the 7-entry effective queue.
- Add HW=P (1 extra) = 8 frames queued — overflows by 1.
- Queue size 8 (7 usable) is marginal for the MH+MW+M2+CS+MM+SW+SH+HW=P burst.
- In normal usage this burst is unlikely but possible via rapid MQTT commands.

### Verdict
- AC1, AC5: PASS.
- AC2, AC4: FAIL — 8 direct otCmdEnqueue() callers silently drop frames with no log.
- AC3: MARGINAL — queue size 8 (7 usable) is just sufficient for most bursts but can overflow
  under the MH+MW+M2+CS+MM+SW+SH+HW=P scenario.
- Audit-fix task to be created.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audit of command queue overflow behavior — ISSUES FOUND, two ACs failed.

Findings:

PASS:
- otCmdEnqueue() correctly returns false when full (AC1).
- Ring buffer head/tail arithmetic is correct, no off-by-one (AC5).
- enqueueWriteCommand() helper logs "dropped (queue full)" correctly.

FAIL:
- 8 direct otCmdEnqueue() callers ignore the return value and silently drop frames:
  HW=P, MH=, MW=, M2=, RR=, PM=, RS=, TP= (AC2, AC4).
- Queue size 8 (7 usable) can overflow under the worst-case MH+MW+M2+CS+MM+SW+SH+HW=P burst (8 frames in rapid succession) (AC3 — marginal).

Audit-fix created: TASK-183 — add return-value checks and logging to all 8 direct callers; review queue size (recommend 12).
<!-- SECTION:FINAL_SUMMARY:END -->
