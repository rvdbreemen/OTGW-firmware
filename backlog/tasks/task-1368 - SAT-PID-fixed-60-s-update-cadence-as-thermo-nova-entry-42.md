---
id: TASK-1368
title: 'SAT PID: fixed 60 s update cadence as thermo-nova (entry 42)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T21:43:00Z'
updated_date: '2026-10-28T21:30:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
references:
  - TASK-1412
documentation:
  - docs/sat/sat-low-load-agent-handoff-2026-10-28.md
priority: medium
ordinal: 503000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (Q10, entry 42): follow thermo-nova, PID updated every 60 s (climate.py:712-716) instead of on the 30 s control tick (audit #14). Blocks ADR-197.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 PID updates every 60 s independent of the control tick (host test red/green)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Status: plan written, implementation WAITS for Sergeant D (Q55, queued behind Q15b). Full scout report with citations: session scratchpad scout-3.md / verify-10.json (2026-10-28).

1. Covered by recorded entries (thermo-nova pinned 7beaaf5):
- Entry 42 (Q10 'follow thermo-nova'): the main PID runs on its own 60 s timer (climate.py:712-716, pid.py:20), independent of the control tick, plus one start-up run (climate.py:137, :143). pid.update has no time or error-change gate (pid.py:159-171); entry 75 (:3425) puts the unchanged-error skip with this task and permits the 10 s gate removal here.
- Entry 28 (PID identical to pid.py): the output is computed on every read: curve + kp(curve)*held error + I + kd(curve)*held raw derivative (pid.py:60-132; climate.py:375 reads it every loop). Only error, integral and raw derivative are held between steps. Visible change: between steps the CS follows the curve (outside temperature, target) at tick rate; today the whole output is frozen until the next update (SATpid.ino:299-309 return the stored output).
2. Open, needs his decision (Q55):
- Zones: area PIDs on their own 60 s timer as area.py:183-190 (entry 42 records he did not mention zones). Firmware zone PID adds ki*error*60 every tick (SATcontrol.ino:2278).
- Does the 60 s step run while the control tick returns early? thermo-nova keeps stepping during hot water (control_pid is its own timer). Firmware-only early returns with no thermo-nova counterpart: boiler fault (SATcontrol.ino:5302-5306), thermal-safe fallback (:5318-5335), summer simmer (:5355-5366), valves closed (:5369-5381, 'Don't update PID integral'), plus SAT off / trip / OT loss / MQTT fallback (before DUE, :5130-5160).
3. Handled whatever he answers:
- satPidCurveOnly sets _pid_lastError = NAN (SATpid.ino:127): a per-tick output must give curve-only while the held error is NaN, never a NaN CS.
- The 10 s gate (SATpid.ino:299-304) blocks the start-up step after satPidReset (:102): remove it here (entry 75 allows) or land 1416's gate removal in the same build; no tester build in between.
- Step count under SKIP drift (each step slightly > 60 s after the previous): AC states 'steps = floor(elapsed/60 s) within one, plus the start-up step'.
- Solar-freeze (bSolarGainActive) and deadband-widening inputs are read at step time; the widening (SATcontrol.ino:5314-5340) must not silently leak or be lost: name the choice in the implementation.
- New timer declared with an explicit type and never re-set by an untyped CHANGE_INTERVAL_SEC (entry 109).
Verification: LL-003 adapter (scripts/verify_sat_low_load.py:127-171) red on HEAD (.66/.66/.30/.15 integral after 600 s at 10/30/60/120 s ticks), green on the fix with a host-compilable satPidService(nowMs); source guards; bench old vs new (telnet SAT lines, I moves only on 60 s steps, CS each tick). Not cross-line (SAT 2.0.0 only). ADR-197 Context 3 / Decision 3 updated (Proposed).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): TASK-1412.3 dependency removed: decision recorded (entry 42) and LL-003 (scripts/verify_sat_low_load.py:127-171) is its red test in substance; this task must carry its own red/green host test and update the LL-003 adapter. LL-020 (PID/clamp/scheduler, PENDING in 1412.3) absorbs the test later.

2026-10-28 advisor review: open points are SAT control behaviour -> implementation waits for Sergeant D (skill step 4; CLAUDE.md 'an open question blocks the implementation'). Q55 queued in open-questions.md ahead of Q53. The 2026-10-28 'do not resume before approved contract and red tests' note still applies; the graph edge to 1412.3 was removed only because the task carries its own red test (LL-003).

2026-10-28 Q55 answered (entry 111): full thermo-nova; zones get the 60 s timer; the PID steps through hot water, fault, thermal-safe, simmer and valves closed; stops only when SAT is off (OT loss turns SAT off); no room temperature = curve only (entries 38/46), no integration. Unblocked.

2026-10-28 review round 1 (opus correctness, sonnet SAT/rules, sonnet tests with 12 mutants). Fixing in round 2: first-output D=0 (entry 95 Q39b) here instead of waiting for TASK-1270; deadband widening capped at 2 h as the tick and applied to zones; reset/start-up and room-recovery tests; real widening path test; comment/spec truth; LL-003 comment (it cannot tell old from new: green on HEAD); env-override path; broken TASK-1195 derivative harness adapted. Not fixed, put to Sergeant D: boot with a saved state but no room reading yet (entries 46 vs 83) -> Q56 queued. Noted, not fixed: derivative interval depends on ms jitter at exactly 60 s (deltaTime <= 60 boundary, as thermo-nova pid.py; bench item with entry 77/TASK-1418); published P/D/output stay at the last tick value while the tick skips (I and error move).
<!-- SECTION:NOTES:END -->

## Comments

<!-- COMMENTS:BEGIN -->
author: @codex
created: 2026-10-28T20:07:00Z

---

Robert requested this builder handoff from Codex. Read docs/sat/sat-low-load-execution-backlog-2026-10-28.md (current schedule) and docs/sat/sat-low-load-agent-handoff-2026-10-28.md before the next task. Keep current 1368 and 1409 work; this update changes only metadata/docs. Main PID chain: 1368 -> 1270 -> 1416 -> 1417 -> 1418 -> 1414 -> 1423 -> 1415. 1381 branches after 1270 and still waits on Q53; 1415 waits on Robert for write cadence. Q55 is answered by entry 111. 1409 is independent, followed by telemetry verification with 1425. Include 1422/1424 and 1413 in their integration gates. Do not resolve the ID6/MaxTSet contract or any open question by assumption. Per-fix bench old-vs-new proof is required before tester alphas; formal 1412.18 -> .26 validation remains required. Do not stage or overwrite another worker's source/task changes; coordinate shared SATpid.ino edits one at a time.

---
<!-- COMMENTS:END -->
