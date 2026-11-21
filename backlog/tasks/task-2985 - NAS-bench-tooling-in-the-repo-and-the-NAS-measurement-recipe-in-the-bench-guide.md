---
id: TASK-2985
title: >-
  NAS bench tooling in the repo and the NAS measurement recipe in the bench
  guide
status: Done
assignee:
  - '@claude'
created_date: '2026-11-21T00:19:00Z'
updated_date: '2026-11-21T00:21:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1114000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-21: all OTGW measurements (telnet loggers, REST pollers, MQTT subscriptions, soaks, captures, A/B reruns) run as jobs on the NAS, not on the laptop. The tools (bench.sh, tellog.py, poll.py, satcount.py) lived only on the NAS and in a session scratchpad. Put them under scripts/nas-bench/ and document start/stop/status, output location, one logger per unit, no other client during heap runs, and the copy to OTGW-field-data in docs/guides/agent-build-and-bench.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 scripts/nas-bench/ holds bench.sh, tellog.py, poll.py, satcount.py byte-identical to the deployed NAS copies (sha256 compared)
- [x] #2 docs/guides/agent-build-and-bench.md has a 'Measurements run on the NAS' section with start/status/stop/list, the job table, the one-logger and no-other-client rules, and the copy-with-SHA256SUMS step
- [x] #3 The documented status and list commands run on the NAS with exit 0
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-11-21 01:4x: sha256 NAS vs scripts/nas-bench equal for all four (bench.sh 366d31ee, tellog.py c0cfeadc, poll.py 90e04039, satcount.py aa8f70d5). ssh terradisk bench.sh list and status ran, status exit 0; five stale t2019 runs whose jobs had ended moved pids -> pids.stopped so status lists only live runs (now none). Docs-only plus scripts outside src/: no firmware bump.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: four tools copied from the NAS, sha256 equal to the deployed copies. AC2: section 'Measurements run on the NAS (Robert, 2026-11-21)' in docs/guides/agent-build-and-bench.md (commands, job table checked against poll.py/satcount.py docstrings, one-logger rule, no-other-client rule, copy with SHA256SUMS, laptop-broker caveat). AC3: bench.sh list/status on the NAS exit 0. Not done: moving the MQTT broker to the NAS (separate task).
<!-- SECTION:FINAL_SUMMARY:END -->
