---
id: TASK-1915
title: >-
  Health API carries the simulation state; the web UI stops polling
  /api/v2/simulate every 15 s
status: To Do
assignee: []
created_date: '2026-11-13T23:57:00Z'
labels: []
dependencies: []
priority: medium
ordinal: 981000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer go, Robert in #alpha-testing (message 1570947181945299066): "Yes add the simulation into there health api." Follow-up to TASK-1899. Today the v2 web UI asks /api/v2/simulate after every /api/v2/health poll (v2.js:3751 chain.then(fetchSim); fetchSim at v2.js:2296), about every 15 s per tab, only to show or hide the SIMULATION badge (#hdrSim). gruijter's capture had 7,668 such calls in 18.5 h. The health answer already comes every 15 s, so it can carry the state: /api/v2/health gets the simulation active flag and its source (file or builtin), taken from the loop-task state (the TASK-1899 LittleFSsimFilePresent flag and the simulation state), no LittleFS call in sendHealth. v2.js drops fetchSim from the poll chain and sets the badge from the health answer. /api/v2/simulate stays for its other callers (the simulation page and the one-time offer, v2.js:7978 and :8438). One request less per poll also leaves more room under the 2-request REST cap (TASK-1747).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 /api/v2/health carries the simulation state (active, source), from loop-task state; sendHealth makes no LittleFS call (test_health_no_fs_flash_ops and the TASK-1899 gate stay green)
- [ ] #2 Test on the real v2.js: the 15 s poll chain sends no /api/v2/simulate request, and the SIMULATION badge follows the health answer (shown when active, hidden when not or when the field is missing); red on the old v2.js
- [ ] #3 C:/Program Files/Git/api/v2/simulate still answers as before for the simulation page and the one-time offer
- [ ] #4 Bench: in simulation mode the badge appears from the health poll and the browser network log shows no periodic /simulate request
<!-- AC:END -->
