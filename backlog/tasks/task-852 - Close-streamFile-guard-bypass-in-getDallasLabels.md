---
id: TASK-852
title: Close streamFile guard bypass in getDallasLabels
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-10 20:10'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
getDallasLabels() (restAPI.ino:1557) calls raw httpServer.streamFile() instead of streamFileGuarded() like every other file-serving caller, bypassing the HTTP_SERVE_MIN_MAXBLOCK heap pre-flight. Surfaced by the George crash root-cause workflow as a genuine guard bypass (defense-in-depth hygiene; NOT the George fault path, which beta.4 proved is not the streamFile/get_buffer site).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 getDallasLabels uses streamFileGuarded(labelsFile, F("application/json")) instead of raw streamFile
- [ ] #2 build passes (python build.py --firmware exit 0)
- [ ] #3 evaluate.py --quick shows no new failures
<!-- AC:END -->
