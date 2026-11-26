---
id: TASK-3104
title: >-
  REST concurrency feasibility: how many parallel requests (2 to 8) can the
  ESP32-S3 serve, per board class, and what must change to allow 4 or 8
status: To Do
assignee: []
created_date: '2026-11-26T11:35:00Z'
labels:
  - research
  - rest
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1030000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-26 (session, interactive): research what is feasible for 2 to 8 parallel REST requests. Facts from code (origin/dev, T2): base REST_MAX_INFLIGHT=2 (restAPI.ino:44, ADR-165), effective cap restEffectiveInflightCap() restAPI.ino:84-95 (block<16000 gives 1; 16000..23999 gives 2; >=24000 gives restCapBase(), release = 2, prerelease restcap 2..8), static files 2 (restAPI.ino:128-139), send side WEB_MAX_TX_CONNECTIONS 4 (ADR-188), client queue 2 (ADR-184). Steady largest block on the bench unit .63 is 19444 (R1 soak 17396..22516), so the cap can never exceed 2 there; the 24000 threshold is a guess (restAPI.ino:76-82), not measured. Sweep rule of TASK-3091 counts an arm only if N was the effective cap for 95 percent of samples, which cannot happen above N=2 on .63. This task is the study that TASK-3091's sweep depends on.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Measure the steady-state largest free block and free heap of every unit class available (.63 PSRAM, .61 no PSRAM, Classic, any field data/captures with hd_min_max_block) and record the table with sources
- [ ] #2 Measure the contiguous heap each large REST response needs today (settings, info, health, sat/status, file list) and at which N the worst case sum exceeds the steady block per board class; show the arithmetic with measured sizes, not estimates
- [ ] #3 Inventory which responses can be streamed in chunks (JsonEmit, ADR-146) to cut the contiguous need, with the expected block saving per endpoint, and a ranked list of what to change first
- [ ] #4 Re-derive the clamp thresholds (16000, 24000, free heap 24000) from the measured steady blocks and state which are guesses today; propose calibrated values per board class
- [ ] #5 Redesign the TASK-3091 sweep so N above 2 can actually be the effective cap on the test unit (what must be changed for the run, prerelease only) and keep its decision rule otherwise; written plan agreed by otgw-architect
- [ ] #6 Result note for Robert: feasible N per board class (2, 4, 8) with evidence or an explicit 'not feasible because', the changes needed (streaming, thresholds, ADR superseding ADR-165, client change ADR-184 and the v2 UI limit, CLAUDE.md single-flight rule), cost and risk, and a recommendation
<!-- AC:END -->
