---
id: TASK-452
title: >-
  Investigate SAT scaffolding flagged as unused: satOvpCalibrate,
  satGetHeatingSystemName, thermal/PWM state
status: Done
assignee:
  - '@claude'
created_date: '2026-04-27 19:21'
updated_date: '2026-04-27 20:14'
labels:
  - sat
  - investigation
  - warnings
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Context

ESP8266 build flags four SAT-related symbols as "defined but not used" / "unused variable":

- `SATcontrol.ino:221` — `static const char* satGetHeatingSystemName()`
- `SATcontrol.ino:242` — `static uint32_t _thermal_learningStartMs = 0;` (Task #21 thermal-drop learning)
- `SATcontrol.ino:254` — `static void satOvpCalibrate()` (full state machine, ~80 lines)
- `SATcontrol.ino:566` — `static uint32_t _pwm_lastSampleMs = 0;` (PWM control mode)

These are not orphan helpers: ADR-076, doc-2 (sat-opv-calibration), feature-completeness matrix and sat-python-cpp-mapping all list `satOvpCalibrate` as the implementation of a documented feature. The compiler's "unused" verdict means the call-site is missing or behind a never-true condition.

## Goals

1. **Per symbol**: locate the *intended* call-site in the SAT control loop / REST/MQTT command path / discovery flow.
2. Decide outcome per item:
   - **Wire up** — feature is real, missing call should be added (links back to feature task).
   - **Mark unused** — feature is parked; annotate with `[[maybe_unused]]` and a `// TASK-NNN: parked, see ADR-XXX` comment so the warning stays silent without losing the code.
   - **Delete** — feature is superseded; link to the superseding ADR/task in the commit message.
3. Coordinate with any open SAT tasks (Task #21 thermal learning, ADR-076 OPV calibration) before touching shared state.

## Acceptance Criteria
<!-- AC:BEGIN -->
See AC list. Decision per symbol must be documented in the task notes with file:line reference and rationale.

## References

- `docs/adr/ADR-076-sat-opv-calibration.md`
- `backlog/docs/sat-feature-completeness-matrix.md`
- `backlog/docs/sat-python-cpp-mapping.md`
- `backlog/tasks/task-5 - Overshoot-Protection-Value-OPV-calibration.md`
- `backlog/tasks/task-97 - SAT-Audit-B6-OPV-calibration-algorithm.md`
- `backlog/tasks/task-224 - SAT-Add-minimum-OPV-calibration-dataset-requirement.md`
<!-- SECTION:DESCRIPTION:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Plan

Apply the four recommendations from the investigation agent's report (verified via grep + ADR-076 + feature-completeness matrix):

### 1. satOvpCalibrate → WIRE UP

Add the missing dispatch in `satControlLoop()` near line 3425. Pattern recommended by agent: `if (state.sat.eCalibPhase != SAT_CALIB_IDLE) satOvpCalibrate();`. Verify exact insertion point first by reading satControlLoop body — must come AFTER state-establishing reads but BEFORE any return paths that might skip dispatch.

### 2. satGetHeatingSystemName → PARK

Mark with `[[maybe_unused]]` (or `__attribute__((unused))` for compiler portability — verify which attribute the codebase prefers) + one-line comment pointing to TASK-452 / future diagnostic endpoint. Keep the function body unchanged.

### 3. _thermal_learningStartMs → DELETE

Single-line static variable at line 242. Superseded by `_thermal_totalLearnMs` (cumulative tracking, lines 3041-3044). Confirm no read/write sites via grep before deleting.

### 4. _pwm_lastSampleMs → DELETE

Single-line static variable at line 566. PWM implementation gates on flame state, not sampling intervals. Confirm no references via grep before deleting.

## Verification

- ESP8266 + ESP32 incremental build clean.
- Grep confirms no orphan references to deleted symbols.
- For wire-up: smoke test the OPV calibration path mentally (REST/MQTT trigger → eCalibPhase set → control loop dispatches state machine).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation completed (2026-04-27 20:15):

**Wire up — satOvpCalibrate**: Added 1 dispatch line in satControlLoop() at SATcontrol.ino:3427-3428, between satUpdateThermalLearning() and the safety-tripped bail. Pattern: `if (state.sat.eCalibPhase != SAT_CALIB_IDLE) satOvpCalibrate();`. Placed above the timer-gated control logic so the state machine polls every loop iteration (calibration phases need fast polling, not 10s timer cadence). Honors the SAT-disabled / flashing early-bail at line 3415 (calibration cannot run if SAT is disabled).

**Park — satGetHeatingSystemName**: Added `__attribute__((unused))` prefix annotation (GCC-portable, conservative for Arduino vs C++17 [[maybe_unused]]). Added comment: 'Kept available for future diagnostic endpoint exposing heating-system type.' Did not reference TASK-IDs in comment per CLAUDE.md 'comments about the present only'.

**Delete — _thermal_learningStartMs and _pwm_lastSampleMs**: Both were single-line static declarations with zero read/write sites confirmed via grep. Removed cleanly without context shifts.

**Verification**:
- Build set-diff TASK-455 baseline (62) vs TASK-452 (58) = -4/+0 (exactly the four SAT warnings, no new warnings).
- Both ESP8266 and ESP32 builds exit 0.
- grep for the four symbols in /src returns: satGetHeatingSystemName (1 def + 0 calls — annotated unused), satOvpCalibrate (1 def + 1 call — wired), _thermal_learningStartMs and _pwm_lastSampleMs (0 hits — deleted).

**Hardware caveat**: The OPV calibration wire-up is a real behavioral change. End-to-end verification (REST trigger → state machine traversal → CS/MM commands → boiler response → OPV value persisted) requires hardware. Build test only proves it compiles; runtime correctness needs a boiler + 30 min calibration session.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## Summary

Acted on the four investigation-agent recommendations for SAT scaffolding flagged as compiler-unused:

- **`satOvpCalibrate` — wired up.** ADR-076 documents this as a FULL feature, but the state-machine pump was missing from the control loop. Added 1-line dispatch in `satControlLoop()` between thermal-learning update and safety-trip check, gated by `state.sat.eCalibPhase != SAT_CALIB_IDLE`. The state machine now polls every loop iteration (above the 10s control timer) so calibration phases progress at proper cadence.
- **`satGetHeatingSystemName` — parked.** Annotated with `__attribute__((unused))` and a one-line comment noting it is kept available for a future diagnostic endpoint. No current consumer; safe to retain at zero compile cost.
- **`_thermal_learningStartMs` — deleted.** Superseded by `_thermal_totalLearnMs` (cumulative tracking at lines 3041-3044). Confirmed zero read/write sites before removal.
- **`_pwm_lastSampleMs` — deleted.** Scaffolding from earlier PWM-design iteration; current `satApplyPWM()` gates on flame state, not sampling intervals. Confirmed zero references before removal.

## Verification

- ESP8266 build: 62 warnings → 58 warnings, set-diff -4/+0 (exactly the four SAT-related warnings, no regressions).
- ESP32 build: clean (exit 0).
- `git grep` confirms: deleted symbols absent, parked symbol annotated, wired symbol now has its dispatch.

## Behavioral change (note for hardware testing)

The OPV calibration wire-up is the only non-cosmetic change in this commit. With it in place, triggering calibration via REST `/api/v2/sat/calibrate/start` or MQTT command will actually pump the state machine through STARTING → WARMING → MEASURING → DONE/FAILED → IDLE phases. Build tests only prove compilation; end-to-end correctness (boiler response under CS=max + MM=0 commands, OPV value persistence) requires real hardware and a ~30-minute calibration run.

## Files touched

- `src/OTGW-firmware/SATcontrol.ino` — one file, four non-overlapping edits totalling +6 / -3 lines.
<!-- SECTION:FINAL_SUMMARY:END -->

- [x] #1 satOvpCalibrate wired into satControlLoop() between thermal learning and safety check (1 line dispatch, gated by SAT_CALIB_IDLE)
- [x] #2 satGetHeatingSystemName parked with __attribute__((unused)) and clarifying comment
- [x] #3 _thermal_learningStartMs deleted (superseded by _thermal_totalLearnMs)
- [x] #4 _pwm_lastSampleMs deleted (PWM gates on flame state, not sample interval)
- [x] #5 ESP8266 build clean: all four SAT warnings gone (-4 vs TASK-455 baseline, +0 introduced)
- [x] #6 ESP32 build clean
- [x] #7 Behavioral change documented: OPV calibration now actually pumps the state machine when triggered by REST/MQTT — hardware verification recommended before claiming the feature production-ready
<!-- AC:END -->

- [ ] #1 Located intended call-site (or absence) for satOvpCalibrate; outcome decided (wire up / park / delete) with file:line citation
- [ ] #2 Located intended call-site (or absence) for satGetHeatingSystemName; outcome decided
- [ ] #3 Located intended call-site (or absence) for _thermal_learningStartMs; outcome decided
- [ ] #4 Located intended call-site (or absence) for _pwm_lastSampleMs; outcome decided
- [ ] #5 Each parked symbol has [[maybe_unused]] or equivalent + comment pointing to owning task/ADR
- [ ] #6 Each wired-up symbol verified by tracing call from setup() / loop / command handler down to the symbol
- [ ] #7 ESP8266 build clean for these four symbols
- [ ] #8 ESP32 build clean for these four symbols
- [ ] #9 Decision per symbol logged in task notes for future reference
<!-- AC:END -->
