---
id: TASK-1015
title: Research optimal client/firmware parallelism for v2 Web UI (load-test study)
status: To Do
assignee: []
created_date: '2026-07-05 22:18'
labels: []
dependencies: []
ordinal: 226000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Epic: empirically find the highest parallelism N (client offered-concurrency + REST/file-serve gate caps) that stays robust under combined real-world load on ESP32-S3, then bake it in. Two-phase method (capacity curve, then policy confirmation). Objective: robustness-first. Supersedes the pure single-flight rule of TASK-1014. Full design: docs/superpowers/specs/2026-07-06-parallelism-loadtest-design.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All subtasks complete
- [ ] #2 N* baked into REST_MAX_INFLIGHT + WEB_FILE_MAX_INFLIGHT + client MAX_INFLIGHT
- [ ] #3 ADR written documenting method + data + chosen N
<!-- AC:END -->
