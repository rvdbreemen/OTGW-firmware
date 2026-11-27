---
id: TASK-3123
title: >-
  refuse /ws handshakes for a cooldown (30 s) after an emergency recovery that
  aborted >= 1 /ws client (reconnect loop observed on .76 with emulated tabs,
  bench-observed only)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-27T12:37:00Z'
updated_date: '2026-11-27T13:37:00Z'
labels:
  - webui
  - heap
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1178000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After emergencyHeapRecovery() aborts the /ws clients (ADR-295), the browser tabs reconnect after 5 s (v2.js:3037-3040). On the unit without PSRAM this drove the heap under 3 KB again within about 50 s and the next recovery aborted the tabs again (B1 run 2: recoveries at 13:24:05 and 13:25:05). The handshake gate (TASK-3119, wsAdmitHandshake() in webSocketStuff.ino) refuses a handshake on low heap or at the client cap. Add a third reason: an emergency recovery less than WS_RECOVERY_COOLDOWN_MS (30000) ago; the refusal takes the existing path (count, log, abort). Evidence: OTGW-field-data/bench-61/2026-11-27_b1k/, OTGW-field-data/bench-61/2026-11-27_456/crash_run1/decoded.txt, decode note OTGW-field-data/_comms/decode_456_76_k_panic_2026-11-27.md. Draft by otgw-architect: OTGW-field-data/_comms/task_draft_ws_cooldown_and_badalloc_2026-11-27.md. Bench-observed only with a prober heavier than the UI, not a verified field behaviour. No ADR needed (tightens the TASK-3119 gate; the canSendWebSocket() contract of ADR-167 is not touched); if review finds otherwise, ask otgw-architect for an ADR. Owner: otgw-dev, after TASK-3122.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 wsAdmitHandshake() gets the time of the last emergency recovery as an argument (pure, testable) and returns false while now - lastRecoveryMs < 30000, with its current behaviour after that; the millis() wrap is handled with an unsigned difference
- [ ] #2 The recovery path stores the time only when it aborted at least one /ws client (ws_aborted > 0); a recovery that did not touch /ws does not block the web UI
- [ ] #3 A refusal for this reason is counted separately (a new burst event or a new reason in the existing counter) and shows in telnet as 'refused at the handshake (recovery cooldown, N s left)'
- [ ] #4 Host test tests/test_ws_handshake_cooldown.py compiling the real wsAdmitHandshake(): refuses at +0 s and +29.9 s, admits at +30 s, admits when no recovery happened, handles a wrap across 2^32; red on the code before the change; a mutant with the cooldown at 0 ms also fails
- [ ] #5 Bench (otgw-bench-tester) on the unit without PSRAM, B1 run 2 recipe, n = 3: after each recovery no accepted /ws handshake within 30 s, no second recovery within 120 s of the first, tabs back within 40 s, the control run on the build before shows the loop; the prober load is recorded so the result is not credited to the prober
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 otgw-dev: implemented locally on lane/ws-cooldown (wt-d3-cool, on top of 3f9176298 + a merge of origin/dev), not built, not pushed. wsAdmitHandshake(freeHeap, connected, nowMs, lastRecoveryMs) refuses while (uint32_t)(now - lastRecoveryMs) < WS_RECOVERY_COOLDOWN_MS (30000); 0 = no recovery yet; emergencyHeapRecovery() calls webSocketNoteRecoveryAbort() only inside the branch where doWebSocketDisconnectAll() aborted >= 1 client (AC2); the handler counts the refusal as its own burst event WS_BURST_REJECTED_COOLDOWN and logs 'refused at the handshake (recovery cooldown, N s left)' (AC3). tests/test_ws_handshake_cooldown.py (7 tests): REAL functions on the host: refuses at +0 and +29.9 s, admits at +30 s, admits with no recovery, wrap across 2^32, old rules after the cooldown, mutant with 0 ms cooldown red, wiring guards; red before (functions absent). test_ws_handshake_heap_gate updated for the new signature. evaluate --quick 0 failed. NOT done: AC5 bench (n=3 on .76, control run before). The cooldown refuses through the library 401 path (see the gate-abort discussion): this lane does not include 2404fb9ae.

2026-11-27 otgw-dev: lane A (lane/ws-lowmem, wt-d3-cool, local, not built): the cooldown and the gate abort (from 2404fb9ae, re-written into the new handler) are runtime arms in prerelease builds: telnet m = wscool, t = wsabort (REST names wscool, wsabort), RAM only, both ON at boot; a release build has no switch and runs both. tests/test_ws_handshake_lowmem_arms.py compares the REAL new handler with the REAL B1 handler (3f9176298, via git show) on a grid: both off = byte-identical output and no abort; abort only = identical plus one abort per refusal; cooldown only = own burst event and text inside 30 s, identical outside; release build ignores the switches; mutant (abort arm stuck on) red. K-hook question: the K bench hook only drives free heap under 3072, the recovery then runs through the same emergencyHeapRecovery() (OTGW-firmware.ino:1109), so the cooldown applies to K-induced recoveries when they aborted >= 1 /ws client. User-visible effect: after a recovery that closed the web UI's live link, a browser tab cannot reconnect for 30 s (the page shows its reconnect banner and retries every 5 s); a recovery that did not touch /ws changes nothing.

2026-11-27 otgw-dev (coordinator rule): the release-build default is one compile-time constant per measure in webSocketStuff.ino, WS_RELEASE_COOLDOWN and WS_RELEASE_ABORT, both false (= B1 behaviour). Neither measure ships in a release candidate until the lane A matrix (n >= 5 per arm on .76: base / cooldown / abort / both) shows the arm is no worse than base on stall length and panics; the release default then follows the data with a one-line change (could stay base only). Prerelease builds keep both ON at boot for the bench.
<!-- SECTION:NOTES:END -->
