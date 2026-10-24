---
id: TASK-1359
title: >-
  SAT coefficient range: make REST metadata and the wizard slider match the
  0.1-5.0 setting clamp
status: To Do
assignee: []
created_date: '2026-10-24T20:21:00Z'
labels:
  - sat
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 494000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-24 (ADR-198 open question 1): keep 0.1-5.0 (settingStuff.ino:1001) and align everything: REST settings metadata (restAPI.ino:3888, now 0-5 as integers) becomes 0.1-5.0 with decimals; the wizard slider (data/v2.js:5547, 0.2-2.4) goes to 0.1-5.0. Whether 5 is high enough for SAT's design is asked of Sergeant D separately (queue Q21).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 REST metadata and the wizard slider both expose 0.1-5.0 with 0.1 steps (test red on HEAD, green); setting clamp unchanged
<!-- AC:END -->
