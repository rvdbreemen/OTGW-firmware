---
id: TASK-2966
title: >-
  Flash tool: download esptool without PowerShell and check its SHA-256 (CLI
  review C5)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-20T15:31:00Z'
labels:
  - tools
  - security
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1098000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2963 CLI review (docs/reviews/2026-11-20_ux-cli-tools/README.md, C5). flash_otgw.bat downloads esptool with powershell -ExecutionPolicy Bypass Invoke-WebRequest and Expand-Archive (flash_otgw.bat:110-140), the pattern TASK-1711 removed from the capture launcher after a tester's Defender quarantined it as a Trojan (capture-mqtt-debug.bat:72-75). Neither flash tool checks the SHA-256 of the esptool archive (flash_otgw.bat:110-140, flash_otgw.sh:265-270), while both capture launchers pin one. The .bat also calls PowerShell at :450 (layout check), :468 (timestamp) and :523 (settings compare, Bypass).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The esptool download in flash_otgw.bat uses curl.exe, certutil and tar.exe from System32 like capture-mqtt-debug.bat, with no PowerShell
- [ ] #2 Both flash tools pin the SHA-256 of the esptool v4.8.1 assets they download and refuse a mismatching archive with a message that names the mismatch, deleting the file
- [ ] #3 The three other PowerShell calls in flash_otgw.bat are replaced or keep a written reason in a comment
<!-- AC:END -->
