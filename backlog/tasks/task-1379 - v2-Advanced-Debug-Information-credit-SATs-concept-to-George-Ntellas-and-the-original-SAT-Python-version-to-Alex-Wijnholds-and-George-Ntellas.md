---
id: TASK-1379
title: >-
  v2 Advanced > Debug Information: credit SAT's concept to George Ntellas and
  the original SAT Python version to Alex Wijnholds and George Ntellas
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T09:31:00Z'
updated_date: '2026-10-25T20:02:00Z'
labels:
  - webui
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 512000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-25: add two fields to Advanced tab -> Debug Information: 'SAT concept by: George Ntellas' and 'Original SAT Python version: Authors Alex Wijnholds - George Ntellas'. It must be clear that the original idea of SAT is George Ntellas's.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Debug Information shows 'SAT concept by' = George Ntellas and 'Original SAT Python version' = Authors Alex Wijnholds - George Ntellas (screenshot)
- [x] #2 Fields are static text, present on every board, no new REST field needed unless the page renders only REST data
- [x] #3 Third field 'C/C++ version' = Robert + Claude (Robert 2026-10-25)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Static UI only (KISS): in v2.js renderDebugGroups() (~3634) add a group 'Credits' right after 'Firmware' with three rows: 'SAT concept by' = George Ntellas; 'Original SAT Python version' = Authors Alex Wijnholds - George Ntellas; 'C/C++ version' = Robert + Claude. No firmware/REST change: the rows are not device data. Sequencing: TASK-1366's workflow is the single writer in the dev tree right now and may touch v2.js; apply this edit after TASK-1366 is committed, then its own prerelease bump, one build for both, OTA to bench, screenshot of Advanced > Debug Information.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 Robert's follow-ups: drop 'Authors' from the Python field; C/C++ version = Robert van den Breemen + Claude + George Ntellas. Final: SAT concept by = George Ntellas; Original SAT Python version = Alex Wijnholds - George Ntellas; C/C++ version = Robert van den Breemen + Claude + George Ntellas.

2026-10-25 correction: the bench unit at 192.168.88.61 (MAC 10:20:BA:21:B4:F8) used for this task's evidence was NOT the OTGW32 (Robert: OTGW32 not connected in the 48 h before 2026-10-25 evening). It was an ESP32-S3 without PIC and without boiler running the combo image in OT-Direct mode (ADR-160 bench default). The evidence stands as 'combo image, OT-Direct mode, no boiler'; any 'OTGW32' wording in earlier notes is wrong.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Static 'Credits' group in v2 Advanced > Debug Information, right after Firmware: SAT concept by = George Ntellas; Original SAT Python version = Alex Wijnholds - George Ntellas; C/C++ version = Robert van den Breemen + Claude + George Ntellas (Robert's follow-ups applied: 'Authors' dropped, George and full name added). No firmware/REST change. Evidence: alpha.504+51c1290 OTA'd to the bench OTGW32 (app then fs, 177 settings kept); Playwright on http://192.168.88.61 Advanced > Debug Information read the three rows verbatim and a screenshot was taken (.playwright-mcp/task1379-credits.png, not shared: shows the SSID); node --check v2.js ok; build green; zip alpha.504+5e57434 posted.
<!-- SECTION:FINAL_SUMMARY:END -->
