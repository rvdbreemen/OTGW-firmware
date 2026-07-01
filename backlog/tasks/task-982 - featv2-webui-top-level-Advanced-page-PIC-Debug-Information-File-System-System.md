---
id: TASK-982
title: >-
  feat(v2-webui): top-level Advanced page (PIC / Debug Information / File System
  / System)
status: To Do
assignee:
  - '@claude'
created_date: '2026-07-01 22:19'
labels: []
dependencies: []
ordinal: 194000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit area advanced-page (11 findings). Add 5th nav seg data-page=advanced + page-advanced with sub-tabs pic/debug/files/system per mockup IA; relocate existing Monitor Debug content (Tools/debug/crash/picFlash) into the sub-tabs and remove the Monitor Debug tab. PIC tab: pic-head identity grid (picdeviceid/picfwtype/picfwversion), auto update-available banner via GET /api/v2/pic/update-check, pictable restyle with .newer highlight, KEEP v2 safety extras (Delete + two-click flash confirm; a bad flash bricks the PIC), Gateway settings cached PR= values prtable via GET /api/v2/pic/settings (render cached values immediately; the GET triggers a fresh ~45s PR readout cycle). Debug tab: grouped devinfo rows (Firmware/Network/Hardware/Memory+uptime/Integrations) from device/info, crashlog-ok banner when crashlog.available is false. Files tab: in-page FSexplorer from GET /api/v2/filesystem/files (dirs via path param, delete via delete param, usage bar from usedBytes/totalBytes, multipart POST /upload; keep the /FSexplorer link as escape hatch). System tab: fw-badge status row (WS/mode/sim) + System Actions (Update Firmware to /update, ReBoot and Reset Wireless with confirms, Home).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Advanced nav entry + four sub-tabs work; Monitor Debug removed without losing any existing function
- [ ] #2 PIC tab shows identity grid, auto banner, restyled table with kept safety extras, PR= values table
- [ ] #3 Debug tab shows grouped rows + crashlog-ok banner logic
- [ ] #4 Files tab lists/navigates/downloads/deletes/uploads against real LittleFS endpoints
- [ ] #5 System tab actions wired with confirm dialogs
- [ ] #6 python evaluate.py --quick green; build green
<!-- AC:END -->
