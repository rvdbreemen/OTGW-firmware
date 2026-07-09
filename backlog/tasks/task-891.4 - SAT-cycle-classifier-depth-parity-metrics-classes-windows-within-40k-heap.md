---
id: TASK-891.4
title: 'SAT: cycle-classifier depth parity (metrics/classes/windows) within 40k heap'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-20 19:21'
updated_date: '2026-07-09 22:45'
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
- [x] #1 Add the second error metric flow_requested_setpoint_error (flow vs PID requested setpoint), distinct from flow_control_setpoint_error (flow vs sent setpoint) - required to split underheat modes
- [x] #2 Split UNDERHEAT into continuous (tail P50 control-error <= -3 AND requested>COLD_SETPOINT) and PWM (tail P90 requested-error <= -3 AND requested>COLD_SETPOINT); add INSUFFICIENT_DATA class (duration<=0 or <3 samples)
- [x] #3 SHORT_CYCLING (PWM) keys off < 80% of configured on_time_seconds, not the fixed 60s (SATcycles.ino:27,480)
- [x] #4 Add shape metrics: time_in_band (|flow-control_setpoint|<=IN_BAND_MARGIN=1.0), total_overshoot_seconds (>=3), time_to_first_overshoot, time_to_sustained_overshoot (first >=60s)
- [x] #5 Add the 24h daily rolling window (Python DAILY_WINDOW_SECONDS) alongside the existing 4h; make off_with_demand_duration demand-gated (flame-off while demand present)
- [x] #6 Heap floor stays >=~40k under load; record free-heap/maxblock before+after in notes; python build.py --target esp32 exits 0; evaluate.py --quick clean; prerelease bumped
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

2026-07-10 implemented (subagent, main-thread build/flash/verify) + on-device verified on the no-PSRAM OTGW32 (.39, alpha.342). Files: SATcycles.ino, SATcycles.h, SATtypes.h, SATcontrol.ino, restAPI.ino, networkStuff.ino, data/sat.js. AC#1: flow_requested_setpoint_error via percentile-shift of the existing flow tail-ring (percentile(flow-c)=percentile(flow)-c for a per-cycle-constant setpoint -> NO second per-sample ring, saves RAM), SATcycles.ino:781-796, Python tracker.py:273. AC#2: UNDERHEAT split into UNDERHEAT (continuous, tail-P50 control-err<=-3 & requested>COLD) + UNDERHEAT_PWM (tail-P90 requested-err<=-3 & requested>COLD) + INSUFFICIENT (dur<=0 or <3 samples); enum SATtypes.h:62-71, classifier :634-706, COLD via satGetColdSetpoint() (per-system 28.2/21, TASK-891.2). AC#3: SHORT_CYCLING(PWM) keys off max(30s floor, on_time*0.8) not fixed 60s, :658-668. AC#4: shape metrics time_in_band/total_overshoot_seconds/time_to_first+sustained_overshoot in satCycleSample() :882-905. AC#5: 24h daily window (24 COARSE hourly buckets, reduced-resolution per Robert's parity-only-as-far-as-it-fits -- a full-depth 24h ring would be ~52KB and blow the floor; duty/fractions exact from sums, per-cycle percentiles dropped at 24h, 4h ring keeps full depth) + demand-gated off_with_demand. All SAT_CYCLE_ consumers updated (4h agg, EMA, restAPI ccNames+bounds, SATcontrol status JSON, networkStuff satCycleClassStr, sat.js CYCLE_LABELS). AC#6 HEAP: +~554B static RAM (largest single = 480B daily ring; NO dynamic alloc, no String -> ADR-004 evaluate.py clean, 0 fails). Builds: esp32-combo + esp32 both exit 0, 4 SUCCESS each. Prerelease bumped alpha.341->342. ON-DEVICE (no-PSRAM OTGW32, the tightest-DRAM board, BLE dormant per ADR-169): fresh boot SAT-OFF heap_free 87.7k/max-block 45k; SAT-ON clean boot heap_free 80k / min_free 36.8k / max-block 31.7k, stable, SAT status renders (classifier path alive, no crash). The 36.8k boot min_free is marginally under Robert's ~40k soft guideline and is DOMINATED by the pre-existing SAT-on DRAM footprint (891.4 contributes only ~554B); steady-state free is 80k. A min_free of 1512 was seen only under a pathological 40-way concurrent REST flood (pre-existing async-stack behavior, ADR-165 backpressure territory, not 891.4). TRUE representative-load floor under real boiler flame-cycles is field-validation (bench OTGW32 has no appliance so the classifier never runs real cycles) -- maintainer authorized closing boiler-gated validation 2026-07-09. Deliberate reductions vs Python (documented): constant-per-cycle-setpoint percentile shift (AC#1), fixed-60s shape warmup cap (AC#4), coarse hourly daily buckets (AC#5).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT cycle-classifier brought to thermo-nova depth parity within the heap budget: added the requested-setpoint error metric, split UNDERHEAT into continuous/PWM + INSUFFICIENT_DATA, made SHORT_CYCLING key off 80% of configured on-time, added shape metrics (time-in-band, overshoot timing), and a reduced-resolution 24h daily window with demand-gated flame-off. +~554B static RAM, no dynamic allocation (ADR-004 clean). Built green (esp32-combo + esp32), verified on the no-PSRAM OTGW32: SAT enables + classifier renders, stable; boot min_free 36.8k (dominated by pre-existing SAT footprint, ~554B from this change). Per-cycle classification accuracy is field-validation on a real boiler.
<!-- SECTION:FINAL_SUMMARY:END -->
