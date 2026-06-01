---
id: TASK-801
title: 'feat-2.0.0: SAT sim F6 — command-trace ring buffer'
status: To Do
assignee: []
created_date: '2026-05-31 22:56'
updated_date: '2026-06-01 04:16'
labels:
  - sat
  - simulation
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up F6 from SAT simulation plan section 12. Replace the single-slot last_blocked_cmd with a ring of the last 10-16 blocked commands for short-burst diagnostic scenarios. MVP single-slot is sufficient to verify the contract; this is the enhancement. Depends on TASK-795. Milestone 2.0.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Ring buffer of the last N (default 16) blocked commands replaces the single-slot trace; bounded BSS, no heap
- [ ] #2 satSimulationBlocksBusTx() pushes into the ring; satOnBoilerDetected() teardown clears the whole ring
- [ ] #3 REST sat status JSON exposes a last_blocked_cmds array (most-recent-first); MQTT keeps the single newest non-retained
- [ ] #4 python build.py both targets SUCCESS; evaluate.py --quick clean
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Source: plan §12 F6 + §4.3 (command trace) + §17 pitfall #7 (MQTT shadow publish-on-change). Depends on TASK-795 commit-3 (the single-slot sLastBlockedCmd trace must exist first).

GOAL: replace the single-slot last-blocked-command trace (sLastBlockedCmd[24] / iLastBlockedCmdMs from commit-3) with a small ring buffer of the last 10-16 blocked commands, for short-burst diagnostic scenarios where one slot loses history.

DESIGN:
- Ring in SATRuntimeSection: struct { char cmd[24]; uint32_t ms; } simTrace[SAT_SIM_TRACE_RING]; uint8_t simTraceHead; with SAT_SIM_TRACE_RING = 16 (in boards.h per ESP-abstraction Tier 3 if it should differ ESP8266 vs ESP32 — 16 is ~640 bytes, fine on both, so a plain const is OK). RAM budget: 16*28 ~= 448 bytes BSS, acceptable.
- satSimulationBlocksBusTx() (commit-3 helper) pushes into the ring instead of overwriting the single slot. Keep the single sLastBlockedCmd as head alias for the existing REST/MQTT surfaces (back-compat) OR migrate them to read ring[head].
- New REST surface: extend the sat status JSON (restAPI.ino ~1896) with a last_blocked_cmds array (most-recent-first, capped). MQTT: keep sat/sim/last_cmd as the single newest (non-retained); optionally add nothing else to avoid topic churn.
- Clear the ring in satOnBoilerDetected() teardown (commit-3 already clears the single slot — extend to the ring).

OPEN QUESTION (morning): ring size 16 vs 10; expose full ring over REST array or keep MQTT single-slot. Default: 16, REST array + MQTT single newest.

VERIFY: build both; evaluate --quick; with bSimulation, issue >16 blocked commands and confirm the ring holds the last 16 in order via REST.
<!-- SECTION:PLAN:END -->
