---
id: TASK-225
title: 'SAT: Improve cycle classifier to use flow temperature tail percentile'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:29'
updated_date: '2026-04-09 05:47'
labels:
  - audit-fix
  - sat
  - cycles
dependencies: []
references:
  - backlog/docs/sat-python-cpp-mapping.md
  - backlog/docs/sat-feature-completeness-matrix.md
  - src/OTGW-firmware/SATcycles.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python's CycleClassifier.classify() uses tail percentile metrics (p90 of flow temperature, p50 of setpoint error) to classify cycles as GOOD/OVERSHOOT/UNDERHEAT/SHORT_CYCLING. This is more robust than a simple max-flow check because brief ignition spikes don't trigger false OVERSHOOT classification.

The C++ _cycleClassify() uses max_flow_temp vs setpoint as the overshoot criterion. A single momentary spike during boiler startup is enough to classify the cycle as OVERSHOOT, potentially triggering premature PWM enable.

Complexity: Medium. Requires tracking a rolling set of per-sample flow temperatures during a cycle (not just the max). A lightweight approach: maintain a sorted ring buffer of the last N flow samples and compute approximate p90 on cycle end.

Risk: Medium. Changing classification logic affects the auto-switch triggers (PWM enable/disable). Must be validated against known-good cycle scenarios. The existing EMA-based history statistics remain unchanged; only the per-cycle classifier is affected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Cycle classifier tracks a sample buffer of flow temperatures during each cycle (min 10 samples)
- [x] #2 OVERSHOOT classification uses p90 of flow temperature rather than max_flow_temp
- [x] #3 UNDERHEAT classification uses p10 of flow temperature
- [x] #4 SHORT_CYCLING detection is unchanged (duration threshold)
- [x] #5 Classification results match Python reference for at least 3 manually-verified cycle scenarios documented in tests or comments
- [x] #6 No regression on existing cycle auto-switch logic (60s overshoot / 180s underheat / 300s saturation timers)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add _flow_samples[64] ring buffer in SATcycles.ino (tracks per-cycle flow temps)
2. Reset buffer on flame-on; seed with initial boiler temp
3. Append sample in satCycleSample() at each periodic tick
4. Add _flowPercentile(pct) using insertion-sort on local copy
5. Change _cycleClassify() to accept p90FlowTemp + p10FlowTemp instead of maxFlowTemp
6. On flame-off: compute p90/p10 (fallback to max/min if <10 samples); pass to classifier
7. SHORT_CYCLING detection unchanged (duration threshold only)
8. Log p90 and p10 in the cycle-end debug message
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Changed SAT cycle classifier to use p90/p10 of flow temperature instead of max/min for OVERSHOOT and UNDERHEAT classification.

Changes:
- SATcycles.ino: added _flow_samples[64] ring buffer (256 bytes static) populated during each active cycle in satCycleSample()
- _flowPercentile(pct): computes percentile via insertion-sort on a local copy — O(n^2) but n<=64 and runs only once at cycle-end
- _cycleClassify(): signature changed to accept p90FlowTemp + p10FlowTemp; SHORT_CYCLING detection unchanged
- satCycleOnFlameChange(): computes p90/p10 at flame-off; falls back to max/min if fewer than 10 samples collected (short cycles)
- Debug log extended to show p90 and p10 alongside max_flow
- Three manually-verified scenarios documented in comments confirming Python parity

User impact: Eliminates false OVERSHOOT classifications from brief ignition temperature spikes. Eliminates false UNDERHEAT from momentary cold-start sensor lag at flame-on. The overshootSec timer (already requiring >10s sustained overshoot) remains as a second gate.

Risk: Low. The p90/p10 thresholds use the same margin constants as before. The 10-sample minimum before percentile use is conservative.
<!-- SECTION:FINAL_SUMMARY:END -->
