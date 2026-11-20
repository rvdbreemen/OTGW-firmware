---
id: TASK-2953
title: >-
  docs: diag/recording REST routes in openapi + api README, classic-UI part of
  manual ch03 out, em dashes out of openapi/README
status: Done
assignee:
  - '@claude'
created_date: '2026-11-20T13:50:00Z'
updated_date: '2026-11-20T14:02:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
ordinal: 1089000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Coordinator follow-up of TASK-2950 (2026-11-20): (1) document /api/v2/diag/recording (ADR-213) from the routes in restAPI.ino with file:line; (2) the classic-UI half of ch03/h03 contradicts the v2-only 2.0.0 line (ADR-192): remove it or replace it with one line that the classic UI is no longer shipped in 2.x, EN and NL; (3) remove the em dashes in docs/api/openapi.yaml and docs/api/README.md (Robert's writing rule). ESP8266 mention counts stay (Robert's call).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 openapi.yaml and docs/api/README.md document /api/v2/diag/recording and its sub-routes as the code has them (file:line), YAML parses
- [x] #2 Classic-UI sections of ch03/h03 removed or replaced by one line (ADR-192); MANUAL.md regenerated, test_manual_generated green
- [x] #3 0 em dashes in docs/api/openapi.yaml and docs/api/README.md
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: openapi.yaml gets /v2/diag/recording (GET, binary download, ?since, 400/405/500/503), /status (GET), /start and /stop (POST, PUT same) and schemas DiagRecording, DiagRecordingStatus, DiagRecordingControl, all from restAPI.ino handleDiag() (around :2071-2097), diagEmitStatus()/sendDiagRecordingStatus()/sendDiagRecordingControl()/sendDiagRecordingDownload() (around :3695-3830) and the reason texts in diagRecorderStuff.h:104-110; docs/api/README.md gets a 'Diagnostic recording' section with the same facts (example numbers marked illustrative). yaml.safe_load: 'yaml ok', 73 paths. AC2: ch03/h03 classic-UI sections (EN Navigation .. Heap Memory Display, 157 lines; NL Navigatiebalk .. Heap-geheugenweergave, 194 lines) replaced by one section saying the classic UI is no longer shipped in 2.x (ADR-192); the DHW controls paragraph (v2 SAT Control view) moved under the v2 SAT page section; Settings, update page and file manager sections kept (still v2). No other doc links to the removed sections (grep). build_manual.py regenerated MANUAL.md, test_manual_generated OK. AC3: em dashes in openapi.yaml (15) and README (9) replaced by colons/commas; one YAML scalar needed a comma instead of a colon (Simulation inactive line); 0 left. Not verified: rendered markdown.
<!-- SECTION:FINAL_SUMMARY:END -->
