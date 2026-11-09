---
id: TASK-1752
title: 'OT-Direct: flush queued boiler writes on a mode switch'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-09T09:30:00Z'
updated_date: '2026-11-09T20:48:00Z'
labels:
  - otdirect
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 854000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1733 / ADR-223 (found by the 2026-11-09 correctness review, otgw-firmware-80). resetTransientState() (OTDirect.ino ~2445-2510) clears tables, caches and state on a mode switch but not otCmdQueue, so a WRITE 16/100 queued for a TT= in master mode can still go out once after switching to gateway mode (through the scheduler or an alternative slot). Low impact (one frame), but it contradicts ADR-223's 'no gateway WRITE 16/100 in gateway mode'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After a mode switch no frame queued under the previous mode reaches the boiler; host test red before, green after
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-09 otgw-firmware-80: v1 fix (otCmdHead=otCmdTail=0 in resetTransientState) rejected after review: resetTransientState also runs on async_tcp (REST POST /api/v2/otdirect/mode -> addCommandToQueue GW= -> handleOTDirectCommand, restAPI.ino:534, OTGW-Core.ino:3851) while the loop-task consumers peek, send, then advance otCmdTail; a reset in between gives tail=1/head=0 -> depth 11 -> stale slots replayed (worse than the original bug). Confirmed by reading the code.

Fix (v3): single writer per index. resetTransientState records otCmdFlushHead=otCmdHead (relaxed atomic) then sets otCmdFlushPending (release). otCmdApplyPendingFlush() (loop task only) moves otCmdTail forward to the snapshot, never back and never past head (distance guard); frames queued after the reset are kept. Applied at the top of loopOTDirect (every pass, also in modes without a consumer), before the peek in scheduleMasterRequest and otdAltSlotTaken, and right after each tail advance (a reset during the send flushes at once; a stale snapshot is rejected). docs/c4/c4-code-otdirect.md resetTransientState list updated.

Evidence: tests/test_otd_mode_switch_queue_reset.py compiles the real ring, otCmdEnqueue, otCmdApplyPendingFlush, scheduleMasterRequest, otdAltSlotTaken, resetTransientState and applyOTDirectMode (sendMasterRequestAsync stubbed to record; a hook runs the reset inside the send to model async_tcp). v3: Ran 8 tests OK. HEAD (git show HEAD): FAILED failures=20. v1 fix: FAILED failures=4 (race + race_keep, both consumers). Mutant without the distance guard: FAILED failures=2 (flush_never_moves_the_tail_back). Reviewer mutants m1/m2 (apply dropped in one consumer) failed 10 each, m4 (tail=head) failed 4.

Harness updates (shims only): __ATOMIC_RELAXED/__atomic_load_n MSVC stand-ins in test_otd_gateway_alt_slots.py and test_otd_mode_live_apply.py; otCmdApplyPendingFlush no-op stub in test_otd_mode_live_apply.py and test_otd_gateway_own_status.py (their own stubbed rings). Green: pic_parity 40, alt_slots 12 (1 skip), mode_live_apply 7, own_status 6, overrides_remote_json 5, msgid9 10, status_relay 9, autodetect 3, ch_override 9; evaluate --quick 92 checks, 0 FAIL.

Not covered (filed): one frame already peeked when the async_tcp reset lands can still go out after the switch, and the rest of resetTransientState is written from two tasks -> TASK-1761. Coalesce edge: a new-mode frame whose MsgID matches a stale pre-snapshot slot is dropped with it (window now one loop pass). Boot auto-detect drops the restored MsgID 71 vent setpoint -> TASK-1760. Real two-task timing not tested (modelled by the hook); PIC parity for GW=0/1 queue behaviour not checked.

Bench .61 (OT-Direct, master mode, no thermostat), beta.164+d99e552 built 21:27 from d99e552d2 (= this commit before the final docs-only rebase): OTA app+fs via scripts/ota_flash.py, 164 settings kept (changed: none), v2.html.gz complete. 15-min soak 21:29-21:44: 80/80 GET /api/v2/device/info 200, bootcount 195 steady (193 before; +2 = OTA restarts), free heap 67.6-71.7 KB, max block 31,732 B flat. The mode-switch flush itself was not exercised on the bench: switching modes on .61 persists OTDmode and there is no thermostat or wire capture to see a stale frame; evidence is the host test on the real ring and consumers.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: tests/test_otd_mode_switch_queue_reset.py compiles the real ring, otCmdEnqueue, otCmdApplyPendingFlush, scheduleMasterRequest, otdAltSlotTaken, resetTransientState and applyOTDirectMode: 8/8 OK on the fix; red on HEAD (20 failures), on the first head=tail=0 fix (4: reset between peek and tail advance replays stale slots) and on a mutant without the distance guard (2). Full suite green after harness shims (sat_command_delivery 22, sat_no_return_startup 20, override_reply 4, sat_lowload_contract 35), evaluate --quick 0 FAIL. Bench .61 beta.164: 15-min soak 80/80, no restart. Not verified on hardware: the flush on a real mode switch (host only). Follow-ups: TASK-1760, TASK-1761.
<!-- SECTION:FINAL_SUMMARY:END -->
