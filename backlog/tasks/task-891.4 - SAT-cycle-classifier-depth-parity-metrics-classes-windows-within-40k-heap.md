---
id: TASK-891.4
title: 'SAT: cycle-classifier depth parity (metrics/classes/windows) within 40k heap'
status: To Do
assignee: []
created_date: '2026-06-20 19:21'
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
