---
id: TASK-1386
title: >-
  Heap tier counters miss short dips: enter_low/warning/critical only count when
  getHeapHealth() is polled
status: To Do
assignee: []
created_date: '2026-10-25T11:08:00Z'
labels:
  - heap
  - telemetry
milestone: 2.0.0
dependencies: []
ordinal: 525000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1339 (2026-10-25 bench): boot min free heap 4596 B with hd_enter_warning 0, and 3360 B with enter_critical 0. getHeapHealth() counts a tier entry only when it is called (helperStuff.ino:922-931; callers OTGW-firmware.ino:1049, helperStuff.ino:957/977/998); thresholds 8192/5120/3072 (helperStuff.ino:877-879). The counters therefore under-report, which misleads diagnostics (ADR-167 keeps them as telemetry). Options: derive tier entries from the allocator low-water mark, or document them as sampled.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision recorded (derive from the low-water mark, or document as sampled) with the trade-off
- [ ] #2 If changed: host or bench test shows a short dip below 5120 counted (red on HEAD)
<!-- AC:END -->
