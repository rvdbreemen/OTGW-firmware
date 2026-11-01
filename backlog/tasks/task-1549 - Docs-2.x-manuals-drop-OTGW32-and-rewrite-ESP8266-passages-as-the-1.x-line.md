---
id: TASK-1549
title: 'Docs: 2.x manuals drop OTGW32 and rewrite ESP8266 passages as the 1.x line'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-01T17:57:00Z'
updated_date: '2026-11-01T18:33:00Z'
labels:
  - docs
dependencies: []
ordinal: 703000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-01 (AskUserQuestion after TASK-1548): one cleanup task. docs/manuals EN+NL on dev name OTGW32 1-22x per chapter and ESP8266/Wemos 1-39x (counts from scripts/docs-scope-2x.sh). OTGW32 is an unannounced prototype: remove it from the public manuals, describe OT-Direct neutrally. ESP8266 appears only as 'the 1.x line'. Run as /update-docs-2x --scope full before beta.2.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 scripts/docs-scope-2x.sh reports OTGW32=0 for every EN chapter and the NL chapters grep clean
- [ ] #2 ESP8266/Wemos mentions only in 1.x-line or going-back context, each remaining one listed in the notes
- [ ] #3 EN and NL chapters stay in sync; no em dashes
- [ ] #4 Done before the v2.0.0-beta.2 tag
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ch01/h01 done: OTGW32 EN 7->0, NL 6->0; ESP8266 EN 13->5, NL 17->6 (all 1.x context or 'Wemos D1 mini socket'). NL flash_esp.py lines (158,178,112) replaced by flash_otgw.bat/.sh. Renamed headings: 'Both Platforms (ESP8266 and ESP32)' -> 'Standard Features', 'ESP32 / OTGW32 Only' -> 'Hardware-Dependent Features', NL 'Platformvergelijking: ESP8266 vs ESP32' -> 'Platformvergelijking'; no inbound links. NL RAM row dropped (no source for S3 figure); NL bug-report field 'platformtype' -> 'firmwarelijn (1.x of 2.x)'. docs/MANUAL.md in scope too (combined manual, hand-maintained, diverged from chapters: targeted edits only). scripts/docs-scope-2x.sh findings now cover NL + MANUAL.md with the widened pattern.

ch02/h02 done: OTGW32 EN 13->0, NL 16->0; ESP8266 EN 27->5, NL 27->6 (socket wording or 1.x context). Supported-hardware, pin-map and flash sections rewritten for Classic + S3 Mini/Mini Pro and flash_otgw.bat/.sh; W5500 section cut to one neutral sentence; BLE/OLED labels corrected; Dallas default doc'd as GPIO 10 from code (Sensorstypes.h:23) - wiring on S3 not verified. Fixed by lead: ch01/h01 'one-click OTA from GitHub' removed (no GitHub API call in src), broken ../docs/BREAKING_CHANGES.md link -> ../../BREAKING_CHANGES.md in ch02/h02. Out-of-scope findings: flash_otgw.sh/.bat banners and --help print 'Nodoshop OTGW32' (user-visible); docs/MANUAL.md line 13 names OTGW32 (handled in the MANUAL.md pass).

ch03/h03 done: OTGW32 EN 10->0, NL 10->0 (incl. 'OTTHING-platform'); ESP8266 4->0 both. OT-Direct labels neutral; Ethernet indicator 'only with Ethernet hardware'; one-click GitHub update sections removed (2.x Update page takes the zip, updateServerHtml.h:88-107); EN heap throttle list replaced by ADR-167 critical-only gate (helperStuff.ino:894,995); NL overview file list corrected to v2.html/v2.js/v2-bundle.css. Findings: firmware UI string v2.html:830 names OTGW32 and has an em dash (needs a firmware task + bump); classic-UI sections in ch03 still outdated (index.js ETag flow); OT-Direct mode table ch03 vs NL ch01 mode list disagree, not checked against code.
<!-- SECTION:NOTES:END -->
