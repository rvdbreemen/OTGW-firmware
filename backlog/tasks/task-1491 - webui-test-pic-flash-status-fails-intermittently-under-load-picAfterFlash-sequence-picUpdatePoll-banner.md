---
id: TASK-1491
title: >-
  webui test pic-flash-status fails intermittently under load (picAfterFlash
  sequence, picUpdatePoll banner)
status: To Do
assignee: []
created_date: '2026-10-29T22:39:00Z'
labels:
  - webui
  - tests
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 650000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-firmware-d5 during the TASK-1489 load run (2026-10-29). Not a browser test, so the port fix does not touch it. Load iteration 4: 'picAfterFlash() runs the real refresh functions one request at a time' - the 'banner 6.8' step was missing from the request sequence. Load iteration 6: 'picUpdatePoll: a persistent network error ends on Update check failed' - banner stuck. Both look timing-sensitive (a wall-clock budget inside the code under test). Logs: d5 scratchpad soak1/load_4.txt, load_6.txt.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each failure is reproduced deterministically (e.g. by slowing the event loop or timers) and the cause is named with file:line
- [ ] #2 After the fix, pic-flash-status passes 20/20 under parallel load
<!-- AC:END -->
