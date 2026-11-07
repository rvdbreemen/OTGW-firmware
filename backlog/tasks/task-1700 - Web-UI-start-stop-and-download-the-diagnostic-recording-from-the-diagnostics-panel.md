---
id: TASK-1700
title: >-
  Web UI: start, stop and download the diagnostic recording from the diagnostics
  panel
status: To Do
assignee: []
created_date: '2026-11-07T16:58:00Z'
labels:
  - webui
  - diagnostics
dependencies: []
priority: medium
ordinal: 812000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07, follow-up of TASK-1667: controls in the v2 diagnostics panel to start and stop the ADR-213 recording (POST /api/v2/diag/recording/start and /stop) and to download it (GET /api/v2/diag/recording) as a .bin file, with the status from /api/v2/diag/recording/status (state, ring size, refused reason). One request at a time (ADR-165). Decode on the host with scripts/diag_decode.py.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Start, stop and download controls with a status line; a refused start shows its reason; browser test red on the old v2.js and green on the fix
- [ ] #2 Bench: a recording started and downloaded from the web UI decodes with scripts/diag_decode.py
<!-- AC:END -->
