---
id: TASK-249
title: Add browser console logging for SAT WebUI debug
status: To Do
assignee: []
created_date: '2026-04-11 11:07'
labels:
  - frontend
  - sat
  - debug
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add structured console.log output to the SAT WebUI JavaScript so that SAT state changes, data fetches, render decisions, and errors are visible in the browser developer console. This aids debugging without needing telnet access. Should use a consistent prefix (e.g. '[SAT]') so logs can be filtered easily.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SAT data fetch (refreshSAT) logs received data summary to console with [SAT] prefix
- [ ] #2 SAT view switch logs new view name to console
- [ ] #3 SAT state render logs key fields (room temp, target, mode, flame) on each update
- [ ] #4 SAT errors (fetch failure, parse error) log to console.warn or console.error with [SAT] prefix
- [ ] #5 Logs are off by default and only active when a debug flag is set (e.g. window.SAT_DEBUG = true), to avoid noise in production
<!-- AC:END -->
