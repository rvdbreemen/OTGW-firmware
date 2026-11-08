---
id: TASK-1731
title: >-
  otd-mode-running-note.test.mjs flaky: row not dirty after the
  Switch-to-gateway click
status: To Do
assignee: []
created_date: '2026-11-08T14:42:00Z'
labels:
  - tests
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 838000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
system32-78 2026-11-08: tests/webui/otd-mode-running-note.test.mjs (TASK-1702) fails intermittently (3x today, also on old data; old 1/2, fix 0/2, once under -j 4): after the 'Switch to gateway' click the row is not dirty and the count is 0, although the button existed. markDirty('otdmode', cv, row) runs only while otdRunningDiffers(cv); otdMode is only ever set, never cleared. Suspects (unproven): (a) a settings refetch/re-render detaches the row right at the click; (b) the button lookup hits a render window. Needs instrumentation (log setDirty and row identity at failure).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause shown with instrumentation at the moment of failure (test bug or real UI race)
- [ ] #2 Fix: 20 consecutive runs green, also under run_tests -j 4
<!-- AC:END -->
