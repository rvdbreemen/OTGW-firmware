---
id: TASK-1956
title: test_dhw_water_meter runs over 250 s per file under run_tests -j 4
status: To Do
assignee: []
created_date: '2026-11-15T03:22:00Z'
labels:
  - test
  - tooling
dependencies: []
priority: low
ordinal: 998000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
In run_tests --changed (beta.282-286 lane, 2026-11-15) test/host/test_dhw_water_meter.py took 247-285 s and hit the per-file timeout under -j 4, also on origin/dev 617789d34, ending in 'COMPILATION FAILED (MRn ...)' for the mutant being built when it was cut off (34 mutants, each a host compile). Not caused by the lane (it touches only banners there). Structural: the mutant count times the MSVC compile time. Options: compile the mutants in parallel, cache the unchanged object, or mark it slow so the chunked foreground runs give it its own slot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Measure the file alone (wall time, per-mutant compile time)
- [ ] #2 The file finishes under the run_tests default timeout at -j 4, or is listed as slow with its own run instruction; red/green behaviour of the mutants unchanged
<!-- AC:END -->
