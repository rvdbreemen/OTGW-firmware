---
id: TASK-986
title: >-
  feat(v2-webui): SAT page in v2 (L1 simple / L2 control / L3 technical per
  updated mockup)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-01 22:25'
updated_date: '2026-07-02 05:16'
labels: []
dependencies: []
ordinal: 198000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit area sat (all 4 findings). Build the v2 SAT page from the UPDATED mockup (which moved the Temperature-history card + mini-cards row from L1 into L2, and uses the new label.sat-switch text-before-toggle pattern for the satEnable header toggle and the satSim bench toggle). Nav seg SAT between Home and Monitor. Data: GET /api/v2/sat/status (enabled, target_temp, final_setpoint, room_temp, outside_temp, dhw keys, force_pwm, control_mode); writes via POST /api/v2/sat/settings/satenabled, satsimulation, satforcepwm, satdhwsetpoint, satdhwenable; optional sim events via POST /api/v2/sat/sim/event. History series accumulates client-side (matches mockup + classic sat.js, by design). Reuse classic sat.js knowledge; do NOT load sat.js itself into v2.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT nav entry + page with L1/L2/L3 layer picker matching updated mockup layout (history+minis in L2)
- [x] #2 Header enable toggle and bench-simulation toggle use the sat-switch pattern and really read/write REST
- [x] #3 L1 dial + presets bound to real sat/status; L2 tiles + history chart live; L3 diagnostics + raw JSON
- [ ] #4 python evaluate.py --quick green; build green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented by impl-986 (+711): full v2 SAT page L1/L2/L3, nav seg between Home/Monitor, visible-only 5s poll (satPageStart/Stop, no leak/double-start), client-accumulated history (720-pt ring), heating-curve port. Bound to REAL /api/v2/sat/status. Corrected POST routes vs task-suggested names: /sat/enable, /sat/target, /sat/preset, /sat/mode, /sat/settings/dhw_setpoint+dhw_enable, /sat/settings/simulation (key literally 'simulation'), Detect-location -> /api/v2/settings SATweatherlat/lon only. Omitted: L1 Mode row (dup of enable), simmer/autotune tiles (no backing field), markers read-only. Adversarial review rev-986: PASS - every route body-shape + field emitter + all 3 enums verified against restAPI.ino/SATcontrol.ino/SATtypes.h, 6 MINOR findings only. Main-thread applied 2 robustness fixes: satDetectLocation per-POST .ok check, fetchSatPage hard-fail shows #satNoData instead of dash-wall. evaluate.py 0 FAIL, drift gate PASS, parse OK. On-device verify pending.
<!-- SECTION:NOTES:END -->
