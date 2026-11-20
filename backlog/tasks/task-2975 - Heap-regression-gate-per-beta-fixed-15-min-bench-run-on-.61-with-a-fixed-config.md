---
id: TASK-2975
title: >-
  Heap regression gate per beta: fixed 15-min bench run on .61 with a fixed
  config
status: To Do
assignee:
  - '@otgw-bench-tester'
  - '@otgw-dev'
created_date: '2026-11-20T16:22:00Z'
updated_date: '2026-11-20T16:33:00Z'
labels:
  - tests
  - bench
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1107000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
test-pyramid gap analysis 2026-11-20 (Robert). Goal: catch a heap or memory regression per beta before it reaches testers, instead of finding it in a field capture (TASK-2019, TASK-2954). A fixed 15-min bench run on .61 (no PSRAM) with a fixed config: OT-Direct loopback (otdmode 4), SAT on, satsimulation false, weather off, BLE off, no telnet client, no /ws client, REST info poll every 10 s. Each run records hd_min_free_heap, free heap at the end, largest free block and bootcount, with the githash, into a CSV in OTGW-field-data and a line in the alpha ledger. Proposed thresholds (architect, from today's numbers; calibrate after 3 baseline runs on one good build): FAIL when hd_min_free_heap < 10 KB (the TASK-2954 AC3 floor); WARN when hd_min drops more than 8 KB against the median of the previous beta's runs. Today's beta.401 numbers vary from 116 B to 5048 B depending on /ws and telnet, which is why the config is fixed and the noise floor is measured first (CLAUDE.md principle 'Explain the number').
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Script plus recipe in docs/guides/agent-build-and-bench.md: one command runs the 15-min fixed-config run on .61 and writes the CSV row (githash, hd_min, free, maxblk, bootcount, config hash)
- [ ] #2 Noise floor measured: 3 runs on one build, spread recorded; thresholds set from it (start: FAIL < 10 KB, WARN drop > 8 KB) and written in the recipe
- [ ] #3 C:/Program Files/Git/beta-release-2x (.claude/skills/beta-release-2x/SKILL.md) has a step that runs the gate and stops on FAIL, reports WARN in the release notes draft
- [ ] #4 Red/green shown: a beta.401-equivalent build is FAIL, a good build is green, with both CSV rows quoted
<!-- AC:END -->
