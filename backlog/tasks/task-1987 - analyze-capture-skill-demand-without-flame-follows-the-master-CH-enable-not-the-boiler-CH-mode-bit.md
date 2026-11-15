---
id: TASK-1987
title: >-
  analyze-capture skill: 'demand without flame' follows the master CH enable,
  not the boiler CH-mode bit
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T21:15:00Z'
updated_date: '2026-11-15T22:29:00Z'
labels:
  - capture
  - skill
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1015000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The analyze-capture skill's capture_summary.py counts 'demand without flame' from the boiler's CH-mode status bit, which includes the boiler's pump overrun. On gruijter's beta.250 capture (10 min overrun) that gave ~600 s 'demand without flame' per run while SAT had ch=0 (no CH enable), and the overview chart shades the overrun as 'Heating demand, no flame'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 capture_summary.py and capture_plot.py take demand from the master CH enable (MsgID 0 master status bit 0), and show the boiler CH-mode-only time (overrun) separately
- [x] #2 Test on a small frame fixture with a pump overrun: old count wrong, new right
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: capture_summary.py takes the CH request from the master status CH-enable bit (MsgID 0 HB bit 0) as the boiler receives it: the gateway's R frame when it rewrites the request (SAT on a PIC gateway), else the thermostat's T frame (master_segments()); demand_spans() splits the no-flame time into 'demand' and 'overrun' (boiler CH mode without a request); build_runs() reports 'demand w/o flame' and a new 'overrun after' column. capture_plot.py shades a request light blue and the pump overrun grey ('Boiler CH mode only (pump overrun)') and the zoom shades the request. On gruijter's beta.250 capture runs 16/17 now show 5 and 6 s before ignition (SATTICK: CH enable 07:42:56, flame 07:43:03) and 597/601 s overrun after (his 10 min pump overrun); the old table said 598/603 s demand. Chart checked. AC2: tests/test_analyze_capture_demand.py (thermostat request and gateway-rewritten request, overrun straight into the next request as on gruijter's boiler): the old rule gives 920 s demand for the second run, the new <= 30 s and >= 590 s overrun; origin's script errors (no build_runs), the fix passes.
<!-- SECTION:FINAL_SUMMARY:END -->
