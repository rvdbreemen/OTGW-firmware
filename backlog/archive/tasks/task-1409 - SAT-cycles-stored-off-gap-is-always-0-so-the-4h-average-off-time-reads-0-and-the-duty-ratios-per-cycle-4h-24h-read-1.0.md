---
id: TASK-1409
title: >-
  SAT cycles: stored off-gap is always 0, so the 4h average off-time reads 0 and
  the duty ratios (per cycle, 4h, 24h) read 1.0
status: Done
assignee:
  - '@claude'
created_date: '2026-10-26T20:12:00Z'
updated_date: '2026-10-28T22:16:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
references:
  - TASK-1412
documentation:
  - docs/sat/sat-low-load-agent-handoff-2026-10-28.md
ordinal: 547000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-10-26 while answering Alexw (flame tracking), second reader confirmed: the off-gap stored with each cycle record is always 0, so reporting shows avg off-time 0 and duty ratio 1.0 (per cycle, 4h window, 24h window). Reporting only; no control path reads it. See workflow wf_4f8a262f-33d research 'flame-tracking' for the exact lines.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Off-gap per cycle = time from the previous flame-off to this flame-on (host test red on HEAD)
- [x] #2 4h average off-time and duty ratios show real values on a replay or bench run
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Cause: at flame-off satCycleOnFlameChange sets _cycle_flameOffStartMs = now (SATcycles.ino:825) before the off gap (:891-893) and the per-cycle duty (:671-672, in _cycleRecord) are computed, so flameOnStart > flameOffStart is never true -> offMs 0, duty 1.0, 4h avg off 0, 4h/24h duty 1.0. Fix: compute both from _cycle_lastFlameOffMs, the previous flame-off edge (set after use at :911, init :306). Readers (grep): JSON/MQTT/debug only (SATcontrol.ino:2917, 2928, 3353-3363; restAPI.ino:2389; handleDebug.ino:233); satCycleGetFlameOffStartMs (flame status, SATcontrol.ino:4220) keeps reading _cycle_flameOffStartMs, unchanged. No control path, no SAT behaviour choice. Test: extend tests/test_sat_cycle_classifier.py (compiles the real satCycleOnFlameChange): two cycles with a known off gap -> stored offDurationMs, per-cycle duty, 4h avg off and duty; red on HEAD copy, green on fix. Not cross-line.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): TASK-1412.3 dependency removed: reporting only, own host-test AC#1.
<!-- SECTION:NOTES:END -->

## Comments

<!-- COMMENTS:BEGIN -->
author: @codex
created: 2026-10-28T20:07:00Z

---

Robert requested this builder handoff from Codex. Read docs/sat/sat-low-load-execution-backlog-2026-10-28.md (current schedule) and docs/sat/sat-low-load-agent-handoff-2026-10-28.md before the next task. Keep current 1368 and 1409 work; this update changes only metadata/docs. Main PID chain: 1368 -> 1270 -> 1416 -> 1417 -> 1418 -> 1414 -> 1423 -> 1415. 1381 branches after 1270 and still waits on Q53; 1415 waits on Robert for write cadence. Q55 is answered by entry 111. 1409 is independent, followed by telemetry verification with 1425. Include 1422/1424 and 1413 in their integration gates. Do not resolve the ID6/MaxTSet contract or any open question by assumption. Per-fix bench old-vs-new proof is required before tester alphas; formal 1412.18 -> .26 validation remains required. Do not stage or overwrite another worker's source/task changes; coordinate shared SATpid.ino edits one at a time.

---
<!-- COMMENTS:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cause: satCycleOnFlameChange(false) set _cycle_flameOffStartMs = now (SATcycles.ino) before the stored off gap and the per-cycle duty were computed, so flameOnStart > flameOffStart was never true: off gap 0, duty 1.0, hence 4h avg off 0 and 4h/24h duty 1.0. Fix: both now use _cycle_lastFlameOffMs, the previous flame-off edge (wrap-safe signed compare). Readers checked (grep): JSON, MQTT, REST and debug only; no control path; the flame-status check keeps reading _cycle_flameOffStartMs unchanged.

Evidence (this session):
- AC1/AC2: tests/test_sat_cycle_classifier.py, new test compiling the real satCycleOnFlameChange, satCycleSample and satGetWindow4hStats; scenario 300 s on, 600 s off, 300 s on. Working tree: Ran 26, OK. HEAD copy: FAILED (failures=4): off gap 0 != 600000 ms; EMA duty 0.2775 != 0.1775 (300/900); 4h avg off 0.0 != 300.0; 4h duty 1.0 != 0.5.
- build.bat esp32-combo SUCCESS (firmware 23:14, littlefs 23:15), evaluate --quick Failed 0 / 100%; SAT tests overshoot_sustain, pwm_override, pwm_start_limit, ot_loss, pwm_anchors OK.
Not verified: on a bench or real boiler (host replay of the real code only); the 24 h duty path (_dailyRecord stubbed in the harness). The first cycle after boot has off gap 0 (no previous flame-off edge).
<!-- SECTION:FINAL_SUMMARY:END -->
