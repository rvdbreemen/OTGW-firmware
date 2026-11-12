---
id: TASK-1839
title: >-
  OT-Direct: GW=0 or GW=1 wipes all command state, even when the mode does not
  change
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:50:00Z'
updated_date: '2026-11-12T04:06:00Z'
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
- [ ] #2 decide and record (ADR or task note) what a real mode change keeps (PIC parity = everything except what ADR-223 flushes from the queue)
- [ ] #3 host test for GW=0 then GW=1 with HW=0 and an SR= entry set
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): which resets master and loopback mode need for their own reasons.

Part of the PIC 6.8 parity audit, parent TASK-1851.

AC1 implemented (lane-1762b, local): setOTDirectMode() applies the mode only when it differs from otCurrentMode (the setting is still written, since a boot auto-detect can leave running and stored mode different). The REST mode path (OTDirect.ino:2308) already compared. tests/test_otd_gw_same_mode.py compiles the real setOTDirectMode() + applyOTDirectMode() with a resetTransientState() counter: 4 OK; on the pre-1839 code 3 failures (GW=1 in gateway (1, 1) != (0, 1), GW=0 in monitor, three GW=1 give 3 resets); a real change (gateway -> monitor) still resets in both. OTD suites 22 files pass, override_reply OK, evaluate 86/0. AC2 (what a real mode change keeps; PIC parity would be everything except what ADR-223 flushes) is Robert's decision, asked via the coordinator's DM; AC3 (GW=0 then GW=1 with HW=0 and an SR= entry) depends on that answer.

2026-11-12 shipped: origin/dev 6be2fa59b..f1bb2a000, zip 2.0.0-beta.200+f1bb2a0 posted #alpha-testing msg 1570288402668982272. Before the build: full run_tests.py 212/213 (test_stream_real_cbuf under -j 4, exit 0 alone, TASK-1854), evaluate --quick 86/0. Bench by 09 on .61 (old beta.197+4cafc97 vs new): OTA app+fs, githash f1bb2a0, bootcount 420 -> 422, health 200, MQTT, crashlog none, settings 164 unchanged; restored (settings diff 0). Evidence: 09 scratchpad b200/ (OLD-results.txt, NEW-results.txt, check200.py). TASK-1839 AC1 (beta.199) bench PASS old-vs-fix over TCP 25238 (PR=W / PR=O replies, REST override lists), HW=0, CS=40, SR=60:1,2 set first: (a) gateway, same-mode GW=1: OLD all cleared (W=A, O=N, overrides empty), NEW all kept (W=0, O=C40.00, write msgid 1 = 10240, response msgid 60 = 258); (b) real change GW=0 then GW=1: both cleared, as before; (c) master, same-mode GW=2: OLD cleared, NEW kept. AC2/AC3 wait for Robert's DM answer (coordinator msg 1570229987825418384).
<!-- SECTION:NOTES:END -->
