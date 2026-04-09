---
id: TASK-224
title: 'SAT: Add minimum OPV calibration dataset requirement'
status: To Do
assignee: []
created_date: '2026-04-09 05:29'
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
- [ ] #1 MEASURING phase counts temperature samples received from OT MsgID 25 (flow temp)
- [ ] #2 Calibration transitions to DONE only if at least 40 samples were collected during MEASURING
- [ ] #3 If sample count is below threshold, calibration transitions to FAILED with a diagnostic log message
- [ ] #4 Sample count and threshold are logged at calibration completion
- [ ] #5 Existing STARTING/WARMING/MEASURING/DONE/FAILED state machine is otherwise unchanged
<!-- AC:END -->
