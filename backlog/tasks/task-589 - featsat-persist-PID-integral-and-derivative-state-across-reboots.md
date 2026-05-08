---
id: TASK-589
title: 'feat(sat): persist PID integral and derivative state across reboots'
status: To Do
assignee: []
created_date: '2026-05-08 16:49'
labels: []
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The Python SAT persists integral accumulator, raw derivative, last temperature, and last timestamps to HA storage so the PID warm-up period is not lost on restart. The C++ port reinitializes all PID state to zero on reboot. This causes SAT control to be suboptimal for several minutes after every reboot (OTA, watchdog, power cycle) while the integral re-establishes. Field testers observe a cold-start undershoot pattern after each beta flash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 PID integral (fPidI) saved to settings.sat or a separate persistence file on LittleFS
- [ ] #2 Raw derivative and last-temperature saved alongside the integral
- [ ] #3 Values restored on boot before first SAT control tick
- [ ] #4 Graceful default (zero) when no persisted state exists
- [ ] #5 Persisted state invalidated if SAT is disabled/reset via UI
<!-- AC:END -->
