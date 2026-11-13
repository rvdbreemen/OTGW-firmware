---
id: TASK-1881
title: 'SAT self-tune: expose per-reason exclusion counters in REST/MQTT'
status: To Do
assignee: []
created_date: '2026-11-13T06:13:00Z'
labels:
  - sat
  - telemetry
dependencies:
  - TASK-1879
priority: low
ordinal: 954000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Lesson 2 of docs/sat/vtherm-comparison-2026-11-13.md (TASK-1879). The tuner already logs the first SatTuneReason per hour over telnet (SATcurveTune.h:37-40, SATcontrol.ino:6424-6428). Add counters per reason (hours excluded per reason over the current tuning window) to REST and MQTT so a long-run capture shows why days did not count without a telnet transcript. Telemetry only; the tuner decision does not change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: excluded hours increment the counter of their reason; the decision path is unchanged (tests/test_sat_curve_selftune.py green)
- [ ] #2 Counters visible in REST and as flat MQTT topics (ADR-101)
<!-- AC:END -->
