---
id: TASK-1461
title: >-
  sat/auto_tune_rate echo is non-retained and only published while auto-tune is
  on
status: Done
assignee: []
created_date: '2026-10-29T07:20:00Z'
updated_date: '2026-10-30T20:12:00Z'
labels:
  - sat
  - mqtt
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 621000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. Unlike the other settings echoes (SATcontrol.ino:3538), auto_tune_rate is published non-retained and only while auto-tune runs (:3545-3551).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 sat/auto_tune_rate is retained and published unconditionally like the other settings echoes
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Obsolete: TASK-1413 (alpha.562, d7f835b3f, Sergeant D entry 72) removed the coefficient auto-tune with its setting, the sat/auto_tune_rate topic and the echo this task was about; the retained topic is cleared by the TASK-1413 retire rows. Evidence: grep for auto_tune_rate in src/OTGW-firmware at d7f835b3f finds only the RETIRED-SAT-ENTITY row. Nothing left to implement.
<!-- SECTION:FINAL_SUMMARY:END -->
