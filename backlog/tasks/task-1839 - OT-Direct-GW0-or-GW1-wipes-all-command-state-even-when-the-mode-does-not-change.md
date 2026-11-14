---
id: TASK-1839
title: >-
  OT-Direct: GW=0 or GW=1 wipes all command state, even when the mode does not
  change
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:50:00Z'
updated_date: '2026-11-14T21:25:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 920000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N7), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: SetGatewayMode only switches comparators and saves the mode; current mode = no-op (G:4729-4747, :4749-4774). CS, MM, HW, BW, SR=, UI=, TT/TC stay.
- OT-Direct: every GW= -> setOTDirectMode -> applyOTDirectMode -> resetTransientState (O:3368-3416, :2735-2737, :2656-2728); no unchanged-mode check.
- Consequence: a client sending GW=1 (settings help gives `PS=0;GW=1` as boot-command example) drops CS, HW=0, MM, SR=, UI=.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 GW=1 in gateway mode changes no state; host test red on current code
- [x] #2 decide and record (ADR or task note) what a real mode change keeps (PIC parity = everything except what ADR-223 flushes from the queue)
- [x] #3 host test for GW=0 then GW=1 with HW=0 and an SR= entry set
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): which resets master and loopback mode need for their own reasons.

Part of the PIC 6.8 parity audit, parent TASK-1851.

AC1 implemented (lane-1762b, local): setOTDirectMode() applies the mode only when it differs from otCurrentMode (the setting is still written, since a boot auto-detect can leave running and stored mode different). The REST mode path (OTDirect.ino:2308) already compared. tests/test_otd_gw_same_mode.py compiles the real setOTDirectMode() + applyOTDirectMode() with a resetTransientState() counter: 4 OK; on the pre-1839 code 3 failures (GW=1 in gateway (1, 1) != (0, 1), GW=0 in monitor, three GW=1 give 3 resets); a real change (gateway -> monitor) still resets in both. OTD suites 22 files pass, override_reply OK, evaluate 86/0. AC2 (what a real mode change keeps; PIC parity would be everything except what ADR-223 flushes) is Robert's decision, asked via the coordinator's DM; AC3 (GW=0 then GW=1 with HW=0 and an SR= entry) depends on that answer.

2026-11-12 shipped: origin/dev 6be2fa59b..f1bb2a000, zip 2.0.0-beta.200+f1bb2a0 posted #alpha-testing msg 1570288402668982272. Before the build: full run_tests.py 212/213 (test_stream_real_cbuf under -j 4, exit 0 alone, TASK-1854), evaluate --quick 86/0. Bench by 09 on .61 (old beta.197+4cafc97 vs new): OTA app+fs, githash f1bb2a0, bootcount 420 -> 422, health 200, MQTT, crashlog none, settings 164 unchanged; restored (settings diff 0). Evidence: 09 scratchpad b200/ (OLD-results.txt, NEW-results.txt, check200.py). TASK-1839 AC1 (beta.199) bench PASS old-vs-fix over TCP 25238 (PR=W / PR=O replies, REST override lists), HW=0, CS=40, SR=60:1,2 set first: (a) gateway, same-mode GW=1: OLD all cleared (W=A, O=N, overrides empty), NEW all kept (W=0, O=C40.00, write msgid 1 = 10240, response msgid 60 = 258); (b) real change GW=0 then GW=1: both cleared, as before; (c) master, same-mode GW=2: OLD cleared, NEW kept. AC2/AC3 wait for Robert's DM answer (coordinator msg 1570229987825418384).

Robert's answer found in the bot DM channel (stale audit 2026-11-14, coordinator request): DM 1570230662743592961, 2026-11-12 01:16:03 UTC, verbatim 'Vraag taak 1834: A / Vraag taak 1839: A', answering the bot's question 1570229987825418384. Option A: like the PIC, a real mode change (monitor <-> gateway) keeps all command state; only the queue is flushed as ADR-223 already does; a CS= stays valid after the switch back to gateway until it expires. This settles the decision AC2/AC3 waited for.

AC2/AC3 implemented (d3, 2026-11-14, local, not yet built). Scope of Robert's answer A, read from the question itself (bot DM 1570229987825418384: 'wat een ECHTE moduswissel (monitor <-> gateway) moet bewaren'): a switch between gateway and monitor keeps all command state and only flushes the queue (new helper otdFlushCmdQueue(), ADR-223); a switch into or out of master, loopback or bypass keeps the full resetTransientState() (the PIC has none of those modes, outside the question; ask Robert separately if wanted). GW=R unchanged (full reset + flush). Checked: monitor mode applies none of the kept state (raw relay OTDirect.ino:2476, no own frames :2545, reply modifiers gateway-only :1699); the CS expiry clock runs every second in every mode (:2557-2581), so a CS= kept across monitor expires on time. New tests/test_otd_mode_change_keeps_state.py on the real otdFlushCmdQueue/resetTransientState/applyOTDirectMode/setOTDirectMode: GW=0 then GW=1 with HW=0, SR=60, CS=40: HEAD hw 255, SR/CS cleared (2 FAIL); fix all kept, flush set (6/6 OK); into master and GW=R still clear on both. test_otd_gw_same_mode updated (gateway->monitor resets 0, ->master 1), test_otd_mode_switch_queue_reset cuts the new helper. OTD suite 36/36 after the fix. Docs: c4-code-otdirect.md. Finding (not in scope, to file): at boot initOTDirect() restores the persisted HW/UI (otdRestorePersistedCommands) BEFORE the auto-detect switch into master (:1311), whose full reset then wipes HW=0 and the UI= list again.

Bench 09 on .61 (OTGW32, OT-Direct), bench build 2.0.0-beta.252+941a224 (d3 lane; sha256 match, OTA OK, bootcount 647 -> 649, crashlog identical, MQTT up). PASS core case: in gateway HW=0, CS=40, SR=60:1,2 (write override [1:10240], response [60:258], PR W=0, O=C40.00); GW=0 -> M=M with W=0, O=C40.00, overrides unchanged; GW=1 -> M=G, all unchanged. PASS: GW=S (master) and GW=R from gateway with state clear everything (W=A, O=N, overrides empty); a first GW=0 from S (boot auto-detect master) clears, as a switch out of master should. NOT testable on .61: monitor-mode relay with a kept SR=/HW=0 (no thermostat or boiler, no relayed frames; covered by the source checks in test_otd_mode_change_keeps_state) and the queue flush (queue empty before every switch; covered by the host test). Evidence: 09 scratchpad b1839/. CORRECTION of d3's bench brief: it said 'GW=3 (master)'; master is GW=2 / GW=S in the handler (OTDirect.ino ~3725); 3 is only the enum value OTD_MODE_MASTER. 09's finding from that: GW=3 gets an echo 'GW: 3' with no mode change, where the PIC answers BV (BadValue) to any GW= value other than 0, 1 or R (gateway.asm 6.8 CheckBoolean :4072-4082, ResetGateway :4776-4780); also after GW=3 the REST write-override list lost [1:10240] while PR=O still said C40.00 (cause not traced). Filed as a separate low parity task.

Cause of the lost [1:10240] after GW=3 in 09's bench (see TASK-1946): the ~64 s CS heartbeat expiry (TASK-1811), logged at 19:33:12.36, 0.5 s after PR=O still answered C40.00 (09 scratchpad b1839/telnet.raw). Not related to TASK-1839's mode-change code.
<!-- SECTION:NOTES:END -->
