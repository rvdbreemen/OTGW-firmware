---
id: TASK-34
title: Temperature history and statistics
status: To Do
assignee: []
created_date: '2026-04-05 20:51'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Track temperature error samples over time and calculate windowed statistics (mean, std deviation). Used for PID auto-tuning, performance analysis, and detecting anomalies.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/temperature/history.py
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Record temperature error samples with timestamps
- [ ] #2 Calculate windowed statistics: mean and std deviation
- [ ] #3 Configurable window size
- [ ] #4 MQTT publish: temp_error_mean, temp_error_stddev
<!-- AC:END -->
