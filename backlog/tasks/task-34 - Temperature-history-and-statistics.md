---
id: TASK-34
title: Temperature history and statistics
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:51'
updated_date: '2026-04-05 23:29'
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
- [x] #1 Record temperature error samples with timestamps
- [x] #2 Calculate windowed statistics: mean and std deviation
- [x] #3 Configurable window size
- [x] #4 MQTT publish: temp_error_mean, temp_error_stddev
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Temperature error statistics (stddev) added to existing curve recommendation buffer.\n\nChanges:\n- Standard deviation calculated from 24-sample ring buffer\n- REST: error_stddev field in status\n- MQTT: sat/error_mean, sat/error_stddev\n- Reuses curve recommendation buffer (no extra RAM)\n\nFiles: OTGW-firmware.h, SATcontrol.ino
<!-- SECTION:FINAL_SUMMARY:END -->
