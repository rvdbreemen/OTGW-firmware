---
id: TASK-1933
title: >-
  SAT: replace the PWM underheat exit with Sergeant D's rule (flame on, request
  above flow for 60 s -> continuous), entry 342
status: To Do
assignee:
  - '@d3'
created_date: '2026-11-14T10:36:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1924
priority: medium
ordinal: 989000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer + Sergeant D decision, entry 342: Q-338a A (https://discord.com/channels/812969634638725140/1514720723980259460/1571110480997523527), his rule from entry 339 (1571097503636652094). Today the PWM-to-continuous underheat exit is flow <= request - 3.0 C for 180 s while PWM is ON (thermo-nova heating_control.py:321-356, const.py:22, cycles/const.py:11; firmware SATcycles.ino:39-40). The saturation exit (300 s) stays. Deviates from thermo-nova. For lane d3 after TASK-1924 (same area, SATcontrol.ino / SATcycles.ino).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Simulation check FIRST, before any firmware change: the new exit against today's exit on the gruijter-calibrated sim houses; toggling against the entry-327 entry rule (+1.5 C for 30 s) and burner starts per day; numbers labelled MODEL RESULT
- [ ] #2 The simulation result is reported to Sergeant D and his OK is recorded as an entry before the firmware is changed
- [ ] #3 Open points of entry 342 settled with Sergeant D: (2) whether the rule runs only while PWM is ON (thermo-nova heating_control.py:321-323), (3) boiler temperature = flow temperature
- [ ] #4 ADR via adr-kit recording the deviation from thermo-nova's underheat exit
- [ ] #5 Host test on the real exit code: red on the old code (flow 1 C below request for 60 s does not exit), green on the fix; the saturation exit unchanged
- [ ] #6 Bench run by the bench-tester (otgw-bench-tester): no reboot, the exit fires as specified in a simulated warm-up
<!-- AC:END -->
