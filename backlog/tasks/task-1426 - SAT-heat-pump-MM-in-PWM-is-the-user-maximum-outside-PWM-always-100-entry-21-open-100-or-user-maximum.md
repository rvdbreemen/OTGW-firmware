---
id: TASK-1426
title: >-
  SAT: heat pump MM in PWM is the user maximum, outside PWM always 100 (entry 21
  open: 100 or user maximum?)
status: To Do
assignee: []
created_date: '2026-10-28T14:59:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 590000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Review finding on TASK-1419 (2026-10-28). satComputeMM (SATcontrol.ino:379-388): heat pump outside PWM -> MM=100; in PWM -> iMaxRelModulation (100 if 0). With entry 87 heat pumps now enter PWM automatically, so a user max below 100 makes MM drop when PWM starts. Entry 21 allows '100 % or the desired max modulation limit the user selects'; its Open item (which one is meant) is unresolved. settingsHelp.h:70 says heat pumps keep full modulation. Needs a question to Sergeant D (queue in docs/sat/open-questions.md) before any change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's answer recorded in docs/sat/sergeant-d-corrections.md
- [ ] #2 satComputeMM and settingsHelp.h follow the answer; host test red/green
<!-- AC:END -->
