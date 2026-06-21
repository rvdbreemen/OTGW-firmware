---
id: TASK-891.4
title: 'SAT: cycle-classifier depth parity (metrics/classes/windows) within 40k heap'
status: To Do
assignee:
  - '@claude'
created_date: '2026-06-20 19:21'
updated_date: '2026-06-20 21:01'
labels: []
milestone: 2.0.0
dependencies: []
references:
  - src/OTGW-firmware/SATcycles.ino
  - src/OTGW-firmware/SATtypes.h
parent_task_id: TASK-891
priority: medium
ordinal: 111000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bring the cycle classifier toward thermo-nova Python depth (cycles/classifier.py, history.py, types.py). Robert's constraint 2026-06-20: keep the heap floor ~40k; measure and build parity only as far as it fits.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Add the second error metric flow_requested_setpoint_error (flow vs PID requested setpoint), distinct from flow_control_setpoint_error (flow vs sent setpoint) - required to split underheat modes
- [ ] #2 Split UNDERHEAT into continuous (tail P50 control-error <= -3 AND requested>COLD_SETPOINT) and PWM (tail P90 requested-error <= -3 AND requested>COLD_SETPOINT); add INSUFFICIENT_DATA class (duration<=0 or <3 samples)
- [ ] #3 SHORT_CYCLING (PWM) keys off < 80% of configured on_time_seconds, not the fixed 60s (SATcycles.ino:27,480)
- [ ] #4 Add shape metrics: time_in_band (|flow-control_setpoint|<=IN_BAND_MARGIN=1.0), total_overshoot_seconds (>=3), time_to_first_overshoot, time_to_sustained_overshoot (first >=60s)
- [ ] #5 Add the 24h daily rolling window (Python DAILY_WINDOW_SECONDS) alongside the existing 4h; make off_with_demand_duration demand-gated (flame-off while demand present)
- [ ] #6 Heap floor stays >=~40k under load; record free-heap/maxblock before+after in notes; python build.py --target esp32 exits 0; evaluate.py --quick clean; prerelease bumped
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
IMPLEMENTATION PLAN (read SATcycles.ino classifier 456-504, sampling 506-550). Do incrementally, one AC-group per commit (each its own prerelease bump):
1) Enum: add SAT_CYCLE_UNDERHEAT_PWM + SAT_CYCLE_INSUFFICIENT to SATCycleClass; update CYCLE_LABELS in data/sat.js + any bounds/array sized to the enum.
2) Dual error metric: track per-cycle BOTH the sent control setpoint (_cycle_setpointAtStart / fFinalSetpoint) AND the PID requested setpoint (fPidOutput). Compute flow_control_setpoint_error (flow - sent) and flow_requested_setpoint_error (flow - requested); take tail P50/P90 of each (extend the _tail_samples ring to carry the per-sample setpoint, or accumulate percentile inputs).
3) Classifier on tail metrics: OVERSHOOT = tail control-err P90 >= +3; UNDERHEAT(cont) = tail control-err P50 <= -3 AND requested>COLD_SETPOINT; UNDERHEAT(pwm) = tail requested-err P90 <= -3 AND requested>COLD_SETPOINT; SHORT_CYCLING(pwm) = PWM status AND duration < 80% of configured on_time (not fixed 60s); INSUFFICIENT_DATA = duration<=0 or <3 samples; else GOOD.
4) Shape metrics: time_in_band (|flow-control|<=IN_BAND=1.0), total_overshoot_seconds (flow-control>=3), time_to_first_overshoot, time_to_sustained_overshoot (first >=60s run). Store on the cycle record + expose via state/MQTT.
5) 24h window: add a cycle-RECORD ring (NOT per-sample) parallel to _win4h, sized for ~24h of cycles (~150-512 records). RAM RISK: a full 24h record ring may threaten the 40k heap floor - MEASURE free heap before/after on target; if too heavy, store 24h AGGREGATES (rolling sums/counts) instead of per-cycle records. off_with_demand: gate the off gap on demand-present (setpoint>flow+hysteresis) at OFF->ON; store separately from raw offDurationMs.
DEPENDENCY: the UNDERHEAT gating needs requested>COLD_SETPOINT; COLD_SETPOINT lands in 891.2/891.8. Until then use a placeholder (current SAT_MIN_SETPOINT or literal 22) and reconcile when COLD_SETPOINT exists.
HEAP: keep floor >=40k (Robert) - verify on hardware before marking Done.

HELD (Robert 2026-06-20): implement in a hardware-verified focused session - the 24h window must be validated against the 40k heap floor on target. Plan is recorded above.
<!-- SECTION:NOTES:END -->
