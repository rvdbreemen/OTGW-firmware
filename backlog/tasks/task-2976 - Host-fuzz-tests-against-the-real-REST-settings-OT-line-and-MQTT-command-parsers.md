---
id: TASK-2976
title: >-
  Host fuzz tests against the real REST settings, OT line and MQTT command
  parsers
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-20T16:25:00Z'
updated_date: '2026-11-20T16:34:00Z'
labels:
  - tests
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1108000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
test-pyramid gap analysis 2026-11-20 (Robert). Goal: find crashes and memory errors in input parsers on the host, before a malformed message from the network or the bus does it on a unit. Fuzz the REAL parser code with the compile-the-real-code pattern (as tests/test_bridge25238_sat_filter.py), never a re-implementation: (1) the REST settings POST / JSON input path, (2) the OT line parser and the ADR-217 port-25238 bridge line filter, (3) the MQTT command dispatch (ADR-078 tables). Tool choice per the reuse rule: compare at least two open-source options on primary sources (for example libFuzzer, which MSVC and clang support with -fsanitize=fuzzer; atheris; Hypothesis for a seeded property harness; or a deterministic seeded random harness) and record the choice and why in the task notes. Runs in scripts/run_tests.py with a time cap so the suite stays fast; every crash found becomes a regression test case.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Tool choice recorded in the notes: at least two candidates compared (licence, Windows/MSVC support, maintenance), with the reason for the choice
- [ ] #2 One harness per parser (REST settings JSON, OT line + ADR-217 bridge filter, MQTT command dispatch) that compiles the real source
- [ ] #3 Each harness runs at least 1 minute without a crash, or every crash found is filed as a task with its reproducer committed as a test case
- [ ] #4 AddressSanitizer and UndefinedBehaviorSanitizer on where the host compiler supports them (say which); run_tests.py runs the harnesses with a time cap and stays green
<!-- AC:END -->
