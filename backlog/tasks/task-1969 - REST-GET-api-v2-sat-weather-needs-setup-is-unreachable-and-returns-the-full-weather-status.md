---
id: TASK-1969
title: >-
  REST: GET /api/v2/sat/weather/needs-setup is unreachable and returns the full
  weather status
status: To Do
assignee: []
created_date: '2026-11-15T10:43:00Z'
labels:
  - rest
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1006000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during the TASK-1964 route inventory (2026-11-15). In restAPI.ino the SAT weather handler checks the sub-route with wc == 5 (restAPI.ino ~1532), but /api/v2/sat/weather/needs-setup has 6 words ("", api, v2, sat, weather, needs-setup), so the check never matches and the request falls through to the full weather status. Coordinator: file low, red/green on the routing, fix in the ba lane when small.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real handleSAT weather routing: GET /api/v2/sat/weather/needs-setup reaches the needs-setup answer; red on the current code, green on the fix; /api/v2/sat/weather still returns the status
- [ ] #2 Every web UI or doc caller of needs-setup checked (grep), openapi.yaml matches the answer
- [ ] #3 evaluate.py --quick no new FAIL; esp32-combo build
<!-- AC:END -->
