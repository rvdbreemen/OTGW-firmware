---
id: TASK-2993
title: 'SAT: PWM duty always with the live flow, no held BT (entry 437, Sergeant D)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T03:28:00Z'
updated_date: '2026-11-21T12:47:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1120000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, entry 437 (#dev-sat-mqtt 2026-11-21 03:25 UTC, msg 1573539719147552939): duty = (requested setpoint - 27.2) / (flow - 27.2), always with the live flow (MsgID 25). Today BT is an EMA for 180 s after ignition and then held (SATcontrol.ino:1648, 1669-1686; as thermo-nova pwm.py:144-149, 229-239). Field case indigo_light 19 Nov 21:27-21:33: live-flow duty 101 % then ~47 %, logged duty 97-100 % with BT held at 32.12 C -> range 5, MM=T, saturation exit 21:32:30, fail-safe 21:33:30. Reverses Robert's entry-64 choice (keep 7beaaf5): Robert's OK asked in-channel (msg 1573540314436730942). Open: start-up filter kept for 30 s or dropped (asked, msg 1573540311874142270). Likely makes TASK-2992 (entry 436) moot; keep both until Sergeant D says so.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert's OK (reversing entry 64) and Sergeant D's answer on the start-up filter are recorded in docs/sat/sergeant-d-corrections.md before any code change
- [ ] #2 A host test on the real PWM duty code shows the duty staying at 1.00 with a held BT while the live flow rises (current code, red) and following the live flow with the fix (green); base+1 floor kept
- [ ] #3 The start-up filter follows Sergeant D's answer (none, or N s), with a test for the window edge
- [ ] #4 Replay of indigo_light 19 Nov 21:27-21:33 gives live-flow duties of about 101 % then about 47 %, no range 5 after 21:28:30, no MM=T, no saturation exit; replay reports the remaining switches with TASK-2987 + 2989
- [ ] #5 Bench via the NAS (replay or simulation) before the push
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: Sergeant D answered B (entry 438): keep the start-up EMA for the first 30 s after ignition, then the live flow until the next ignition. Waiting only for Robert's OK on reversing entry 64.

2026-11-21 03:34 UTC: Sergeant D entry 439: this duty fix is the only SAT change for 2.0.0 from this thread; afterwards the next field runs are checked for remaining switching. Still waiting for Robert's OK on reversing entry 64.

2026-11-21 (92): Robert's OK for 2.0.0 recorded as entry 441 (DM 1573595110879395942, 'A', reverses entry 64). Q-438a answered A (entry 443): the 30 s start-up filter stays. Q-438b (live flow in the OFF phase) asked 07:07 UTC, msg 1573595715278602335; open.

2026-11-21 (92): Q-438b answered B (entry 444, msg 1573596529984544881): the live flow only in the PWM ON phase; in OFF the duty keeps the last ON value. Interpretation for you to confirm with a test: hold the last ON flow and recompute with the current request. All open Sergeant D questions on this thread are answered (queue 0).

2026-11-21 (92): Q-444a asked 07:41 UTC (msg 1573604247805890651): hold the last ON flow (A, as built) or hold the duty (B) during OFF; without an answer before 17:30 the 20:00 beta goes with A. Known limit (Q-444b, parked for 2.1.0): a hot-water draw near the end of an ON phase makes the held flow the DHW-heated primary flow (about 55-60 C) for the whole OFF phase, so a long pause; rare, not handled in 2.0.0.

2026-11-21 (92): Q-444a answered (entry 446, msg 1573605458000879747): 'during the PWM OFF we keep the last PWM ON boiler flow temperature ... we freeze the flow temperature'. Read as A = as built (frozen flow, duty recomputed with the current request). No code change needed.

2026-11-21 (92): entry 447: Sergeant D confirmed explicitly (msg 1573607198859460671): during PWM OFF the request stays active and may change the OFF duty; only the flow is frozen. That matches the build.

2026-11-21 (92): otgw-dev found on the .61 bench that the +30 s tick lands at +30003 / +30023 ms (firmware tick jitter), so a strict 30 s window would miss that tick about half the time. 1 s slack added: an implementation of entry 443 (one tick after ignition, incl. the +30 s tick), not a behaviour change; no question to Sergeant D. Requested test: tick at +30.02 s filtered, +60 s live.

2026-11-21 05:30 (otgw-dev): implemented with TASK-2992 in lane/d3-duty2993. Live-flow duty after a 30 s EMA window, inclusive (<=) as thermo-nova pwm.py:148 (review finding: with < the filter never reached the duty at the 30 s tick). Tests: test_sat_pwm_live_flow_duty_2993.py 9/9 (old code 939f9406b reproduces the indigo logged duty within 0.011; 18 FAIL on old code; window test red on the < mutant); pwm_override 83/83, pwm_anchors 64/64, pwm_first_start 15/15; SAT sweep 115/117 then the 2 rechecked green (n11 updated to the 1.03 rule, lowload_contract 35 OK alone = -j2 flake); evaluate --quick 88 passed 0 failed. Reviewer (read-only subagent) findings fixed: inclusive window, pinned test base, d.atMax on range 5, relight test keeps the window assert. Open for Sergeant D (via BlueFlame): Q1 a 30 s filter covers one 30 s tick; Q2 live flow also in the OFF phase. Stale docs -> TASK-2994. No push before Robert's OK on entry 64 and bench OK.

2026-11-21 (otgw-dev): entry 444 (Sergeant D, Q-438b B, via BlueFlame): live flow only in the PWM ON phase; in OFF the duty uses the flow of the last ON tick with the current request (_pwm_lastOnFlowTemp, reset in satPwmReset). Entry 441 (Robert A, reverses entry 64) and entry 443 (Q-438a A, 30 s window as built) recorded by BlueFlame. Test tests/test_sat_pwm_live_flow_duty_2993.py: 13/13 OK on the fix; on the 1693b64 source (OTGW_SAT_INO) 3 FAIL (LiveFlowDuty 21:31:30/21:32:00 OFF ticks use the live flow 41.5 -> 0.465 instead of the held 40.7 -> 0.49) + new class refuses (no OFF branch); mutant without the OFF line: shorter pause, duty 1.00 at the cooled flow 34.0 (red side asserted in the class).

2026-11-21 (otgw-dev): bench .61 run 1 (beta.406+1693b64, sim, control interval 10 s; otgw-bench-tester, OTGW-field-data\bench-otgw32\2026-11-21_task2993-2992\t2993-sim-61): checks 3 (MM=T only at raw > 1.03: 73/73 T, 156/156 numeric) and 4 (no saturation exit) PASS. Check 2: the +20 s ticks correctly use the EMA (08:29:29 0.33 = 2.21/(33.95-27.2), 08:57:30 0.36), but the +30 s tick landed at +30003 ms (08:29:39) and +30023 ms (08:57:40) after the ignition and already used the live flow (0.31/0.34 instead of 0.32/0.35): the <= 30000 ms bound depends on tick jitter, so at the default 30 s interval the one filtered tick of entry 443 would be missed about half the time. Fix: SAT_PWM_BT_FILTER_SLACK_MS = 1000 on both window checks. Tests WindowToleratesTickJitter (ticks at +30.026 s / +30.020 s filtered, +41.2 s and +60.03 s live): red on 9d2007d (0.77 and 1.00), green on the fix; 16/16 in the file.

2026-11-21 (92): field evidence from marceld91d's capture (beta.392, Remeha + Toon, heating_system 2 = underfloor, base 20; WeTransfer upload, analysis msg 1573678851236700273). Morning ignition 21 Nov 09:03:50 local: flow 35.1 at +16 s, then 32.2-33.3 C from +40 s to +90 s with the request 34.5-34.85, flow back above the request at about +100 s. With the beta.410 rule (30 s smoothing incl. the +30 s tick, then live flow) the raw duty would be 1.19 (+50 s), 1.12 (+80 s), 1.05 (+110 s), so MM=T for about 3 ticks / 90 s. On 392 MM=T was sent from +20 s and SAT went continuous 09:05:40-09:14:10. This is the 'dip that lasts' case of Q-438a on a real boiler; Sergeant D chose 30 s (entry 443) on indigo's dip that passed within 30 s. Night setback 17 C: 0 CH ticks between 21:31 and 09:02, room 19.0 -> 17.8. Charts: scratchpad marcel1121_charts (overview, morning-ignition).

2026-11-21 (92) CORRECTION of my marceld91d note: my raw duty 1.19 at +50 s was a hand calculation and wrong. otgw-dev's replay through the real beta.410 code (REPORT.md in his scratchpad marcel/): the 30 s window is anchored at the first tick that sees the flame (09:04:10), so 09:04:40 still uses the EMA (0.962); +80 s 1.117 and +110 s 1.046 match (base 20). MM=T at 09:05:10 and 09:05:40 at both bases. Key result: beta.410 switches exactly like beta.392 at this ignition: 09:05:40 PWM -> continuous by ADR-236 (request > flow for 60 s, 0 C margin; gate ended 09:04:50), 09:14:10 fail-safe -> PWM. The duty fix and the 30 s window do not change these switches; this is the guard case (TASK-2989 margin), sequenced after the public duty release by entry 445.
<!-- SECTION:NOTES:END -->
