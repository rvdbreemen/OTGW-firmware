---
id: TASK-1924
title: >-
  SAT: send CS= on change with a 30 s repeat, recompute the flow clamp every 5 s
  while the flame is on (entries 334-337)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T07:58:00Z'
updated_date: '2026-11-14T10:49:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 986000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer order by Robert, #alpha-testing 2026-11-14: https://discord.com/channels/812969634638725140/1514720723980259460/1571061727917768815 . Repeat interval 30 s chosen by Robert (DM 1571063604466417735). Sergeant D agrees (entry 335, 1571063490989523060) and answered Q-336a B (entry 337, 1571066425966067824). Today: satSendCS() sends on every control tick and satCmdResend() repeats every 5 s (SATcontrol.ino:595, :673-683, :756-761; entries 44/237, ADR-203/207). Open points from entry 337 for the plan: the exact code condition for 'flow clamp active', and whether the PWM ON phase uses the same clamp; both go back to Sergeant D if the code does not settle them.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CS= is sent when the sent value (0.1 resolution) changes, and an unchanged CS= is repeated every 30 s; the 5 s resend of an unchanged CS= is gone (entries 334, 335)
- [ ] #2 While the flame is on and the flow clamp of entry 112 is active, the clamped CS is recomputed every 5 s and sent when it changes (entry 337)
- [ ] #3 MM=, TP= and CH= keep their on-change plus 60 s repair cadence (entries 237, 239); PIC keepalive and the 3 s PWM suppression refresh are consistent with the new CS rule
- [ ] #4 A new ADR supersedes the CS part of ADR-207 (and ADR-203 where still in force), via adr-kit
- [ ] #5 Host tests call the real sender code and fail on the old code (5 s repeat, no 5 s clamp recompute) and pass on the fix
- [ ] #6 Bench (bench-tester): CS= count per minute in steady state and during a warm-up, and no PIC CS expiry over 30 min
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Plan TASK-1924 (d3, 2026-11-14). Entries relied on: 23, 44, 112, 237, 239, 334, 335, 336, 337. Agrees with 334/335 (cadence) and 337 option B as worded; conflict found between 336's intent and the 112 clamp, see Q below.  PART 1 (AC1, AC3; settled, proceeds now) - satSendCS() (SATcontrol.ino:673) becomes on-change: it sends when the quantised value (satQuantizeTenths) differs from fLastSentCS, or bLastSentValid is false (first send after satResetCmdCache, hand-back, PIC restart, reboot), or the last CS= is SAT_CS_REPEAT_MS = 30000 old (minus 1 s slack, the TASK-1602 lesson). Commit-on-success unchanged (TASK-1574). - satCmdResend() (:756): its 5 s repeat of an unchanged CS goes. It keeps two jobs: (a) the 30 s repeat of an unchanged CS, on raw millis() in the loop (not on the 30 s tick, so the repeat cannot slip to 60 s, near the PIC's ~63 s expiry, TASK-1834); (b) the prompt retry (5 s) of a CS that the queue dropped (TASK-1574 comment :678), tracked by a pending flag set when satEnqueueOwnCS() fails. Gates unchanged: armed flag, bActive, hasOTCommandInterface, the DHW/fault pause (SlaveStatus & 0x05). - satPicCSKeepalive() (:691, PIC only, >= 8 C, 30 s, ignores DHW) stays as is: it shares iLastSentCSMs, so the two together send at most one repeat per 30 s; folding it in would drop its DHW-independent PIC keepalive. Named difference: during a DHW draw only the keepalive repeats (PIC), as today. - satEnqueueIfChangedCS() (:654, the 3 s PWM suppression refresh, entry 23) unchanged: on change, 5 min stale window; the 30 s repeat now covers it. - Callers of satSendCS: tick (:7881), DHW/fault tick paths (:7605, :7635, :7651), flood and hand-over (:1923-1944). All become on-change; each keeps its first send after a cache reset. - MM=/TP=/CH= untouched (entries 237, 239). - ADR (adr-kit, Proposed): supersedes the CS part of ADR-207 and ADR-203 where still in force. Acceptance by Robert's DM yes. - Tests: new host test on the real satSendCS/satCmdResend/satPicCSKeepalive with a fake clock: CS= count over 60 s steady state (old ~13, fix 2), change sent at once, dropped enqueue retried within 5 s, 30 s repeat lands before 31 s. The ~16 tests that encode the 5 s resend are updated one by one, each with its reason in the commit.  PART 2 (AC2; blocked on the Sergeant D question) - Code finding (sim on the real satApplyContinuous, SATcontrol.ino:2455-2497; same formula as thermo-nova heating_control.py:485-489, previous = last computed CS :454): with a constant request the clamp never follows a RISING flow (request 50, flow 30 -> 60: CS stays 50). It follows a FALLING flow (request 40, flow 45 -> 35: CS 43 -> 40). Only a rising request at a tick jumps to flow - offset (40 -> 50 at flow 55: CS 53), and then holds. - So option B (5 s recompute of that clamp) changes the ramp-down, not the start-up of entry 336. - In thermo-nova the CS that follows a rising flow at start-up is the PWM suppression (heating_control.py:435-448, flow - modulation_suppression_offset after the suppression delay, and the flame-off hold :392-408), not the continuous clamp. The firmware has that path (satApplyPWM, refreshed every 3 s by satPwmSuppressionRefresh, entry 23). Exception named: :1720, PWM idle without a flow temperature, calls satApplyContinuous. - Open point 2 of entry 337 is settled by the code: PWM ON computes its CS through the suppression, not the 112 clamp. - A second reader must check this claim before it is published (SAT rule).  QUESTION for Sergeant D (one message, via comms), see q-1924.md.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Part 1 (entries 334, 335; GO by coordinator 2026-11-14): satSendCS() on change (0.1) plus a 30 s repeat (SAT_CS_REPEAT_MS, 1 s slack for the tick stamp lag); satCmdResend() repeats an unchanged CS after 30 s on raw millis() and retries a CS the queue dropped after 5 s with the dropped value (was: the older cached value); satPicCSKeepalive() unchanged in cadence, shares iLastSentCSMs, carries a pending dropped value, caps like satCapRepeatCS and caches only on success. Choice (coordinator OK, low question queued with 92): the no-return start-up flood (TASK-1371, entries 45/142/149/151) keeps one CS= every 2 s also when unchanged, via satSendCS(cs, force=true); entry 334 read as the control tick.

Part 1 evidence (host, real code): tests/test_sat_cmd_cache.py new test_steady_state_sends_two_cs_per_minute_and_a_change_at_once: fix cs=2 per 60 s, old code (origin/dev SATcontrol.ino) cs=13; whole file fix 29/29 OK, old 13 FAIL. Updated for a stated reason (5 s resend -> 30 s repeat, CS on change): test_sat_cmd_cache, test_sat_ceiling_paths (inventory + forced flood), test_sat_cs_ceiling_1536 (repeat after 30 s), test_sat_cs_wire_matches_cache (cut starts at the new constants, repeat after 30 s), test_sat_no_return_ignition (normal boiler hold not repeated within 30 s), test_sat_ot_loss (premise repeat at 31 s, none at 2-30 s), test_sat_push_setpoint (one repeat at 130 s), test_sat_lowload_contract (LL-013 renamed tests). All 93 tests/test_sat_*.py files PASS (chunks 26+18+45 files, lowload contract 35 tests separately). Non-SAT: 153/156 PASS; test/host/test_dhw_water_meter.py FAILED after 452 s in a run that overlapped a peer build (sources not touched by this diff), rerun pending; test_refresh_storm cut off by my timeout, rerun pending. One transient WinError 225 (Defender on a temp test exe) in test_sat_ceiling_paths, rerun OK.

Part 2 (AC2): Q-337b answered by Robert B (entry 338, 'improve on the boost but only in the same method of thermo nova'): continuous mode, start-up period after flame-on: CS = max(request, flow - offset) every 5 s without the previous-CS hold; outside it the entry-112 clamp with the 5 s recompute. Open before building Part 2: start-up period length/end from thermo-nova timers (cite), which offset, Sergeant D's OK. Host sim of the real satApplyContinuous: scratchpad sim1924-clamp.cpp (request 50 constant, flow 30->60: CS stays 50).
<!-- SECTION:NOTES:END -->
