---
id: TASK-224
title: 'SAT: Add minimum OPV calibration dataset requirement'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-09 05:29'
updated_date: '2026-04-09 06:21'
labels:
  - audit-fix
  - sat
  - calibration
dependencies: []
references:
  - backlog/docs/sat-python-cpp-mapping.md
  - src/OTGW-firmware/SATcontrol.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python requires OVERSHOOT_PROTECTION_REQUIRED_DATASET = 40 temperature samples before accepting an OPV calibration result. This guards against accepting a noisy or truncated measurement run as valid.

The C++ firmware's satOvpCalibrate() accepts whatever maximum temperature was observed during the 20-minute MEASURING phase, with no minimum sample count. If the boiler flame is intermittent or the measurement is cut short (e.g. user restarts firmware), an incorrect (too-low) OPV value could be stored, causing unnecessary setpoint suppression.

Complexity: Low. Add a sample counter to the SAT_CALIB_MEASURING phase. Transition to DONE only if sample_count >= 40 (or equivalent threshold for 20-min window at 30s intervals = 40 samples). Otherwise transition to FAILED with a reason code.

Risk: Low. The existing behavior already stores to LittleFS; the only change is adding a guard before the store.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MEASURING phase counts temperature samples received from OT MsgID 25 (flow temp)
- [x] #2 Calibration transitions to DONE only if at least 40 samples were collected during MEASURING
- [x] #3 If sample count is below threshold, calibration transitions to FAILED with a diagnostic log message
- [x] #4 Sample count and threshold are logged at calibration completion
- [x] #5 Existing STARTING/WARMING/MEASURING/DONE/FAILED state machine is otherwise unchanged
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add constant SAT_CALIB_MIN_SAMPLES = 40 near the OPV calibration constants section.
2. In satOvpCalibrate(), in the SAT_CALIB_MEASURING case, after the elapsed >= SAT_CALIB_MEASURE_MS check: if iCalibSamples < SAT_CALIB_MIN_SAMPLES, transition to FAILED with a diagnostic log instead of DONE.
3. Log sample count and threshold in both success and failure paths.
4. Verify the state machine transitions (STARTING -> WARMING -> MEASURING -> DONE/FAILED) are otherwise unchanged.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added a minimum sample count guard to satOvpCalibrate() before accepting an OPV calibration result, matching Python parity (OVERSHOOT_PROTECTION_REQUIRED_DATASET = 40).

Changes:
- Added SAT_CALIB_MIN_SAMPLES = 40 constant alongside the other OPV calibration constants.
- In the SAT_CALIB_MEASURING phase, when elapsed >= SAT_CALIB_MEASURE_MS: if iCalibSamples < 40, transition to FAILED with a diagnostic log showing actual vs required sample count; otherwise proceed to DONE as before.
- Both success and failure log messages now include the sample count and threshold for observability.
- All other state machine transitions (STARTING -> WARMING -> MEASURING -> DONE/FAILED) are unchanged.
<!-- SECTION:FINAL_SUMMARY:END -->
