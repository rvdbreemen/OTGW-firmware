---
id: TASK-1015
title: Research optimal client/firmware parallelism for v2 Web UI (load-test study)
status: Done
assignee: []
created_date: '2026-07-05 22:18'
updated_date: '2026-07-06 04:40'
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
- [x] #1 All subtasks complete
- [x] #2 N* baked into REST_MAX_INFLIGHT + WEB_FILE_MAX_INFLIGHT + client MAX_INFLIGHT
- [x] #3 ADR written documenting method + data + chosen N
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
All subtasks (1016/1018/1019/1020/1021/1022/1024/1026) Done. N*=2 confirmed by two-phase load test and baked into production (REST_MAX_INFLIGHT/WEB_FILE_MAX_INFLIGHT 4/6->2, alpha.330, commit 190dbbdfc). ADR-165 Accepted 2026-07-06. No client MAX_INFLIGHT knob built (not needed - frontend N=1 already within the new N=2 ceiling).
<!-- SECTION:FINAL_SUMMARY:END -->
