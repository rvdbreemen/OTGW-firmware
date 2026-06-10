---
id: TASK-852
title: Close streamFile guard bypass in getDallasLabels
status: Done
assignee:
  - '@claude'
created_date: '2026-06-10 20:10'
updated_date: '2026-06-10 20:15'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
getDallasLabels() (restAPI.ino:1557) calls raw httpServer.streamFile() instead of streamFileGuarded() like every other file-serving caller, bypassing the HTTP_SERVE_MIN_MAXBLOCK heap pre-flight. Surfaced by the George crash root-cause workflow as a genuine guard bypass (defense-in-depth hygiene; NOT the George fault path, which beta.4 proved is not the streamFile/get_buffer site).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 getDallasLabels uses streamFileGuarded(labelsFile, F("application/json")) instead of raw streamFile
- [x] #2 build passes (python build.py --firmware exit 0)
- [x] #3 evaluate.py --quick shows no new failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Swapped getDallasLabels() raw httpServer.streamFile() for streamFileGuarded(labelsFile, F("application/json")), matching every other file-serving caller. Now a low-maxBlock serve returns 503 instead of triggering an unchecked core per-chunk allocation. Build exit 0 (single TU resolves the cross-file static call; FSexplorer.ino concatenates before restAPI.ino), evaluate.py --quick 100% health. Pushed c239c9a3. Defense-in-depth hygiene from the George crash root-cause workflow; explicitly NOT the George fault path (beta.4 falsified the streamFile/get_buffer site).
<!-- SECTION:FINAL_SUMMARY:END -->
