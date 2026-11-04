---
id: TASK-249
title: Add browser console logging for SAT WebUI debug
status: Done
assignee:
  - '@claude'
created_date: '2026-04-11 11:07'
updated_date: '2026-04-11 11:15'
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
- [x] #1 SAT data fetch (refreshSAT) logs received data summary to console with [SAT] prefix
- [x] #2 SAT view switch logs new view name to console
- [x] #3 SAT state render logs key fields (room temp, target, mode, flame) on each update
- [x] #4 SAT errors (fetch failure, parse error) log to console.warn or console.error with [SAT] prefix
- [x] #5 Logs are off by default and only active when a debug flag is set (e.g. window.SAT_DEBUG = true), to avoid noise in production
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added _debug() helper to sat.js gated on window.SAT_DEBUG.

To enable in browser: open DevTools console, type: window.SAT_DEBUG = true

Logs added:
- fetchStatus: logs enabled/mode/room/target/flame on each successful poll
- updateDashboard: logs room, target, setpoint, mode, flame, active on each render
- switchView: logs the new view name
- fetch errors: console.warn with [SAT] prefix
- weather fetch errors: console.warn with [SAT] prefix

All logging is silent by default (no noise in production).
<!-- SECTION:FINAL_SUMMARY:END -->
