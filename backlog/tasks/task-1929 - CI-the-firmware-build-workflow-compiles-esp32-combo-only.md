---
id: TASK-1929
title: 'CI: the firmware build workflow compiles esp32-combo only'
status: To Do
assignee: []
created_date: '2026-11-14T08:47:00Z'
labels:
  - ci
dependencies: []
ordinal: 987000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-14: 'Remove the remote CI for building all firmwares remotely. Just build combo.' .github/workflows/build.yml ran a matrix of esp32, esp32-classic and esp32-combo on every PR and every non-docs push to dev (TASK-1626). From now on it compiles esp32-combo only; esp32 and esp32-classic are no longer built remotely (locally: build --target all).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 build.yml matrix holds esp32-combo only; the header comment says so
- [ ] #2 A push of the change shows one build job (pio run -e esp32-combo) on GitHub Actions, green
- [ ] #3 Docs that describe what CI builds are updated
<!-- AC:END -->
